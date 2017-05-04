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
	m_lightBufferData{ XMFLOAT4(0.8f, 0.8f, 0.7f, 1.0f), XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f), XMFLOAT3(-sqrt(3.0f / 4), -sqrt(1.0f / 4), 0) }
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

	UINT stride = sizeof(VertexPositionColorNormal);
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
	XMStoreFloat4x4(&m_mvpBufferData.model, XMMatrixTranspose(XMMatrixRotationY(-XM_PI / 2)));
	context->UpdateSubresource1(m_mvpBuffer.Get(), 0, NULL, &m_mvpBufferData, 0, 0, 0);
	context->VSSetConstantBuffers1(0, 1, m_mvpBuffer.GetAddressOf(), nullptr, nullptr);

	context->PSSetShader(m_shadowPixelShader.Get(), nullptr, 0);
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

	context->DrawIndexed(m_indexCount, 0, 0);

	float factor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_deviceResources->GetD3DDeviceContext()->OMSetBlendState(m_blendState.Get(), factor, 0xffffffff);

	// Render fog cells
	stride = sizeof(VertexPositionColor);
	offset = 0;
	context->IASetVertexBuffers(0, 1, m_cellVertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_cellIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_cellInputLayout.Get());

	context->VSSetShader(m_cellVertexShader.Get(), nullptr, 0);
	XMStoreFloat4x4(&m_mvpBufferData.model, XMMatrixIdentity());
	context->UpdateSubresource1(m_mvpBuffer.Get(), 0, NULL, &m_mvpBufferData, 0, 0, 0);
	context->VSSetConstantBuffers1(0, 1, m_mvpBuffer.GetAddressOf(), nullptr, nullptr);

	context->PSSetShader(m_cellPixelShader.Get(), nullptr, 0);
	context->PSSetShaderResources(0, 1, m_shadowSRV.GetAddressOf());
	context->PSSetSamplers(0, 1, m_sceneSampler.GetAddressOf());

	context->DrawIndexed(m_cellIndexCount, 0, 0);

	// Release shadow map SRV and reset blend state
	ID3D11ShaderResourceView *null_srv = nullptr;
	context->PSSetShaderResources(0, 1, &null_srv);

	m_deviceResources->GetD3DDeviceContext()->OMSetBlendState(nullptr, factor, 0xffffffff);
}

void MainRenderer::CreateDeviceDependentResources()
{
	auto loadSceneVSTask = DX::ReadDataAsync(L"SceneVertexShader.cso");
	auto loadScenePSTask = DX::ReadDataAsync(L"ScenePixelShader.cso");
	auto createSceneVSTask = loadSceneVSTask.then([this](const std::vector<byte>& fileData) {
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
	auto createScenePSTask = loadScenePSTask.then([this](const std::vector<byte>& fileData) {
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

	auto loadShadowVSTask = DX::ReadDataAsync(L"ShadowVertexShader.cso");
	auto loadShadowPSTask = DX::ReadDataAsync(L"ShadowPixelShader.cso");
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

	auto loadCellVSTask = DX::ReadDataAsync(L"CellVertexShader.cso");
	auto loadCellPSTask = DX::ReadDataAsync(L"CellPixelShader.cso");
	auto createCellVSTask = loadCellVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_cellVertexShader
		));

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateInputLayout(
			vertexDesc,
			ARRAYSIZE(vertexDesc),
			&fileData[0],
			fileData.size(),
			&m_cellInputLayout
		));
	});
	auto createCellPSTask = loadCellPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_cellPixelShader
		));
	});
	auto createCellTask = (createCellVSTask && createCellPSTask).then([this]() {
		constexpr int fogMapSize = 64;
		VertexPositionColor cellVertices[fogMapSize * 4];
		for (int z = 0; z < fogMapSize; ++z)
		{
			cellVertices[z * 4 + 0] = { XMFLOAT3(-4.5f, 0.0f, z * 9.0f / fogMapSize - 4.5f),
				XMFLOAT3(m_lightBufferData.diffuseColor.x, m_lightBufferData.diffuseColor.y, m_lightBufferData.diffuseColor.z) };
			cellVertices[z * 4 + 1] = { XMFLOAT3(4.5f, 0.0f, z * 9.0f / fogMapSize - 4.5f),
				XMFLOAT3(m_lightBufferData.diffuseColor.x, m_lightBufferData.diffuseColor.y, m_lightBufferData.diffuseColor.z) };
			cellVertices[z * 4 + 2] = { XMFLOAT3(-4.5f, 4.0f, z * 9.0f / fogMapSize - 4.5f),
				XMFLOAT3(m_lightBufferData.diffuseColor.x, m_lightBufferData.diffuseColor.y, m_lightBufferData.diffuseColor.z) };
			cellVertices[z * 4 + 3] = { XMFLOAT3(4.5f, 4.0f, z * 9.0f / fogMapSize - 4.5f),
				XMFLOAT3(m_lightBufferData.diffuseColor.x, m_lightBufferData.diffuseColor.y, m_lightBufferData.diffuseColor.z) };
		}
		D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
		vertexBufferData.pSysMem = cellVertices;
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(cellVertices), D3D11_BIND_VERTEX_BUFFER),
			&vertexBufferData,
			&m_cellVertexBuffer
		));

		unsigned short cellIndices[fogMapSize * 6];
		for (int z = 0; z < fogMapSize; ++z)
		{
			cellIndices[z * 6 + 0] = z * 4 + 0;
			cellIndices[z * 6 + 1] = z * 4 + 2;
			cellIndices[z * 6 + 2] = z * 4 + 1;
			cellIndices[z * 6 + 3] = z * 4 + 1;
			cellIndices[z * 6 + 4] = z * 4 + 2;
			cellIndices[z * 6 + 5] = z * 4 + 3;
		}
		m_cellIndexCount = ARRAYSIZE(cellIndices);
		D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
		indexBufferData.pSysMem = cellIndices;
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(cellIndices), D3D11_BIND_INDEX_BUFFER),
			&indexBufferData,
			&m_cellIndexBuffer
		));
	});

	auto loadCubeTask = DX::ReadDataAsync(L"model.obj").then([this](const std::vector<byte>& fileData) {
		std::stringstream ss;
		for (auto c : fileData) ss << c;
		std::vector<XMFLOAT3> vert;
		std::vector<XMFLOAT3> norm;
		std::unordered_map<int, std::pair<int, VertexPositionColorNormal>> vnBuffer;

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
						vnBuffer[ind[i]] = std::make_pair(vnBuffer.size(), VertexPositionColorNormal{ vert[vi - 1], XMFLOAT3(0.9f, 0.9f, 0.9f), norm[ni - 1] });
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
	auto createCubeTask = (createScenePSTask && createSceneVSTask && createShadowVSTask && createShadowPSTask && loadCubeTask).then([this]() {
		D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
		vertexBufferData.pSysMem = vertices.data();
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(VertexPositionColorNormal) * vertices.size(), D3D11_BIND_VERTEX_BUFFER),
			&vertexBufferData,
			&m_vertexBuffer
		));

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

	createCubeTask.then([this]() {
		D3D11_BLEND_DESC desc;
		desc.AlphaToCoverageEnable = FALSE;
		desc.IndependentBlendEnable = FALSE;
		const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
		{
			TRUE,
			D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
			D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD,
			D3D11_COLOR_WRITE_ENABLE_ALL,
		};
		for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			desc.RenderTarget[i] = defaultRenderTargetBlendDesc;
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBlendState(&desc, &m_blendState));
		m_loadingComplete = true;
	});
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
