#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"

namespace FogMap
{
	class MainRenderer
	{
	public:
		MainRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateDeviceDependentResources();
		void CreateWindowSizeDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void Render();

	private:
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_mvpBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>		m_lightBuffer;

		DirectX::XMFLOAT4X4 m_cubeModelMatrix;
		DirectX::XMFLOAT4X4 m_floorModelMatrix;
		ModelViewProjectionConstantBuffer m_mvpBufferData;
		LightBuffer m_lightBufferData;
		uint32	m_cubeIndexCount;
		uint32	m_floorIndexCount;

		bool	m_loadingComplete;
	};
}

