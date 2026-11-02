#pragma once
#ifndef GUI_H
#define GUI_H

#include "game.h"

#define WIDTH 900
#define HEIGHT 470

// The players of playerA and playerB: 0 player, 1 MCTS, 2 Random, 3 Model (EwnAI::get_ai_mode() + 1)
extern const string player_names[4];

class GUI {
private:
	// Draw the options of a menu (option k is text_list[k] at (x, y_list[k])) and return the one clicked;
	// the option under the mouse turns yellow
	int choose_option(int x, const int y_list[], const LPCTSTR text_list[], int n);
	// The white squares of the board and the line beside the information panel
	void draw_squares();
	void draw_pieces(const ChessPosition& chp);
	// The board while the player chooses a move: the movable chess, the chosen chess (square chosen, -1 if
	// none) and what a click on square hovered would pick: a movable chess, or with hovered_move the square
	// the chosen chess would move to
	void draw_choice(const Game& game, int chosen, int hovered, bool hovered_move);

public:
	void init_environment();
	int begin_gui();

	int select_AI();
	bool select_side();
	// The player's move with a chess in game.movable_chs_index_list, which count_movable_chs() sets: a click on a
	// movable chess chooses it (a second click takes it back), and a click on a square it can move to moves it
	Movement select_move(const Game& game);

	void draw_chess(int x, int y, char chess, int turn);
	void draw_movable_cnt_color(const Game& game);
	void draw_selected_chess(int chess);
	void draw_selected_direction(bool side, int direct);

	void rstboard(const ChessPosition& chp);

	void print_info();
	void round_txt(int round);
	void dice_txt(bool side, int dice);
	void movabledice_txt(bool side, int chess1, int chess2);
	void prob_txt(bool side, int mov_chs, int direct, float prob, int chs_index);
	void clock_txt(bool side, double clock_p);
	void result_txt(string result);
	void win_rate_txt(string win_rate);
};

#endif 