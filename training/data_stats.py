"""Print the statistics of training_data.csv listed in the README's Training data section.

Run it after parse_game_logs.py. It reads training_data.csv and the matches cached in game_logs/matches/.
"""
import glob
import os
from collections import Counter

# The directions of a move, as the notebook's labels and direct in Game::update_game_status()
DIRECTIONS = ('horizontal', 'vertical', 'diagonal')

from parse_game_logs import blue_init_pos, cols, data_dir, log_dir, parse_games, red_init_pos, resolve_result


def game_summary(line):
    # (moves, captures, whether red moved first, whether red won, how the game was won, moves per direction,
    # moves with a single legal direction)
    fields = line.strip().split(',')
    board = {}
    for i in range(6):
        board[red_init_pos[i]] = int(fields[1][i])
        board[blue_init_pos[i]] = int(fields[0][i + 1]) + 6
    moves = fields[2:-1]
    red_won = fields[-1] == '0-1'
    # dst stays None for a game without moves
    captures, red_first, dst = 0, None, None
    directions, single = [0, 0, 0], 0
    for move in moves:
        src = (int(move[3]) - 1, cols[move[2]])
        dst = (int(move[6]) - 1, cols[move[5]])
        directions[0 if src[0] == dst[0] else 1 if src[1] == dst[1] else 2] += 1
        piece = board.pop(src)
        # on the far row or column, a piece can only move along that edge
        single += (4 in src) if piece <= 6 else (0 in src)
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
    return len(moves), captures, red_first, red_won, ending, directions, single


def has_games(match_file):
    # Whether parse_game_logs.py keeps any game of a cached match: 5 header lines and the line of moves
    with open(match_file, encoding='utf-8') as f:
        lines = f.read().splitlines()
    return len(lines) == 6 and any(resolve_result(game) for game in parse_games(lines[5]))


if __name__ == '__main__':
    with open(os.path.join(data_dir, 'training_data.csv'), encoding='utf-8') as f:
        games = [game_summary(line) for line in f]
    n = len(games)
    lengths = [game[0] for game in games]
    moves = sum(lengths)
    captures = sum(game[1] for game in games)
    red_first = sum(game[2] for game in games)
    red_won = sum(game[3] for game in games)
    first_won = sum(game[2] == game[3] for game in games)
    endings = Counter(game[4] for game in games)
    directions = [sum(game[5][d] for game in games) for d in range(3)]
    single = sum(game[6] for game in games)
    matches = [has_games(path) for path in glob.glob(os.path.join(log_dir, 'matches', '*.txt'))]

    print(f'Matches: {sum(matches):,} with games, {len(matches) - sum(matches):,} without any')
    print(f'Games: {n:,}')
    print(f'Moves: {moves:,}, {moves / n:.1f} per game on average, {min(lengths)} to {max(lengths)}')
    print('How the game was won:')
    for ending in ('reached the corner', 'captured all pieces', 'resigned or ran out of time'):
        print(f'  {ending}: {endings[ending]:,} ({endings[ending] / n:.1%})')
    print('Directions of the moves:')
    for d in range(3):
        print(f'  {DIRECTIONS[d]}: {directions[d]:,} ({directions[d] / moves:.1%})')
    # a diagonal move needs all three directions to be legal
    print(f'  a single legal direction in {single / moves:.1%} of the moves; diagonal in {directions[2] / (moves - single):.1%} '
          f'of those where all three are legal')
    print(f'Games won: blue {n - red_won:,} ({(n - red_won) / n:.1%}), red {red_won:,} ({red_won / n:.1%})')
    print(f'Games started: blue {n - red_first:,} ({(n - red_first) / n:.1%}), red {red_first:,} ({red_first / n:.1%})')
    print(f'The side that moved first won {first_won / n:.1%} of the games; {captures / moves:.1%} of all moves capture a piece')
