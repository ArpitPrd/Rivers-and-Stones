#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <algorithm>
#include <limits>
#include <iostream>
#include <cmath>
#include <set>
#include <iomanip>

namespace py = pybind11;

// ---- Move struct ----
struct Move {
    std::string action;
    std::vector<int> from;
    std::vector<int> to;
    std::vector<int> pushed_to;
    std::string orientation;
};

// ---- Safe map access helper ----
static std::string get_key(const std::map<std::string, std::string>& m, const std::string& k) {
    auto it = m.find(k);
    return it == m.end() ? "" : it->second;
}

// ---- Student Agent ----
class StudentAgent {
private:
    std::string side;
    std::string opponent_side;
    std::random_device rd;
    std::mt19937 gen;
    int board_rows = 12;
    int board_cols = 13;
    const int MAX_DEPTH = 2;

    bool is_valid(int x, int y) const {
        return x >= 0 && x < board_cols && y >= 0 && y < board_rows;
    }

    bool is_in_score_area(int x, int y, const std::string& player_side, const std::vector<int>& score_cols) const {
        if (!is_valid(x,y)) return false;
        bool is_in_score_row = (player_side == "circle") ? (y == 0 || y == 1) : (y == board_rows - 2 || y == board_rows - 1);
        if (!is_in_score_row) return false;
        return std::find(score_cols.begin(), score_cols.end(), x) != score_cols.end();
    }
    
    void trace_river_path(std::vector<Move>& moves, const auto& board, int start_x, int start_y, int curr_x, int curr_y, const std::string& player_side, const std::vector<int>& score_cols, std::set<std::pair<int, int>>& visited) const {
        if (!is_valid(curr_x, curr_y) || is_in_score_area(curr_x, curr_y, (player_side == side) ? opponent_side : side, score_cols)) return;
        
        visited.insert({curr_x, curr_y});
        const auto& cell = board[curr_y][curr_x];
        if (cell.empty() || get_key(cell, "side") != "river") return;

        std::string current_orientation = get_key(cell, "orientation");
        int dx = (current_orientation == "horizontal") ? 1 : 0;
        int dy = (current_orientation == "vertical") ? 1 : 0;

        for (int dir = -1; dir <= 1; dir += 2) {
            int next_x = curr_x + (dx * dir), next_y = curr_y + (dy * dir);
            while (is_valid(next_x, next_y)) {
                if (is_in_score_area(next_x, next_y, (player_side == side) ? opponent_side : side, score_cols)) break;
                
                const auto& next_cell = board[next_y][next_x];
                if (next_cell.empty()) {
                    moves.push_back({"move", {start_x, start_y}, {next_x, next_y}, {}, ""});
                } else if (get_key(next_cell, "side") == "river") {
                    if (get_key(next_cell, "orientation") == current_orientation && visited.find({next_x, next_y}) == visited.end()) {
                        trace_river_path(moves, board, start_x, start_y, next_x, next_y, player_side, score_cols, visited);
                    }
                    break;
                } else {
                    break;
                }
                next_x += (dx * dir);
                next_y += (dy * dir);
            }
        }
    }
    
    void start_river_trace(std::vector<Move>& moves, const auto& board, int start_x, int start_y, int curr_x, int curr_y, const std::string& player_side, const std::vector<int>& score_cols) const {
        std::set<std::pair<int, int>> visited;
        trace_river_path(moves, board, start_x, start_y, curr_x, curr_y, player_side, score_cols, visited);
    }

    std::vector<Move> generate_all_moves(const auto& board, const std::string& player_side, const std::vector<int>& score_cols) const {
        std::vector<Move> moves;
        std::vector<std::pair<int, int>> dirs = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
        std::string current_opponent_side = (player_side == side) ? opponent_side : side;

        for (int y = 0; y < board_rows; ++y) {
            for (int x = 0; x < board_cols; ++x) {
                const auto& cell = board[y][x];
                if (cell.empty() || get_key(cell, "owner") != player_side) continue;
                std::string piece_side = get_key(cell, "side");

                for (auto const& [dx, dy] : dirs) {
                    int nx = x + dx, ny = y + dy;
                    if (!is_valid(nx, ny) || is_in_score_area(nx, ny, current_opponent_side, score_cols)) continue;
                    const auto& next_cell = board[ny][nx];
                    if (next_cell.empty()) moves.push_back({"move", {x, y}, {nx, ny}, {}, ""});
                    else if (get_key(next_cell, "side") == "river") start_river_trace(moves, board, x, y, nx, ny, player_side, score_cols);
                }

                for (auto const& [dx, dy] : dirs) {
                    int nx = x + dx, ny = y + dy;
                    if (!is_valid(nx, ny) || board[ny][nx].empty() || get_key(board[ny][nx], "owner") == player_side) continue;
                    if (is_in_score_area(nx, ny, current_opponent_side, score_cols)) continue;

                    if (piece_side == "stone") {
                        int nx2 = x + 2*dx, ny2 = y + 2*dy;
                        if (is_valid(nx2, ny2) && board[ny2][nx2].empty() && !is_in_score_area(nx2, ny2, player_side, score_cols))
                            moves.push_back({"push", {x, y}, {nx, ny}, {nx2, ny2}, ""});
                    } else {
                        if (get_key(board[ny][nx], "side") == "stone") {
                            std::string orientation = get_key(cell, "orientation");
                            int push_dx = (orientation == "horizontal") ? 1 : 0, push_dy = (orientation == "vertical") ? 1 : 0;
                            for (int dir = -1; dir <= 1; dir += 2) {
                                int cur_px = nx + push_dx * dir, cur_py = ny + push_dy * dir;
                                while(is_valid(cur_px, cur_py)) {
                                     if (is_in_score_area(cur_px, cur_py, player_side, score_cols)) break;
                                     if(board[cur_py][cur_px].empty()) moves.push_back({"push", {x, y}, {nx, ny}, {cur_px, cur_py}, ""});
                                     else break;
                                     cur_px += push_dx * dir; cur_py += push_dy * dir;
                                }
                            }
                        }
                    }
                }
                if (piece_side == "stone") {
                    moves.push_back({"flip", {x, y}, {x, y}, {}, "horizontal"});
                    moves.push_back({"flip", {x, y}, {x, y}, {}, "vertical"});
                } else {
                    moves.push_back({"flip", {x, y}, {x, y}, {}, ""});
                    moves.push_back({"rotate", {x, y}, {x, y}, {}, ""});
                }
            }
        }
        return moves;
    }

    auto apply_move(const auto& board, const Move& move) const {
        auto next_board = board;
        if (move.from.size() < 2) return next_board;
        int from_x = move.from[0], from_y = move.from[1];
        if (!is_valid(from_x, from_y) || next_board[from_y][from_x].empty()) return next_board;

        if (move.action == "move") {
            if (move.to.size() < 2) return next_board;
            int to_x = move.to[0], to_y = move.to[1];
            if (!is_valid(to_x, to_y)) return next_board;
            next_board[to_y][to_x] = next_board[from_y][from_x];
            next_board[from_y][from_x].clear();
        } else if (move.action == "push") {
            if (move.to.size() < 2 || move.pushed_to.size() < 2) return next_board;
            int to_x = move.to[0], to_y = move.to[1];
            int p_x = move.pushed_to[0], p_y = move.pushed_to[1];
            if (!is_valid(to_x, to_y) || !is_valid(p_x, p_y)) return next_board;
            next_board[p_y][p_x] = next_board[to_y][to_x];
            next_board[to_y][to_x] = next_board[from_y][from_x];
            if (get_key(next_board[to_y][to_x], "side") == "river") {
                next_board[to_y][to_x]["side"] = "stone";
                next_board[to_y][to_x].erase("orientation");
            }
            next_board[from_y][from_x].clear();
        } else if (move.action == "flip") {
            auto& piece_to_flip = next_board[from_y][from_x];
            if (get_key(piece_to_flip, "side") == "stone") {
                piece_to_flip["side"] = "river";
                piece_to_flip["orientation"] = move.orientation;
            } else {
                piece_to_flip["side"] = "stone";
                piece_to_flip.erase("orientation");
            }
        } else if (move.action == "rotate") {
            auto& piece_to_rotate = next_board[from_y][from_x];
            piece_to_rotate["orientation"] = (get_key(piece_to_rotate, "orientation") == "horizontal") ? "vertical" : "horizontal";
        }
        return next_board;
    }

    float heuristic(const auto& board, const std::vector<int>& score_cols) const {
        const float W_WIN = 10000000.0f, W_SCORE = 1000.0f, W_THREAT = 150.0f, W_PROXIMITY = 2.0f, W_MATERIAL = 15.0f, W_RIVER = 1.0f;
        float total_score = 0;
        int my_scored_stones = 0, opp_scored_stones = 0;

        for (int y = 0; y < board_rows; ++y) {
            for (int x = 0; x < board_cols; ++x) {
                if (board[y][x].empty()) continue;
                const auto& cell = board[y][x];
                if (get_key(cell, "side") == "stone" && is_in_score_area(x, y, get_key(cell, "owner"), score_cols)) {
                    if (get_key(cell, "owner") == side) my_scored_stones++; else opp_scored_stones++;
                }
            }
        }
        
        if (my_scored_stones >= 4) return W_WIN;
        if (opp_scored_stones >= 4) return -W_WIN;

        for (int y = 0; y < board_rows; ++y) {
            for (int x = 0; x < board_cols; ++x) {
                if (board[y][x].empty()) continue;
                const auto& cell = board[y][x];
                bool is_my_piece = (get_key(cell, "owner") == side);
                int sign = is_my_piece ? 1 : -1;

                if (get_key(cell, "side") == "stone") {
                    total_score += W_MATERIAL * sign;
                    if (is_in_score_area(x, y, get_key(cell, "owner"), score_cols)) {
                        total_score += W_SCORE * sign;
                    }
                    float proximity_score = (get_key(cell, "owner") == "circle") ? (float)(board_rows - 1 - y) : (float)y;
                    total_score += W_PROXIMITY * proximity_score * sign;
                    
                    std::vector<std::pair<int, int>> dirs = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
                    for (auto const& [dx, dy] : dirs) {
                        int nx = x + dx, ny = y + dy;
                        if (is_valid(nx, ny) && board[ny][nx].empty() && is_in_score_area(nx, ny, get_key(cell, "owner"), score_cols))
                            total_score += W_THREAT * sign;
                    }
                } else {
                    total_score += W_RIVER * sign;
                }
            }
        }
        return total_score;
    }
    
    float minimax(const auto& board, int depth, float alpha, float beta, bool is_maximizing_player, const std::vector<int>& score_cols) {
        if (depth <= 0) return heuristic(board, score_cols);
        std::string current_player = is_maximizing_player ? side : opponent_side;
        auto moves = generate_all_moves(board, current_player, score_cols);
        if (moves.empty()) return heuristic(board, score_cols);

        if (is_maximizing_player) {
            float max_eval = std::numeric_limits<float>::lowest();
            for (const auto& move : moves) {
                auto new_board = apply_move(board, move);
                float eval = minimax(new_board, depth - 1, alpha, beta, false, score_cols);
                max_eval = std::max(max_eval, eval);
                alpha = std::max(alpha, eval);
                if (beta <= alpha) break;
            }
            return max_eval;
        } else {
            float min_eval = std::numeric_limits<float>::max();
            for (const auto& move : moves) {
                auto new_board = apply_move(board, move);
                float eval = minimax(new_board, depth - 1, alpha, beta, true, score_cols);
                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);
                if (beta <= alpha) break;
            }
            return min_eval;
        }
    }

public:
    explicit StudentAgent(std::string s) : side(std::move(s)), gen(rd()) {
        opponent_side = (side == "circle") ? "square" : "circle";
    }

    Move choose(const std::vector<std::vector<std::map<std::string, std::string>>>& board, int, int, const std::vector<int>& score_cols, float, float) {
        // std::cout << "@ Choosing" << std::endl;
        try {
            board_rows = board.size();
            board_cols = board[0].size();
            auto legal_moves = generate_all_moves(board, side, score_cols);
            if (legal_moves.empty()) return {"move", {0,0}, {0,0}, {}, ""};

            float best_score = std::numeric_limits<float>::lowest();
            Move best_move = legal_moves[0];
            std::shuffle(legal_moves.begin(), legal_moves.end(), gen);

            for (const auto& move : legal_moves) {
                auto new_board = apply_move(board, move);
                float move_score = minimax(new_board, MAX_DEPTH, std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max(), false, score_cols);
                if (move_score > best_score) {
                    best_score = move_score;
                    best_move = move;
                }
            }
            return best_move;
        } catch (const std::exception& e) {
            std::cerr << "Exception in choose: " << e.what() << std::endl;
            return {"move", {0,0}, {0,0}, {}, ""};
        }
    }
};

// ---- PyBind11 bindings ----
PYBIND11_MODULE(student_agent_module, m) {
    py::class_<Move>(m, "Move")
        .def_readonly("action", &Move::action)
        .def_readonly("from_pos", &Move::from)
        .def_readonly("to_pos", &Move::to)
        .def_readonly("pushed_to", &Move::pushed_to)
        .def_readonly("orientation", &Move::orientation);

    py::class_<StudentAgent>(m, "StudentAgent")
        .def(py::init<std::string>())
        .def("choose", &StudentAgent::choose);
}
// ---- Helper Functions for Debugging ----
// void print_move(const Move& move) {
//     std::cout << "Action: " << move.action << std::endl;
//     if (!move.from.empty()) {
//         std::cout << "  From: (" << move.from[0] << ", " << move.from[1] << ")" << std::endl;
//     }
//     if (!move.to.empty()) {
//         std::cout << "  To:   (" << move.to[0] << ", " << move.to[1] << ")" << std::endl;
//     }
//     if (!move.pushed_to.empty()) {
//         std::cout << "  Pushed To: (" << move.pushed_to[0] << ", " << move.pushed_to[1] << ")" << std::endl;
//     }
//     if (!move.orientation.empty()) {
//         std::cout << "  Orientation: " << move.orientation << std::endl;
//     }
// }

// void print_board(const std::vector<std::vector<std::map<std::string, std::string>>>& board) {
//     std::cout << "    ";
//     for(int i = 0; i < board[0].size(); ++i) {
//         std::cout << std::setw(3) << i << " ";
//     }
//     std::cout << "\n  +";
//     for(int i = 0; i < board[0].size(); ++i) {
//         std::cout << "----";
//     }
//     std::cout << std::endl;

//     for (int y = 0; y < board.size(); ++y) {
//         std::cout << std::setw(2) << y << "| ";
//         for (int x = 0; x < board[0].size(); ++x) {
//             if (board[y][x].empty()) {
//                 std::cout << ".   ";
//             } else {
//                 const auto& cell = board[y][x];
//                 char owner = (cell.at("owner") == "circle") ? 'C' : 'S';
//                 char type = (cell.at("side") == "stone") ? 's' : 'r';
//                 std::cout << owner << type << "  ";
//             }
//         }
//         std::cout << std::endl;
//     }
// }


// // ---- Main Debugging Script ----
// int main() {
//     // 1. Define game parameters
//     std::vector<int> score_cols = {2, 3, 4, 8, 9, 10};
//     int board_rows = 12;
//     int board_cols = 13;
    
//     // 2. Create a sample board state
//     std::vector<std::vector<std::map<std::string, std::string>>> board(board_rows, std::vector<std::map<std::string, std::string>>(board_cols));

//     // Add Agent's pieces ('circle')
//     board[9][5] = {{"owner", "circle"}, {"side", "stone"}};
//     board[8][6] = {{"owner", "circle"}, {"side", "river"}, {"orientation", "horizontal"}};
//     board[10][4] = {{"owner", "circle"}, {"side", "stone"}};
//     board[9][7] = {{"owner", "circle"}, {"side", "stone"}};


//     // Add Opponent's pieces ('square')
//     board[2][5] = {{"owner", "square"}, {"side", "stone"}};
//     board[3][6] = {{"owner", "square"}, {"side", "river"}, {"orientation", "vertical"}};
//     board[2][7] = {{"owner", "square"}, {"side", "stone"}};
//     // Place an opponent stone in a position to be pushed
//     board[8][5] = {{"owner", "square"}, {"side", "stone"}};


//     // 3. Instantiate the agent (playing as 'circle')
//     StudentAgent agent("circle");

//     // 4. Print the initial state
//     std::cout << "--- Initial Board State ---" << std::endl;
//     print_board(board);

//     // 5. Call the choose method to find the best move
//     std::cout << "\n--- Agent is Choosing a Move ---" << std::endl;
//     // The dummy arguments (0, 0, 0.0f, 0.0f) are placeholders for values not used by this agent.
//     Move chosen_move = agent.choose(board, 0, 0, score_cols, 0.0f, 0.0f);

//     // 6. Print the result
//     std::cout << "\n--- Agent's Chosen Move ---" << std::endl;
//     print_move(chosen_move);

//     return 0;
// }
