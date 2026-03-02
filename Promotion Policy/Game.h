#pragma once
#include <string>
#include <vector>
#include <random>
#include "Input.h"
#include "Dice.h"
#include "Drinks.h"
#include "Entities.h"
#include "TurnState.h"

class Game {
public:
	Game();
	bool init();
	void run();

private:
	// ループ分割
	void update();
	void draw();

	// ゲーム管理
	void startGame();
	void loadCustomer(int idx);
	Drink pickDrink(const Customer& c);

	// UI
	void drawHPBar(int x, int y, int w, int h, int hp, int hpMax, const wchar_t* label, int colorFill);
	void drawDiceBox(int x, int y, int val, bool blur);
	void drawDebuffInfo(int x, int y, const DebuffState& d);

	// 内部RNG
	struct RNG {
		std::mt19937 mt;
		RNG();
		int irand(int a, int b); // inclusive
		float frand();
	};

private:
	// カラー
	int cWhite, cGray, cYellow, cCyan, cGreen, cRed, cBlue, cOrange, cPink;

	// 状態
	InputState in;
	RNG rng;
	SceneType scene = SceneType::Title;
	std::wstring lastLog;

	DrinkEffectParams params;

	Fighter player;
	Fighter customer;
	std::vector<Customer> queue;
	int currentCustomerIdx = 0;

	TurnState T;

	// ヘルプ
	const wchar_t* helpTitle;
	const wchar_t* helpGame;
	const wchar_t* helpSafe;
};