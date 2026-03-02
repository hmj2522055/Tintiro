#pragma once
#include <string>
#include <vector>
#include "Drinks.h"

struct Fighter {
	std::wstring name;
	int hp = 100;
	int hpMax = 100;
	Resist resist;
	DebuffState debuff;
	float x = 0.0f, y = 0.0f; // 表示用
};

struct Customer {
	std::wstring name;
	int hp = 100;
	int hpMax = 100;
	Resist resist;
	std::vector<Drink> drinkPool;
	int difficulty = 1; // 1〜
};