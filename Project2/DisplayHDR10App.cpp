#include "DisplayHDR10App.h"
#include "TeapotModel.h"

using namespace DirectX;

void DisplayHDR10App::Prepare()
{
	SetTitle("DisplayHDR10");

	// �o�b�t�@�̓]�����s�����߂ɃR�}���h���X�g���g���̂ŏ�������.
	m_commandAllocators[m_frameIndex]->Reset();

	PrepareTeapot();
}