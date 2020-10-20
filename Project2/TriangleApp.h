#pragma once
#include "D3D12AppBase.h"
#include <DirectXMath.h>
#include < DirectXTex.h>
#include <WICTextureLoader.h>

class TriangleApp :
	public D3D12AppBase
{
public:
	int x = 0;
	TriangleApp() :D3D12AppBase() { }

	virtual void Prepare() override;
	virtual void Cleanup() override;
	virtual void MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command) override;

	struct Vertex
	{
		DirectX::XMFLOAT3 Pos;
		DirectX::XMFLOAT4 Color;
		DirectX::XMFLOAT2 UV;

	};

	struct ShaderParameters
	{
		DirectX::XMFLOAT4X4 mtxWorld;
		DirectX::XMFLOAT4X4 mtxView;
		DirectX::XMFLOAT4X4 mtxProj;
	};

private:
	ComPtr<ID3D12Resource1> CreateBuffer(UINT buffersize, const void* intialData);
	ComPtr<ID3D12Resource> CreateTexture(const wchar_t* name);
	ComPtr<ID3D12Resource1> CreateTexture(const std::string& fileName);
	void WaitGPU();
	void PrepareDescriptorHeapForCubeApp();
	ComPtr<ID3D12Resource1> m_vertexBuffer;
	ComPtr<ID3D12Resource1> m_indexBuffer;
	ComPtr<ID3D12Resource> m_texture;
	ComPtr<ID3D12Resource> textureUploadHeap;
	D3D12_VERTEX_BUFFER_VIEW  m_vertexBufferView;
	ComPtr<ID3D12DescriptorHeap> m_heapSampler;
	UINT  m_samplerDescriptorSize;

	D3D12_INDEX_BUFFER_VIEW   m_indexBufferView;
	UINT  m_indexCount;
	ComPtr<ID3D12DescriptorHeap> m_heapCbv;
	ComPtr<ID3DBlob>  m_vs, m_ps;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipeline;
	std::vector<ComPtr<ID3D12Resource1>> m_constantBuffers;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_cbViews;
	D3D12_GPU_DESCRIPTOR_HANDLE m_sampler;
	D3D12_GPU_DESCRIPTOR_HANDLE m_srv;
};
