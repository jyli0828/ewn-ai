"""Download the EinStein wurfelt nicht! match records of LittleGolem's top-rated players.

Writes game_logs/<player>.txt in the format parse_game_logs.py reads, plus game_logs/players.csv.
Downloaded matches are cached in game_logs/matches/, so the script can be stopped and run again.
--workers downloads several matches at once; --delay still spaces out the start of every request.
"""
import argparse
import html
import http.client
import os
import re
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from urllib.parse import urlsplit

SITE = 'https://www.littlegolem.net'
RANKING_URL = SITE + '/web/rankings?scope=einstein%2FDEFAULT'
HISTORY_URL = SITE + '/web/players/{}?state=all'
EXPORT_URL = 'https://api.littlegolem.net/gameexport/{}'
USER_AGENT = 'EWN-AI-training-data-scraper/1.0 (research use; rate-limited)'

log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'game_logs')
match_dir = os.path.join(log_dir, 'matches')
# Start time reserved for the next request, shared by all download threads
next_start = 0.0
start_lock = threading.Lock()
# Each thread keeps its HTTPS connections open, since a new TLS handshake takes most of a request's time
connections = threading.local()


def get(url):
    # (status, body) of a GET request over this thread's kept-alive connection to the host
    parts = urlsplit(url)
    if not hasattr(connections, 'by_host'):
        connections.by_host = {}
    conn = connections.by_host.get(parts.netloc)
    if conn is None:
        conn = connections.by_host[parts.netloc] = http.client.HTTPSConnection(parts.netloc, timeout=30)
    try:
        conn.request('GET', parts.path + ('?' + parts.query if parts.query else ''), headers={'User-Agent': USER_AGENT})
        response = conn.getresponse()
        return response.status, response.read().decode('utf-8')
    except (OSError, http.client.HTTPException):
        # e.g. the server closed an idle connection; the next request opens a new one
        conn.close()
        del connections.by_host[parts.netloc]
        raise


def fetch(url, delay, retries=4):
    global next_start
    for attempt in range(retries):
        # Requests start at least `delay` seconds apart, however many threads are downloading
        with start_lock:
            start = max(time.time(), next_start)
            next_start = start + delay
        time.sleep(max(0.0, start - time.time()))
        try:
            status, body = get(url)
        except (OSError, http.client.HTTPException) as e:
            error = e
        else:
            if status == 200:
                return body
            # Client errors other than rate limiting will not go away by retrying
            if status < 500 and status != 429:
                raise RuntimeError(f'HTTP {status} for {url}')
            error = f'HTTP {status}'
        if attempt == retries - 1:
            raise RuntimeError(f'{url} failed: {error}')
        print(f'  retrying {url} after error: {error}')
        # Retry at once, since a kept-alive connection may just have been closed, then back off
        time.sleep(delay * 10 * attempt)


def cell_text(cell):
    return re.sub(r'\s+', ' ', html.unescape(re.sub(r'<[^>]+>', ' ', cell))).strip()


def top_players(n, delay):
    # (id, name, rating) of the first n players in the EinStein rating list
    players = []
    for row in re.findall(r'(?s)<tr[^>]*>(.*?)</tr>', fetch(RANKING_URL, delay)):
        player = re.search(r'href="/web/players/(\d+)"[^>]*>(.*?)</a>', row)
        if not player:
            continue
        cells = [cell_text(c) for c in re.findall(r'(?s)<td[^>]*>(.*?)</td>', row)]
        players.append((player.group(1), cell_text(player.group(2)), cells[3]))
        if len(players) == n:
            break
    return players


def match_ids(player_id, delay):
    # Ids of the player's finished standard EinStein matches, newest first
    ids, url, seen = [], HISTORY_URL.format(player_id), set()
    while url and url not in seen:
        seen.add(url)
        page = fetch(url, delay)
        for row in re.findall(r'(?s)<tr[^>]*>(.*?)</tr>', page):
            game = re.search(r'href="/web/game\?id=(\d+)">([^<]*)</a>', row)
            finished = re.search(r'data-label="Finished">([^<]*)<', row)
            if not game or not finished:
                continue
            # e.g. "#2399283 | EinStein wurfelt nicht! | 7-points match" (with middle dots); variants have another label
            label = [part.strip() for part in html.unescape(game.group(2)).split('\u00b7')]
            if label[1:2] == ['EinStein w\u00fcrfelt nicht!'] and re.fullmatch(r'\d+-points? match', label[-1]) \
                    and re.search(r'\d{4}', finished.group(1)):
                ids.append(game.group(1))
        before = re.search(r'href="/web/players/\d+\?before=(\d+)&amp;state=all', page)
        url = HISTORY_URL.format(player_id) + f'&before={before.group(1)}' if before else None
    return ids


def is_played(record):
    # A record that parse_game_logs.py reads: 5 header lines and the line of moves, which may hold no game
    # when a player resigned at once
    lines = record.splitlines()
    return len(lines) == 6 and lines[5].startswith('1. ')


def match_record(match_id, delay):
    # The exported match (5 header lines and one line of moves), cached on disk
    path = os.path.join(match_dir, match_id + '.txt')
    if os.path.exists(path):
        with open(path, encoding='utf-8') as f:
            return f.read()
    text = fetch(EXPORT_URL.format(match_id), delay).replace('\r\n', '\n').strip('\n') + '\n'
    # Write a temporary file first, so a stopped run never leaves a partial record behind
    with open(path + '.tmp', 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    os.replace(path + '.tmp', path)
    return text


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--top', type=int, default=10, help='number of top-rated players (default: 10)')
    parser.add_argument('--delay', type=float, default=1.0, help='minimum seconds between the starts of two requests (default: 1.0)')
    parser.add_argument('--workers', type=int, default=1, help='matches downloaded at once (default: 1)')
    args = parser.parse_args()
    os.makedirs(match_dir, exist_ok=True)

    players = top_players(args.top, args.delay)
    summary = ['rank,player_id,player,rating,matches,skipped\n']
    for rank, (player_id, name, rating) in enumerate(players, 1):
        ids = match_ids(player_id, args.delay)
        print(f'{rank}. {name} ({rating}): {len(ids)} matches')
        records, skipped = [], 0
        with ThreadPoolExecutor(args.workers) as pool:
            for i, record in enumerate(pool.map(lambda match_id: match_record(match_id, args.delay), ids), 1):
                # parse_game_logs.py reads records of exactly 7 lines, the last one blank
                if is_played(record):
                    records.append(record + '\n')
                else:
                    skipped += 1
                if i % 100 == 0:
                    print(f'  {i}/{len(ids)}')
        filename = re.sub(r'[\\/:*?"<>|]', '_', name) + '.txt'
        with open(os.path.join(log_dir, filename), 'w', encoding='utf-8', newline='\n') as f:
            f.writelines(records)
        summary.append(f'{rank},{player_id},"{name}",{rating},{len(records)},{skipped}\n')

    with open(os.path.join(log_dir, 'players.csv'), 'w', encoding='utf-8', newline='\n') as f:
        f.writelines(summary)
    print(''.join(summary))
