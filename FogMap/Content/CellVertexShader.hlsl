cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	matrix model;
	matrix view;
	matrix projection;
	matrix lightView;
	matrix lightProjection;
};

struct VertexShaderInput
{
	float3 pos : POSITION;
	float3 color : COLOR0;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
	float4 lightViewPos : TEXCOORD0;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	output.pos = mul(mul(mul(float4(input.pos, 1.0f), model), view), projection);
	output.color = input.color;
	output.lightViewPos = mul(mul(mul(float4(input.pos, 1.0f), model), lightView), lightProjection);
	return output;
}
