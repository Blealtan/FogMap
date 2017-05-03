Texture2D shadowMap : register(t0);
SamplerState samplerClamp : register(s0);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
	float4 lightViewPos : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 projectTexCoord = float2(input.lightViewPos.x / input.lightViewPos.w / 2.0f + 0.5f, -input.lightViewPos.y / input.lightViewPos.w / 2.0f + 0.5f);
	float visibility = 0.025f;

	if ((saturate(projectTexCoord.x) == projectTexCoord.x) && (saturate(projectTexCoord.y) == projectTexCoord.y))
	{
		float bias = 0.001;
		float selfDepth = input.lightViewPos.z / input.lightViewPos.w - bias;

		float2 offset[5] = { float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(-1.0f, 0.0f), float2(0.0f, 1.0f), float2(0.0f, -1.0f) };
		for (int i = 0; i < 5; ++i)
			if (selfDepth > shadowMap.Sample(samplerClamp, projectTexCoord + offset[i] / 1024.0f).r)
				visibility -= 0.005f;
	}

	return float4(input.color, visibility);
}
