cbuffer LightBuffer
{
	float4 diffuseColor;
	float4 ambientColor;
	float3 lightDirection;
	float padding;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
	float3 norm : NORMAL;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 baseColor = float4(input.color, 1.0f);
	float4 lightColor = saturate(diffuseColor * saturate(dot(input.norm, -lightDirection)) + ambientColor);
	return lightColor * baseColor;
}
