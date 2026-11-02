#include "tree_node.h"
#include "model.h"

Tree_node::Tree_node() {
	this->win_count = 0;
	this->visit_count = 0;
	this->prob = 0;
	this->model_chs_mask = 0;
	this->game_status = 0;
	this->parent_ptr = NULL;
	for (int i = 0; i < 18; i++)
		this->children_ptr_list[i] = NULL;
}

Tree_node::Tree_node(const Game& game_) {
	this->win_count = 0;
	this->visit_count = 0;
	this->prob = 0;
	this->model_chs_mask = 0;
	this->game_status = 0;
	this->game = game_;
	this->parent_ptr = NULL;
	for (int i = 0; i < 18; i++)
		this->children_ptr_list[i] = NULL;
}

bool Tree_node::is_terminate() {
	return this->game_status != 0;
}

bool Tree_node::is_visit() {
	return this->visit_count != 0;
}

int Tree_node::new_child_nodes() {
	int legal_child_num = 0;
	// new the legal nodes for children_ptr
	for (int i = 0; i < 18; i++) {
		// check if the movement is legal
		int chs_index = i / 3, direction = i % 3;
		if (this->game.cur_chs_ptr[chs_index] == 0) continue;
		Movement movement(chs_index, direction);
		if (!this->game.check_in_board(movement)) continue;

		// new legal child nodes
		this->children_ptr_list[legal_child_num] = new Tree_node();
		this->children_ptr_list[legal_child_num]->chess_number = chs_index;
		this->children_ptr_list[legal_child_num]->direct = direction;
		Tree_node* node_ptr = this->children_ptr_list[legal_child_num];
		legal_child_num++;

		// reset the values of the members
		node_ptr->game = this->game;
		node_ptr->game_status = node_ptr->game.update_game_status(movement);
		node_ptr->game.switch_player();
		node_ptr->parent_ptr = this;
	}
	// reset the rest of the children_ptr to NULL
	for (int i = legal_child_num; i < 18; i++)
		this->children_ptr_list[i] = NULL;
	return legal_child_num;
}

void Tree_node::set_children_probs() {
	for (int m = 0; m < 2; m++) {
		int chs_index = this->game.get_chess_index(this->game.movable_chs_list[m]);
		if (this->model_chs_mask & (1 << chs_index)) continue;
		this->model_chs_mask |= 1 << chs_index;

		float probs[3];
		model_probs(this->game, chs_index, probs);
		for (int i = 0; i < 18 && this->children_ptr_list[i] != NULL; i++)
			if (this->children_ptr_list[i]->chess_number == chs_index)
				this->children_ptr_list[i]->prob = probs[this->children_ptr_list[i]->direct];
	}
}

void Tree_node::update(int win_count_, int visit_count_) {
	this->visit_count += visit_count_;
	this->win_count += win_count_;
}

int Tree_node::get_win_count() const {
	return this->win_count;
}

int Tree_node::get_visit_count() const {
	return this->visit_count;
}

float Tree_node::get_prob() const {
	return this->prob;
}

int Tree_node::get_game_status() const {
	return this->game_status;
}

Tree_node* Tree_node::get_parent_ptr() const {
	return this->parent_ptr;
}

Tree_node* Tree_node::get_child_ptr(const int& child_index) const {
	return this->children_ptr_list[child_index];
}
