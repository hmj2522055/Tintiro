#include "Drinks.h"
#include <algorithm>
#include <cmath>

std::wstring DrinkName(Drink d) {
	switch (d) {
	case Drink::Tequila:       return L"テキーラ";
	case Drink::Gin:           return L"ジン";
	case Drink::Vodka:         return L"ウォッカ";
	case Drink::Rum:           return L"ラム";
	case Drink::OrangeJuice:   return L"オレンジジュース";
	}
	return L"?";
}

void DebuffState::decay() {
	if (ginBlurTurns > 0) ginBlurTurns--;
	if (vodkaPenaltyTurns > 0) vodkaPenaltyTurns--;
	if (extraDrinkBias > 0.0f) {
		extraDrinkBias *= 0.9f;
		if (extraDrinkBias < 0.01f) extraDrinkBias = 0.0f;
	}
	// tequilaMult は自然減衰しない（OJで解除）
}

void DebuffState::clearAll() {
	ginBlurTurns = 0;
	vodkaPenaltyTurns = 0;
	tequilaMult = 1.0f;
	extraDrinkBias = 0.0f;
}

int Resist::get(Drink d) const {
	auto it = level.find(d);
	if (it != level.end()) return it->second;
	return 1; // 既定: 普通
}

int ComputeDamage(const DrinkEffectParams& par, Drink d, const Resist& resist, const DebuffState& loserDebuff, int wagerMult) {
	if (d == Drink::OrangeJuice) return 0;
	float dmg = (float)par.baseDamage;
	dmg *= (float)wagerMult;

	float tequila = 1.0f;
	if (d == Drink::Tequila) tequila = par.tequilaBaseMult;

	int lv = resist.get(d);
	float resistFactor = 1.0f;
	if (lv == 0) resistFactor = 1.2f; // 弱い → 増える
	else if (lv == 2) resistFactor = 0.8f; // 強い → 減る

	dmg *= tequila * resistFactor;

	int idmg = (int)std::round(dmg);
	if (idmg < 0) idmg = 0;
	return idmg;
}

void ApplyDrinkDebuff(Drink d, DebuffState& loserDebuff, const DrinkEffectParams& par, const Resist& resist, bool taunted, std::wstring& log) {
	int lv = resist.get(d);
	if (d == Drink::Gin) {
		int turns = std::max(1, par.ginTurns - (lv >= 2 ? 1 : 0));
		loserDebuff.ginBlurTurns += turns;
		log += L" [ジン]視界ぼかし+" + std::to_wstring(turns) + L"T";
	}
	else if (d == Drink::Vodka) {
		int turns = std::max(1, par.vodkaTurns - (lv >= 2 ? 1 : 0));
		loserDebuff.vodkaPenaltyTurns += turns;
		log += L" [ウォッカ]役弱体+" + std::to_wstring(turns) + L"T";
	}
	else if (d == Drink::Tequila) {
		float t = par.tequilaBaseMult * (lv == 2 ? 0.8f : (lv == 0 ? 1.2f : 1.0f));
		loserDebuff.tequilaMult = std::max(loserDebuff.tequilaMult, t);
		log += L" [テキーラ]被ダメ倍率×" + std::to_wstring((int)std::round(loserDebuff.tequilaMult));
	}
	else if (d == Drink::Rum) {
		float base = par.rumExtraBase;
		if (lv == 2) base *= 0.5f;
		if (lv == 0) base *= 1.3f;
		if (taunted) base += 0.15f;
		loserDebuff.extraDrinkBias += base * 0.5f;
		log += L" [ラム]もう一杯リスク↑";
	}
	else if (d == Drink::OrangeJuice) {
		loserDebuff.clearAll();
		log += L" [オレンジジュース]デバフ解除";
	}
}

bool CheckOneMoreDrink(Drink d, const DebuffState& loserDebuff, const Resist& resist, bool taunted, float rand01) {
	if (d != Drink::Rum) return false;
	float p = 0.5f + loserDebuff.extraDrinkBias;
	if (p < 0.05f) p = 0.05f;
	if (p > 0.9f)  p = 0.9f;
	return rand01 < p;
}
