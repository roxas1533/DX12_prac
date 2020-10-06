#include "TriangleApp.h"
#include <stdexcept>
void TriangleApp::Prepare() {
	const float k = 1.0f;
	const DirectX::XMFLOAT4 red(1.0f, 0.0f, 0.0f, 1.0f);
	const DirectX::XMFLOAT4 green(0.0f, 1.0f, 0.0f, 1.0f);
	const DirectX::XMFLOAT4 blue(0.0f, 0.0f, 1.0f, 1.0);
	const DirectX::XMFLOAT4 white(1.0f, 1.0f, 1.0f, 1.0f);
	const DirectX::XMFLOAT4 black(0.0f, 0.0f, 0.0f, 1.0f);
	const DirectX::XMFLOAT4 yellow(1.0f, 1.0f, 0.0f, 1.0f);
	const DirectX::XMFLOAT4 magenta(1.0f, 0.0f, 1.0f, 1.0f);
	const DirectX::XMFLOAT4 cyan(0.0f, 1.0f, 1.0f, 1.0f);
	Vertex triangleVertices[] = {
		// ����
		  { {-k,-k,-k}, red },
		  { {-k, k,-k}, yellow },
		  { { k, k,-k}, white},
		  { { k,-k,-k}, magenta },
		  // �E
		  { { k,-k,-k}, magenta },
		  { { k, k,-k}, white },
		  { { k, k, k}, cyan },
		  { { k,-k, k}, blue },
		  // ��
		  { {-k,-k, k}, black },
		  { {-k, k, k}, green},
		  { {-k, k,-k}, yellow },
		  { {-k,-k,-k}, red },
		  // ��
		  { { k,-k, k}, blue },
		  { { k, k, k}, cyan},
		  { {-k, k, k}, green },
		  { {-k,-k, k}, black},
		  // ��
		  { {-k, k,-k}, yellow },
		  { {-k, k, k}, green },
		  { { k, k, k}, cyan },
		  { { k, k,-k}, white },
		  // ��
		  { {-k,-k, k}, red },
		  { {-k,-k,-k}, red },
		  { { k,-k,-k}, magenta },
		  { { k,-k, k}, blue },
	};
	
	uint32_t indices[] = {
		0, 1, 2, 2, 3,0,
		4, 5, 6, 6, 7,4,
		8, 9, 10, 10, 11, 8,
		12,13,14, 14,15,12,
		16,17,18, 18,19,16,
		20,21,22, 22,23,20,
	};
	m_vertexBuffer = CreateBuffer(sizeof(triangleVertices), triangleVertices);
	m_indexBuffer = CreateBuffer(sizeof(indices), indices);
	m_indexCount = _countof(indices);

	//m_indexBuffer = CreateBuffer(sizeof(indices), indices);
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = sizeof(triangleVertices);
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = sizeof(indices);
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// �V�F�[�_�[���R���p�C��.
	HRESULT hr;
	ComPtr<ID3DBlob> errBlob;
	hr = CompileShaderFromFile(L"simpleVS.hlsl", L"vs_6_0", m_vs, errBlob);
	if (FAILED(hr))
	{
		OutputDebugStringA((const char*)errBlob->GetBufferPointer());
	}
	hr = CompileShaderFromFile(L"simplePS.hlsl", L"ps_6_0", m_ps, errBlob);
	if (FAILED(hr))
	{
		OutputDebugStringA((const char*)errBlob->GetBufferPointer());
	}

	CD3DX12_DESCRIPTOR_RANGE cbv;
	cbv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0 ���W�X�^
	CD3DX12_ROOT_PARAMETER rootParams[1];
	rootParams[0].InitAsDescriptorTable(1, &cbv, D3D12_SHADER_VISIBILITY_VERTEX);



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

	// �C���v�b�g���C�A�E�g
	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
	  { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
	  { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, offsetof(Vertex,Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA},
	};

	CD3DX12_RASTERIZER_DESC re(D3D12_DEFAULT);
	//re.CullMode = D3D12_CULL_MODE_NONE;

	// �p�C�v���C���X�e�[�g�I�u�W�F�N�g�̐���.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	// �V�F�[�_�[�̃Z�b�g
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vs.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_ps.Get());
	// �u�����h�X�e�[�g�ݒ�
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	// ���X�^���C�U�[�X�e�[�g
	psoDesc.RasterizerState =re;
	// �o�͐��1�^�[�Q�b�g
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	// �f�v�X�o�b�t�@�̃t�H�[�}�b�g��ݒ�
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.InputLayout = { inputElementDesc, _countof(inputElementDesc) };
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	// ���[�g�V�O�l�`���̃Z�b�g
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// �}���`�T���v���ݒ�
	psoDesc.SampleDesc = { 1,0 };
	psoDesc.SampleMask = UINT_MAX; // �����Y���ƊG���o�Ȃ����x�����o�Ȃ��̂Œ���.

	hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipeline));
	if (FAILED(hr))
	{
		throw std::runtime_error("CreateGraphicsPipelineState failed");
	}

	PrepareDescriptorHeapForCubeApp();

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
		CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(m_heapCbv->GetCPUDescriptorHandleForHeapStart(), i, m_srvcbvDescriptorSize);
		m_device->CreateConstantBufferView(&cbDesc, handleCBV);
		m_cbViews[i] = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heapCbv->GetGPUDescriptorHandleForHeapStart(),  i, m_srvcbvDescriptorSize);
	}


}

void TriangleApp::Cleanup() {
	auto index = m_swapchain->GetCurrentBackBufferIndex();
	auto fence = m_frameFences[index];
	auto value = ++m_frameFenceValues[index];
	m_commandQueue->Signal(fence.Get(), value);
	fence->SetEventOnCompletion(value, m_fenceWaitEvent);
	WaitForSingleObject(m_fenceWaitEvent, GpuWaitTimeout);
}

void TriangleApp::MakeCommand(ComPtr<ID3D12GraphicsCommandList>& command)
{
	using namespace DirectX;

	ShaderParameters shaderParams;
	XMStoreFloat4x4(&shaderParams.mtxWorld, XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMConvertToRadians(35.0f)));
	auto mtxView = XMMatrixLookAtLH(
		XMVectorSet(0.0f, 3.0f, -5.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
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

	// �p�C�v���C���X�e�[�g�̃Z�b�g
	command->SetPipelineState(m_pipeline.Get());
	// ���[�g�V�O�l�`���̃Z�b�g
	command->SetGraphicsRootSignature(m_rootSignature.Get());
	// �r���[�|�[�g�ƃV�U�[�̃Z�b�g
	command->RSSetViewports(1, &m_viewport);
	command->RSSetScissorRects(1, &m_scissorRect);

	// �f�B�X�N���v�^�q�[�v���Z�b�g.
	ID3D12DescriptorHeap* heaps[] = {
	  m_heapCbv.Get()
	};
	command->SetDescriptorHeaps(_countof(heaps), heaps);

	// �v���~�e�B�u�^�C�v�A���_�E�C���f�b�N�X�o�b�t�@�̃Z�b�g
	command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	command->IASetIndexBuffer(&m_indexBufferView);
	command->SetGraphicsRootDescriptorTable(0, m_cbViews[m_frameIndex]);
	//command->IASetIndexBuffer(&m_indexBufferView);

	// �`�施�߂̔��s
	command->DrawIndexedInstanced(m_indexCount, 1, 0, 0,0);
}

TriangleApp::ComPtr<ID3D12Resource1> TriangleApp::CreateBuffer(UINT bufferSize, const void* initialData)
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
void TriangleApp::PrepareDescriptorHeapForCubeApp(){
	UINT count = FrameBufferCount;
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{
   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
   count,
   D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
   0
	};
	m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_heapCbv));
	m_srvcbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}