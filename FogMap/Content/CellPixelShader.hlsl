Texture2D shadowMap : register(t0);
SamplerState samplerClamp : register(s0);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
	float4 lightViewPos : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 projectTexCoord = float2(input.lightViewPos.x / input.lightViewPos.w / 2.0f + 0.5f, -input.lightViewPos.y / input.lightViewPos.w / 2.0f + 0.5f);
	float visibility = 1.0f;

	if ((saturate(projectTexCoord.x) == projectTexCoord.x) && (saturate(projectTexCoord.y) == projectTexCoord.y))
	{
		float bias = 0.001;
		float selfDepth = input.lightViewPos.z / input.lightViewPos.w - bias;
		if (selfDepth > shadowMap.Sample(samplerClamp, projectTexCoord).r)
			visibility = 0.0f;
	}

	input.color.a *= visibility;
	return input.color;
}
