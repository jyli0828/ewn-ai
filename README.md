# EWN-AI
![Starting position of a game, drawn by the game GUI](docs/images/board.png)

An AI engine and GUI for **EinStein würfelt nicht** (EWN), a dice-driven abstract strategy board game. The project combines a classic **Monte Carlo Tree Search (MCTS)** engine with a **deep learning policy network** (trained on real human game records) to guide move selection, and ships with a simple Windows GUI built with EasyX.

## About the Game

EinStein würfelt nicht ("Einstein doesn't play dice") is played on a 5×5 board. Each player controls six numbered pieces (1–6) placed in a triangle in their home corner. On each turn a die is rolled, which constrains which piece may move; players race to either move a piece into the opponent's home corner or capture all of the opponent's pieces.

## Features

- **Three play modes**: Player vs. Player, Player vs. AI, and AI vs. AI
- **Three AI strategies**, selectable at runtime:
  - `MCTS` — Upper Confidence Bound for Trees (UCT) with random rollouts
  - `Random` — a random-move baseline AI
  - `Model` — MCTS/UCT guided by a trained neural network's move probabilities (an AlphaZero-style policy prior)
- **GUI** built with [EasyX](https://easyx.cn/) for board interaction and move visualization
- **Training pipeline** that downloads top players' games from LittleGolem, turns them into training data, and trains a residual policy network in PyTorch for the game to load

## Project Structure

```
EWN-AI/
├── EWN/                        # C++ game engine, MCTS AI, and GUI (Visual Studio project)
│   ├── EWN.sln                 # Visual Studio solution
│   ├── main.cpp                # Entry point / mode selection
│   ├── play.cpp / play.h       # Game-loop orchestration for each play mode
│   ├── game.cpp / game.h       # Board representation (bitboards), rules, win detection
│   ├── gui.cpp / gui.h         # EasyX-based GUI
│   ├── ewnAI.cpp / ewnAI.h     # AI dispatcher (MCTS / Random / Model)
│   ├── mcts.cpp / mcts.h       # Monte Carlo Tree Search driver
│   ├── uct.cpp / uct.h         # UCT selection logic (incl. model-guided variant)
│   ├── tree_node.cpp / .h      # Search tree node representation
│   ├── model.cpp               # LibTorch integration: builds the feature map and queries the policy model
│   ├── randomAi.cpp / .h       # Random-move baseline AI
│   ├── model_ewn.pt            # Trained policy network (TorchScript)
│   └── picture/                # GUI image assets loaded at runtime
├── training/                   # Policy network training pipeline (Python)
│   ├── scrape_littlegolem.py   # Downloads the top-rated players' match records from LittleGolem
│   ├── parse_game_logs.py      # Turns the match records into a training CSV
│   ├── data_stats.py           # Prints the statistics of the training data
│   ├── train_policy.ipynb      # Feature maps, HDF5 conversion, policy network training and export
│   ├── requirements.txt        # Python packages for the training pipeline
│   └── game_logs/              # Downloaded match records (generated, not committed)
└── docs/
    └── images/                 # README images
```

## How the AI Works

### Board encoding

Each position is encoded as a 16×5×5 feature map, built the same way by `board_to_FM()` in `EWN/model.cpp` and by the training notebook. The game queries the model once per movable piece:

| Channel | Content |
|---|---|
| 0–5 | Red pieces 1–6 |
| 6–11 | Blue pieces 1–6 |
| 12 | The piece to move |
| 13 | All ones, which lets the convolutions find the board edges |
| 14 | How many of the six dice faces let each piece move (a captured piece's faces pass to its nearest remaining neighbors) |
| 15 | How many dice faces let the opponent move a piece onto each square next turn |

### Policy model

For the marked piece, the model outputs a probability for each direction (horizontal, vertical, diagonal). `training/train_policy.ipynb` trains a small residual network and exports it with illegal directions masked out. The shipped `EWN/model_ewn.pt` was trained this way on the data below: it picks the direction the human player chose for 76.5% of the test moves, against 57.4% for always picking the most common legal direction.

### Guided search

In `Model` mode, `uct.cpp` blends the classic UCT exploration/exploitation terms with the policy network's move probability as an additional prior, biasing the tree search toward moves the network favors while still exploring via rollouts.

### Training data

`scrape_littlegolem.py` downloads the match records of the top 10 players in [LittleGolem](https://www.littlegolem.net/)'s EinStein ranking, and `parse_game_logs.py` turns them into one CSV line per game (initial setups, move list, result), dropping duplicates. The notebook then turns each game into board states to train the policy network.

The current training data was downloaded on 2026-11-02 from these players' finished matches:

| Rank | Player | Rating | Matches |
|---:|---|---:|---:|
| 1 | EWN_ghost_ | 2028.7 | 416 |
| 2 | Micco | 2016.0 | 745 |
| 3 | opmp | 1984.9 | 1484 |
| 4 | Fraggle_Reloaded_c | 1968.9 | 1630 |
| 5 | hanfried_c | 1968.8 | 481 |
| 6 | 1Stone | 1965.7 | 220 |
| 7 | bennok | 1958.8 | 588 |
| 8 | Ray Garrison | 1958.7 | 952 |
| 9 | Force majeure | 1949.7 | 427 |
| 10 | Florian Jamain | 1949.2 | 582 |

A match between two listed players counts for both above. Overall, the data has the statistics below, printed by `data_stats.py` (the notebook prints the training / test split):

| | Count |
|---|---:|
| Matches, each counted once (338 matches without any moves are skipped) | 7,260 |
| Games, after removing the 3,750 that appear in two players' logs | 58,127 |
| Moves, i.e. training samples | 970,703 |
| Training / test moves, split by game | 821,449 / 149,254 |
| Moves per game | 16.7 on average, 2 to 32 |

| How the game was won | Games | Share |
|---|---:|---:|
| A piece reached the opponent's home corner | 53,527 | 92.1% |
| All of the opponent's pieces were captured | 4,173 | 7.2% |
| Neither: the opponent resigned or ran out of time | 427 | 0.7% |

| | Blue | Red |
|---|---:|---:|
| Games won | 29,141 (50.1%) | 28,986 (49.9%) |
| Games started | 32,457 (55.8%) | 25,670 (44.2%) |

The side that moved first won 52.4% of the games, and 40.9% of all moves capture a piece.

## Requirements

- **Windows** + **Visual Studio 2022 or later** with the v143 (MSVC) build tools (the project is a `.sln`/`.vcxproj` targeting Windows)
- [**EasyX**](https://easyx.cn/) graphics library (for the GUI)
- [**LibTorch**](https://pytorch.org/get-started/locally/) (C++ distribution of PyTorch, for loading `model_ewn.pt` in `model.cpp`). The model runs on the CPU, so the CPU build is sufficient.
- **Python 3** with the packages in `training/requirements.txt` (for the `training/` pipeline)

## Getting Started

1. Clone the repository and open `EWN/EWN.sln` in Visual Studio.
2. Install EasyX and LibTorch, and configure their include/lib paths in the project settings.
3. Select the `Release | x64` configuration (the only one with LibTorch settings), then build and run. On launch, choose a game mode (Player vs. Player / Player vs. AI / AI vs. AI) and, if applicable, an AI strategy (MCTS / Random / Model).

### Visual Studio Project Configuration

Assuming LibTorch is extracted to `C:\libtorch`, configure the following in **Project → Properties** (apply to the relevant Configuration/Platform, e.g. `Release | x64`). LibTorch 2.13 requires C++20. `EWN.vcxproj` already includes these settings for `Release | x64`, except **Debugging → Environment**, which Visual Studio stores in the uncommitted per-user `EWN.vcxproj.user` file:

| Section | Setting | Value |
|---|---|---|
| Debugging → Environment | — | `PATH=C:\libtorch\lib;%PATH%` |
| General → C++ Language Standard | — | ISO C++20 Standard (`/std:c++20`) |
| C/C++ → General → Additional Include Directories | — | `C:\libtorch\include\torch\csrc\api\include;C:\libtorch\include` |
| Linker → General → Additional Library Directories | — | `C:\libtorch\lib` |
| Linker → Input → Additional Dependencies | — | `C:\libtorch\lib\*.lib;%(AdditionalDependencies)` |

> **Note:** Set the **Debugging → Environment** PATH so the LibTorch `.dll` files (e.g. `torch.dll`, `c10.dll`) can be found at runtime when launching from Visual Studio. If running the built `.exe` outside Visual Studio, either add `C:\libtorch\lib` to your system PATH or copy the required `.dll` files next to the executable.
>
> The game also loads `picture/gui_background.jpg` and `model_ewn.pt` relative to the working directory. Visual Studio uses the `EWN/` project folder by default; when running the `.exe` elsewhere, start it from `EWN/` or copy `picture/` and `model_ewn.pt` next to it.

## Training the Policy Network

1. Install the Python packages: `pip install -r training/requirements.txt`.
2. Run `python training/scrape_littlegolem.py` to download the current top 10 players' finished standard-rule matches into `training/game_logs/`. By default it sends at most one request per second, so this takes a while; `--workers 4 --delay 0.25` keeps four downloads going at once and is about four times faster. Downloaded matches are cached, so it can be stopped and run again.
3. Run `python training/parse_game_logs.py` to turn the logs in `training/game_logs/` into `training/training_data.csv`, and `python training/data_stats.py` to print its statistics.
4. Open `training/train_policy.ipynb` to convert the CSV into feature maps/HDF5, train the model, and export a new `model_ewn.pt`. The notebook reads and writes these files in `DATA_DIR` (set in its first cell): `/content/drive/MyDrive/EWN-AI` on Colab, otherwise the notebook's own folder. On a Colab T4 GPU, building the HDF5 file takes about 7 minutes and training about 4. The notebook also prints baseline accuracies to judge the model's accuracy by.
5. Copy the exported `model_ewn.pt` into `EWN/`, where the game loads it.

> **Note:** The game's LibTorch 2.13 loads models exported by the same or an older PyTorch, such as the one preinstalled on Colab, so `training/requirements.txt` only keeps PyTorch below 2.14, and the notebook's first cell warns about a newer one. Do not install another PyTorch on Colab: its CUDA libraries clash with Colab's own and cuDNN fails.
