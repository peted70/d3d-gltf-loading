#pragma once

namespace ShaderCode
{
	// Constant buffer used to send MVP matrices to the vertex shader.
	struct ModelViewProjectionConstantBuffer
	{
		DirectX::XMFLOAT4X4 model;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	struct PixelShaderConstantBuffer
	{
		DirectX::XMFLOAT4 ambientColour;
		float ambientIntensity;
		float pad1;
		float pad2;
		float pad3;
	};

	// Used to send per-vertex data to the vertex shader.
	struct VertexPositionColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 color;
		DirectX::XMFLOAT4 normal;
	};
}