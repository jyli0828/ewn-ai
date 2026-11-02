#pragma once
#ifndef MCTS_H
#define MCTS_H

#include "uct.h"

const float eps = 1e-6f;

class MCTS {
public:
    bool ai_side;
    int max_iterations;
    Selection* selection_ptr;

    MCTS();
    ~MCTS();

    // with select, the policy model guides the search (the Model AI)
    Movement AI_move(Game& cur_game, int dice, int select);
    // Run 4 stages of MCTS for max_iterations times; AI_move() calls it from several threads at once,
    // so it must not change the members
    float run(const Game& cur_game, int select);
    // Play one random game from cur_game and return 1 if the AI wins; with select, a move into the
    // opponent's corner is always taken
    int simulation(const Game& cur_game, int select);
    // Free a search tree
    void recursive_delete_tree_node(Tree_node* node_ptr);
};
// src: https://github.com/memo/ofxMSAmcts/blob/master/src/ofxMSAmcts.h

#endif
