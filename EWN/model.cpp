#include "model.h"
#include <fstream>
#include <immintrin.h>
#include <intrin.h>

// The policy network of training/train_policy.ipynb (PolicyNet), run without a deep learning library:
// a 3x3 convolution of the 16 feature maps, residual blocks of two 3x3 convolutions, a 1x1 convolution to
// 2 channels and a linear layer to the 3 directions. The notebook folds every batch norm into the convolution
// before it. An activation holds the 5x5 board inside a border of zeros, channels last: channel c of
// square (y, x) is at ((y + 1) * 7 + x + 1) * channels + c.

struct Conv {
	int in_ch, out_ch;
	// weight[(tap * in_ch + in) * out_ch + out] of tap dy * k + dx, so the weights of one input are contiguous
	vector<float> weight, bias;
};

// Loaded by load_model() when the Model AI is chosen, so the other modes run without the file
static Conv stem, head;
// two convolutions per residual block
static vector<Conv> block_convs;
static vector<float> fc_weight, fc_bias;
static bool model_loaded = false, use_avx2 = false;

static bool read_floats(ifstream& in, vector<float>& v, size_t n) {
	v.resize(n);
	return (bool)in.read((char*)v.data(), n * sizeof(float));
}

// a k x k convolution, stored as PyTorch's weight[out][in][dy][dx] and then the bias
static bool read_conv(ifstream& in, Conv& conv, int in_ch, int out_ch, int k) {
	vector<float> w;
	if (!read_floats(in, w, (size_t)out_ch * in_ch * k * k) || !read_floats(in, conv.bias, out_ch)) return false;
	conv.in_ch = in_ch, conv.out_ch = out_ch;
	conv.weight.resize(w.size());
	for (int o = 0; o < out_ch; o++)
		for (int i = 0; i < in_ch; i++)
			for (int t = 0; t < k * k; t++)
				conv.weight[((size_t)t * in_ch + i) * out_ch + o] = w[((size_t)o * in_ch + i) * k * k + t];
	return true;
}

// The file of export_weights() in the notebook: "EWN1", the numbers of input channels, channels and residual
// blocks, then the float32 weights and biases of every layer in order
static bool read_model(const char* path) {
	ifstream in(path, ios::binary);
	char magic[4];
	int header[3];
	if (!in.read(magic, 4) || memcmp(magic, "EWN1", 4) != 0 || !in.read((char*)header, sizeof(header))) return false;
	int in_ch = header[0], channels = header[1], blocks = header[2];
	// the convolutions work on 16 output channels at a time
	if (in_ch != 16 || channels <= 0 || channels > 256 || channels % 16 != 0 || blocks < 0 || blocks > 64) return false;

	if (!read_conv(in, stem, in_ch, channels, 3)) return false;
	block_convs.resize(2 * blocks);
	for (Conv& conv : block_convs)
		if (!read_conv(in, conv, channels, channels, 3)) return false;
	if (!read_conv(in, head, channels, 2, 1)) return false;
	if (!read_floats(in, fc_weight, 3 * 2 * 25) || !read_floats(in, fc_bias, 3)) return false;
	// and nothing after them
	return in.peek() == EOF;
}

// AVX2 and FMA, with an OS that saves the 256-bit registers
static bool cpu_has_avx2() {
	int info[4];
	__cpuid(info, 0);
	if (info[0] < 7) return false;
	__cpuid(info, 1);
	bool fma = info[2] & (1 << 12), osxsave = info[2] & (1 << 27), avx = info[2] & (1 << 28);
	if (!fma || !osxsave || !avx || (_xgetbv(0) & 6) != 6) return false;
	__cpuidex(info, 7, 0);
	return info[1] & (1 << 5);
}

void load_model() {
	if (model_loaded) return;
	if (!read_model("./model_ewn.bin")) {
		cout << "Cannot load ./model_ewn.bin; run the game from the EWN folder that has the model" << endl;
		system("pause");
		exit(1);
	}
	use_avx2 = cpu_has_avx2();
	model_loaded = true;
}

// out = ReLU(conv(in) + bias + residual), where residual may be NULL
static void conv3x3(const Conv& conv, const float* in, float* out, const float* residual) {
	for (int sq = 0; sq < 25; sq++) {
		int y = sq / 5, x = sq % 5, pos = ((y + 1) * 7 + x + 1) * conv.out_ch;
		float* o = out + pos;
		for (int c = 0; c < conv.out_ch; c++) o[c] = conv.bias[c];
		for (int tap = 0; tap < 9; tap++) {
			// the 3x3 window of the square starts at (y, x) of the bordered board
			const float* a = in + ((y + tap / 3) * 7 + x + tap % 3) * conv.in_ch;
			const float* w = conv.weight.data() + tap * conv.in_ch * conv.out_ch;
			for (int i = 0; i < conv.in_ch; i++, w += conv.out_ch)
				for (int c = 0; c < conv.out_ch; c++) o[c] += a[i] * w[c];
		}
		for (int c = 0; c < conv.out_ch; c++) {
			if (residual) o[c] += residual[pos + c];
			o[c] = max(o[c], 0.0f);
		}
	}
}

// The same with AVX2: the 5 squares of a row and 16 output channels at a time, whose sums stay in registers
static void conv3x3_avx2(const Conv& conv, const float* in, float* out, const float* residual) {
	const int in_ch = conv.in_ch, out_ch = conv.out_ch;
	for (int y = 0; y < 5; y++) {
		for (int c = 0; c < out_ch; c += 16) {
			__m256 sum[5][2];
			for (int x = 0; x < 5; x++) {
				sum[x][0] = _mm256_loadu_ps(&conv.bias[c]);
				sum[x][1] = _mm256_loadu_ps(&conv.bias[c + 8]);
			}
			// the rows of the border above the first row and below the last are zero, so they are skipped
			for (int dy = (y == 0); dy < 3 - (y == 4); dy++) {
				for (int dx = 0; dx < 3; dx++) {
					const float* a = in + ((y + dy) * 7 + dx) * in_ch;
					const float* w = conv.weight.data() + (dy * 3 + dx) * in_ch * out_ch + c;
					for (int i = 0; i < in_ch; i++, w += out_ch) {
						__m256 w0 = _mm256_loadu_ps(w), w1 = _mm256_loadu_ps(w + 8);
						for (int x = 0; x < 5; x++) {
							__m256 v = _mm256_broadcast_ss(a + x * in_ch + i);
							sum[x][0] = _mm256_fmadd_ps(v, w0, sum[x][0]);
							sum[x][1] = _mm256_fmadd_ps(v, w1, sum[x][1]);
						}
					}
				}
			}
			for (int x = 0; x < 5; x++) {
				int pos = ((y + 1) * 7 + x + 1) * out_ch + c;
				for (int h = 0; h < 2; h++) {
					__m256 v = sum[x][h];
					if (residual) v = _mm256_add_ps(v, _mm256_loadu_ps(residual + pos + h * 8));
					_mm256_storeu_ps(out + pos + h * 8, _mm256_max_ps(v, _mm256_setzero_ps()));
				}
			}
		}
	}
	_mm256_zeroupper();
}

// The squares of the chess that one side (range 0 is red, 6 is blue) can move with dice d, as in
// Game::count_movable_chs(): the chess of the dice, or the nearest lower and higher ones if it was captured.
// Unused entries are -1.
void dice_chs_pos(const int chs_pos[13], int range, int d, int movable[2]) {
	int lower = d, higher = d;
	while (lower >= 1 && chs_pos[lower + range] < 0) lower--;
	while (higher <= 6 && chs_pos[higher + range] < 0) higher++;
	movable[0] = (lower >= 1) ? chs_pos[lower + range] : -1;
	movable[1] = (higher <= 6 && higher != lower) ? chs_pos[higher + range] : -1;
}

void board_to_FM(const Game& game, int dice, int prev_board[16][5][5]) {
	memset(prev_board, 0, sizeof(int) * 16 * 5 * 5);
	// square (i * 5 + j) of each chess 1~12; -1 if it was captured
	int chs_pos[13];
	for (int k = 0; k < 13; k++) chs_pos[k] = -1;

	for (int i = 0; i < 5; i++) {
		for (int j = 0; j < 5; j++) {
			int k = game.get_board_P(i, j);

			if (k > 0) {
				prev_board[k - 1][i][j] = 1;
				chs_pos[k] = i * 5 + j;
			}
			prev_board[13][i][j] = 1;
		}
	}

	// channel 12: the chess of the dice, or the nearest lower and higher ones if it was captured
	int dice_range = (dice <= 6) ? 0 : 6, dice_movable[2];
	dice_chs_pos(chs_pos, dice_range, dice - dice_range, dice_movable);
	for (int m = 0; m < 2; m++)
		if (dice_movable[m] >= 0) prev_board[12][dice_movable[m] / 5][dice_movable[m] % 5] = 1;

	// channel 14: how many dice faces let each chess move
	for (int range = 0; range <= 6; range += 6) {
		for (int d = 1; d <= 6; d++) {
			int movable[2];
			dice_chs_pos(chs_pos, range, d, movable);
			for (int m = 0; m < 2; m++)
				if (movable[m] >= 0) prev_board[14][movable[m] / 5][movable[m] % 5] += 1;
		}
	}

	// channel 15: how many dice faces let the opponent move a chess onto each square next turn
	int opponent = (dice <= 6) ? 6 : 0, step = (dice <= 6) ? -1 : 1;
	for (int d = 1; d <= 6; d++) {
		int movable[2];
		bool reach[25] = { false };
		dice_chs_pos(chs_pos, opponent, d, movable);
		for (int m = 0; m < 2; m++) {
			if (movable[m] < 0) continue;
			// horizontal, vertical and diagonal moves of the opponent
			int i = movable[m] / 5, j = movable[m] % 5;
			int di[3] = { 0, step, step }, dj[3] = { step, 0, step };
			for (int k = 0; k < 3; k++) {
				int ni = i + di[k], nj = j + dj[k];
				if (ni >= 0 && ni < 5 && nj >= 0 && nj < 5) reach[ni * 5 + nj] = true;
			}
		}
		for (int sq = 0; sq < 25; sq++)
			if (reach[sq]) prev_board[15][sq / 5][sq % 5] += 1;
	}
}

void model_probs(const Game& game, int chs_index, float probs[3]) {
	const int channels = stem.out_ch;
	// the borders stay zero, since only the squares of the board are written; each thread has its own
	thread_local vector<float> input(7 * 7 * 16), act[3];
	for (vector<float>& a : act) a.resize(7 * 7 * channels);

	int fm[16][5][5];
	board_to_FM(game, chs_index + 1 + ((!game.side) ? 0 : 6), fm);
	for (int k = 0; k < 16; k++)
		for (int sq = 0; sq < 25; sq++)
			input[((sq / 5 + 1) * 7 + sq % 5 + 1) * 16 + k] = (float)fm[k][sq / 5][sq % 5];

	auto conv = use_avx2 ? conv3x3_avx2 : conv3x3;
	// x is the input of the next block and h the inside of a block
	float *x = act[0].data(), *h = act[1].data(), *next = act[2].data();
	conv(stem, input.data(), x, NULL);
	for (int b = 0; b < (int)block_convs.size(); b += 2) {
		conv(block_convs[b], x, h, NULL);
		conv(block_convs[b + 1], h, next, x);
		swap(x, next);
	}

	// the 1x1 convolution to 2 channels, then the linear layer over them, flattened as (channel, square)
	float logits[3] = { fc_bias[0], fc_bias[1], fc_bias[2] };
	for (int k = 0; k < 2; k++) {
		for (int sq = 0; sq < 25; sq++) {
			const float* a = x + ((sq / 5 + 1) * 7 + sq % 5 + 1) * channels;
			float v = head.bias[k];
			for (int i = 0; i < channels; i++) v += a[i] * head.weight[i * 2 + k];
			v = max(v, 0.0f);
			for (int d = 0; d < 3; d++) logits[d] += fc_weight[d * 50 + k * 25 + sq] * v;
		}
	}

	// the softmax of the legal directions; an illegal one gets 0, as the notebook masks it out
	bool legal[3];
	float max_logit = -1e30f, total = 0;
	for (int d = 0; d < 3; d++) {
		legal[d] = game.check_in_board(Movement(chs_index, d));
		if (legal[d]) max_logit = max(max_logit, logits[d]);
	}
	for (int d = 0; d < 3; d++) {
		probs[d] = legal[d] ? exp(logits[d] - max_logit) : 0;
		total += probs[d];
	}
	for (int d = 0; d < 3; d++) probs[d] /= total;
}
