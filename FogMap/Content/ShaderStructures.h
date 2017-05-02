#pragma once

namespace FogMap
{
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	struct LightBuffer
	{
		DirectX::XMFLOAT4 diffuseColor;
		DirectX::XMFLOAT4 ambientColor;
		DirectX::XMFLOAT3 lightDirection;
		float padding;
	};

	struct VertexPositionColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 color;
		DirectX::XMFLOAT3 norm;
		float padding;
	};
}