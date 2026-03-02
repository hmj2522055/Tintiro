#include "Dice.h"
#include <algorithm>

std::tuple<int, int> HandResult::scoreKey() const {
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

HandResult Judge3(int a, int b, int c) {
	int d[3] = { a,b,c };
	std::sort(d, d + 3);
	if (d[0] == d[1] && d[1] == d[2]) {
		if (d[0] == 1) return { HandType::Pinzoro, 1, L"ƒsƒ“ƒ]ƒ(1-1-1)" };
		return { HandType::Zorome, d[0], L"ƒ]ƒ–Ú(" + std::to_wstring(d[0]) + L")" };
	}
	if (d[0] == 1 && d[1] == 2 && d[2] == 3) return { HandType::Hifumi, 0, L"ƒqƒtƒ~(1-2-3)" };
	if (d[0] == 4 && d[1] == 5 && d[2] == 6) return { HandType::Shigoro, 0, L"ƒVƒSƒ(4-5-6)" };
	if (d[0] == d[1] && d[1] != d[2]) return { HandType::MeKaku, d[2], L"–ÚŠm(" + std::to_wstring(d[2]) + L")" };
	if (d[1] == d[2] && d[0] != d[1]) return { HandType::MeKaku, d[0], L"–ÚŠm(" + std::to_wstring(d[0]) + L")" };
	return { HandType::None, 0, L"–ğ‚È‚µ" };
}

HandResult ApplyVodkaPenaltyIf(const HandResult& h, int vodkaPenaltyTurns) {
	if (vodkaPenaltyTurns <= 0) return h;
	HandResult r = h;
	if (h.type == HandType::Pinzoro)      r.type = HandType::Zorome;
	else if (h.type == HandType::Zorome)  r.type = HandType::Shigoro;
	else if (h.type == HandType::Shigoro) r.type = HandType::MeKaku;
	else if (h.type == HandType::MeKaku)  r.type = HandType::Hifumi;
	else if (h.type == HandType::Hifumi)  r.type = HandType::None;
	return r;
}

int CompareHands(const HandResult& A, const HandResult& B) {
	if (A.scoreKey() > B.scoreKey()) return 1;
	if (A.scoreKey() < B.scoreKey()) return -1;
	return 0;
}