#pragma once
#include "Dice.h"
#include "Drinks.h"

// 役の序数：scoreKey() の (major, sub) を 10 進 packed int で扱う（大きいほど強い）
int ScoreOrdinal(const HandResult& h);

// 残り r 回（1〜3）のロール機会があるとき、都度「良ければ確定」できると仮定した
// 期待最大序数 E[max]（ウォッカ弱体ターンを考慮）を返す
double ExpectedBestOrdinal(int rollsRemaining, int vodkaPenaltyTurns);

// 現在の手 current を「確定すべきか？」を判定
// -> 現在の強さ >= 残り r 回振ったときの期待最大値 なら確定
bool ShouldFixCurrent(const HandResult& current, int rollsRemaining, int vodkaPenaltyTurns);

// （オプション）勝利時の「即時ダメージ最大化」ドリンク選択
// ※ 今回は既存仕様（客が先に選ぶ）を維持するため未使用。
//   使う場合は Game 側で呼んでください。
Drink ChooseBestDrinkForAttack(
	const DrinkEffectParams& par,
	const Resist& targetRes, const DebuffState& targetDebuff,
	bool taunted, int wager,
	int selfHp, int selfHpMax, const DebuffState& selfDebuff
);