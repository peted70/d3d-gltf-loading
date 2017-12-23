#include "pch.h"
#include "Sample3DSceneRenderer.h"

#include "..\Common\DirectXHelper.h"
#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include <filesystem> 

class Mesh
{
private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> _meshBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> _meshIndexBuffer;
};

using namespace tinygltf;
using namespace ShaderCode;
using namespace DirectX;
using namespace Windows::Foundation;

namespace fs = std::experimental::filesystem::v1;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
Sample3DSceneRenderer::Sample3DSceneRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_loadingComplete(false),
	m_degreesPerSecond(45),
	m_indexCount(0),
	m_tracking(false),
	m_deviceResources(deviceResources)
{
	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}

// Initializes view parameters when the window size changes.
void Sample3DSceneRenderer::CreateWindowSizeDependentResources()
{
	Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;
	float fovAngleY = 70.0f * XM_PI / 180.0f;

	// This is a simple example of change that can be made when the app is in
	// portrait or snapped view.
	if (aspectRatio < 1.0f)
	{
		fovAngleY *= 2.0f;
	}

	// Note that the OrientationTransform3D matrix is post-multiplied here
	// in order to correctly orient the scene to match the display orientation.
	// This post-multiplication step is required for any draw calls that are
	// made to the swap chain render target. For draw calls to other targets,
	// this transform should not be applied.

	// This sample makes use of a right-handed coordinate system using row-major matrices.
	XMMATRIX perspectiveMatrix = XMMatrixPerspectiveFovRH(
		fovAngleY,
		aspectRatio,
		0.01f,
		100.0f
		);

	XMFLOAT4X4 orientation = m_deviceResources->GetOrientationTransform3D();

	XMMATRIX orientationMatrix = XMLoadFloat4x4(&orientation);

	XMStoreFloat4x4(
		&m_constantBufferData.projection,
		XMMatrixTranspose(perspectiveMatrix * orientationMatrix)
		);

	// Eye is at (0,0.7,1.5), looking at point (0,-0.1,0) with the up-vector along the y-axis.
	static const XMVECTORF32 eye = { 0.0f, 0.7f, 1.5f, 0.0f };
	static const XMVECTORF32 at = { 0.0f, -0.1f, 0.0f, 0.0f };
	static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

	XMStoreFloat4x4(&m_constantBufferData.view, XMMatrixTranspose(XMMatrixLookAtRH(eye, at, up)));
}

// Called once per frame, rotates the cube and calculates the model and view matrices.
void Sample3DSceneRenderer::Update(DX::StepTimer const& timer)
{
	if (!m_tracking)
	{
		// Convert degrees to radians, then convert seconds to rotation angle
		float radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
		double totalRotation = timer.GetTotalSeconds() * radiansPerSecond;
		float radians = static_cast<float>(fmod(totalRotation, XM_2PI));

		Rotate(radians);

		m_pixelShaderConstantBufferData.ambientIntensity = 1.0f;
		m_pixelShaderConstantBufferData.ambientColour = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
	}
}

// Rotate the 3D cube model a set amount of radians.
void Sample3DSceneRenderer::Rotate(float radians)
{
	// Prepare to pass the updated model matrix to the shader
	XMStoreFloat4x4(&m_constantBufferData.model, XMMatrixTranspose(XMMatrixRotationY(radians)));
}

void Sample3DSceneRenderer::StartTracking()
{
	m_tracking = true;
}

// When tracking, the 3D cube can be rotated around its Y axis by tracking pointer position relative to the output screen width.
void Sample3DSceneRenderer::TrackingUpdate(float positionX)
{
	if (m_tracking)
	{
		float radians = XM_2PI * 2.0f * positionX / m_deviceResources->GetOutputSize().Width;
		Rotate(radians);
	}
}

void Sample3DSceneRenderer::StopTracking()
{
	m_tracking = false;
}

// Renders one frame using the vertex and pixel shaders.
void Sample3DSceneRenderer::Render()
{
	// Loading is asynchronous. Only draw geometry after it's loaded.
	if (!m_loadingComplete)
	{
		return;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// Prepare the constant buffer to send it to the graphics device.
	context->UpdateSubresource1(
		m_constantBuffer.Get(),
		0,
		NULL,
		&m_constantBufferData,
		0,
		0,
		0
		);

	// Prepare the constant buffer to send it to the graphics device.
	context->UpdateSubresource1(
		m_PSConstantBuffer.Get(),
		0,
		NULL,
		&m_pixelShaderConstantBufferData,
		0,
		0,
		0
	);

	// Each vertex is one instance of the VertexPositionColor struct.
	UINT stride = sizeof(VertexPositionColor);
	UINT offset = 0;
	context->IASetVertexBuffers(
		0,
		1,
		m_vertexBuffer.GetAddressOf(),
		&stride,
		&offset
		);

	context->IASetIndexBuffer(
		m_indexBuffer.Get(),
		DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
		0
		);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->IASetInputLayout(m_inputLayout.Get());

	// Attach our vertex shader.
	context->VSSetShader(
		m_vertexShader.Get(),
		nullptr,
		0
		);

	// Send the constant buffer to the graphics device.
	context->VSSetConstantBuffers1(
		0,
		1,
		m_constantBuffer.GetAddressOf(),
		nullptr,
		nullptr
		);

	// Attach our pixel shader.
	context->PSSetShader(
		m_pixelShader.Get(),
		nullptr,
		0
		);

	// Send the constant buffer to the graphics device.
	context->PSSetConstantBuffers1(
		0,
		1,
		m_PSConstantBuffer.GetAddressOf(),
		nullptr,
		nullptr
	);

	// Draw the objects.
	context->DrawIndexed(
		m_indexCount,
		0,
		0
		);


	// Draw these....
	//Microsoft::WRL::ComPtr<ID3D11Buffer>		m_meshBuffer;
	//Microsoft::WRL::ComPtr<ID3D11Buffer>		m_meshIndexBuffer;

}

inline Concurrency::task<Model> ReadModelAsync(const std::wstring& filename)
{
	using namespace Concurrency;

	// Convert std::wstring to std::string
	std::string filenameStr(filename.begin(), filename.end());
	filenameStr.assign(filename.begin(), filename.end());

	return create_task([filenameStr]()
	{
		Model model;
		TinyGLTF loader;
		std::string err;

		fs::path pth = fs::u8path(filenameStr);
		
		bool ret = false;
		auto ext = pth.extension();
		if (ext == L".glb")
		{
			ret = loader.LoadBinaryFromFile(&model, &err, filenameStr);
		}
		else
		{
			ret = loader.LoadASCIIFromFile(&model, &err, filenameStr);
		}
		if (!err.empty())
		{
			// Just print these as they are not all terminal..
			OutputDebugStringA(err.c_str());
		}
		return model;
	});
}

void Sample3DSceneRenderer::CreateDeviceDependentResources()
{
	// Load shaders asynchronously.
	auto loadVSTask = DX::ReadDataAsync(L"SampleVertexShader.cso");
	auto loadPSTask = DX::ReadDataAsync(L"SamplePixelShader.cso");

	// After the vertex shader file is loaded, create the shader and input layout.
	auto createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateVertexShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_vertexShader
				)
			);

		static const D3D11_INPUT_ELEMENT_DESC vertexDesc [] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateInputLayout(
				vertexDesc,
				ARRAYSIZE(vertexDesc),
				&fileData[0],
				fileData.size(),
				&m_inputLayout
				)
			);
	});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	auto createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData) {
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				&fileData[0],
				fileData.size(),
				nullptr,
				&m_pixelShader
				)
			);

		CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelViewProjectionConstantBuffer) , D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&constantBufferDesc,
				nullptr,
				&m_constantBuffer
				)
			);

		{
			CD3D11_BUFFER_DESC constantBufferDesc(sizeof(PixelShaderConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateBuffer(
					&constantBufferDesc,
					nullptr,
					&m_PSConstantBuffer
				)
			);
		}
	});

	// Create a task to load a GLTF file in...

#if 0
	// Create a task to load a GLTF file in...
	auto modelPath = Windows::ApplicationModel::Package::Current->InstalledLocation->Path +
		fs::path::preferred_separator + "Assets" + fs::path::preferred_separator + "BoomBox.glb";

	auto modelReadTask = ReadModelAsync(modelPath->Data()).then([this](Model model)
	{
		for (auto currScene : model.scenes)
		{
			for (auto node : currScene.nodes)
			{
				auto currNode = model.nodes[node];
				auto mesh = model.meshes[currNode.mesh];

				for (auto primitive : mesh.primitives)
				{
					auto indicesAccessor = model.accessors[primitive.indices];
					auto bufferView = model.bufferViews[indicesAccessor.bufferView];
					auto indexBuffer = model.buffers[bufferView.buffer];

					D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
					indexBufferData.pSysMem = indexBuffer.data.data();
					indexBufferData.SysMemPitch = 0;
					indexBufferData.SysMemSlicePitch = 0;
					CD3D11_BUFFER_DESC indexBufferDesc(bufferView.byteLength, D3D11_BIND_INDEX_BUFFER);
					DX::ThrowIfFailed(
						m_deviceResources->GetD3DDevice()->CreateBuffer(
							&indexBufferDesc,
							&indexBufferData,
							&m_meshIndexBuffer
						)
					);

					for (auto attribute : primitive.attributes)
					{
						const tinygltf::Accessor &accessor = model.accessors[attribute.second];

						if (attribute.first.compare("NORMAL") == 0)
						{
						}
						if (attribute.first.compare("POSITION") == 0)
						{
							auto vertexArray = model.buffers[model.bufferViews[accessor.bufferView].buffer];
							CD3D11_BUFFER_DESC vertexBufferDesc(bufferView.byteLength, D3D11_BIND_VERTEX_BUFFER);
							D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
							vertexBufferData.pSysMem = vertexArray.data.data();
							vertexBufferData.SysMemPitch = 0;
							vertexBufferData.SysMemSlicePitch = 0;
							DX::ThrowIfFailed(
								m_deviceResources->GetD3DDevice()->CreateBuffer(
									&vertexBufferDesc,
									&vertexBufferData,
									&m_meshBuffer
								)
							);
						}
						if (attribute.first.compare("TANGENT") == 0)
						{
						}
						if (attribute.first.compare("TEXTCOORD_0") == 0)
						{
						}
					}
				}
			}
		}

		//for (int i = 0; i < model.accessors.size(); i++)
		//{
		//	auto accessor = model.accessors[i];
		//	accessor.
		//	auto name = accessor.name;
		//	auto bufferView = model.bufferViews[accessor.bufferView];
		//	auto type = accessor.type;
		//}

		//// Create required resources to render the mesh for our model..
		//for (int i = 0; i < model.bufferViews.size(); i++)
		//{
		//	const tinygltf::BufferView &bufferView = model.bufferViews[i];
		//	if (bufferView.target == 0) 
		//	{
		//		//std::cout << "WARN: bufferView.target is zero" << std::endl;
		//		continue;  // Unsupported bufferView.
		//	}
		//	const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
		//	if (bufferView.target == TINYGLTF_TARGET_ARRAY_BUFFER)
		//	{
		//		// Vertex buffer
		//		CD3D11_BUFFER_DESC vertexBufferDesc(bufferView.byteLength, D3D11_BIND_VERTEX_BUFFER);
		//		D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
		//		vertexBufferData.pSysMem = buffer.data.data();
		//		vertexBufferData.SysMemPitch = 0;
		//		vertexBufferData.SysMemSlicePitch = 0;
		//		DX::ThrowIfFailed(
		//			m_deviceResources->GetD3DDevice()->CreateBuffer(
		//				&vertexBufferDesc,
		//				&vertexBufferData,
		//				&m_meshBuffer
		//			)
		//		);
		//	}
		//	else if (bufferView.target == TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER)
		//	{
		//		// index buffer
		//		D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
		//		indexBufferData.pSysMem = buffer.data.data();
		//		indexBufferData.SysMemPitch = 0;
		//		indexBufferData.SysMemSlicePitch = 0;
		//		CD3D11_BUFFER_DESC indexBufferDesc(bufferView.byteLength, D3D11_BIND_INDEX_BUFFER);
		//		DX::ThrowIfFailed(
		//			m_deviceResources->GetD3DDevice()->CreateBuffer(
		//				&indexBufferDesc,
		//				&indexBufferData,
		//				&m_meshIndexBuffer
		//			)
		//		);
		//	}
		});
#endif

	// Once both shaders are loaded, create the mesh.
	auto createCubeTask = (createPSTask && createVSTask).then([this] () {

		// Load mesh vertices. Each vertex has a position and a color.
		static const VertexPositionColor cubeVertices[] = 
		{
			{XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }, 
			{XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
			{XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
			{XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
			{XMFLOAT3( 0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
			{XMFLOAT3( 0.5f, -0.5f,  0.5f), XMFLOAT3(1.0f, 0.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
			{XMFLOAT3( 0.5f,  0.5f, -0.5f), XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
			{XMFLOAT3( 0.5f,  0.5f,  0.5f), XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		};

		D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
		vertexBufferData.pSysMem = cubeVertices;
		vertexBufferData.SysMemPitch = 0;
		vertexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&vertexBufferDesc,
				&vertexBufferData,
				&m_vertexBuffer
				)
			);

		// Load mesh indices. Each trio of indices represents
		// a triangle to be rendered on the screen.
		// For example: 0,2,1 means that the vertices with indexes
		// 0, 2 and 1 from the vertex buffer compose the 
		// first triangle of this mesh.
		static const unsigned short cubeIndices [] =
		{
			0,2,1, // -x
			1,2,3,

			4,5,6, // +x
			5,7,6,

			0,1,5, // -y
			0,5,4,

			2,6,7, // +y
			2,7,3,

			0,4,6, // -z
			0,6,2,

			1,3,7, // +z
			1,7,5,
		};

		m_indexCount = ARRAYSIZE(cubeIndices);

		D3D11_SUBRESOURCE_DATA indexBufferData = {0};
		indexBufferData.pSysMem = cubeIndices;
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;
		CD3D11_BUFFER_DESC indexBufferDesc(sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&indexBufferDesc,
				&indexBufferData,
				&m_indexBuffer
				)
			);
	});

	// Once the cube is loaded, the object is ready to be rendered.
	createCubeTask.then([this] () {
		m_loadingComplete = true;
	});
}

void Sample3DSceneRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_constantBuffer.Reset();
	m_PSConstantBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();

	m_meshBuffer.Reset();
	m_meshIndexBuffer.Reset();
}