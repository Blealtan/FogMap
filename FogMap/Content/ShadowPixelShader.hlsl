
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 depthPos : TEXTURE0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float d = input.depthPos.z / input.depthPos.w;
	return float4(d, d, d, 1.0f);
}
