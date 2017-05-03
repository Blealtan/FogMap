Texture2D shadowMap : register(t0);
SamplerState samplerClamp : register(s0);

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
	float4 lightViewPos : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 baseColor = float4(input.color, 1.0f);
	float cosTheta = dot(input.norm, -lightDirection);
	float2 projectTexCoord = float2(input.lightViewPos.x / input.lightViewPos.w / 2.0f + 0.5f, -input.lightViewPos.y / input.lightViewPos.w / 2.0f + 0.5f);
	float visibility = 1.0f;

	if ((saturate(projectTexCoord.x) == projectTexCoord.x) && (saturate(projectTexCoord.y) == projectTexCoord.y))
	{
		float bias = 0.0005*tan(acos(cosTheta));
		float selfDepth = input.lightViewPos.z / input.lightViewPos.w - bias;

		float2 offset[5] = { float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(-1.0f, 0.0f), float2(0.0f, 1.0f), float2(0.0f, -1.0f) };
		for (int i = 0; i < 5; ++i)
			if (selfDepth > shadowMap.Sample(samplerClamp, projectTexCoord + offset[i]/1024.0f).r)
				visibility -= 0.15f;
	}

	float4 lightColor = saturate(ambientColor + visibility * diffuseColor * saturate(cosTheta));
	return lightColor * baseColor;
}
