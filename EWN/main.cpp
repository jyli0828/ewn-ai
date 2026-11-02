#include "play.h"
#include "gui.h"

using namespace std;

int main() {
	ios_base::sync_with_stdio(0);
	cin.tie(nullptr);

	initgraph(WIDTH, HEIGHT, 1);
	GUI g;
	clock_t start = clock(), finish;

	int cmd = g.begin_gui();
	Play ewn;
	if (cmd == 0) ewn.two_players_mode();
	else if (cmd == 1) ewn.player_AI_mode();
	else ewn.two_AIs_mode();

	finish = clock();
	cout << "time cost : " << (finish - start) / (double)(CLOCKS_PER_SEC) << " seconds\n";
	system("pause");
	return 0;
}