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
	float3 norm : NORMAL;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 depthPos : TEXTURE0;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	output.pos = mul(mul(mul(float4(input.pos, 1.0f), model), lightView), lightProjection);
	output.depthPos = output.pos;

	return output;
}
