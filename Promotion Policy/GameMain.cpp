#include "DxLib.h"
#include "Game.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	SetOutApplicationLogValidFlag(FALSE);
	ChangeWindowMode(TRUE);
	SetGraphMode(1280, 720, 32);
	SetBackgroundColor(18, 22, 26);
	if (DxLib_Init() == -1) return -1;
	SetDrawScreen(DX_SCREEN_BACK);

	{
		Game game;
		if (game.init()) {
			game.run();
		}
	}

	DxLib_End();
	return 0;
}