#include "ModelApp.h"
#include "streamreader.h"
#include <DirectXTex.h>

using namespace DirectX;
using namespace std;

void ModelApp::Prepare()
{
    // ���f���f�[�^�̓ǂݍ���
    auto modelFilePath = experimental::filesystem::path("alicia-solid.vrm");
    if (modelFilePath.is_relative())
    {
        auto current = experimental::filesystem::current_path();
        current /= modelFilePath;
        current.swap(modelFilePath);
    }
    auto reader = make_unique<StreamReader>(modelFilePath.parent_path());
    auto glbStream = reader->GetInputStream(modelFilePath.filename().u8string());
    auto glbResourceReader = make_shared<Microsoft::glTF::GLBResourceReader>(std::move(reader), std::move(glbStream));
    auto document = Microsoft::glTF::Deserialize(glbResourceReader->GetJson());

    // �}�e���A��������SRV�f�B�X�N���v�^���K�v�ɂȂ�̂ł����ŏ���.
    PrepareDescriptorHeapForModelApp(UINT(document.materials.Elements().size()));

    MakeModelGeometry(document, glbResourceReader);
    MakeModelMaterial(document, glbResourceReader);

    // �V�F�[�_�[���R���p�C��.
    HRESULT hr;
    ComPtr<ID3DBlob> errBlob;
    hr = CompileShaderFromFile(L"shaderVS.hlsl", L"vs_6_0", m_vs, errBlob);
    if (FAILED(hr))
    {
        OutputDebugStringA((const char*)errBlob->GetBufferPointer());
    }
    hr = CompileShaderFromFile(L"shaderOpaquePS.hlsl", L"ps_6_0", m_psOpaque, errBlob);
    if (FAILED(hr))
    {
        OutputDebugStringA((const char*)errBlob->GetBufferPointer());
    }
    hr = CompileShaderFromFile(L"shaderAlphaPS.hlsl", L"ps_6_0", m_psAlpha, errBlob);
    if (FAILED(hr))
    {
        OutputDebugStringA((const char*)errBlob->GetBufferPointer());
    }

    m_srvDescriptorBase = FrameBufferCount;

    CD3DX12_DESCRIPTOR_RANGE cbv, srv, sampler;
    cbv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0 ���W�X�^
    srv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0 ���W�X�^
    sampler.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0); // s0 ���W�X�^

    CD3DX12_ROOT_PARAMETER rootParams[3];
    rootParams[0].InitAsDescriptorTable(1, &cbv, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParams[1].InitAsDescriptorTable(1, &srv, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParams[2].InitAsDescriptorTable(1, &sampler, D3D12_SHADER_VISIBILITY_PIXEL);

    // ���[�g�V�O�l�`���̍\�z
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.Init(
        _countof(rootParams), rootParams,   //pParameters
        0, nullptr,   //pStaticSamplers
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );
    ComPtr<ID3DBlob> signature;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &errBlob);
    if (FAILED(hr))
    {
        throw std::runtime_error("D3D12SerializeRootSignature faild.");
    }
    // RootSignature �̐���
    hr = m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)
    );
    if (FAILED(hr))
    {
        throw std::runtime_error("CrateRootSignature failed.");
    }

    m_pipelineOpaque = CreateOpaquePSO();
    m_pipelineAlpha = CreateAlphaPSO();


    // �萔�o�b�t�@/�萔�o�b�t�@�r���[�̐���
    m_constantBuffers.resize(FrameBufferCount);
    m_cbViews.resize(FrameBufferCount);
    for (UINT i = 0; i < FrameBufferCount; ++i)
    {
        UINT bufferSize = sizeof(ShaderParameters) + 255 & ~255;
        m_constantBuffers[i] = CreateBuffer(bufferSize, nullptr);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc{};
        cbDesc.BufferLocation = m_constantBuffers[i]->GetGPUVirtualAddress();
        cbDesc.SizeInBytes = bufferSize;
        CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(m_heapSrvCbv->GetCPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase + i, m_srvcbvDescriptorSize);
        m_device->CreateConstantBufferView(&cbDesc, handleCBV);

        m_cbViews[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSrvCbv->GetGPUDescriptorHandleForHeapStart(), ConstantBufferDescriptorBase + i, m_srvcbvDescriptorSize);
    }

    // �T���v���[�̐���
    D3D12_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D12_ENCODE_BASIC_FILTER(
        D3D12_FILTER_TYPE_LINEAR, // min
        D3D12_FILTER_TYPE_LINEAR, // mag
        D3D12_FILTER_TYPE_LINEAR, // mip
        D3D12_FILTER_REDUCTION_TYPE_STANDARD);
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.MaxLOD = FLT_MAX;
    samplerDesc.MinLOD = -FLT_MAX;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

    // �T���v���[�p�f�B�X�N���v�^�q�[�v��0�Ԗڂ��g�p����
    auto descriptorSampler = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heapSampler->GetCPUDescriptorHandleForHeapStart(), SamplerDescriptorBase, m_samplerDescriptorSize);
    m_device->CreateSampler(&samplerDesc, descriptorSampler);
    m_sampler = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapSampler->GetGPUDescriptorHandleForHeapStart(), SamplerDescriptorBase, m_samplerDescriptorSize);
}

void ModelApp::Cleanup()
{
    WaitGPU();
}

void ModelApp::MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command)
{
    using namespace DirectX;
    using namespace Microsoft::glTF;

    // �e�s��̃Z�b�g.
    ShaderParameters shaderParams;
    XMStoreFloat4x4(&shaderParams.mtxWorld, XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(0.0f)));
    auto mtxView = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 1.5f, -1.0f, 0.0f),
        XMVectorSet(0.0f, 1.25f, 0.0f, 0.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );
    auto mtxProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), m_viewport.Width / m_viewport.Height, 0.1f, 100.0f);
    XMStoreFloat4x4(&shaderParams.mtxView, XMMatrixTranspose(mtxView));
    XMStoreFloat4x4(&shaderParams.mtxProj, XMMatrixTranspose(mtxProj));

    // �萔�o�b�t�@�̍X�V.
    auto& constantBuffer = m_constantBuffers[m_frameIndex];
    {
        void* p;
        CD3DX12_RANGE range(0, 0);
        constantBuffer->Map(0, &range, &p);
        memcpy(p, &shaderParams, sizeof(shaderParams));
        constantBuffer->Unmap(0, nullptr);
    }

    // ���[�g�V�O�l�`���̃Z�b�g
    command->SetGraphicsRootSignature(m_rootSignature.Get());
    // �r���[�|�[�g�ƃV�U�[�̃Z�b�g
    command->RSSetViewports(1, &m_viewport);
    command->RSSetScissorRects(1, &m_scissorRect);

    // �f�B�X�N���v�^�q�[�v���Z�b�g.
    ID3D12DescriptorHeap* heaps[] = {
      m_heapSrvCbv.Get(), m_heapSampler.Get()
    };
    command->SetDescriptorHeaps(_countof(heaps), heaps);

    for (auto mode : { ALPHA_OPAQUE, ALPHA_MASK, ALPHA_BLEND })
    {
        for (const auto& mesh : m_model.meshes)
        {
            auto& material = m_model.materials[mesh.materialIndex];
            // �Ή�����|���S�����b�V���݂̂�`�悷��
            if (material.alphaMode == mode)
            {
                continue;
            }
            // ���[�h�ɉ����Ďg�p����p�C�v���C���X�e�[�g��ς���.
            switch (mode)
            {
            case Microsoft::glTF::ALPHA_OPAQUE:
                command->SetPipelineState(m_pipelineOpaque.Get());
                break;
            case Microsoft::glTF::ALPHA_MASK:
                command->SetPipelineState(m_pipelineOpaque.Get());
                break;
            case Microsoft::glTF::ALPHA_BLEND:
                command->SetPipelineState(m_pipelineAlpha.Get());
                break;
            }

            // �v���~�e�B�u�^�C�v�A���_�E�C���f�b�N�X�o�b�t�@�̃Z�b�g
            command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            command->IASetVertexBuffers(0, 1, &mesh.vertexBuffer.vertexView);
            command->IASetIndexBuffer(&mesh.indexBuffer.indexView);

            command->SetGraphicsRootDescriptorTable(0, m_cbViews[m_frameIndex]);
            command->SetGraphicsRootDescriptorTable(1, material.shaderResourceView);
            command->SetGraphicsRootDescriptorTable(2, m_sampler);

            // ���̃��b�V����`��
            command->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
        }
    }

}

void ModelApp::WaitGPU()
{
    HRESULT hr;
    const auto finishExpected = m_frameFenceValues[m_frameIndex];
    hr = m_commandQueue->Signal(m_frameFences[m_frameIndex].Get(), finishExpected);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed Signal(WaitGPU)");
    }
    m_frameFences[m_frameIndex]->SetEventOnCompletion(finishExpected, m_fenceWaitEvent);
    WaitForSingleObject(m_fenceWaitEvent, GpuWaitTimeout);
    m_frameFenceValues[m_frameIndex] = finishExpected + 1;
}
ModelApp::ComPtr<ID3D12Resource1> ModelApp::CreateBuffer(UINT bufferSize, const void* initialData)
{
    HRESULT hr;
    ComPtr<ID3D12Resource1> buffer;
    hr = m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer)
    );

    // �����f�[�^�̎w�肪����Ƃ��ɂ̓R�s�[����
    if (SUCCEEDED(hr) && initialData != nullptr)
    {
        void* mapped;
        CD3DX12_RANGE range(0, 0);
        hr = buffer->Map(0, &range, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped, initialData, bufferSize);
            buffer->Unmap(0, nullptr);
        }
    }

    return buffer;
}

ModelApp::TextureObject ModelApp::CreateTextureFromMemory(const std::vector<char>& imageData)
{
    // VRM �Ȃ̂� png/jpeg �Ȃǂ̃t�@�C����z�肵�AWIC �œǂݍ���.
    ComPtr<ID3D12Resource1> staging;
    HRESULT hr;
    ScratchImage image;
    hr = LoadFromWICMemory(imageData.data(), imageData.size(), 0, nullptr, image);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed LoadFromWICMemory");
    }
    auto metadata = image.GetMetadata();
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;

    DirectX::PrepareUpload(
        m_device.Get(), image.GetImages(), image.GetImageCount(),
        metadata, subresources);

    ComPtr<ID3D12Resource> texture;
    DirectX::CreateTexture(m_device.Get(), metadata, &texture);

    // �X�e�[�W���O�o�b�t�@�̏���
    const auto totalBytes = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
    m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(totalBytes),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&staging)
    );

    // �]������
    ComPtr<ID3D12GraphicsCommandList> command;
    m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[m_frameIndex].Get(),
        nullptr, IID_PPV_ARGS(&command));

    UpdateSubresources(
        command.Get(),
        texture.Get(), staging.Get(),
        0, 0, uint32_t(subresources.size()), subresources.data()
    );

    // ���\�[�X�o���A�̃Z�b�g
    auto barrierTex = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    command->ResourceBarrier(1, &barrierTex);

    // �R�}���h�̎��s.
    command->Close();
    ID3D12CommandList* cmds[] = { command.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmds);
    m_frameFenceValues[m_frameIndex]++;
    WaitGPU();

    TextureObject ret;
    texture.As(&ret.texture);
    ret.format = metadata.format;

    return ret;
}

void ModelApp::PrepareDescriptorHeapForModelApp(UINT materialCount)
{
    m_srvDescriptorBase = FrameBufferCount;
    UINT countCBVSRVDescriptors = materialCount + FrameBufferCount;

    // CBV/SRV �̃f�B�X�N���v�^�q�[�v
    //  0, 1:�萔�o�b�t�@�r���[ (FrameBufferCount�����g�p)
    //  FrameBufferCount �` :  �e�}�e���A���̃e�N�X�`��(�V�F�[�_�[���\�[�X�r���[)
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
      countCBVSRVDescriptors,
      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      0
    };
    m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_heapSrvCbv));
    m_srvcbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // �_�C�i�~�b�N�T���v���[�̃f�B�X�N���v�^�q�[�v
    D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc{
      D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
      1,
      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
      0
    };
    m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_heapSampler));
    m_samplerDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

void ModelApp::MakeModelGeometry(const Microsoft::glTF::Document& doc, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader)
{
    using namespace Microsoft::glTF;
    for (const auto& mesh : doc.meshes.Elements())
    {
        for (const auto& meshPrimitive : mesh.primitives)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // ���_�ʒu���A�N�Z�b�T�̎擾
            auto& idPos = meshPrimitive.GetAttributeAccessorId(ACCESSOR_POSITION);
            auto& accPos = doc.accessors.Get(idPos);
            // �@�����A�N�Z�b�T�̎擾
            auto& idNrm = meshPrimitive.GetAttributeAccessorId(ACCESSOR_NORMAL);
            auto& accNrm = doc.accessors.Get(idNrm);
            // �e�N�X�`�����W���A�N�Z�b�T�̎擾
            auto& idUV = meshPrimitive.GetAttributeAccessorId(ACCESSOR_TEXCOORD_0);
            auto& accUV = doc.accessors.Get(idUV);
            // ���_�C���f�b�N�X�p�A�N�Z�b�T�̎擾
            auto& idIndex = meshPrimitive.indicesAccessorId;
            auto& accIndex = doc.accessors.Get(idIndex);

            // �A�N�Z�b�T����f�[�^����擾
            auto vertPos = reader->ReadBinaryData<float>(doc, accPos);
            auto vertNrm = reader->ReadBinaryData<float>(doc, accNrm);
            auto vertUV = reader->ReadBinaryData<float>(doc, accUV);

            auto vertexCount = accPos.count;
            for (uint32_t i = 0; i < vertexCount; ++i)
            {
                // ���_�f�[�^�̍\�z
                int vid0 = 3 * i, vid1 = 3 * i + 1, vid2 = 3 * i + 2;
                int tid0 = 2 * i, tid1 = 2 * i + 1;
                vertices.emplace_back(
                    Vertex{
                      XMFLOAT3(vertPos[vid0], vertPos[vid1],vertPos[vid2]),
                      XMFLOAT3(vertNrm[vid0], vertNrm[vid1],vertNrm[vid2]),
                      XMFLOAT2(vertUV[tid0],vertUV[tid1])
                    }
                );
            }
            // �C���f�b�N�X�f�[�^
            indices = reader->ReadBinaryData<uint32_t>(doc, accIndex);

            auto vbSize = UINT(sizeof(Vertex) * vertices.size());
            auto ibSize = UINT(sizeof(uint32_t) * indices.size());
            ModelMesh modelMesh;
            auto vb = CreateBuffer(vbSize, vertices.data());
            D3D12_VERTEX_BUFFER_VIEW vbView;
            vbView.BufferLocation = vb->GetGPUVirtualAddress();
            vbView.SizeInBytes = vbSize;
            vbView.StrideInBytes = sizeof(Vertex);
            modelMesh.vertexBuffer.buffer = vb;
            modelMesh.vertexBuffer.vertexView = vbView;

            auto ib = CreateBuffer(ibSize, indices.data());
            D3D12_INDEX_BUFFER_VIEW ibView;
            ibView.BufferLocation = ib->GetGPUVirtualAddress();
            ibView.Format = DXGI_FORMAT_R32_UINT;
            ibView.SizeInBytes = ibSize;
            modelMesh.indexBuffer.buffer = ib;
            modelMesh.indexBuffer.indexView = ibView;

            modelMesh.vertexCount = UINT(vertices.size());
            modelMesh.indexCount = UINT(indices.size());
            modelMesh.materialIndex = int(doc.materials.GetIndex(meshPrimitive.materialId));
            m_model.meshes.push_back(modelMesh);
        }
    }
}

void ModelApp::MakeModelMaterial(const Microsoft::glTF::Document& doc, std::shared_ptr<Microsoft::glTF::GLTFResourceReader> reader)
{
    int textureIndex = 0;
    for (auto& m : doc.materials.Elements())
    {
        auto textureId = m.metallicRoughness.baseColorTexture.textureId;
        if (textureId.empty())
        {
            textureId = m.normalTexture.textureId;
        }
        auto& texture = doc.textures.Get(textureId);
        auto& image = doc.images.Get(texture.imageId);
        auto imageBufferView = doc.bufferViews.Get(image.bufferViewId);
        auto imageData = reader->ReadBinaryData<char>(doc, imageBufferView);

        // imageData ���摜�f�[�^
        Material material{};
        material.alphaMode = m.alphaMode;
        auto texObj = CreateTextureFromMemory(imageData);
        material.texture = texObj.texture;

        // �V�F�[�_�[���\�[�X�r���[�̐���.
        auto descriptorIndex = m_srvDescriptorBase + textureIndex;
        auto srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_heapSrvCbv->GetCPUDescriptorHandleForHeapStart(),
            descriptorIndex,
            m_srvcbvDescriptorSize);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = texObj.format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        m_device->CreateShaderResourceView(
            texObj.texture.Get(), &srvDesc, srvHandle);
        material.shaderResourceView =
            CD3DX12_GPU_DESCRIPTOR_HANDLE(
                m_heapSrvCbv->GetGPUDescriptorHandleForHeapStart(),
                descriptorIndex,
                m_srvcbvDescriptorSize);

        m_model.materials.push_back(material);

        textureIndex++;
    }
}

ModelApp::ComPtr<ID3D12PipelineState> ModelApp::CreateOpaquePSO()
{
    // �C���v�b�g���C�A�E�g
    D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
      { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, offsetof(Vertex,Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex,UV), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
    };

    // �p�C�v���C���X�e�[�g�I�u�W�F�N�g�̐���.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    // �V�F�[�_�[�̃Z�b�g
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_psOpaque.Get());
    // �u�����h�X�e�[�g�ݒ�
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    // ���X�^���C�U�[�X�e�[�g
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // �o�͐��1�^�[�Q�b�g
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    // �f�v�X�o�b�t�@�̃t�H�[�}�b�g��ݒ�
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };

    // ���[�g�V�O�l�`���̃Z�b�g
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // �}���`�T���v���ݒ�
    psoDesc.SampleDesc = { 1,0 };
    psoDesc.SampleMask = UINT_MAX; // �����Y���ƊG���o�Ȃ����x�����o�Ȃ��̂Œ���.

    ComPtr<ID3D12PipelineState> pipeline;
    HRESULT hr;
    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
    if (FAILED(hr))
    {
        throw std::runtime_error("CreateGraphicsPipelineState failed");
    }
    return pipeline;
}

ModelApp::ComPtr<ID3D12PipelineState> ModelApp::CreateAlphaPSO()
{
    // �C���v�b�g���C�A�E�g
    D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
      { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, offsetof(Vertex,Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex,UV), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA}
    };

    // �p�C�v���C���X�e�[�g�I�u�W�F�N�g�̐���.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    // �V�F�[�_�[�̃Z�b�g
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_psAlpha.Get());
    // �u�����h�X�e�[�g�ݒ�(�������p�ݒ�)
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    auto& target = psoDesc.BlendState.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    target.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
    target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    // ���X�^���C�U�[�X�e�[�g
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // �o�͐��1�^�[�Q�b�g
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    // �f�v�X�o�b�t�@�̃t�H�[�}�b�g��ݒ�
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    // �f�v�X�e�X�g�݂̂�L����(�������݂��s��Ȃ�)
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };

    // ���[�g�V�O�l�`���̃Z�b�g
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // �}���`�T���v���ݒ�
    psoDesc.SampleDesc = { 1,0 };
    psoDesc.SampleMask = UINT_MAX; // �����Y���ƊG���o�Ȃ����x�����o�Ȃ��̂Œ���.

    ComPtr<ID3D12PipelineState> pipeline;
    HRESULT hr;
    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
    if (FAILED(hr))
    {
        throw std::runtime_error("CreateGraphicsPipelineState failed");
    }
    return pipeline;
}
