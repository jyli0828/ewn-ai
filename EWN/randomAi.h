#pragma once
#ifndef RANDOMAI_H
#define RANDOMAI_H

#include "game.h"

class RandomAi {
public:
	Movement AI_move(Game& cur_game, int dice);
};

#endif