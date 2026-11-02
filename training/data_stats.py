"""Print the statistics of training_data.csv listed in the README's Training data section.

Run it after parse_game_logs.py. It reads training_data.csv and the matches cached in game_logs/matches/.
"""
import glob
import os
from collections import Counter

from parse_game_logs import blue_init_pos, cols, data_dir, log_dir, red_init_pos
from scrape_littlegolem import is_played


def game_summary(line):
    # (moves, captures, whether red moved first, whether red won, how the game was won)
    fields = line.strip().split(',')
    board = {}
    for i in range(6):
        board[red_init_pos[i]] = int(fields[1][i])
        board[blue_init_pos[i]] = int(fields[0][i + 1]) + 6
    moves = fields[2:-1]
    red_won = fields[-1] == '0-1'
    # dst stays None for a game without moves
    captures, red_first, dst = 0, None, None
    for move in moves:
        src = (int(move[3]) - 1, cols[move[2]])
        dst = (int(move[6]) - 1, cols[move[5]])
        piece = board.pop(src)
        if red_first is None:
            red_first = piece <= 6
        captures += dst in board
        board[dst] = piece

    # The winner's last move reached the enemy corner, the loser has no pieces left, or neither
    if dst == ((4, 4) if red_won else (0, 0)) and (board[dst] <= 6) == red_won:
        ending = 'reached the corner'
    elif all((piece <= 6) == red_won for piece in board.values()):
        ending = 'captured all pieces'
    else:
        ending = 'resigned or ran out of time'
    return len(moves), captures, red_first, red_won, ending


def is_played_file(match_file):
    with open(match_file, encoding='utf-8') as f:
        return is_played(f.read())


if __name__ == '__main__':
    with open(os.path.join(data_dir, 'training_data.csv'), encoding='utf-8') as f:
        games = [game_summary(line) for line in f]
    n = len(games)
    lengths = [moves for moves, _, _, _, _ in games]
    moves = sum(lengths)
    captures = sum(captured for _, captured, _, _, _ in games)
    red_first = sum(first for _, _, first, _, _ in games)
    red_won = sum(won for _, _, _, won, _ in games)
    first_won = sum(first == won for _, _, first, won, _ in games)
    endings = Counter(ending for _, _, _, _, ending in games)
    matches = [is_played_file(path) for path in glob.glob(os.path.join(log_dir, 'matches', '*.txt'))]

    print(f'Matches: {sum(matches):,} with moves, {len(matches) - sum(matches):,} without any moves')
    print(f'Games: {n:,}')
    print(f'Moves: {moves:,}, {moves / n:.1f} per game on average, {min(lengths)} to {max(lengths)}')
    print('How the game was won:')
    for ending in ('reached the corner', 'captured all pieces', 'resigned or ran out of time'):
        print(f'  {ending}: {endings[ending]:,} ({endings[ending] / n:.1%})')
    print(f'Games won: blue {n - red_won:,} ({(n - red_won) / n:.1%}), red {red_won:,} ({red_won / n:.1%})')
    print(f'Games started: blue {n - red_first:,} ({(n - red_first) / n:.1%}), red {red_first:,} ({red_first / n:.1%})')
    print(f'The side that moved first won {first_won / n:.1%} of the games; {captures / moves:.1%} of all moves capture a piece')
