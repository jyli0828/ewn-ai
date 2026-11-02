#pragma once
#ifndef TREE_NODE_H
#define TREE_NODE_H

#include "game.h"

class Tree_node {
public:
	int win_count;
	int visit_count;
	// for model: the policy model's probability of the move to this node
	float prob;
	// for model: the chess (bit 0~5) whose children already have their probabilities
	int model_chs_mask;

	int chess_number;
	int direct;

	int game_status;
	Game game;
	Tree_node* parent_ptr;
	// List for legal child nodes
	Tree_node* children_ptr_list[18];

	// A node of game_ without children
	Tree_node(const Game& game_);

	// If the node is a end-gamed node
	bool is_terminate();
	// If the node has been visited (it is expanded)
	bool is_visit();

	// New legal child nodes and reset the values of the members(except children_ptr_list)
	int new_child_nodes();
	// for model: give the children of the movable chess (game.movable_chs_index_list) the policy model's probabilities,
	// the first time the dice lets them move; most nodes are never selected from, so the model skips them
	void set_children_probs();
	// Add the results of visit_count_ simulations
	void update(int win_count_, int visit_count_);

	int get_win_count() const;
	int get_visit_count() const;
	float get_prob() const;
	int get_game_status() const;
	Tree_node* get_parent_ptr() const;
	// Get child node pointer from children_ptr_list
	Tree_node* get_child_ptr(const int& child_index) const;
};

#endif
