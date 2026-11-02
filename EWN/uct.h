#pragma once
#ifndef UCT_H
#define UCT_H

#include "tree_node.h"

class Selection {
public:
	float eps;

	Selection(float eps_) { this->eps = eps_; }
	virtual ~Selection() {}

	// Pick a child among the moves the dice allows, for the side to move in cur_node
	// (the win counts of the nodes are those of the AI on ai_side); with select, the policy model's
	// probabilities also guide the choice
	virtual int select_children_list_index(Tree_node* cur_node, int dice, bool ai_side, int select) = 0;
};

class UCT : public Selection {
public:
	float exploration_param;
	float t_param;

	UCT(float eps_);

	int select_children_list_index(Tree_node* cur_node, int dice, bool ai_side, int select);
};

#endif
