#pragma once
#include <string>
#include <map>
#include <vector>

enum class Drink {
	Tequila, Gin, Vodka, Rum, OrangeJuice
};

std::wstring DrinkName(Drink d);

// デバフ状態（飲んだ側に蓄積）
struct DebuffState {
	int   ginBlurTurns = 0;       // ジン: 視界ぼかし
	int   vodkaPenaltyTurns = 0;  // ウォッカ: 役強度-1
	float tequilaMult = 1.0f;     // テキーラ: 被ダメ倍率（強い方優先）
	float extraDrinkBias = 0.0f;  // ラム: もう一杯基礎確率の上乗せ

	void decay();
	void clearAll();
};

// 耐性: 0=弱い,1=普通,2=強い
struct Resist {
	std::map<Drink, int> level; // 未登録は1扱い
	int get(Drink d) const;
};

struct DrinkEffectParams {
	int   baseDamage = 10;     // 一杯あたり基礎ダメージ
	int   ginTurns = 2;
	int   vodkaTurns = 2;
	float tequilaBaseMult = 2.0f;
	float rumExtraBase = 0.5f;
	int   ojHeal = 8;
};

// 計算・適用（HP変動はGame側で行う）
// wagerMult: 倍賭け倍率（1 or 2）
int  ComputeDamage(const DrinkEffectParams& par, Drink d, const Resist& resist, const DebuffState& loserDebuff, int wagerMult);
void ApplyDrinkDebuff(Drink d, DebuffState& loserDebuff, const DrinkEffectParams& par, const Resist& resist, bool taunted, std::wstring& log);

// ラムの「もう一杯」判定（rand01は0.0〜1.0未満の乱数）
bool CheckOneMoreDrink(Drink d, const DebuffState& loserDebuff, const Resist& resist, bool taunted, float rand01);