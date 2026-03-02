#pragma once
#include "DxLib.h"

struct InputState {
	int nowMouse = 0;
	int prevMouse = 0;

	// keys
	bool keyEsc = false;
	bool keyW = false, keyA = false, keyS = false, keyD = false;
	bool keyUp = false, keyDown = false, keyLeft = false, keyRight = false;

	// edge-detect—p
	bool qNow = false, qPrev = false;
	bool eNow = false, ePrev = false;

	void update() {
		prevMouse = nowMouse;
		nowMouse = GetMouseInput();

		keyEsc = (CheckHitKey(KEY_INPUT_ESCAPE) != 0);
		keyW = (CheckHitKey(KEY_INPUT_W) != 0);
		keyA = (CheckHitKey(KEY_INPUT_A) != 0);
		keyS = (CheckHitKey(KEY_INPUT_S) != 0);
		keyD = (CheckHitKey(KEY_INPUT_D) != 0);
		keyUp = (CheckHitKey(KEY_INPUT_UP) != 0);
		keyDown = (CheckHitKey(KEY_INPUT_DOWN) != 0);
		keyLeft = (CheckHitKey(KEY_INPUT_LEFT) != 0);
		keyRight = (CheckHitKey(KEY_INPUT_RIGHT) != 0);

		qPrev = qNow;
		ePrev = eNow;
		qNow = (CheckHitKey(KEY_INPUT_Q) != 0);
		eNow = (CheckHitKey(KEY_INPUT_E) != 0);
	}

	bool pressedL() const { return ((nowMouse & MOUSE_INPUT_LEFT) && !(prevMouse & MOUSE_INPUT_LEFT)); }
	bool pressedR() const { return ((nowMouse & MOUSE_INPUT_RIGHT) && !(prevMouse & MOUSE_INPUT_RIGHT)); }

	bool pressedQ() const { return (qNow && !qPrev); }
	bool pressedE() const { return (eNow && !ePrev); }
};