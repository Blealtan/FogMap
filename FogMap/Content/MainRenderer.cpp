#include "pch.h"
#include "MainRenderer.h"

#include "..\Common\DirectXHelper.h"

#include <sstream>
#include <unordered_map>

using namespace FogMap;

using namespace DirectX;
using namespace Windows::Foundation;

MainRenderer::MainRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_indexCount(0),
	m_deviceResources(deviceResources),
	m_lightBufferData{ XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f), XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f), XMFLOAT3(-sqrt(3.0f / 4), -sqrt(1.0f / 4), 0) }
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

void MainRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	if (aspectRatio < 1.0f) fovAngleY *= 2.0f;

	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(fovAngleY, aspectRatio, 0.01f, 100.0f);
	XMMATRIX orientationMatrix = XMLoadFloat4x4(&m_deviceResources->GetOrientationTransform3D());
	XMStoreFloat4x4(&m_mvpBufferData.projection, XMMatrixTranspose(perspectiveMatrix * orientationMatrix));

	static const XMVECTORF32 eye = { 0.0f, 5.0f, 10.0f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMStoreFloat4x4(&m_mvpBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));

	XMStoreFloat4x4(&m_mvpBufferData.model, XMMatrixTranspose(XMMatrixRotationY(-XM_PI / 2)));

	XMStoreFloat4x4(&m_mvpBufferData.lightView, XMMatrixTranspose(XMMatrixLookAtRH(
		-12.0f * XMLoadFloat3(&m_lightBufferData.lightDirection), XMVECTOR{ 0.0f, 0.0f, 0.0f }, XMVECTOR{ 0.0f, 0.1f, 0.0f })));
	XMStoreFloat4x4(&m_mvpBufferData.lightProjection, XMMatrixTranspose(XMMatrixOrthographicRH(12.0f, 12.0f, 0.0f, 24.0f)));
}

void MainRenderer::Update(DX::StepTimer const& timer)
{
}

void MainRenderer::Render()
{
	if (!m_loadingComplete)
		return;

	auto context = m_deviceResources->GetD3DDeviceContext();

	context->UpdateSubresource1(m_sceneLightingBuffer.Get(), 0, NULL, &m_lightBufferData, 0, 0, 0);

	UINT stride = sizeof(VertexPositionColor);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());

	// Render shadow map
	context->OMSetRenderTargets(1, m_shadowRTV.GetAddressOf(), m_shadowDSV.Get());
	static const D3D11_VIEWPORT vp{ 0.0f, 0.0f, 1024.0f, 1024.0f, 0.0f, 1.0f };
	context->RSSetViewports(1, &vp);
	static const float color[]{ 0.0f, 0.0f, 0.0f, 1.0f };
	context->ClearRenderTargetView(m_shadowRTV.Get(), color);
	context->ClearDepthStencilView(m_shadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	context->VSSetShader(m_shadowVertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_shadowPixelShader.Get(), nullptr, 0);

	context->UpdateSubresource1(m_mvpBuffer.Get(), 0, NULL, &m_mvpBufferData, 0, 0, 0);
	context->VSSetConstantBuffers1(0, 1, m_mvpBuffer.GetAddressOf(), nullptr, nullptr);
	context->DrawIndexed(m_indexCount, 0, 0);

	// Render scene
	auto targets = (ID3D11RenderTargetView*)m_deviceResources->GetBackBufferRenderTargetView();
	context->OMSetRenderTargets(1, &targets, m_deviceResources->GetDepthStencilView());
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	context->VSSetShader(m_sceneVertexShader.Get(), nullptr, 0);

	context->PSSetShader(m_scenePixelShader.Get(), nullptr, 0);
	context->PSSetShaderResources(0, 1, m_shadowSRV.GetAddressOf());
	context->PSSetSamplers(0, 1, m_sceneSampler.GetAddressOf());
	context->PSSetConstantBuffers1(0, 1, m_sceneLightingBuffer.GetAddressOf(), nullptr, nullptr);

	context->UpdateSubresource1(m_mvpBuffer.Get(), 0, NULL, &m_mvpBufferData, 0, 0, 0);
	context->VSSetConstantBuffers1(0, 1, m_mvpBuffer.GetAddressOf(), nullptr, nullptr);
	context->DrawIndexed(m_indexCount, 0, 0);
	ID3D11ShaderResourceView *null_srv = nullptr;
	context->PSSetShaderResources(0, 1, &null_srv);
}

void MainRenderer::CreateDeviceDependentResources()
{
	auto loadVSTask = DX::ReadDataAsync(L"SceneVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"ScenePixelShader.cso");
	auto loadShadowVSTask = DX::ReadDataAsync(L"ShadowVertexShader.cso");
	auto loadShadowPSTask = DX::ReadDataAsync(L"ShadowPixelShader.cso");

	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_sceneVertexShader
		));

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateInputLayout(
			vertexDesc,
			ARRAYSIZE(vertexDesc),
			&fileData[0],
			fileData.size(),
			&m_inputLayout
		));
		static const float input[]{ 0.0f, 0.0f, 0.0f, 0.0f };
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateSamplerState(
			&CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_LINEAR,
				D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP,
				0.0f, 1, D3D11_COMPARISON_ALWAYS, input, 0, D3D11_FLOAT32_MAX),
			&m_sceneSampler
		));
	});

	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_scenePixelShader
		));
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER),
			nullptr,
			&m_mvpBuffer
		));
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(LightBuffer), D3D11_BIND_CONSTANT_BUFFER),
			nullptr,
			&m_sceneLightingBuffer
		));
	});

	auto createShadowVSTask = loadShadowVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_shadowVertexShader
		));
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture2D(
			&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_D24_UNORM_S8_UINT, 1024, 1024, 1, 1, D3D11_BIND_DEPTH_STENCIL),
			nullptr,
			&m_shadowDepthStencilBuffer
		));
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateDepthStencilView(
			m_shadowDepthStencilBuffer.Get(),
			&CD3D11_DEPTH_STENCIL_VIEW_DESC(m_shadowDepthStencilBuffer.Get(), D3D11_DSV_DIMENSION_TEXTURE2D),
			&m_shadowDSV
		));
	});

	auto createShadowPSTask = loadShadowPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_shadowPixelShader
		));
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateTexture2D(
			&CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R32G32B32A32_FLOAT, 1024, 1024, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE),
			nullptr,
			&m_shadowTexture
		));
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateRenderTargetView(
			m_shadowTexture.Get(),
			&CD3D11_RENDER_TARGET_VIEW_DESC(m_shadowTexture.Get(), D3D11_RTV_DIMENSION_TEXTURE2D),
			&m_shadowRTV
		));
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateShaderResourceView(
			m_shadowTexture.Get(),
			&CD3D11_SHADER_RESOURCE_VIEW_DESC(m_shadowTexture.Get(), D3D11_SRV_DIMENSION_TEXTURE2D),
			&m_shadowSRV
		));
	});

	auto loadCubeTask = DX::ReadDataAsync(L"model.obj").then([this](const std::vector<byte>& fileData) {
		std::stringstream ss;
		for (auto c : fileData) ss << c;
		std::vector<XMFLOAT3> vert;
		std::vector<XMFLOAT3> norm;
		std::unordered_map<int, std::pair<int, VertexPositionColor>> vnBuffer;

		vertices.clear();
		indices.clear();

		while (!ss.eof())
		{
			std::string line;
			std::getline(ss, line);
			std::istringstream iss(line);
			std::string head; iss >> head;
			if (head == "v")
			{
				XMFLOAT3 v;
				iss >> v.x >> v.y >> v.z;
				vert.push_back(v);
			}
			else if (head == "vn")
			{
				XMFLOAT3 vn;
				iss >> vn.x >> vn.y >> vn.z;
				XMFLOAT3 vn_n;
				XMStoreFloat3(&vn_n, XMVector3Normalize(XMLoadFloat3(&vn)));
				norm.push_back(vn_n);
			}
			else if (head == "f")
			{
				int ind[3];
				for (int i = 0; i < 3; ++i)
				{
					int vi, ni;
					iss >> vi; iss.get(); iss.get(); iss >> ni;
					ind[i] = vi * norm.size() + ni;
					if (vnBuffer.count(ind[i]) == 0)
						vnBuffer[ind[i]] = std::make_pair(vnBuffer.size(), VertexPositionColor{ vert[vi - 1], XMFLOAT3(0.9f, 0.9f, 0.9f), norm[ni - 1] });
				}
				auto v0 = XMLoadFloat3(&vnBuffer[ind[0]].second.pos);
				auto v1 = XMLoadFloat3(&vnBuffer[ind[1]].second.pos);
				auto v2 = XMLoadFloat3(&vnBuffer[ind[2]].second.pos);
				auto dot = XMVector3Dot(XMVector3Cross(v2 - v0, v1 - v0), XMLoadFloat3(&vnBuffer[ind[1]].second.norm));
				float dot_value;
				XMStoreFloat(&dot_value, dot);
				if (dot_value < 0) std::swap(ind[1], ind[2]);
				for (int i = 0; i < 3; ++i) indices.push_back(vnBuffer[ind[i]].first);
			}
		}

		vertices.resize(vnBuffer.size());
		for (auto p : vnBuffer)
			vertices[p.second.first] = p.second.second;
	});

	auto createCubeTask = (createPSTask && createVSTask && createShadowVSTask && createShadowPSTask && loadCubeTask).then([this]() {
		static const VertexPositionColor cubeVertices[] =
		{
			// x = -0.5
			{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
			// x = 0.5
			{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(+1.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT3(+1.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT3(+1.0f, 0.0f, 0.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(+1.0f, 0.0f, 0.0f) },
			// y = -0.5
			{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
			{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
			// y = 0.5
			{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, +1.0f, 0.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, +1.0f, 0.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, +1.0f, 0.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, +1.0f, 0.0f) },
			// z = -0.5
			{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
			// z = 0.5
			{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, +1.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, +1.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, +1.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, +1.0f) },
			// Floor
			{ XMFLOAT3(-4.0f, 0.0f, -4.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
			{ XMFLOAT3(-4.0f, 0.0f, +4.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
			{ XMFLOAT3(+4.0f, 0.0f, -4.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
			{ XMFLOAT3(+4.0f, 0.0f, +4.0f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
		};

		D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
		vertexBufferData.pSysMem = vertices.data();
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(VertexPositionColor) * vertices.size(), D3D11_BIND_VERTEX_BUFFER),
			&vertexBufferData,
			&m_vertexBuffer
		));

		static const unsigned short cubeIndices[] =
		{
			0,2,1, // -x
			1,2,3,

			4,5,6, // +x
			5,7,6,

			8,9,10, // -y
			10,9,11,

			12,14,13, // +y
			15,13,14,

			16,18,17, // -z
			17,18,19,

			20,21,22, // +z
			21,23,22,

			24,26,25, // Floor
			25,26,27,
		};
		m_indexCount = indices.size();

		D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
		indexBufferData.pSysMem = indices.data();
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(unsigned short) * indices.size(), D3D11_BIND_INDEX_BUFFER),
			&indexBufferData,
			&m_indexBuffer
		));
	});

	createCubeTask.then([this]() { m_loadingComplete = true; });
}

void MainRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
	m_sceneVertexShader.Reset();
	m_inputLayout.Reset();
	m_scenePixelShader.Reset();
	m_mvpBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
}
