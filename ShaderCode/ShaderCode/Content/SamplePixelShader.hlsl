cbuffer PixelShaderConstantBuffer : register(b0)
{
	float4 ambientColour;
	float ambientIntensity;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
	return /*0.5f * float4(input.color, 1.0f) + 0.5f */ ambientIntensity * ambientColour;
}
