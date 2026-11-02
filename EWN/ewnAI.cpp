#include "gui.h"
#include "ewnAI.h"
#include "model.h"

using namespace std;

EwnAI::EwnAI() {
    GUI g;
    this->ai_mode = g.select_AI();
    // a missing model stops the game now rather than at the first move
    if (this->ai_mode == 2) load_model();
}

Movement EwnAI::AI_move(Game& cur_game, int dice) {
    switch (this->ai_mode) {
    case 1:
        return this->randomAi.AI_move(cur_game, dice);
    case 2:
        return this->mcts.AI_move(cur_game, dice, 1);
    default:
        return this->mcts.AI_move(cur_game, dice, 0);
    }
}

int EwnAI::get_ai_mode() const {
    return this->ai_mode;
}