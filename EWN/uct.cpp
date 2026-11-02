#include "uct.h"

UCT::UCT(float eps_) : Selection(eps_) {
	this->exploration_param = sqrt(2.0f);
	this->t_param = 1;
}

int UCT::select_children_list_index(Tree_node* cur_node, int dice, bool ai_side, int select) {
	float child_value_list[18] = { 0 }, best_value = -1e9;
	bool movable_list[18] = { false };
	// the opponent of the AI picks the moves that the AI wins least
	bool ai_turn = (cur_node->game.side == ai_side);
	cur_node->game.count_movable_chs(dice);
	if (select) cur_node->set_children_probs();
	// calculate every UCT value of the child nodes to find the max
	for (int i = 0; i < 18; i++) {
		if (cur_node->get_child_ptr(i) == NULL) break;

		Tree_node* node_ptr = cur_node->get_child_ptr(i);
		// only the chess the dice allows can move
		BitBoard chs = cur_node->game.cur_chs_ptr[node_ptr->chess_number];
		movable_list[i] = (chs == cur_node->game.movable_chs_list[0] || chs == cur_node->game.movable_chs_list[1]);
		if (!movable_list[i]) continue;

		float win_rate = (float)node_ptr->get_win_count() / (float)node_ptr->get_visit_count();
		float exploitation = ai_turn ? win_rate : 1 - win_rate;
		float exploration = this->exploration_param * sqrt(log((float)cur_node->get_visit_count() + this->t_param) / (float)node_ptr->get_visit_count());
		child_value_list[i] = exploitation + exploration;
		// for model: the policy model's probability of the move, which fades as the move gets visits
		if (select) child_value_list[i] += 10 * node_ptr->get_prob() / (float)node_ptr->get_visit_count();

		if (child_value_list[i] > best_value)
			best_value = child_value_list[i];
	}

	// gather the nodes whose UCT value approaches best_value
	int best_children_num = 0, child_index_list[18];
	for (int i = 0; i < 18; i++) {
		if (cur_node->get_child_ptr(i) == NULL) break;
		if (movable_list[i] && best_value - child_value_list[i] < this->eps)
			child_index_list[best_children_num++] = i;
	}
	// select one of best child node randomly and return its index
	return child_index_list[random_int(best_children_num)];
}
