#include "game.h"
#include "gui.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

ChessPosition::ChessPosition() {
	memset(this->chessRed, 0, sizeof(this->chessRed));
	memset(this->chessBlue, 0, sizeof(this->chessBlue));
	this->all = 0b1110011000100010001100111;
}

// mt19937 is a random generator based on Mersenne twister algorithm; each thread seeds its own once
static mt19937& random_generator() {
	thread_local mt19937 gen(random_device{}());
	return gen;
}

int random_int(int n) {
	uniform_int_distribution uni_dis(0, n - 1);
	return uni_dis(random_generator());
}

Game::Game() {
	this->side = 0;

	int chs_red[6] = { 0, 1, 2, 3, 4, 5 };
	int chs_blue[6] = { 0, 1, 2, 3, 4, 5 };
	mt19937& gen = random_generator();

	shuffle(chs_red, chs_red + 6, gen);
	this->chessPosition.chessRed[chs_red[0]] = this->chessPosition.fullMask[0];
	this->chessPosition.chessRed[chs_red[1]] = this->chessPosition.fullMask[1];
	this->chessPosition.chessRed[chs_red[2]] = this->chessPosition.fullMask[2];
	this->chessPosition.chessRed[chs_red[3]] = this->chessPosition.fullMask[5];
	this->chessPosition.chessRed[chs_red[4]] = this->chessPosition.fullMask[6];
	this->chessPosition.chessRed[chs_red[5]] = this->chessPosition.fullMask[10];

	shuffle(chs_blue, chs_blue + 6, gen);
	this->chessPosition.chessBlue[chs_blue[0]] = this->chessPosition.fullMask[24];
	this->chessPosition.chessBlue[chs_blue[1]] = this->chessPosition.fullMask[23];
	this->chessPosition.chessBlue[chs_blue[2]] = this->chessPosition.fullMask[22];
	this->chessPosition.chessBlue[chs_blue[3]] = this->chessPosition.fullMask[19];
	this->chessPosition.chessBlue[chs_blue[4]] = this->chessPosition.fullMask[18];
	this->chessPosition.chessBlue[chs_blue[5]] = this->chessPosition.fullMask[14];

	for (int i = 0; i < 2; i++)
		this->movable_chs_list[i] = 0;
	this->cur_chs_ptr = this->chessPosition.chessRed;
}

Game::Game(const Game& game) {
	this->side = game.side;
	this->chessPosition = game.chessPosition;
	for (int i = 0; i < 2; i++)
		this->movable_chs_list[i] = game.movable_chs_list[i];
	this->cur_chs_ptr = (!this->side) ? this->chessPosition.chessRed : this->chessPosition.chessBlue;
}

void Game::operator=(const Game& game) {
	this->side = game.side;
	this->chessPosition = game.chessPosition;
	for (int i = 0; i < 2; i++)
		this->movable_chs_list[i] = game.movable_chs_list[i];
	this->cur_chs_ptr = (!this->side) ? this->chessPosition.chessRed : this->chessPosition.chessBlue;
}

int Game::roll_dice() {
	return random_int(6);
}

void Game::switch_player() {
	this->cur_chs_ptr = (!this->side) ? this->chessPosition.chessBlue : this->chessPosition.chessRed;
	this->side = !this->side;
}

int Game::count_movable_chs(const int& dice) {
	for (int i = 0; i < 2; i++)
		this->movable_chs_list[i] = 0;
	if (this->cur_chs_ptr[dice] != 0) {
		for (int i = 0; i < 2; i++)
			this->movable_chs_list[i] = this->cur_chs_ptr[dice];
		return 1;
	}
	else {
		int movable_chs_cnt = 2;
		for (int i = dice - 1; i >= 0; i--) {
			if (this->cur_chs_ptr[i] != 0) {
				this->movable_chs_list[0] = this->cur_chs_ptr[i];
				break;
			}
		}
		for (int i = dice + 1; i < 6; i++) {
			if (this->cur_chs_ptr[i] != 0) {
				this->movable_chs_list[1] = this->cur_chs_ptr[i];
				break;
			}
		}
		for (int i = 0; i < 2; i++) {
			if (this->movable_chs_list[i] == 0) {
				this->movable_chs_list[i] = this->movable_chs_list[1 - i];
				movable_chs_cnt--;
			}
		}
		return movable_chs_cnt;
	}
}

int Game::get_chess_index(BitBoard chs) const {
	for (int i = 0; i < 6; i++)
		if (this->chessPosition.chessRed[i] == chs || this->chessPosition.chessBlue[i] == chs)
			return i;
	return -1;
}

int Game::update_game_status(const Movement& mvmt) {
	BitBoard chs = this->cur_chs_ptr[mvmt.first], movemask = 0;
	int direct = mvmt.second;

	if (!this->side) {
		if (direct == 0) movemask = chs << 1;
		else if (direct == 1) movemask = chs << 5;
		else if (direct == 2) movemask = chs << 6;
	}
	else {
		if (direct == 0) movemask = chs >> 1;
		else if (direct == 1) movemask = chs >> 5;
		else if (direct == 2) movemask = chs >> 6;		
	}

	// capture the chess on the destination, which may be one of the mover's own
	if (this->chessPosition.all & movemask) {
		for (int i = 0; i < 6; i++) {
			if (this->chessPosition.chessRed[i] == movemask) this->chessPosition.chessRed[i] = 0;
			if (this->chessPosition.chessBlue[i] == movemask) this->chessPosition.chessBlue[i] = 0;
		}
	}
	this->chessPosition.all ^= chs;
	this->chessPosition.all |= movemask;
	this->cur_chs_ptr[mvmt.first] = movemask;

	// return 0(game continues), 1(red wins), 2(blue wins)
	// The game went on before this move, so only the moved chess can arrive at the end-game position,
	// and only the other player can lose all its chess
	int win = (!this->side) ? 1 : 2;
	if (movemask == this->chessPosition.fullMask[(!this->side) ? 24 : 0]) return win;
	const BitBoard* opponent = (!this->side) ? this->chessPosition.chessBlue : this->chessPosition.chessRed;
	for (int i = 0; i < 6; i++)
		if (opponent[i] != 0) return 0;
	return win;
}

bool Game::check_in_board(const Movement& mvmt) const {
	BitBoard chs = this->cur_chs_ptr[mvmt.first];
	int direct = mvmt.second;

	if (!this->side) {
		if (direct == 0) return ((chs & 0b1000010000100001000010000) == 0);
		else if (direct == 1) return ((chs & 0b1111100000000000000000000) == 0);
		else if (direct == 2) return ((chs & 0b1111110000100001000010000) == 0);
	}
	else {
		if (direct == 0) return ((chs & 0b0000100001000010000100001) == 0);
		else if (direct == 1) return ((chs & 0b0000000000000000000011111) == 0);
		else if (direct == 2) return ((chs & 0b0000100001000010000111111) == 0);
	}
	return false;
}

bool Game::is_winning_move(const Movement& mvmt) const {
	BitBoard chs = this->cur_chs_ptr[mvmt.first], newplace = 0;
	int direct = mvmt.second;

	if (!this->side) {
		if (direct == 0) newplace = chs << 1;
		else if (direct == 1) newplace = chs << 5;
		else if (direct == 2) newplace = chs << 6;
	}
	else {
		if (direct == 0) newplace = chs >> 1;
		else if (direct == 1) newplace = chs >> 5;
		else if (direct == 2) newplace = chs >> 6;
	}
	if (!this->side) return (newplace & this->chessPosition.fullMask[24]);
	else return (newplace & this->chessPosition.fullMask[0]);
}

int Game::get_board_P(int i, int j) const {
	BitBoard tmp = this->chessPosition.fullMask[i * 5 + j];
	if (this->chessPosition.all & tmp) {
		for (int k = 0; k < 6; k++) {
			if (this->chessPosition.chessRed[k] & tmp) return k + 1;
			if (this->chessPosition.chessBlue[k] & tmp) return k + 7;
		}
	}
	return 0;
}

void Game::print_board() {
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n";
	cout << "The current board:\n";

	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++) {
			BitBoard pos = this->chessPosition.fullMask[i * 5 + j];
			if (this->chessPosition.all & pos) {
				for (int k = 0; k < 6; k++) {
					if (this->chessPosition.chessRed[k] & pos) {
						cout << ANSI_COLOR_RED << k + 1 << ' ' << ANSI_COLOR_RESET;
						break;
					}
					else if (this->chessPosition.chessBlue[k] & pos) {
						cout << ANSI_COLOR_BLUE << k + 1 << ' ' << ANSI_COLOR_RESET;
						break;
					}
				}
			}
			else cout << 0 << ' ';
		}
		cout << '\n';
	}
	cout << "\n====================================" << endl;
	GUI g;
	g.rstboard(this->chessPosition);
}
