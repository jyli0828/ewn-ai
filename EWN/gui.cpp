#include "gui.h"

int player = 1;
// indices of player_names
int playerA = 0, playerB = 0;
bool player_is_red = false;
const string player_names[4] = { "player", "MCTS", "Random", "Model" };

// N is the size of buf; a longer string is cut instead of overflowing it
template <size_t N>
void string2tchar(TCHAR (&buf)[N], const string& src) {
#ifdef UNICODE
	_sntprintf_s(buf, N, _TRUNCATE, _T("%S"), src.c_str());
#else
	_sntprintf_s(buf, N, _TRUNCATE, _T("%s"), src.c_str());
#endif
}

// The background image, read from disk only the first time; reading it takes about 35 ms
static IMAGE& background() {
	static IMAGE img;
	static bool loaded = false;
	if (!loaded) {
		loadimage(&img, _T("./picture/gui_background.jpg"), WIDTH, HEIGHT);
		loaded = true;
	}
	return img;
}

void GUI::init_environment() {
	putimage(0, 0, &background());
	this->draw_squares();
	this->print_info();
}

int GUI::choose_option(int x, const int y_list[], const LPCTSTR text_list[], int n) {
	ExMessage msg;
	int hovered = -1;
	// clicks made before the menu appeared do not choose
	flushmessage(EX_MOUSE);
	while (1) {
		// each option over the background it covers, so a yellow one can turn black again
		BeginBatchDraw();
		for (int k = 0; k < n; k++) {
			putimage(x, y_list[k], textwidth(text_list[k]), textheight(text_list[k]), &background(), x, y_list[k]);
			settextcolor((k == hovered) ? YELLOW : BLACK);
			outtextxy(x, y_list[k], text_list[k]);
		}
		EndBatchDraw();

		// wait until the mouse enters or leaves an option, or clicks one
		int at;
		do {
			getmessage(&msg, EX_MOUSE);
			at = -1;
			for (int k = 0; k < n; k++)
				if (msg.x >= x && msg.x < x + textwidth(text_list[k]) && msg.y >= y_list[k] && msg.y < y_list[k] + textheight(text_list[k]))
					at = k;
			if (msg.message == WM_LBUTTONDOWN && at >= 0) return at;
		} while (at == hovered);
		hovered = at;
	}
}

int GUI::begin_gui() {
	putimage(0, 0, &background());

	setbkmode(TRANSPARENT);
	settextcolor(BLACK);
	settextstyle(60, 0, _T("Elephant"));
	outtextxy(200, 70, _T("Please choose a mode")); // 406 x 60

	settextstyle(50, 0, _T("Berlin Sans FB"));
	int y_list[3] = { 170, 250, 330 };
	LPCTSTR text_list[3] = { _T("0)Player vs. Player"), _T("1)AI vs. Player"), _T("2)AI vs. AI") };
	int mode = this->choose_option(250, y_list, text_list, 3);

	if (mode == 0) cout << "Player vs. Player\n";
	else if (mode == 1) cout << "AI vs. Player\n";
	else cout << "AI vs. AI\n";
	return mode;
}

int GUI::select_AI() {
	putimage(0, 0, &background());

	setbkmode(TRANSPARENT);
	settextcolor(BLACK);
	settextstyle(60, 0, _T("Elephant"));
	outtextxy(100, 70, _T("Choose an AI"));	// 243 x 60

	bool first_player = (player == 1);
	if (first_player)
		outtextxy(520, 70, _T("(Player 1):")); // 186 x 60
	else
		outtextxy(520, 70, _T("(Player 2):")); // 186 x 60

	player = -player;
	settextstyle(50, 0, _T("Berlin Sans FB"));
	int y_list[3] = { 170, 250, 330 };
	LPCTSTR text_list[3] = { _T("0)MCTS"), _T("1)Random"), _T("2)Model") };
	int ai = this->choose_option(150, y_list, text_list, 3);

	if (first_player) playerA = ai + 1;
	else playerB = ai + 1;
	cout << "Player " << (first_player ? 1 : 2) << ": " << player_names[ai + 1] << "\n\n";
	return ai;
}

bool GUI::select_side() {
	putimage(0, 0, &background());

	setbkmode(TRANSPARENT);
	settextcolor(BLACK);
	settextstyle(60, 0, _T("Elephant"));
	outtextxy(150, 70, _T("Please choose which side"));	// 449 x 60

	settextstyle(50, 0, _T("Berlin Sans FB"));
	int y_list[2] = { 200, 280 };
	LPCTSTR text_list[2] = { _T("The red side(first)"), _T("The blue side") };
	player_is_red = (this->choose_option(280, y_list, text_list, 2) == 0);

	if (player_is_red) cout << "Player is the red side(first)\n";
	else cout << "Player is the blue side\n";
	cout << '\n';
	return player_is_red;
}

Movement GUI::select_move(const Game& game) {
	// the square of the chosen chess, and the square under the mouse that a click would act on; -1 if none
	int chosen = -1, hovered = -1;
	bool hovered_move = false;
	ExMessage msg;
	// clicks made while the other side was moving do not choose
	flushmessage(EX_MOUSE);
	this->draw_choice(game, chosen, hovered, hovered_move);

	while (1) {
		getmessage(&msg, EX_MOUSE);
		if (msg.message != WM_MOUSEMOVE && msg.message != WM_LBUTTONDOWN) continue;

		// the board starts at (10, 10); a click in that margin would round to row or column 0
		int x = (msg.x - 10) / 90, y = (msg.y - 10) / 90, sq = -1;
		if (msg.x >= 10 && msg.y >= 10 && x < 5 && y < 5) sq = y * 5 + x;
		// what a click on sq does: move the chosen chess there, or choose the movable chess on it
		int chs_index = (chosen >= 0) ? game.get_chess_index(game.chessPosition.fullMask[chosen]) : -1, direct = -1;
		bool movable = false;
		if (sq >= 0) {
			BitBoard tmp = game.chessPosition.fullMask[sq];
			for (int m = 0; m < 2; m++)
				movable = movable || tmp == game.cur_chs_ptr[game.movable_chs_index_list[m]];
			for (int i = 0; i < 3 && chosen >= 0; i++)
				// a move off the board edge would wrap to the next row, so check it first
				if (game.check_in_board(Movement(chs_index, i)) && game.destination(Movement(chs_index, i)) == tmp) direct = i;
		}

		if (msg.message == WM_LBUTTONDOWN && direct >= 0) {
			this->draw_selected_direction(game.side, direct);
			cout << "move: (" << chs_index + 1 << ", " << direct << ")\n";
			return Movement(chs_index, direct);
		}
		bool clicked = (msg.message == WM_LBUTTONDOWN && movable);
		if (clicked) {
			chosen = (sq == chosen) ? -1 : sq;
			int chess = (chosen >= 0) ? game.get_chess_index(game.chessPosition.fullMask[chosen]) + 1 : 0;
			this->draw_selected_chess(chess);
			if (chess) cout << "chess " << chess << '\n';
		}
		int at = (direct >= 0 || movable) ? sq : -1;
		if (clicked || at != hovered || (direct >= 0) != hovered_move) {
			hovered = at, hovered_move = (direct >= 0);
			this->draw_choice(game, chosen, hovered, hovered_move);
		}
	}
}

void GUI::draw_chess(int x, int y, char chess, int turn) {
	settextcolor(WHITE);
	if (turn == -1) {
		setfillcolor(YELLOW); setbkcolor(YELLOW);
	}
	else if (turn == 0) {
		setfillcolor(RED); setbkcolor(RED);
	}
	else {
		setfillcolor(BLUE); setbkcolor(BLUE);
	}
	fillcircle(x, y, 40);
	settextstyle(40, 40, _T("Consolas"));
	outtextxy(x - 20, y - 20, chess);
}

void GUI::draw_movable_cnt_color(const Game& game) {
	int movable_chs_cnt = (game.movable_chs_index_list[0] == game.movable_chs_index_list[1]) ? 1 : 2;

	for (int i = 0; i < movable_chs_cnt; i++) {
		BitBoard tmp = game.cur_chs_ptr[game.movable_chs_index_list[i]];
		char c = game.movable_chs_index_list[i] + '1';

		for (int sq = 0; sq < 25; sq++) {
			if (tmp != game.chessPosition.fullMask[sq]) continue;
			setlinecolor(YELLOW);
			setlinestyle(PS_SOLID, 6);
			this->draw_chess(sq % 5 * 90 + 55, sq / 5 * 90 + 55, c, game.side);
			setlinestyle(PS_SOLID, 1);
			setlinecolor(WHITE);
			break;
		}
	}
}

void GUI::draw_selected_chess(int chess) {
	settextcolor(BLACK);
	setbkmode(TRANSPARENT);
	settextstyle(35, 0, _T("Consolas"));
	// over the background, which erases the chess chosen before; chess 0 only erases it
	putimage(700 + 75, 30 + 157, textwidth(_T('0')), textheight(_T('0')), &background(), 700 + 75, 30 + 157);
	if (chess > 0) outtextxy(700 + 75, 30 + 157, chess + '0');
}

void GUI::draw_selected_direction(bool side, int direct) {
	settextcolor(BLACK);
	setbkmode(TRANSPARENT);
	settextstyle(35, 0, _T("Consolas"));

	string dir_str[3] = {"Left", "Up", "Left-up"};
	if (!side)
		dir_str[0] = "Right", dir_str[1] = "Down", dir_str[2] = "Right-down";

	TCHAR buf[11];
	string2tchar(buf, dir_str[direct]);
	outtextxy(740 + int(-7.5 * dir_str[direct].size() + 55), 250, buf);
}

void GUI::draw_squares() {
	setfillcolor(WHITE);
	//block 90*90
	for (int x = 10; x < 450; x += 90)
		for (int y = 10; y < 450; y += 90)
			fillrectangle(x + 1, y + 1, x + 90 - 1, y + 90 - 1);
	line(700, 0, 700, 470);
}

void GUI::draw_pieces(const ChessPosition& chp) {
	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++) {
			if (chp.all & chp.fullMask[i * 5 + j]) {
				for (int k = 0; k < 6; k++) {
					if (chp.fullMask[i * 5 + j] & chp.chessRed[k]) {
						this->draw_chess(j * 90 + 55, i * 90 + 55, k + '1', 0);
						break;
					}
					else if (chp.fullMask[i * 5 + j] & chp.chessBlue[k]) {
						this->draw_chess(j * 90 + 55, i * 90 + 55, k + '1', 1);
						break;
					}
				}
			}
		}
	}
}

void GUI::rstboard(const ChessPosition& chp) {
	BeginBatchDraw();
	this->init_environment();
	this->draw_pieces(chp);
	EndBatchDraw();
}

void GUI::draw_choice(const Game& game, int chosen, int hovered, bool hovered_move) {
	BeginBatchDraw();
	this->draw_squares();
	if (hovered_move) {
		setfillcolor(RGB(255, 240, 140));
		solidrectangle(hovered % 5 * 90 + 11, hovered / 5 * 90 + 11, hovered % 5 * 90 + 99, hovered / 5 * 90 + 99);
	}
	this->draw_pieces(game.chessPosition);
	this->draw_movable_cnt_color(game);

	// the chosen chess, and a movable one under the mouse, turn yellow
	int yellow[2] = { chosen, hovered_move ? -1 : hovered };
	for (int sq : yellow) {
		if (sq < 0) continue;
		setlinecolor(YELLOW);
		setlinestyle(PS_SOLID, 6);
		this->draw_chess(sq % 5 * 90 + 55, sq / 5 * 90 + 55, game.get_chess_index(game.chessPosition.fullMask[sq]) + '1', -1);
		setlinestyle(PS_SOLID, 1);
		setlinecolor(WHITE);
	}
	EndBatchDraw();
}

void GUI::print_info() {
	int zeroX = 470, zeroY = 10;
	settextcolor(BLACK);
	setbkmode(TRANSPARENT);
	settextstyle(50, 0, _T("Comic Sans MS"));
	outtextxy(zeroX + 40, zeroY, _T("EinStein"));

	settextstyle(20, 0, _T("Times New Roman"));
	outtextxy(zeroX + 10, zeroY + 60, _T("Round:"));
	outtextxy(zeroX + 10, zeroY + 90, _T("Dice:"));
	outtextxy(zeroX + 10, zeroY + 120, _T("Movable chess:"));
	outtextxy(zeroX + 10, zeroY + 170, _T("Next move win rate:"));

	int tmpX = 700, tmpY = 10;
	outtextxy(tmpX + 60, tmpY + 20, _T("Time cost"));

	// a player on the red side plays against playerA; otherwise playerA is red and playerB blue
	string red = "The red side: " + player_names[player_is_red ? 0 : playerA];
	string blue = "The blue side: " + player_names[player_is_red ? playerA : playerB];

	TCHAR buf1[25], buf2[25];
	string2tchar(buf1, red);
	string2tchar(buf2, blue);
	outtextxy(tmpX + 20, tmpY + 60, buf1);
	outtextxy(tmpX + 20, tmpY + 110, buf2);

	outtextxy(tmpX + 20, tmpY + 160, _T("Selected chess"));
	outtextxy(tmpX + 20, tmpY + 210, _T("Selected direction"));
}

void GUI::round_txt(int round) {
	settextcolor(BLACK);
	setbkmode(TRANSPARENT);
	settextstyle(25, 0, _T("Consolas"));

	string round_str = to_string(round);
	TCHAR buf[5];
	string2tchar(buf, round_str);
	outtextxy(550, 66, buf);
}

void GUI::dice_txt(bool side, int dice) {
	if (!side) settextcolor(RED);
	else settextcolor(BLUE);
	setbkmode(TRANSPARENT);
	settextstyle(35, 0, _T("Consolas"));
	outtextxy(530, 92, dice + '0');
}

void GUI::movabledice_txt(bool side, int chess1, int chess2) {
	if (!side) settextcolor(RED);
	else settextcolor(BLUE);
	setbkmode(TRANSPARENT);
	settextstyle(35, 0, _T("Consolas"));
	outtextxy(610, 122, chess1 + '1');
	if (chess1 != chess2) outtextxy(640, 122, chess2 + '1');
}

void GUI::prob_txt(bool side, int mov_chs, int direct, float prob, int chs_index) {
	int zeroX = 470, y = (mov_chs == 0) ? 220 : 330, space = 25 * (direct + 1);

	settextcolor(BLACK);
	setbkmode(TRANSPARENT);
	settextstyle(25, 0, _T("Consolas"));

	string str1 = "Chess:" + to_string(chs_index + 1);
	TCHAR buf1[8];
	string2tchar(buf1, str1);
	outtextxy(zeroX, y, buf1);

	string dir_str[3] = { "L", "U", "LU" };
	if (!side)
		dir_str[0] = "R", dir_str[1] = "D", dir_str[2] = "RD";
	string str2 = "DIR:" + dir_str[direct];
	TCHAR buf2[7];
	string2tchar(buf2, str2);
	outtextxy(zeroX, y + space, buf2);

	string str3 = "PR:" + to_string(prob);
	TCHAR buf3[12];
	string2tchar(buf3, str3);
	outtextxy(zeroX + 90, y + space, buf3);
}

void GUI::clock_txt(bool side, double clock_p) {
	settextcolor(BLACK);
	setbkmode(TRANSPARENT);
	settextstyle(30, 0, _T("Consolas"));

	string clock_str = to_string(clock_p);
	TCHAR buf[15];
	string2tchar(buf, clock_str);

	if (!side) outtextxy(750, 90, buf);
	else outtextxy(750, 140, buf);
}

void GUI::result_txt(string result) {
	putimage(0, 0, &background());

	settextcolor(BLACK);
	setbkmode(TRANSPARENT);
	settextstyle(40, 0, _T("Times New Roman"));

	TCHAR buf[30];
	string2tchar(buf, result);
	outtextxy(230, 170, buf);
}

void GUI::win_rate_txt(string win_rate) {
	settextcolor(BLACK);
	setbkmode(TRANSPARENT);
	settextstyle(40, 0, _T("Times New Roman"));

	TCHAR buf[55];
	string2tchar(buf, win_rate);
	outtextxy(120, 250, buf);
}