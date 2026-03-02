// ChinChiro_DrinkGame_v0_1.cpp
// DXLib minimal prototype based on your spec.
// 機能: 体力ゲージ/客がドリンク選択/デバフ/ちんちろ/倍賭け/煽り/WASD移動/マウス操作
// 注意: 画像・音なし（シェイプとテキストのみ）

#include "DxLib.h"
#include <algorithm>
#include <random>
#include <tuple>
#include <vector>
#include <string>
#include <map>
#include <cmath>

// ===================== 共通ユーティリティ =====================
static int gWhite, gGray, gYellow, gCyan, gGreen, gRed, gBlue, gOrange, gPink;

struct InputState {
	int nowMouse = 0;
	int prevMouse = 0;
	bool keyQ = false, keyE = false;
	bool keyEsc = false;
	bool keyW = false, keyA = false, keyS = false, keyD = false;
	bool keyUp = false, keyDown = false, keyLeft = false, keyRight = false;
	void update() {
		prevMouse = nowMouse;
		nowMouse = GetMouseInput();
		keyQ = (CheckHitKey(KEY_INPUT_Q) != 0);
		keyE = (CheckHitKey(KEY_INPUT_E) != 0);
		keyEsc = (CheckHitKey(KEY_INPUT_ESCAPE) != 0);
		keyW = (CheckHitKey(KEY_INPUT_W) != 0);
		keyA = (CheckHitKey(KEY_INPUT_A) != 0);
		keyS = (CheckHitKey(KEY_INPUT_S) != 0);
		keyD = (CheckHitKey(KEY_INPUT_D) != 0);
		keyUp = (CheckHitKey(KEY_INPUT_UP) != 0);
		keyDown = (CheckHitKey(KEY_INPUT_DOWN) != 0);
		keyLeft = (CheckHitKey(KEY_INPUT_LEFT) != 0);
		keyRight = (CheckHitKey(KEY_INPUT_RIGHT) != 0);
	}
	bool pressedL() const { return ((nowMouse & MOUSE_INPUT_LEFT) && !(prevMouse & MOUSE_INPUT_LEFT)); }
	bool pressedR() const { return ((nowMouse & MOUSE_INPUT_RIGHT) && !(prevMouse & MOUSE_INPUT_RIGHT)); }
};

// ===================== Dice / Hand 判定 =====================
enum class HandType {
	None, Hifumi, MeKaku, Shigoro, Zorome, Pinzoro
};

struct HandResult {
	HandType type = HandType::None;
	int sub = 0; // Zorome:目 / MeKaku:シングルトン目
	std::wstring name = L"未判定";
	std::tuple<int, int> scoreKey() const {
		int major = 0;
		switch (type) {
		case HandType::Pinzoro: major = 5; break;
		case HandType::Zorome:  major = 4; break;
		case HandType::Shigoro: major = 3; break;
		case HandType::MeKaku:  major = 2; break;
		case HandType::Hifumi:  major = 1; break;
		default: major = 0; break;
		}
		return { major, sub };
	}
};

HandResult Judge3(int a, int b, int c) {
	int d[3] = { a,b,c };
	std::sort(d, d + 3);
	if (d[0] == d[1] && d[1] == d[2]) {
		if (d[0] == 1) return { HandType::Pinzoro, 1, L"ピンゾロ(1-1-1)" };
		return { HandType::Zorome, d[0], L"ゾロ目(" + std::to_wstring(d[0]) + L")" };
	}
	if (d[0] == 1 && d[1] == 2 && d[2] == 3) return { HandType::Hifumi, 0, L"ヒフミ(1-2-3)" };
	if (d[0] == 4 && d[1] == 5 && d[2] == 6) return { HandType::Shigoro, 0, L"シゴロ(4-5-6)" };
	if (d[0] == d[1] && d[1] != d[2]) return { HandType::MeKaku, d[2], L"目確(" + std::to_wstring(d[2]) + L")" };
	if (d[1] == d[2] && d[0] != d[1]) return { HandType::MeKaku, d[0], L"目確(" + std::to_wstring(d[0]) + L")" };
	return { HandType::None, 0, L"役なし" };
}

// ===================== お酒・デバフ =====================
enum class Drink {
	Tequila, Gin, Vodka, Rum, OrangeJuice
};

std::wstring DrinkName(Drink d) {
	switch (d) {
	case Drink::Tequila: return L"テキーラ";
	case Drink::Gin: return L"ジン";
	case Drink::Vodka: return L"ウォッカ";
	case Drink::Rum: return L"ラム";
	case Drink::OrangeJuice: return L"オレンジジュース";
	}
	return L"?";
}

struct DebuffState {
	// 効果は「飲んだ側」に蓄積
	int ginBlurTurns = 0;      // 視界ぼかし（UI演出）
	int vodkaPenaltyTurns = 0; // 役の強度 -1 段階
	float tequilaMult = 1.0f;  // 被ダメ倍率（ >1.0 の間継続）
	float extraDrinkBias = 0.0f; // もう一杯の基礎確率に加算
	void decay() {
		if (ginBlurTurns > 0) ginBlurTurns--;
		if (vodkaPenaltyTurns > 0) vodkaPenaltyTurns--;
		// tequilaMult は時間で自然減衰しない（オレンジジュースで解除）
		if (extraDrinkBias > 0.0f) {
			extraDrinkBias *= 0.9f; // 少しずつ戻る
			if (extraDrinkBias < 0.01f) extraDrinkBias = 0.0f;
		}
	}
	void clearAll() {
		ginBlurTurns = 0;
		vodkaPenaltyTurns = 0;
		tequilaMult = 1.0f;
		extraDrinkBias = 0.0f;
	}
};

// 耐性: 0=弱い,1=普通,2=強い
struct Resist {
	std::map<Drink, int> level; // 既定は1
	int get(Drink d) const {
		auto it = level.find(d);
		if (it != level.end()) return it->second;
		return 1;
	}
};

// 効果の係数を耐性で調整
struct DrinkEffectParams {
	// 基本設定（好みに合わせてあとでバランス調整）
	int baseDamage = 10; // 一杯あたり基礎ダメージ
	int ginTurns = 2;
	int vodkaTurns = 2;
	float tequilaBaseMult = 2.0f; // テキーラ基礎倍率
	float rumExtraBase = 0.5f;    // もう一杯確率の基礎
	int ojHeal = 8;               // オレンジジュースの回復量
};

// 実際に適用（飲んだ側への効果）
struct ApplyDrinkContext {
	Drink drink;
	int loserIndex; // 0=プレイヤー,1=客
	int wagerMult;  // 倍賭け(1 or 2)
	bool taunted;   // 煽り発動済み（相手側のもう一杯率に影響）
};

// ===================== プレイヤ/客、ステージ =====================
struct Fighter {
	std::wstring name;
	int hp = 100;
	int hpMax = 100;
	Resist resist;
	DebuffState debuff;

	// 表示用の位置（WASD移動の演出）
	float x = 200.0f, y = 360.0f;
};

struct Customer {
	std::wstring name;
	int hp;
	int hpMax;
	Resist resist;
	std::vector<Drink> drinkPool; // 選択候補
	int difficulty = 1; // 1〜
};

struct RNG {
	std::mt19937 mt;
	RNG() {
		std::random_device rd; mt.seed(rd());
	}
	int irand(int a, int b) { // inclusive
		std::uniform_int_distribution<int> dist(a, b);
		return dist(mt);
	}
	float frand() {
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		return dist(mt);
	}
};

struct TurnState {
	int diceP[3] = { 0,0,0 };
	int diceC[3] = { 0,0,0 };
	int rollCountP = 0;
	int rollCountC = 0;
	HandResult handP;
	HandResult handC;
	bool fixedP = false;
	bool fixedC = false;
	int currentActor = 0; // 0=プレイヤー,1=客
	bool wagerDouble = false;
	bool taunted = false;
	Drink currentDrink = Drink::Tequila;
};

enum class SceneType { Title, Game, Result };

// ===================== 効果適用・ダメージ計算 =====================

int CompareHands(const HandResult& A, const HandResult& B) {
	if (A.scoreKey() > B.scoreKey()) return 1;
	if (A.scoreKey() < B.scoreKey()) return -1;
	return 0;
}

// Vodkaペナルティ：役強度 -1段階（RNGはそのまま）
HandResult ApplyVodkaPenaltyIf(const HandResult& h, const DebuffState& debuff) {
	if (debuff.vodkaPenaltyTurns <= 0) return h;
	HandResult r = h;
	// Noneはそのまま
	if (h.type == HandType::Pinzoro)      r.type = HandType::Zorome;
	else if (h.type == HandType::Zorome)  r.type = HandType::Shigoro;
	else if (h.type == HandType::Shigoro) r.type = HandType::MeKaku;
	else if (h.type == HandType::MeKaku)  r.type = HandType::Hifumi;
	else if (h.type == HandType::Hifumi)  r.type = HandType::None;
	// sub はそのまま（粗めの実装）
	return r;
}

// ダメージ算出（飲んだ側＝loser）
int ComputeDamage(const DrinkEffectParams& par, Drink d, const Resist& resist, const DebuffState& losDebuff, int wagerMult) {
	if (d == Drink::OrangeJuice) return 0;
	float dmg = (float)par.baseDamage;
	// 倍賭け
	dmg *= (float)wagerMult;
	// テキーラ倍率（すでに付いている倍率も乗ると重すぎるので、今回飲んだ分は「maxを取る」運用）
	float tequila = 1.0f;
	if (d == Drink::Tequila) tequila = par.tequilaBaseMult;

	// 耐性補正（弱:0,普:1,強:2） → 強いほど軽減
	int lv = resist.get(d);
	float resistFactor = 1.0f - 0.2f * (lv - 1); // lv=0:1.2, lv=1:1.0, lv=2:0.8
	if (lv == 0) resistFactor = 1.2f;
	if (lv == 2) resistFactor = 0.8f;

	dmg *= tequila * resistFactor;

	// 端数丸め
	int idmg = (int)std::round(dmg);
	if (idmg < 0) idmg = 0;
	return idmg;
}

// デバフ適用（飲んだ直後）
void ApplyDrinkDebuff(Drink d, Fighter& loser, const DrinkEffectParams& par, const Resist& resist, bool taunted, RNG& rng, std::wstring& log) {
	int lv = resist.get(d);
	if (d == Drink::Gin) {
		int turns = std::max(1, par.ginTurns - (lv >= 2 ? 1 : 0));
		loser.debuff.ginBlurTurns += turns;
		log += L" [ジン]視界ぼかし+" + std::to_wstring(turns) + L"ターン";
	}
	else if (d == Drink::Vodka) {
		int turns = std::max(1, par.vodkaTurns - (lv >= 2 ? 1 : 0));
		loser.debuff.vodkaPenaltyTurns += turns;
		log += L" [ウォッカ]役弱体+" + std::to_wstring(turns) + L"ターン";
	}
	else if (d == Drink::Tequila) {
		float t = par.tequilaBaseMult * (lv == 2 ? 0.8f : (lv == 0 ? 1.2f : 1.0f));
		// 重ねがけしない（強い方優先）
		loser.debuff.tequilaMult = std::max(loser.debuff.tequilaMult, t);
		log += L" [テキーラ]被ダメ倍率×" + std::to_wstring((int)std::round(loser.debuff.tequilaMult));
	}
	else if (d == Drink::Rum) {
		float base = par.rumExtraBase;
		if (lv == 2) base *= 0.5f; // 強い→発生率低下
		if (lv == 0) base *= 1.3f; // 弱い→発生率上昇
		if (taunted) base += 0.15f; // 煽り補正
		loser.debuff.extraDrinkBias += base * 0.5f; // 蓄積は控えめ
		log += L" [ラム]もう一杯リスク上昇";
	}
	else if (d == Drink::OrangeJuice) {
		loser.debuff.clearAll();
		log += L" [オレンジジュース]デバフ解除 & 小回復";
	}
}

// もう一杯判定（ラム用）: trueで追加一杯
bool CheckOneMoreDrink(const DebuffState& losDebuff, const Resist& resist, Drink d, RNG& rng) {
	if (d != Drink::Rum) return false;
	float p = 0.5f + losDebuff.extraDrinkBias; // 基礎0.5から
	// 軽くクランプ
	if (p < 0.05f) p = 0.05f;
	if (p > 0.9f) p = 0.9f;
	return rng.frand() < p;
}

// ===================== 描画関連 =====================

void DrawHPBar(int x, int y, int w, int h, int hp, int hpMax, const wchar_t* label, int colorFill) {
	DrawBox(x, y, x + w, y + h, gGray, FALSE);
	float rate = (hpMax > 0) ? (float)hp / (float)hpMax : 0.0f;
	if (rate < 0) rate = 0;
	int ww = (int)(w * rate);
	DrawBox(x + 1, y + 1, x + ww - 1, y + h - 1, colorFill, TRUE);
	DrawFormatString(x, y - 18, gWhite, L"%s %d/%d", label, hp, hpMax);
}

void DrawDiceBox(int x, int y, int val, bool blur) {
	DrawBox(x, y, x + 100, y + 100, gWhite, FALSE);
	if (val > 0) {
		if (blur) {
			// ぼかし風：数字の多重描画
			for (int i = 0; i < 6; i++) {
				int dx = x + 35 + (i % 3) * 2;
				int dy = y + 35 + (i / 3) * 2;
				DrawFormatString(dx, dy, gGray, L"%d", val);
			}
			DrawFormatString(x + 40, y + 40, gPink, L"%d", val);
		}
		else {
			DrawFormatString(x + 40, y + 40, gWhite, L"%d", val);
		}
	}
	else {
		DrawString(x + 40, y + 40, L"-", gGray);
	}
}

void DrawDebuffInfo(int x, int y, const DebuffState& d) {
	int dy = 0;
	if (d.ginBlurTurns > 0) {
		DrawFormatString(x, y + dy, gCyan, L"ジン: ぼかし %dT", d.ginBlurTurns); dy += 18;
	}
	if (d.vodkaPenaltyTurns > 0) {
		DrawFormatString(x, y + dy, gOrange, L"ウォッカ: 役-1 %dT", d.vodkaPenaltyTurns); dy += 18;
	}
	if (d.tequilaMult > 1.0f) {
		DrawFormatString(x, y + dy, gYellow, L"テキーラ: 被ダメ×%.1f", d.tequilaMult); dy += 18;
	}
	if (d.extraDrinkBias > 0.01f) {
		DrawFormatString(x, y + dy, gGreen, L"ラム: もう一杯↑");
	}
}

// ===================== メイン =====================
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	SetOutApplicationLogValidFlag(FALSE);
	ChangeWindowMode(TRUE);
	SetGraphMode(1280, 720, 32);
	SetBackgroundColor(18, 22, 26);
	if (DxLib_Init() == -1) return -1;
	SetDrawScreen(DX_SCREEN_BACK);

	gWhite = GetColor(230, 230, 230);
	gGray = GetColor(90, 90, 90);
	gYellow = GetColor(255, 220, 120);
	gCyan = GetColor(120, 220, 255);
	gGreen = GetColor(120, 220, 120);
	gRed = GetColor(255, 120, 120);
	gBlue = GetColor(120, 150, 255);
	gOrange = GetColor(255, 170, 90);
	gPink = GetColor(255, 140, 200);

	RNG rng;
	InputState in;
	DrinkEffectParams params;

	// ----- プレイヤー初期化 -----
	Fighter player;
	player.name = L"あなた";
	player.hp = player.hpMax = 100;
	player.x = 220; player.y = 520;

	// ----- お客さん（ステージ）定義 -----
	std::vector<Customer> queue;
	{
		Customer c1;
		c1.name = L"会社員・田中";
		c1.hp = c1.hpMax = 80;
		c1.difficulty = 1;
		c1.drinkPool = { Drink::Tequila, Drink::Gin, Drink::Rum, Drink::OrangeJuice };
		c1.resist.level = {
			{Drink::Tequila, 1}, {Drink::Gin, 1}, {Drink::Vodka,1}, {Drink::Rum,1}, {Drink::OrangeJuice,1}
		};
		queue.push_back(c1);

		Customer c2;
		c2.name = L"酒豪・佐藤";
		c2.hp = c2.hpMax = 120;
		c2.difficulty = 2;
		c2.drinkPool = { Drink::Tequila, Drink::Gin, Drink::Vodka, Drink::Rum, Drink::OrangeJuice };
		c2.resist.level = {
			{Drink::Tequila, 2}, {Drink::Gin, 1}, {Drink::Vodka,1}, {Drink::Rum,1}, {Drink::OrangeJuice,1}
		};
		queue.push_back(c2);

		Customer c3;
		c3.name = L"夜の帝王";
		c3.hp = c3.hpMax = 150;
		c3.difficulty = 3;
		c3.drinkPool = { Drink::Tequila, Drink::Gin, Drink::Vodka, Drink::Rum, Drink::OrangeJuice };
		c3.resist.level = {
			{Drink::Tequila, 2}, {Drink::Gin, 2}, {Drink::Vodka,2}, {Drink::Rum,2}, {Drink::OrangeJuice,1}
		};
		queue.push_back(c3);
	}

	int currentCustomerIdx = 0;
	Fighter customer; // 表示用にFighterに落とす
	auto LoadCustomer = [&](const Customer& c) {
		customer.name = c.name;
		customer.hp = c.hp;
		customer.hpMax = c.hpMax;
		customer.resist = c.resist;
		customer.debuff.clearAll();
		customer.x = 960; customer.y = 180;
		};
	LoadCustomer(queue[currentCustomerIdx]);

	// ----- ターン状態 -----
	TurnState T;
	auto PickDrink = [&](const Customer& c)->Drink {
		// 難易度が上がるほど強い酒を選びがち（簡易実装）
		int r = rng.irand(0, (int)c.drinkPool.size() - 1);
		return c.drinkPool[r];
		};
	T.currentDrink = PickDrink(queue[currentCustomerIdx]);

	// UI ヘルプ
	const wchar_t* helpTitle = L"[左クリック]開始 / [Esc]終了";
	const wchar_t* helpGame =
		L"[WASD/矢印]移動  [左クリック]振る  [右クリック]確定/次へ  [Q]倍賭け  [E]煽り  [Esc]終了\n"
		L"勝った側は飲まない。負け側が『客の選んだ酒』を飲み、デバフや回復が適用される。";
	const wchar_t* helpSafe =
		L"※お酒は節度をもって。未成年者はソフトドリンクでのプレイを想定しています。";

	SceneType scene = SceneType::Title;
	std::wstring lastLog;

	// メインループ
	while (ProcessMessage() == 0) {
		in.update();
		if (in.keyEsc) break;

		// 背景
		ClearDrawScreen();

		if (scene == SceneType::Title) {
			DrawString(60, 60, L"ちんちろ飲みげーむ", gYellow);
			DrawString(60, 120, L"閉店まで客をべろべろにして帰らせろ！", gWhite);
			DrawString(60, 160, helpSafe, gCyan);
			DrawString(60, 220, helpTitle, gGreen);
			DrawString(60, 260, L"仕様: サイコロは完全確率。ウォッカ等はRNGではなく“役の強度”へ影響。", gGray);

			if (in.pressedL()) {
				scene = SceneType::Game;
			}
		}
		else if (scene == SceneType::Game) {
			// ---- 入力：移動（演出） ----
			float vx = 0, vy = 0;
			if (in.keyA || in.keyLeft)  vx -= 1.f;
			if (in.keyD || in.keyRight) vx += 1.f;
			if (in.keyW || in.keyUp)    vy -= 1.f;
			if (in.keyS || in.keyDown)  vy += 1.f;
			player.x = std::clamp(player.x + vx * 3.0f, 40.f, 1240.f);
			player.y = std::clamp(player.y + vy * 3.0f, 360.f, 680.f);

			// ---- サイコロ振り/確定 ----
			if (in.pressedL()) {
				if (T.currentActor == 0 && !T.fixedP && T.rollCountP < 3) {
					T.diceP[0] = rng.irand(1, 6); T.diceP[1] = rng.irand(1, 6); T.diceP[2] = rng.irand(1, 6);
					T.handP = Judge3(T.diceP[0], T.diceP[1], T.diceP[2]);
					T.rollCountP++;
					if (T.rollCountP >= 3) { T.fixedP = true; T.currentActor = 1; }
				}
				else if (T.currentActor == 1 && !T.fixedC && T.rollCountC < 3) {
					T.diceC[0] = rng.irand(1, 6); T.diceC[1] = rng.irand(1, 6); T.diceC[2] = rng.irand(1, 6);
					T.handC = Judge3(T.diceC[0], T.diceC[1], T.diceC[2]);
					T.rollCountC++;
					if (T.rollCountC >= 3) { T.fixedC = true; }
				}
			}
			if (in.pressedR()) {
				if (T.currentActor == 0 && !T.fixedP && T.rollCountP > 0) {
					T.fixedP = true; T.currentActor = 1;
				}
				else if (T.currentActor == 1 && !T.fixedC && T.rollCountC > 0) {
					T.fixedC = true;
				}
				else if (T.fixedP && T.fixedC) {
					// ---- 集計 ----
					// Vodkaペナを適用した見かけの役で比較（RNGは公平）
					HandResult effP = ApplyVodkaPenaltyIf(T.handP, player.debuff);
					HandResult effC = ApplyVodkaPenaltyIf(T.handC, customer.debuff);

					int cmp = CompareHands(effP, effC);
					int loser = -1;
					if (cmp > 0) loser = 1;       // 客が飲む
					else if (cmp < 0) loser = 0;  // プレイヤーが飲む
					lastLog.clear();
					lastLog += L"あなた[" + T.handP.name + L"] vs " + customer.name + L"[" + T.handC.name + L"] → ";

					if (cmp == 0) {
						lastLog += L"引き分け（誰も飲まない）";
					}
					else {
						// 倍賭け倍率
						int wager = T.wagerDouble ? 2 : 1;
						// 飲むドリンク
						Drink d = T.currentDrink;

						// ダメージ計算
						Fighter* target = (loser == 0) ? &player : &customer;
						const Resist& res = (loser == 0) ? player.resist : customer.resist;

						int dmg = ComputeDamage(params, d, res, target->debuff, wager);

						// テキーラ蓄積倍率がある場合は最後に乗算
						dmg = (int)std::round(dmg * target->debuff.tequilaMult);

						// 適用
						if (d == Drink::OrangeJuice) {
							// 回復＆デバフ解除
							target->hp = std::min(target->hpMax, target->hp + params.ojHeal);
							ApplyDrinkDebuff(d, *target, params, res, T.taunted, rng, lastLog);
							lastLog += L" / 回復+" + std::to_wstring(params.ojHeal);
						}
						else {
							target->hp = std::max(0, target->hp - dmg);
							ApplyDrinkDebuff(d, *target, params, res, T.taunted, rng, lastLog);
							lastLog += L" / " + DrinkName(d) + L"で -" + std::to_wstring(dmg);
							// ラムの「もう一杯」
							if (CheckOneMoreDrink(target->debuff, res, d, rng)) {
								int dmg2 = ComputeDamage(params, d, res, target->debuff, wager);
								dmg2 = (int)std::round(dmg2 * target->debuff.tequilaMult);
								target->hp = std::max(0, target->hp - dmg2);
								lastLog += L" / もう一杯!! -" + std::to_wstring(dmg2);
							}
						}

						// 次ターンに向け、両者の時間減衰
						player.debuff.decay();
						customer.debuff.decay();
					}

					// 勝敗チェック（客がダウン→次の客 / プレイヤーがダウン→敗北）
					if (customer.hp <= 0) {
						lastLog += L" / " + customer.name + L"はべろべろで退店！";
						currentCustomerIdx++;
						if (currentCustomerIdx >= (int)queue.size()) {
							scene = SceneType::Result;
							// クリア
							lastLog += L" すべての客を帰らせた！閉店成功！";
						}
						else {
							LoadCustomer(queue[currentCustomerIdx]);
							T = TurnState{};
							T.currentDrink = PickDrink(queue[currentCustomerIdx]);
						}
					}
					else if (player.hp <= 0) {
						scene = SceneType::Result;
						lastLog += L" / あなたは潰れてしまった…迎えの車が到着（ゲームオーバー）";
					}
					else {
						// 次ターンへ準備
						T = TurnState{};
						T.currentDrink = PickDrink(queue[currentCustomerIdx]);
					}
				}
			}

			// 倍賭け・煽り
			if (in.keyQ) T.wagerDouble = true;
			if (in.keyE) T.taunted = true;

			// ---- 描画 ----
			DrawString(20, 20, L"ちんちろ飲みげーむ - プレイ中", gYellow);
			DrawString(20, 48, helpGame, gGray);
			DrawString(20, 76, helpSafe, gCyan);

			// HPバー
			DrawHPBar(40, 120, 420, 20, player.hp, player.hpMax, L"あなた", gGreen);
			DrawHPBar(820, 120, 420, 20, customer.hp, customer.hpMax, customer.name.c_str(), gRed);

			// デバフ情報
			DrawDebuffInfo(40, 150, player.debuff);
			DrawDebuffInfo(820, 150, customer.debuff);

			// 倍賭け・煽り表示
			DrawFormatString(40, 210, T.wagerDouble ? gOrange : gWhite, L"[Q] 倍賭け: %s", T.wagerDouble ? L"ON" : L"OFF");
			DrawFormatString(220, 210, T.taunted ? gPink : gWhite, L"[E] 煽り: %s", T.taunted ? L"使用済" : L"未使用");

			// 今回のドリンク
			DrawFormatString(40, 240, gWhite, L"客の選んだ酒: %s", DrinkName(T.currentDrink).c_str());

			// プレイヤーのダイス（左）
			bool blurP = (player.debuff.ginBlurTurns > 0);
			DrawDiceBox(40, 300, T.diceP[0], blurP);
			DrawDiceBox(160, 300, T.diceP[1], blurP);
			DrawDiceBox(280, 300, T.diceP[2], blurP);
			DrawFormatString(40, 420, gWhite, L"あなた: %s (%d/3)", T.handP.name.c_str(), T.rollCountP);
			if (!T.fixedP && T.rollCountP == 0) DrawString(40, 450, L"左クリックで振る / 右クリックで確定", gGray);

			// 客のダイス（右）
			bool blurC = (customer.debuff.ginBlurTurns > 0);
			DrawDiceBox(820, 180, T.diceC[0], blurC);
			DrawDiceBox(940, 180, T.diceC[1], blurC);
			DrawDiceBox(1060, 180, T.diceC[2], blurC);
			DrawFormatString(820, 300, gWhite, L"%s: %s (%d/3)", customer.name.c_str(), T.handC.name.c_str(), T.rollCountC);

			// Actorガイド
			std::wstring actor = (T.currentActor == 0) ? L"あなたの番" : (T.fixedP ? L"相手の番" : L"先に自分を確定してね");
			DrawFormatString(40, 500, gYellow, L"%s", actor.c_str());

			// プレイヤー表示（簡易）
			DrawCircle((int)player.x, (int)player.y, 20, gBlue, TRUE);
			DrawString((int)player.x - 30, (int)player.y + 28, L"YOU", gWhite);

			// 客の表示（簡易）
			DrawCircle((int)customer.x, (int)customer.y, 20, gRed, TRUE);
			DrawString((int)customer.x - 40, (int)customer.y + 28, customer.name.c_str(), gWhite);

			// ログ
			if (!lastLog.empty()) DrawFormatString(40, 560, gWhite, L"%s", lastLog.c_str());
		}
		else if (scene == SceneType::Result) {
			DrawString(60, 60, L"結果", gYellow);
			DrawString(60, 100, lastLog.c_str(), gWhite);
			if (currentCustomerIdx >= (int)queue.size()) {
				DrawString(60, 140, L"閉店成功！本日はこれにて。", gGreen);
			}
			else {
				DrawString(60, 140, L"潰れてしまった…また次回。", gRed);
			}
			DrawString(60, 200, L"[Esc]で終了（必要ならすぐ再実装します）", gGray);
		}

		ScreenFlip();
	}

	DxLib_End();
	return 0;
}