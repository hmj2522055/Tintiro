#include "Game.h"
#include "DxLib.h"
#include <algorithm>
#include <cmath>

// ===== RNG =====
Game::RNG::RNG() {
	std::random_device rd; mt.seed(rd());
}
int Game::RNG::irand(int a, int b) {
	std::uniform_int_distribution<int> dist(a, b);
	return dist(mt);
}
float Game::RNG::frand() {
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	return dist(mt);
}

// ===== Game =====
Game::Game() {}

bool Game::init() {
	// Colors
	cWhite = GetColor(230, 230, 230);
	cGray = GetColor(90, 90, 90);
	cYellow = GetColor(255, 220, 120);
	cCyan = GetColor(120, 220, 255);
	cGreen = GetColor(120, 220, 120);
	cRed = GetColor(255, 120, 120);
	cBlue = GetColor(120, 150, 255);
	cOrange = GetColor(255, 170, 90);
	cPink = GetColor(255, 140, 200);

	// ヘルプ文
	helpTitle = L"[左クリック]開始 / [Esc]終了";
	helpGame =
		L"[WASD/矢印]移動  [左クリック]振る  [右クリック]確定/次へ  [Q]倍賭け  [E]煽り  [Esc]終了\n"
		L"勝った側は飲まない。負け側が『客の選んだ酒』を飲み、デバフや回復が適用される。";
	helpSafe = L"※お酒は節度をもって。未成年はソフトドリンクでのプレイを想定しています。";

	// プレイヤー
	player.name = L"あなた";
	player.hp = player.hpMax = 100;
	player.x = 220; player.y = 520;

	// 客（ステージ）
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

	currentCustomerIdx = 0;
	loadCustomer(currentCustomerIdx);

	scene = SceneType::Title;
	return true;
}

void Game::run() {
	while (ProcessMessage() == 0) {
		in.update();
		if (in.keyEsc) break;

		update();
		draw();
		ScreenFlip();
	}
}

void Game::startGame() {
	// 進行リセット
	player.hp = player.hpMax = 100;
	player.debuff.clearAll();

	currentCustomerIdx = 0;
	loadCustomer(currentCustomerIdx);

	T = TurnState{};
	T.currentDrink = pickDrink(queue[currentCustomerIdx]);
	lastLog.clear();
	scene = SceneType::Game;
}

void Game::loadCustomer(int idx) {
	const Customer& c = queue[idx];
	customer.name = c.name;
	customer.hp = c.hp;
	customer.hpMax = c.hpMax;
	customer.resist = c.resist;
	customer.debuff.clearAll();
	customer.x = 960; customer.y = 180;
}

Drink Game::pickDrink(const Customer& c) {
	// 難易度で重み付けする場合はここで調整（いまは等確率）
	int r = rng.irand(0, (int)c.drinkPool.size() - 1);
	return c.drinkPool[r];
}

// ===== Update =====
void Game::update() {
	if (scene == SceneType::Title) {
		if (in.pressedL()) {
			startGame();
		}
		return;
	}
	if (scene == SceneType::Result) {
		// Esc待ち
		return;
	}

	// ===== Game =====
	// 移動（演出）
	auto clampf = [](float v, float lo, float hi) -> float {
		return v < lo ? lo : (v > hi ? hi : v);
		};

	float vx = 0, vy = 0;
	if (in.keyA || in.keyLeft)  vx -= 1.f;
	if (in.keyD || in.keyRight) vx += 1.f;
	if (in.keyW || in.keyUp)    vy -= 1.f;
	if (in.keyS || in.keyDown)  vy += 1.f;
	player.x = clampf(player.x + vx * 3.0f, 40.0f, 1240.0f);
	player.y = clampf(player.y + vy * 3.0f, 360.0f, 680.0f);

	// 倍賭けトグル / 煽り
	if (in.pressedQ()) T.wagerDouble = !T.wagerDouble;
	if (in.pressedE()) T.taunted = true;

	// サイコロ操作
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
			// ===== 集計 =====
			// ウォッカの役弱体（RNGは公平のまま、見かけの強度だけ -1 段）
			HandResult effP = ApplyVodkaPenaltyIf(T.handP, player.debuff.vodkaPenaltyTurns);
			HandResult effC = ApplyVodkaPenaltyIf(T.handC, customer.debuff.vodkaPenaltyTurns);

			int cmp = CompareHands(effP, effC);
			int loser = -1;
			lastLog = L"あなた[" + T.handP.name + L"] vs " + customer.name + L"[" + T.handC.name + L"] → ";

			if (cmp == 0) {
				lastLog += L"引き分け（誰も飲まない）";
			}
			else {
				loser = (cmp > 0) ? 1 : 0; // 1=客が飲む / 0=プレイヤーが飲む
				int wager = T.wagerDouble ? 2 : 1;
				Drink d = T.currentDrink;

				Fighter* tgt = (loser == 0) ? &player : &customer;
				const Resist& res = (loser == 0) ? player.resist : customer.resist;

				// ダメージ計算
				int dmg = ComputeDamage(params, d, res, tgt->debuff, wager);
				// テキーラ蓄積倍率を最後に掛ける（強い方を保持する運用）
				dmg = (int)std::round(dmg * tgt->debuff.tequilaMult);

				if (d == Drink::OrangeJuice) {
					// 回復＆デバフ解除
					tgt->hp = (std::min)(tgt->hpMax, tgt->hp + params.ojHeal);
					ApplyDrinkDebuff(d, tgt->debuff, params, res, T.taunted, lastLog);
					lastLog += L" / 回復+" + std::to_wstring(params.ojHeal);
				}
				else {
					tgt->hp = (std::max)(0, tgt->hp - dmg);
					ApplyDrinkDebuff(d, tgt->debuff, params, res, T.taunted, lastLog);
					lastLog += L" / " + DrinkName(d) + L"で -" + std::to_wstring(dmg);

					// ラムの「もう一杯」
					if (CheckOneMoreDrink(d, tgt->debuff, res, T.taunted, rng.frand())) {
						int dmg2 = ComputeDamage(params, d, res, tgt->debuff, wager);
						dmg2 = (int)std::round(dmg2 * tgt->debuff.tequilaMult);
						tgt->hp = (std::max)(0, tgt->hp - dmg2);
						lastLog += L" / もう一杯!! -" + std::to_wstring(dmg2);
					}
				}

				// 時間減衰
				player.debuff.decay();
				customer.debuff.decay();
			}

			// 勝敗判定
			if (customer.hp <= 0) {
				lastLog += L" / " + customer.name + L"はべろべろで退店！";
				currentCustomerIdx++;
				if (currentCustomerIdx >= (int)queue.size()) {
					scene = SceneType::Result;
					lastLog += L" すべての客を帰らせた！閉店成功！";
				}
				else {
					loadCustomer(currentCustomerIdx);
					T = TurnState{};
					T.currentDrink = pickDrink(queue[currentCustomerIdx]);
				}
			}
			else if (player.hp <= 0) {
				scene = SceneType::Result;
				lastLog += L" / あなたは潰れてしまった…迎えの車が到着（ゲームオーバー）";
			}
			else {
				// 次ターン
				T = TurnState{};
				T.currentDrink = pickDrink(queue[currentCustomerIdx]);
			}
		}
	}
}

// ===== Draw =====
void Game::draw() {
	ClearDrawScreen();

	if (scene == SceneType::Title) {
		DrawString(60, 60, L"ちんちろ飲みげーむ", cYellow);
		DrawString(60, 120, L"閉店まで客をべろべろにして帰らせろ！", cWhite);
		DrawString(60, 160, helpSafe, cCyan);
		DrawString(60, 220, helpTitle, cGreen);
		DrawString(60, 260, L"仕様: サイコロは完全確率。ウォッカ等はRNGではなく“役の強度”へ影響。", cGray);
		return;
	}
	if (scene == SceneType::Result) {
		DrawString(60, 60, L"結果", cYellow);
		DrawString(60, 100, lastLog.c_str(), cWhite);
		if (currentCustomerIdx >= (int)queue.size()) {
			DrawString(60, 140, L"閉店成功！本日はこれにて。", cGreen);
		}
		else {
			DrawString(60, 140, L"潰れてしまった…また次回。", cRed);
		}
		DrawString(60, 200, L"[Esc]で終了（必要ならすぐ再実装します）", cGray);
		return;
	}

	// ===== Game =====
	DrawString(20, 20, L"ちんちろ飲みげーむ - プレイ中", cYellow);
	DrawString(20, 48, helpGame, cGray);
	DrawString(20, 76, helpSafe, cCyan);

	// HPバー
	drawHPBar(40, 120, 420, 20, player.hp, player.hpMax, L"あなた", cGreen);
	drawHPBar(820, 120, 420, 20, customer.hp, customer.hpMax, customer.name.c_str(), cRed);

	// デバフ
	drawDebuffInfo(40, 150, player.debuff);
	drawDebuffInfo(820, 150, customer.debuff);

	// 倍賭け/煽り
	DrawFormatString(40, 210, T.wagerDouble ? cOrange : cWhite, L"[Q] 倍賭け: %s", T.wagerDouble ? L"ON" : L"OFF");
	DrawFormatString(220, 210, T.taunted ? cPink : cWhite, L"[E] 煽り: %s", T.taunted ? L"使用済" : L"未使用");

	// 今回のドリンク
	DrawFormatString(40, 240, cWhite, L"客の選んだ酒: %s", DrinkName(T.currentDrink).c_str());

	// プレイヤーのダイス
	bool blurP = (player.debuff.ginBlurTurns > 0);
	drawDiceBox(40, 300, T.diceP[0], blurP);
	drawDiceBox(160, 300, T.diceP[1], blurP);
	drawDiceBox(280, 300, T.diceP[2], blurP);
	DrawFormatString(40, 420, cWhite, L"あなた: %s (%d/3)", T.handP.name.c_str(), T.rollCountP);
	if (!T.fixedP && T.rollCountP == 0) DrawString(40, 450, L"左クリックで振る / 右クリックで確定", cGray);

	// 客のダイス
	bool blurC = (customer.debuff.ginBlurTurns > 0);
	drawDiceBox(820, 180, T.diceC[0], blurC);
	drawDiceBox(940, 180, T.diceC[1], blurC);
	drawDiceBox(1060, 180, T.diceC[2], blurC);
	DrawFormatString(820, 300, cWhite, L"%s: %s (%d/3)", customer.name.c_str(), T.handC.name.c_str(), T.rollCountC);

	// 番
	std::wstring actor = (T.currentActor == 0) ? L"あなたの番" : (T.fixedP ? L"相手の番" : L"先に自分を確定してね");
	DrawFormatString(40, 500, cYellow, L"%s", actor.c_str());

	// プレイヤー（演出）
	DrawCircle((int)player.x, (int)player.y, 20, cBlue, TRUE);
	DrawString((int)player.x - 30, (int)player.y + 28, L"YOU", cWhite);

	// 客（演出）
	DrawCircle((int)customer.x, (int)customer.y, 20, cRed, TRUE);
	DrawString((int)customer.x - 40, (int)customer.y + 28, customer.name.c_str(), cWhite);

	// ログ
	if (!lastLog.empty()) DrawFormatString(40, 560, cWhite, L"%s", lastLog.c_str());
}

// ===== UI helpers =====
void Game::drawHPBar(int x, int y, int w, int h, int hp, int hpMax, const wchar_t* label, int colorFill) {
	DrawBox(x, y, x + w, y + h, cGray, FALSE);
	float rate = (hpMax > 0) ? (float)hp / (float)hpMax : 0.0f;
	if (rate < 0) rate = 0;
	int ww = (int)(w * rate);
	DrawBox(x + 1, y + 1, x + ww - 1, y + h - 1, colorFill, TRUE);
	DrawFormatString(x, y - 18, cWhite, L"%s %d/%d", label, hp, hpMax);
}

void Game::drawDiceBox(int x, int y, int val, bool blur) {
	DrawBox(x, y, x + 100, y + 100, cWhite, FALSE);
	if (val > 0) {
		if (blur) {
			for (int i = 0; i < 6; i++) {
				int dx = x + 35 + (i % 3) * 2;
				int dy = y + 35 + (i / 3) * 2;
				DrawFormatString(dx, dy, cGray, L"%d", val);
			}
			DrawFormatString(x + 40, y + 40, cPink, L"%d", val);
		}
		else {
			DrawFormatString(x + 40, y + 40, cWhite, L"%d", val);
		}
	}
	else {
		DrawString(x + 40, y + 40, L"-", cGray);
	}
}

void Game::drawDebuffInfo(int x, int y, const DebuffState& d) {
	int dy = 0;
	if (d.ginBlurTurns > 0) {
		DrawFormatString(x, y + dy, cCyan, L"ジン: ぼかし %dT", d.ginBlurTurns); dy += 18;
	}
	if (d.vodkaPenaltyTurns > 0) {
		DrawFormatString(x, y + dy, cOrange, L"ウォッカ: 役-1 %dT", d.vodkaPenaltyTurns); dy += 18;
	}
	if (d.tequilaMult > 1.0f) {
		DrawFormatString(x, y + dy, cYellow, L"テキーラ: 被ダメ×%.1f", d.tequilaMult); dy += 18;
	}
	if (d.extraDrinkBias > 0.01f) {
		DrawFormatString(x, y + dy, cGreen, L"ラム: もう一杯↑");
	}
}