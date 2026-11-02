"""Turn the LittleGolem match records in game_logs/ into training_data.csv.

Each record in game_logs/<player>.txt is 7 lines: 5 header lines, one line with every game of
the match, and a blank line. Each game becomes one CSV line of blue setup, red setup, moves and
result (1-0 is a blue win, 0-1 a red win), e.g. [523416,342516],3/c5-d4,...,1-0. Only games decided on
the board are kept, where a piece reached the opponent's corner or one side lost all its pieces: the
result of a game that a resignation or a time out ended says little about its moves. Duplicates are
dropped too.
"""
import glob
import os
from collections import defaultdict

data_dir = os.path.dirname(os.path.abspath(__file__))
# One <player>.txt per player, as written by scrape_littlegolem.py
log_dir = os.path.join(data_dir, 'game_logs')
# Squares of the six setup digits of each side, as (row, column)
red_init_pos = [(0, 0), (0, 1), (0, 2), (1, 0), (1, 1), (2, 0)]
blue_init_pos = [(4, 4), (4, 3), (4, 2), (3, 4), (3, 3), (2, 4)]
cols = {k: v for v, k in enumerate('edcba')}


def parse_games(match):
    # The games of one match line, e.g. '1. [523416,342516] 3/c5-d4 4/e1xd2 2. ... 1-0'. Each game starts with
    # its setup, which may come between the two moves of a numbered pair. The line ends with the result of the
    # last game, maybe after 'resign', or with '*' while the match goes on. The earlier games have no result
    # here; resolve_result() finds it on the board.
    lines, game = [], []
    for token in match.split():
        # a new game: the previous one ended on the board
        if token.startswith('['):
            if len(game) > 1:
                lines.append(','.join(game) + '\n')
            game = [token]
        # a move such as 3/c5-d4
        elif len(token) == 7 and game:
            game.append(token)
        elif token in ('1-0', '0-1'):
            if len(game) > 1:
                lines.append(','.join(game) + ',' + token + '\n')
            game = []
        # not yet ended
        elif token == '*':
            game = []
    return lines


def parse_log(f):
    lines = []
    event, i = [], 0
    for line in f.readlines():
        event.append(line)
        i += 1
        # Every 7 lines (including a blank line) form one record
        if i == 7:
            lines += parse_games(event[5])
            event, i = [], 0
    return lines


def resolve_result(line):
    # The line with the result that the board gives, or None if the game was not decided on the board
    moves = line.strip().split(',')
    recorded = moves.pop() if moves[-1] in ('1-0', '0-1') else None
    red_num, blue_num = 6, 6
    # chess on each (row, column); 0 is empty
    board = defaultdict(int)
    for i in range(6):
        board[blue_init_pos[i]] = int(moves[0][i + 1]) + 6
        board[red_init_pos[i]] = int(moves[1][i])

    for move in moves[2:]:
        src, dst = move[2:4], move[5:7]
        src, dst = (int(src[1])-1, cols[src[0]]), (int(dst[1])-1, cols[dst[0]])

        if board[dst] != 0:
            if 1 <= board[dst] <= 6:
                red_num -= 1
            else:
                blue_num -= 1

        board[dst] = board[src]
        board[src] = 0

    # only red can move onto a5 and only blue onto e1, the corners they head for
    if moves[-1][5:7] == 'a5' or blue_num == 0:
        result = '0-1'
    elif moves[-1][5:7] == 'e1' or red_num == 0:
        result = '1-0'
    else:
        return None
    # a recorded result that the board contradicts would mean a broken record
    if recorded not in (None, result):
        return None
    return ','.join(moves) + ',' + result + '\n'


if __name__ == '__main__':
    lines = []
    for path in sorted(glob.glob(os.path.join(log_dir, '*.txt'))):
        with open(path, 'r', encoding='utf-8') as f:
            lines += parse_log(f)

    data = [line for line in map(resolve_result, lines) if line is not None]
    # A game between two listed players appears in both of their logs
    unique = list(dict.fromkeys(data))
    with open(os.path.join(data_dir, 'training_data.csv'), 'w', encoding='utf-8') as f:
        f.writelines(unique)

    # Every line has a result by now; the last move tells whether a piece reached the enemy corner
    corner = sum(line[-7:-5] in ('a5', 'e1') for line in unique)
    not_decided = len(dict.fromkeys(lines)) - len(unique)
    print(f'Total games = {len(unique)}, occupy enemy corner = {corner}, duplicates removed = {len(data) - len(unique)}, '
          f'not decided on the board = {not_decided}')
