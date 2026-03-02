#pragma once
#include "Dice.h"
#include "Drinks.h"

enum class SceneType { Title, Game, Result };

struct TurnState {
	int diceP[3] = { 0,0,0 };
	int diceC[3] = { 0,0,0 };
	int rollCountP = 0;
	int rollCountC = 0;
	HandResult handP;
	HandResult handC;
	bool fixedP = false;
	bool fixedC = false;
	int currentActor = 0; // 0=ÉvÉåÉCÉÑÅ[,1=ãq
	bool wagerDouble = false;
	bool taunted = false;
	Drink currentDrink = Drink::Tequila;
};
