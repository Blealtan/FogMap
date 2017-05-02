#include "pch.h"
#include "MainRenderer.h"

#include "..\Common\DirectXHelper.h"

using namespace FogMap;

using namespace DirectX;
using namespace Windows::Foundation;

MainRenderer::MainRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_cubeIndexCount(0),
	m_deviceResources(deviceResources),
	m_lightBufferData{ XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f), XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f), XMFLOAT3(-0.6f, -0.8f, 0.0f) }
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

	static const XMVECTORF32 eye = { 0.0f, 2.0f, 4.0f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };
	XMStoreFloat4x4(&m_mvpBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
}

void MainRenderer::Update(DX::StepTimer const& timer)
{
	XMStoreFloat4x4(&m_cubeModelMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&m_floorModelMatrix, XMMatrixTranspose(XMMatrixTranslation(0.0f, -1.0f, 0.0f)));
}

void MainRenderer::Render()
{
	if (!m_loadingComplete)
		return;

	auto context = m_deviceResources->GetD3DDeviceContext();

	context->UpdateSubresource1(m_lightBuffer.Get(), 0, NULL, &m_lightBufferData, 0, 0, 0);

	UINT stride = sizeof(VertexPositionColor);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());

	context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

	context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
	context->PSSetConstantBuffers1(0, 1, m_lightBuffer.GetAddressOf(), nullptr, nullptr);

	m_mvpBufferData.model = m_cubeModelMatrix;
	context->UpdateSubresource1(m_mvpBuffer.Get(), 0, NULL, &m_mvpBufferData, 0, 0, 0);
	context->VSSetConstantBuffers1(0, 1, m_mvpBuffer.GetAddressOf(), nullptr, nullptr);
	context->DrawIndexed(m_cubeIndexCount, 0, 0);

	m_mvpBufferData.model = m_floorModelMatrix;
	context->UpdateSubresource1(m_mvpBuffer.Get(), 0, NULL, &m_mvpBufferData, 0, 0, 0);
	context->VSSetConstantBuffers1(0, 1, m_mvpBuffer.GetAddressOf(), nullptr, nullptr);
	context->DrawIndexed(m_floorIndexCount, m_cubeIndexCount, 0);
}

void MainRenderer::CreateDeviceDependentResources()
{
	auto loadVSTask = DX::ReadDataAsync(L"SceneVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"ScenePixelShader.cso");

	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateVertexShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_vertexShader
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
	});

	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreatePixelShader(
			&fileData[0],
			fileData.size(),
			nullptr,
			&m_pixelShader
		));
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(ModelViewProjectionConstantBuffer), D3D11_BIND_CONSTANT_BUFFER),
			nullptr,
			&m_mvpBuffer
		));
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(LightBuffer), D3D11_BIND_CONSTANT_BUFFER),
			nullptr,
			&m_lightBuffer
		));
	});

	auto createCubeTask = (createPSTask && createVSTask).then([this]() {
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
		vertexBufferData.pSysMem = cubeVertices;
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER),
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

		m_floorIndexCount = 6;
		m_cubeIndexCount = ARRAYSIZE(cubeIndices) - m_floorIndexCount;

		D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
		indexBufferData.pSysMem = cubeIndices;
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
		DX::ThrowIfFailed(m_deviceResources->GetD3DDevice()->CreateBuffer(
			&CD3D11_BUFFER_DESC(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER),
			&indexBufferData,
			&m_indexBuffer
		));
	});

	createCubeTask.then([this]() {
		m_loadingComplete = true;
	});
}

void MainRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_mvpBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
}
