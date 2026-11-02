#include "mcts.h"
#include "gui.h"
#include "model.h"
#include <thread>

MCTS::MCTS() {
	this->selection_ptr = new UCT(eps);
}

MCTS::~MCTS() {
	delete this->selection_ptr;
}

Movement MCTS::AI_move(Game& cur_game, int dice, int select) {
	this->max_iterations = 100;
	this->ai_side = cur_game.side;

	float best_val = -1e9;
	int win = (!this->ai_side) ? 1 : 2, movable_chs_cnt = cur_game.count_movable_chs(dice), game_status;
	int movable_chs_index_list[2] = { cur_game.get_chess_index(cur_game.movable_chs_list[0]), cur_game.get_chess_index(cur_game.movable_chs_list[1]) };
	bool end = false;
	Movement answer;
	GUI g;

	g.draw_movable_cnt_color(cur_game);
	g.movabledice_txt(cur_game.side, movable_chs_index_list[0], movable_chs_index_list[1]);
	cout << "movable chess: " << movable_chs_index_list[0] + 1;
	if (movable_chs_index_list[0] != movable_chs_index_list[1]) cout << ", " << movable_chs_index_list[1] + 1;
	cout << '\n';

	// the moves that keep the game going, and which of the movable chess (0 or 1) makes each
	vector<Movement> mvmt_list;
	vector<int> mov_chs_list;
	vector<Game> child_game_list;
	for (int i = 0; i < movable_chs_cnt; i++) {
		for (int direct = 0; direct < 3; direct++) {
			Movement mvmt(movable_chs_index_list[i], direct);
			if (cur_game.check_in_board(mvmt)) {
				Game child_game = cur_game;
				game_status = child_game.update_game_status(mvmt);
				child_game.switch_player();

				// check if the game ends; a move never makes the mover lose
				if (game_status == win) {
					answer = mvmt;
					end = true;
					break;
				}
				mvmt_list.push_back(mvmt);
				mov_chs_list.push_back(i);
				child_game_list.push_back(child_game);
			}
		}
		if (end) break;
	}

	if (!end) {
		// search every move in its own thread; each run() has its own tree
		if (select) load_model();
		vector<float> child_val_list(mvmt_list.size());
		vector<thread> thread_list;
		for (int k = 0; k < (int)mvmt_list.size(); k++)
			thread_list.emplace_back([this, &child_val_list, &child_game_list, k, select] {
				child_val_list[k] = this->run(child_game_list[k], select);
			});
		for (thread& t : thread_list) t.join();

		for (int k = 0; k < (int)mvmt_list.size(); k++) {
			g.prob_txt(cur_game.side, mov_chs_list[k], mvmt_list[k].second, child_val_list[k], mvmt_list[k].first);
			// update the best value and the best movement
			if (child_val_list[k] >= best_val) {
				best_val = child_val_list[k];
				answer = mvmt_list[k];
			}
		}
	}
	g.draw_selected_chess(answer.first + 1);
	g.draw_selected_direction(cur_game.side, answer.second);
	cout << "move: (" << answer.first + 1 << ", " << answer.second << ")\n";
	return answer;
}

// mcts main
float MCTS::run(const Game& cur_game, int select) {
	// initialize root Tree_node with current state; the first iteration expands it
	Tree_node* root_node_ptr = new Tree_node(cur_game);
	int iteration = 0, ai_win = (!this->ai_side) ? 1 : 2;

	while (iteration++ < this->max_iterations) {
		// 1. select
		// Start from the root, digging down until finding an unvisited node
		Tree_node* node_ptr = root_node_ptr;
		// The root has been visited
		if (root_node_ptr->is_visit()) {
			while (!node_ptr->is_terminate()) {
				// the tree has the moves of every chess, so roll the dice to pick the ones allowed this time
				int dice = node_ptr->game.roll_dice();
				int best_child_index = this->selection_ptr->select_children_list_index(node_ptr, dice, this->ai_side, select);
				node_ptr = node_ptr->get_child_ptr(best_child_index);
				// The next child pointer hasn't been visited
				if (node_ptr->get_child_ptr(0) == NULL) break;
			}
		}

		if (node_ptr->is_terminate()) {
			int win_reward = node_ptr->get_game_status() == ai_win;

			// 4. back propagation
			for (; node_ptr != NULL; node_ptr = node_ptr->get_parent_ptr())
				node_ptr->update(win_reward, 1);
		}
		else {
			// 2. expand every legal move of the node
			int legal_child_number = node_ptr->new_child_nodes();

			// 3. simulate 100 games from every child
			for (int k = 0; k < legal_child_number; k++) {
				int win = 0, visit = 100;
				Tree_node* child_ptr = node_ptr->get_child_ptr(k);
				// a child that ends the game needs no simulation
				if (child_ptr->is_terminate())
					win = (child_ptr->get_game_status() == ai_win) ? visit : 0;
				else {
					for (int l = 0; l < visit; l++)
						win += this->simulation(child_ptr->game, select);
				}

				// 4. back propagation
				for (; child_ptr != NULL; child_ptr = child_ptr->get_parent_ptr())
					child_ptr->update(win, visit);
			}
		}
	}

	float result = (float)(root_node_ptr->get_win_count()) / root_node_ptr->get_visit_count();
	this->recursive_delete_tree_node(root_node_ptr);
	return result;
}

int MCTS::simulation(const Game& cur_game, int select) {
	Game simu_game = cur_game;
	int game_status = 0, ai_win = (!this->ai_side) ? 1 : 2;

	while (game_status == 0) {
		Movement available_mvmt_list[6];
		int available_mvmt_cnt = 0;
		// roll the dice and find all available moves (2 chess at most, 3 directions)
		int movable_chs_cnt = simu_game.count_movable_chs(simu_game.roll_dice());
		for (int i = 0; i < movable_chs_cnt; i++) {
			int chs_index = simu_game.get_chess_index(simu_game.movable_chs_list[i]);
			for (int direct = 0; direct < 3; direct++) {
				Movement tmp_mvmt(chs_index, direct);
				if (!simu_game.check_in_board(tmp_mvmt)) continue;
				// for model: always take a move into the opponent's corner
				if (select && simu_game.is_winning_move(tmp_mvmt)) {
					game_status = simu_game.update_game_status(tmp_mvmt);
					return (game_status == ai_win ? 1 : 0);
				}
				available_mvmt_list[available_mvmt_cnt++] = tmp_mvmt;
			}
		}
		// randomly pick a move; a side that still has chess always has one
		Movement next_mvmt = available_mvmt_list[random_int(available_mvmt_cnt)];
		game_status = simu_game.update_game_status(next_mvmt);
		if (game_status == 0) simu_game.switch_player();
	}
	return (game_status == ai_win ? 1 : 0);
}

void MCTS::recursive_delete_tree_node(Tree_node* node_ptr) {
	for (int i = 0; i < 18; i++)
		if (node_ptr->get_child_ptr(i))
			recursive_delete_tree_node(node_ptr->get_child_ptr(i));
	delete node_ptr;
}