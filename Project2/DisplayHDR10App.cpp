#include "DisplayHDR10App.h"
#include "TeapotModel.h"

using namespace DirectX;

void DisplayHDR10App::Prepare()
{
	SetTitle("DisplayHDR10");

	// バッファの転送を行うためにコマンドリストを使うので準備する.
	m_commandAllocators[m_frameIndex]->Reset();

	PrepareTeapot();
}