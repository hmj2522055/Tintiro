#pragma once
#include <string>
#include <tuple>

enum class HandType { None, Hifumi, MeKaku, Shigoro, Zorome, Pinzoro };

struct HandResult {
	HandType type = HandType::None;
	int sub = 0;                 // Zorome:–Ú / MeKaku:ƒVƒ“ƒOƒ‹ƒgƒ“–Ú
	std::wstring name = L"–¢”»’è";
	std::tuple<int, int> scoreKey() const;
};

HandResult Judge3(int a, int b, int c);
HandResult ApplyVodkaPenaltyIf(const HandResult& h, int vodkaPenaltyTurns);
int CompareHands(const HandResult& A, const HandResult& B);