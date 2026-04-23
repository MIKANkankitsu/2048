#include <array>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <utility>
#include <vector>

//エラー出力用
const std::string RESET = "\x1b[0m";
const std::string RED = "\x1b[31m";

//サイズなど
constexpr int N = 4;
bool is_exit = false;

//リアルタイム入力用ラッパ
char read_char_realtime() {
    termios oldt{};
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) {
        return '\0';
    }

    termios raw = oldt;
    raw.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return '\0';
    }

    char c = '\0';
    const ssize_t n = read(STDIN_FILENO, &c, 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    if (n <= 0) {
        return '\0';
    }
    return c;
}

//数字ごとに色を変える
/*
2 白, 4 シアン, 8 緑, 16 黄, 32 明るい赤, 64 赤, 128 マゼンタ
256 明るいマゼンタ, 512 明るい青, 1024 明るい緑, 2048 明るい黄
>2048 青
*/

const char* tile_color(int value) {
    switch (value) {
    case 2: return "\x1b[37m";
    case 4: return "\x1b[36m";
    case 8: return "\x1b[32m";
    case 16: return "\x1b[33m";
    case 32: return "\x1b[91m";
    case 64: return "\x1b[31m";
    case 128: return "\x1b[35m";
    case 256: return "\x1b[95m";
    case 512: return "\x1b[94m";
    case 1024: return "\x1b[92m";
    case 2048: return "\x1b[93m";
    default: return "\x1b[34m";
    }
}

//表出力

/*
+------+------+------+------+
|    2 |    4 |    8 |    2 |
+------+------+------+------+
|   16 |   32 |   64 |  128 |
+------+------+------+------+
|  256 |  512 | 1024 | 2048 |
+------+------+------+------+
|      |      |      |      |
+------+------+------+------+

の形式
*/

void print_table(const int table[N][N]) {
    const std::string sep = "+------+------+------+------+";

    std::cout << "\n";
    for (int r = 0; r < N; ++r) {
        std::cout << sep << "\n";
        std::cout << "|";
        for (int c = 0; c < N; ++c) {
            if (table[r][c] == 0) {
                std::cout << "      |";
            } else {
                std::ostringstream oss;
                oss << std::setw(4) << table[r][c];
                std::cout << " " << tile_color(table[r][c]) << oss.str() << RESET << " |";
            }
        }
        std::cout << "\n";
    }
    std::cout << sep << "\n\n";
}


//動かせるかどうかの判定：動かせないなら停止
bool can_move(const int table[N][N]) {
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (table[r][c] == 0) {
                return true;
            }
        }
    }

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (r + 1 < N && table[r][c] == table[r + 1][c]) {
                return true;
            }
            if (c + 1 < N && table[r][c] == table[r][c + 1]) {
                return true;
            }
        }
    }
    return false;
}

//動かした後に開いているランダムな要素を追加する
bool add_random_tile(int table[N][N]) {
    std::vector<std::pair<int, int>> empty_cells;
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (table[r][c] == 0) {
                empty_cells.push_back({r, c});
            }
        }
    }
    if (empty_cells.empty()) {
        return false;
    }

    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> pos_dist(0, static_cast<int>(empty_cells.size()) - 1);
    std::uniform_int_distribution<int> val_dist(1, 10);
    auto [r, c] = empty_cells[pos_dist(rng)];
    table[r][c] = (val_dist(rng) == 10 ? 4 : 2);
    return true;
}

//移動用関数たち

//slide_and_merge_left_line:
//左方向に動かしたらどうなるかを見る
bool slide_and_merge_left_line(std::array<int, N>& line) {
    std::array<int, N> original = line;

    std::array<int, N> packed = {0, 0, 0, 0};
    int p = 0;
    for (int i = 0; i < N; ++i) {
        if (line[i] != 0) {
            packed[p++] = line[i];
        }
    }

    for (int i = 0; i < N - 1; ++i) {
        if (packed[i] != 0 && packed[i] == packed[i + 1]) {
            packed[i] *= 2;
            packed[i + 1] = 0;
        }
    }

    line = {0, 0, 0, 0};
    p = 0;
    for (int i = 0; i < N; ++i) {
        if (packed[i] != 0) {
            line[p++] = packed[i];
        }
    }

    return line != original;
}

//左方向に動かす
bool to_left(int table[N][N]) {
    bool moved = false;
    for (int r = 0; r < N; ++r) {
        std::array<int, N> line = {table[r][0], table[r][1], table[r][2], table[r][3]};
        if (slide_and_merge_left_line(line)) {
            moved = true;
        }
        for (int c = 0; c < N; ++c) {
            table[r][c] = line[c];
        }
    }
    return moved;
}

//右方向に動かす -- 左右逆転させて左方向に動かした結果を参照する
bool to_right(int table[N][N]) {
    bool moved = false;
    for (int r = 0; r < N; ++r) {
        std::array<int, N> line = {table[r][3], table[r][2], table[r][1], table[r][0]};
        if (slide_and_merge_left_line(line)) {
            moved = true;
        }
        table[r][3] = line[0];
        table[r][2] = line[1];
        table[r][1] = line[2];
        table[r][0] = line[3];
    }
    return moved;
}

//上方向に動かす -- 転置して左方向に動かした結果を参照する
bool to_up(int table[N][N]) {
    bool moved = false;
    for (int c = 0; c < N; ++c) {
        std::array<int, N> line = {table[0][c], table[1][c], table[2][c], table[3][c]};
        if (slide_and_merge_left_line(line)) {
            moved = true;
        }
        for (int r = 0; r < N; ++r) {
            table[r][c] = line[r];
        }
    }
    return moved;
}

//下方向に動かす -- 転置して左右反転して左方向に動かした結果を参照する
bool to_down(int table[N][N]) {
    bool moved = false;
    for (int c = 0; c < N; ++c) {
        std::array<int, N> line = {table[3][c], table[2][c], table[1][c], table[0][c]};
        if (slide_and_merge_left_line(line)) {
            moved = true;
        }
        table[3][c] = line[0];
        table[2][c] = line[1];
        table[1][c] = line[2];
        table[0][c] = line[3];
    }
    return moved;
}

//一文字入力のヘルパ
int input_helper(char e) {
    if (e == 'W' || e == 'w' || e == 'K' || e == 'k' || e == '8') {
        return 8;
    }
    if (e == 'A' || e == 'a' || e == 'H' || e == 'h' || e == '4') {
        return 4;
    }
    if (e == 'S' || e == 's' || e == 'J' || e == 'j' || e == '2') {
        return 2;
    }
    if (e == 'D' || e == 'd' || e == 'L' || e == 'l' || e == '6') {
        return 6;
    }
    if (e == '\n' || e == '\r') {
        return 0;
    }
    return 0;
}

//ヘルプ入力の際の出力
int print_help(void) {
    std::cout << "Controls:\n"
              << "  Realtime move keys:\n"
              << "    W / K / 8 : Up\n"
              << "    A / H / 4 : Left\n"
              << "    S / J / 2 : Down\n"
              << "    D / L / 6 : Right\n"
              << "    Q : Quit game\n"
              << "  Command mode:\n"
              << "    :help  Show this help\n"
              << "    :quit  Exit game\n";
    return 0;
}

//コマンドモード　コロン ":" で入ることができます
void handle_command_mode() {
    std::cout << ":" << std::flush;
    std::string cmd;
    if (!std::getline(std::cin, cmd)) {
        is_exit = true;
        return;
    }

    if (cmd == "q" || cmd == "quit" || cmd == "exit") {
        is_exit = true;
    } else if (cmd == "h" || cmd == "help") {
        print_help();
    } else if (!cmd.empty()) {
        std::cout << RED << "[error]: " << RESET << "unknown command mode input: " << cmd << "\n";
    }
}

//動作部分
int main(void) {
    int table[N][N] = {};
    add_random_tile(table);
    add_random_tile(table);
    std::cout << "(Realtime mode: W/A/S/D, H/J/K/L, 8/4/2/6, Q. Command mode: :help, :quit)\n";
    print_table(table);

    while (true) {
		//コマンドモードではないなら一文字づつ逐次解釈
        const char c = read_char_realtime();
        if (c == '\0') {
            break;
        }

		//コロンが来たらコマンドモードに
        if (c == ':') {
            handle_command_mode();
            if (is_exit) {
                break;
            }
            continue;
        }

        const int t = input_helper(c);
        if (is_exit) {
            break;
        }

        bool moved = false;
        if (t == 2) {
            moved = to_down(table);
        } else if (t == 4) {
            moved = to_left(table);
        } else if (t == 6) {
            moved = to_right(table);
        } else if (t == 8) {
            moved = to_up(table);
        }

		//動いたら表を再出力
        if (moved) {
            add_random_tile(table);
            print_table(table);
        }

        if (!can_move(table)) {
            std::cout << RED << "[game over]: " << RESET << "no more valid moves.\n";
            print_table(table);
            break;
        }
    }

    return 0;
}
