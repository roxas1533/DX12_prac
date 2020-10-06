#pragma once
#include "D3D12AppBase.h"
#include <DirectXMath.h>

class TriangleApp :
    public D3D12AppBase
{
public:
    TriangleApp() :D3D12AppBase(){ }

    virtual void Prepare() override;
    virtual void Cleanup() override;
    virtual void MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command) override;

    struct Vertex
    {
        DirectX::XMFLOAT3 Pos;
        DirectX::XMFLOAT4 Color;

    };

    struct ShaderParameters
    {
        DirectX::XMFLOAT4X4 mtxWorld;
        DirectX::XMFLOAT4X4 mtxView;
        DirectX::XMFLOAT4X4 mtxProj;
    };

private :
    ComPtr<ID3D12Resource1> CreateBuffer(UINT buffersize, const void* intialData);
    void PrepareDescriptorHeapForCubeApp();
    ComPtr<ID3D12Resource1> m_vertexBuffer;
    ComPtr<ID3D12Resource1> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW  m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW   m_indexBufferView;
    UINT  m_indexCount;
    ComPtr<ID3D12DescriptorHeap> m_heapCbv;
    ComPtr<ID3DBlob>  m_vs, m_ps;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipeline;
    std::vector<ComPtr<ID3D12Resource1>> m_constantBuffers;
    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> m_cbViews;



};

