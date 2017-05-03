#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "..\Common\StepTimer.h"

#include <vector>

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

		Microsoft::WRL::ComPtr<ID3D11Buffer>				m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>				m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11InputLayout>			m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer>				m_mvpBuffer;

		Microsoft::WRL::ComPtr<ID3D11Buffer>				m_sceneLightingBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>			m_sceneVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>			m_scenePixelShader;
		Microsoft::WRL::ComPtr<ID3D11SamplerState>			m_sceneSampler;

		Microsoft::WRL::ComPtr<ID3D11Texture2D>				m_shadowTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView>		m_shadowRTV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_shadowSRV;
		Microsoft::WRL::ComPtr<ID3D11VertexShader>			m_shadowVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader>			m_shadowPixelShader;
		Microsoft::WRL::ComPtr<ID3D11Texture2D>				m_shadowDepthStencilBuffer;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView>		m_shadowDSV;

		ModelViewProjectionConstantBuffer m_mvpBufferData;
		LightBuffer m_lightBufferData;
		uint32	m_indexCount;

		std::vector<VertexPositionColor> vertices;
		std::vector<unsigned short> indices;

		bool	m_loadingComplete;
	};
}
