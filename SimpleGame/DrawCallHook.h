#pragma once

// OpenGL 드로우 콜을 후킹해서 게임 프레임당 호출 횟수를 세는 모듈.
//
// 이 실행 파일의 임포트 주소 테이블(IAT)에서 opengl32.dll의 glDrawArrays(그리고
// 임포트되어 있다면 glDrawElements) 주소를 우리 후크 함수로 바꿔치기한다. 그러면
// 어디서든 드로우 콜이 실행되는 순간 제어가 후크로 점프해서 카운터를 올리고, 이어서
// 원본 함수를 호출한다 — 그리는 코드(Renderer/PostProcess)는 전혀 손대지 않아도
// 새로 추가되는 드로우 콜까지 자동으로 센다.
//
// 이 실행 파일이 직접 부르는 드로우 콜만 센다. freeglut.dll 안에서 일어나는
// 이름표 글자 그리기(glutBitmapCharacter → glBitmap)는 다른 모듈의 호출이라 세지 않는다.
namespace DrawCallHook
{
	// IAT를 패치해 후킹을 시작한다. glDrawArrays를 못 찾으면 false(이 경우 카운트/출력
	// 없이 게임은 그대로 동작한다). 첫 드로우 콜 전에 한 번 호출한다.
	bool Install();

	// IAT를 원래 주소로 되돌린다.
	void Uninstall();

	// 게임 프레임의 시작/끝. 시작에서 카운터를 0으로 돌리고, 끝에서 이번 프레임의
	// 드로우 콜 횟수를 집계해 주기적으로 콘솔에 출력한다. note가 있으면 출력 줄 끝에
	// 덧붙인다(예: 씬 컬링 통계).
	void BeginFrame();
	void EndFrame(const char* note = nullptr);
}
