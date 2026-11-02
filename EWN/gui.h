#pragma once
#ifndef GUI_H
#define GUI_H

#include "game.h"

#define WIDTH 900
#define HEIGHT 470

class GUI {
private:
	int states;

	// Draw the options of a menu (option k is text_list[k] at (x, y_list[k]), width_list[k] x 50) and return
	// the one clicked twice in a row; the option clicked last turns yellow
	int choose_option(int x, const int y_list[], const int width_list[], const LPCTSTR text_list[], int n);
	// The white squares of the board and the line beside the information panel
	void draw_squares();
	void draw_pieces(const ChessPosition& chp);

public:
	GUI();

	void init_environment();
	int begin_gui();

	int select_AI();
	bool select_side();
	// the chess in game.movable_chs_list, which count_movable_chs() sets
	int select_chs_index(const Game& game);
	int select_chs_direction(const Game& game, int chs_index);

	void draw_chess(int x, int y, char chess, int turn);
	void draw_movable_cnt_color(const Game& game);
	void draw_selected_chess(int chess);
	void draw_selected_direction(bool side, int direct);

	void rstboard(const ChessPosition& chp);
	void rstboard_1(const Game& game);

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