#pragma once
#ifndef MODEL_H
#define MODEL_H

#include "game.h"

// Load ./model_ewn.bin if it is not loaded yet, or exit with a message if it is missing
void load_model();
// The policy model's probability of each direction (0~2) of the chess chs_index (0~5) of the side to move;
// an illegal direction gets 0
void model_probs(const Game& game, int chs_index, float probs[3]);

#endif
