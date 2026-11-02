#include "play.h"
#include "gui.h"
#include <iomanip>
#include <sstream>

// num rounded to n decimal places
string roundn(double num, int n) {
    ostringstream out;
    out << fixed << setprecision(n) << num;
    return out.str();
}

Movement Play::player_move(const int& dice) {
    int movable_chs_cnt = this->game.count_movable_chs(dice);
    const int* movable_chs_index_list = this->game.movable_chs_index_list;
    GUI g;

    cout << "\nNext chessman able to move: ";
    for (int i = 0; i < movable_chs_cnt; i++) {
        cout << "(" << i << ")";
        cout << movable_chs_index_list[i] + 1 << ' ';
    }
    g.movabledice_txt(this->game.side, movable_chs_index_list[0], movable_chs_index_list[1]);

    if (!this->game.side)
        cout << "\nDirection: 0)Right 1)Down 2)Right-down\n";
    else
        cout << "\nDirection: 0)Left 1)Up 2)Left-up\n";
    return g.select_move(this->game);
}

void Play::two_players_mode() {
    int dice, game_status;
    double clock_p1 = 0, clock_p2 = 0;
    clock_t start, finish;
    Movement mvmt;
    GUI g;

    this->game.print_board();

    while (1) {
        g.round_txt(1);
        dice = this->game.roll_dice();
        g.dice_txt(this->game.side, dice + 1);
        cout << "Dice: " << dice + 1 << '\n';
        g.clock_txt(0, clock_p1);
        g.clock_txt(1, clock_p2);
        
        start = clock();
        mvmt = this->player_move(dice);
        finish = clock();

        if (!this->game.side) clock_p1 += (finish - start) / (double)(CLOCKS_PER_SEC);
        else clock_p2 += (finish - start) / (double)(CLOCKS_PER_SEC);

        // update: 0(game continues), 1(red wins), 2(blue wins)
        game_status = this->game.update_game_status(mvmt);
        this->game.print_board();

        if (game_status != 0) {
            cout << "====================================\n";

            string result;
            if (game_status == 1) result = "Red is the winner!!!";
            else result = "Blue is the winner!!!";

            cout << result << "\n";
            cout << "====================================\n";

            g.result_txt(result);
            return;
        }
        this->game.switch_player();
    }
}

void Play::player_AI_mode() {
    int dice, game_status;
    double clock_p1 = 0, clock_p2 = 0;
    clock_t start, finish;
    Movement mvmt;
    EwnAI ewnAI;
    GUI g;

    bool player_is_red = g.select_side();
    this->game.print_board();

    while (1) {
        g.round_txt(1);
        dice = this->game.roll_dice();
        g.dice_txt(this->game.side, dice + 1);
        cout << "Dice: " << dice + 1 << '\n';
        g.clock_txt(0, clock_p1);
        g.clock_txt(1, clock_p2);

        bool player_turn = player_is_red && !this->game.side || !player_is_red && this->game.side;
        if (player_turn) {
            start = clock();
            mvmt = this->player_move(dice);
            finish = clock();
        }
        else {
            start = clock();
            mvmt = ewnAI.AI_move(this->game, dice);
            finish = clock();
        }

        if (!this->game.side) clock_p1 += (finish - start) / (double)(CLOCKS_PER_SEC);
        else clock_p2 += (finish - start) / (double)(CLOCKS_PER_SEC);

        // pause on the AI's move so the player can see it; the player's own move shows at once
        if (!player_turn) Sleep(500);
        // update: 0(game continues), 1(red wins), 2(blue wins)
        game_status = this->game.update_game_status(mvmt);
        this->game.print_board();

        if (game_status != 0) {
            cout << "====================================\n";

            string result;
            if (game_status == 1) {
                if (player_is_red) result = "Player(red) is the winner!!!";
                else result = "AI(red) is the winner!!!";
            }
            else {
                if (!player_is_red) result = "Player(blue) is the winner!!!";
                else result = "AI(blue) is the winner!!!";
            }

            cout << result << "\n";
            cout << "====================================\n";

            g.result_txt(result);
            return;
        }
        this->game.switch_player();
    }
}

void Play::two_AIs_mode() {
    int dice, game_status;
    double clock_p1 = 0, clock_p2 = 0;
    clock_t start, finish;
    Movement mvmt;
    EwnAI ewnAI1, ewnAI2;
    GUI g;

    // set round
    int round = 500, win_count = 0;

    for (int i = 1; i <= round; i++) {
        clock_p1 = 0, clock_p2 = 0;
        // the red side moves first in odd rounds and the blue side in even rounds
        if (i % 2 == 0) this->game.switch_player();
        cout << "Round " << i << ((i % 2) ? ", red" : ", blue") << " moves first\n";
        this->game.print_board();

        while (1) {
            g.round_txt(i);
            dice = this->game.roll_dice();
            g.dice_txt(this->game.side, dice + 1);
            cout << "Dice: " << dice + 1 << '\n';
            g.clock_txt(0, clock_p1);
            g.clock_txt(1, clock_p2);
            
            if (!this->game.side) {
                start = clock();
                mvmt = ewnAI1.AI_move(this->game, dice);
                finish = clock();
                clock_p1 += (finish - start) / (double)(CLOCKS_PER_SEC);
            }
            else {
                start = clock();
                mvmt = ewnAI2.AI_move(this->game, dice);
                finish = clock();
                clock_p2 += (finish - start) / (double)(CLOCKS_PER_SEC);
            }

            // update: 0(game continues), 1(red wins), 2(blue wins)
            game_status = this->game.update_game_status(mvmt);
            this->game.print_board();

            if (game_status != 0) {
                cout << "====================================\n";

                string result;
                if (game_status == 1) {
                    result = player_names[ewnAI1.get_ai_mode() + 1] + "(red) is the winner!!!";
                    win_count++;
                }
                else result = player_names[ewnAI2.get_ai_mode() + 1] + "(blue) is the winner!!!";

                cout << result << "\n";
                cout << "====================================\n";

                string win_rate = "The win rate of the red side: ";
                win_rate += to_string(win_count) + " / " + to_string(i) + " = ";
                win_rate += roundn((double)win_count / i * 100, 2) + "%";
                cout << win_rate << '\n';

                if (i == round) {
                    g.result_txt(result);
                    g.win_rate_txt(win_rate);
                }
                break;
            }
            this->game.switch_player();
        }
        // reset game
        this->game = Game();
    }
}