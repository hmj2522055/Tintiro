#include "AI.h"
#include <map>
#include <vector>
#include <algorithm>
#include <cmath>

// ============== 基本ユーティリティ ==============

static inline int PackScoreKey(const std::tuple<int, int>& key) {
	return std::get<0>(key) * 10 + std::get<1>(key);
}

int ScoreOrdinal(const HandResult& h) {
	return PackScoreKey(h.scoreKey());
}

// ============== ロール分布と期待最大値 ==============
//
// 216 通り（3D6）の出目で、ApplyVodkaPenaltyIf をかけた後の「序数」分布を作る。
// vodkaPenaltyTurns ごとにキャッシュして使い回す。

struct FreqCache {
	std::map<int, int> freq;  // ord -> count（合計 216）
	std::vector<std::pair<int, double>> cdf; // (ord, F(ord))
};

static const FreqCache& GetOrBuildFreqCache(int vodkaPenaltyTurns) {
	static std::map<int, FreqCache> cache;
	auto it = cache.find(vodkaPenaltyTurns);
	if (it != cache.end()) return it->second;

	FreqCache fc;
	// 順序付き freq（map）にカウント
	for (int a = 1; a <= 6; ++a) {
		for (int b = 1; b <= 6; ++b) {
			for (int c = 1; c <= 6; ++c) {
				HandResult r = ApplyVodkaPenaltyIf(Judge3(a, b, c), vodkaPenaltyTurns);
				int ord = ScoreOrdinal(r);
				fc.freq[ord]++;
			}
		}
	}
	// CDF 構築
	const double N = 216.0;
	int cum = 0;
	for (auto& kv : fc.freq) {
		cum += kv.second;
		fc.cdf.emplace_back(kv.first, (double)cum / N);
	}

	auto res = cache.emplace(vodkaPenaltyTurns, std::move(fc));
	return res.first->second;
}

double ExpectedBestOrdinal(int rollsRemaining, int vodkaPenaltyTurns) {
	if (rollsRemaining <= 0) return 0.0;
	const auto& fc = GetOrBuildFreqCache(vodkaPenaltyTurns);
	// P(max == x_i) = F(x_i)^r - F(x_{i-1})^r

	double expMax = 0.0;
	double prevFr = 0.0;
	for (size_t i = 0; i < fc.cdf.size(); ++i) {
		double Fr = std::pow(fc.cdf[i].second, (double)rollsRemaining);
		double pmass = Fr - prevFr;
		expMax += (double)fc.cdf[i].first * pmass;
		prevFr = Fr;
	}
	return expMax;
}

bool ShouldFixCurrent(const HandResult& current, int rollsRemaining, int vodkaPenaltyTurns) {
	if (rollsRemaining <= 0) return true;
	double expectIfContinue = ExpectedBestOrdinal(rollsRemaining, vodkaPenaltyTurns);
	double cur = (double)ScoreOrdinal(current);
	// 素直に「現在 >= 期待」で確定（攻守性を調整したい場合は係数を入れる）
	return (cur >= expectIfContinue);
}

// ============== ドリンク選択（オプション） ==============

static inline int ImmediateDamageEstimate(
	const DrinkEffectParams& par,
	Drink d, const Resist& targetRes, const DebuffState& targetDebuff,
	bool /*taunted*/, int wager)
{
	// 現行実装に合わせて：ComputeDamage は tequilaMult を内包しないため、
	// ここで既存の被ダメ倍率を掛けて「今ターンの即時ダメージ」を見積もる
	int base = ComputeDamage(par, d, targetRes, targetDebuff, wager);
	double mult = std::max(0.0f, targetDebuff.tequilaMult); // 1.0以上が通常
	return (int)std::round((double)base * mult);
}

Drink ChooseBestDrinkForAttack(
	const DrinkEffectParams& par,
	const Resist& targetRes, const DebuffState& targetDebuff,
	bool taunted, int wager,
	int selfHp, int selfHpMax, const DebuffState& selfDebuff)
{
	// ピンチ & 重デバフなら OJ を優先
	bool selfPinned = (selfHp <= selfHpMax * 3 / 10);
	bool heavyDebuff = (selfDebuff.ginBlurTurns > 0) || (selfDebuff.vodkaPenaltyTurns > 0)
		|| (selfDebuff.tequilaMult > 1.0f);
	if (selfPinned && heavyDebuff) {
		return Drink::OrangeJuice;
	}

	// 即時ダメージ最大
	Drink cands[] = { Drink::Tequila, Drink::Gin, Drink::Vodka, Drink::Rum };
	Drink bestD = Drink::Tequila;
	int bestVal = -1;
	for (Drink d : cands) {
		int val = ImmediateDamageEstimate(par, d, targetRes, targetDebuff, taunted, wager);
		if (val > bestVal) { bestVal = val; bestD = d; }
	}
	return bestD;
}