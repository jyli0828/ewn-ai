#pragma once
#ifndef GAME_H
#define GAME_H
#define NOMINMAX

#include <iostream>
#include <easyX.h>
#include <ctime>
#include <string>
#include <vector>
#include <random>

using namespace std;

// < the index of the currentPlayer (chs), direction >
typedef pair<int, int> Movement;
typedef int BitBoard;

// a random integer in [0, n); each thread has its own generator, so the AI's search threads can use it
int random_int(int n);

class ChessPosition {
public:
	BitBoard chessRed[6];
    BitBoard chessBlue[6];
	BitBoard all;
	// fullMask[i * 5 + j] is the bit of square (i, j)
	static constexpr BitBoard fullMask[25] = {
		1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4,
		1 << 5, 1 << 6, 1 << 7, 1 << 8, 1 << 9,
		1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14,
		1 << 15, 1 << 16, 1 << 17, 1 << 18, 1 << 19,
		1 << 20, 1 << 21, 1 << 22, 1 << 23, 1 << 24
	};

    ChessPosition();
};

class Game {
public:
    bool side; // 0 is red, 1 is blue
    ChessPosition chessPosition;
    BitBoard movable_chs_list[2];
    BitBoard* cur_chs_ptr;

    Game();
    Game(const Game& game);
    void operator=(const Game& game);

    int roll_dice();
    void switch_player();
    int count_movable_chs(const int& dice);
    // return the index (0~5) of the chess on chs, or -1 if no chess is there
    int get_chess_index(BitBoard chs) const;
    // return 0(game continues), 1(red wins), 2(blue wins)
    int update_game_status(const Movement& mvmt);
    bool check_in_board(const Movement& mvmt) const;
    bool is_winning_move(const Movement& mvmt) const;

    // for model: the chess on square (i, j), red 1~6 and blue 7~12; 0 if empty
    int get_board_P(int i, int j) const;

    void print_board();
};

#endif