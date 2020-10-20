#include "ModelApp.h"
#include "streamreader.h"

using namespace DirectX;
using namespace std;

void ModelApp::Prepare() {
    // モデルデータの読み込み
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

    // マテリアル数分のSRVディスクリプタが必要になるのでここで準備.
    PrepareDescriptorHeapForModelApp(UINT(document.materials.Elements().size()));

    MakeModelGeometry(document, glbResourceReader);
    MakeModelMaterial(document, glbResourceReader);
}

void ModelApp::Cleanup()
{
    WaitGPU();
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

    // 初期データの指定があるときにはコピーする
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

