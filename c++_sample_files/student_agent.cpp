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
#include <memory>
#include <chrono>
#include <optional>

using namespace std;

namespace py = pybind11;

struct Move {
    string action;
    vector<int> from;
    vector<int> to;
    vector<int> pushed_to;
    string orientation;
};

static string get_key(const map<string, string>& m, const string& k) {
    auto it = m.find(k);
    return it == m.end() ? "" : it->second;
}

using BoardState = vector<vector<map<string, string>>>;

struct Node {
    BoardState state;
    Node* parent = nullptr;
    vector<unique_ptr<Node>> children;
    Move move;
    
    int wins = 0;
    int playouts = 0;
    
    string pid;
    vector<Move> untried_moves;
    bool is_fully_expanded = false;
    bool is_terminal = false;
    string terminal_result = "";
};

class StudentAgent {
private:
    string side;
    string opponent_side;
    random_device rd;
    mt19937 gen;
    int board_rows = 12;
    int board_cols = 13;

    // MANUAL CHANGE 6
    const double TIME_LIMIT_SECONDS = 0.1; 
    const double UCT_C = 1.414;
    int turn_count = 0;
    
    // int count_pieces_in_score_area(const BoardState& board, const string& pid, const vector<int>& score_cols) const;
    // int count_pieces_near_score_area(const BoardState& board, const string& pid, const vector<int>& score_cols) const;

    Node* mcts_select_init_node(Node* root);
    Node* mcts_expand_node(Node* node, const vector<int>& score_cols);
    void backpropagate(Node* node, double result);
    Move find_playout_move(const vector<Move>& moves, const BoardState& board, const string& pid, const vector<int>& score_cols);
    double simulate_playout(Node* node, const vector<int>& score_cols);

    Move get_opening_move();
    optional<Move> find_immediate_win(const BoardState& board, const vector<int>& score_cols);
    optional<Move> block_opponent_win(const BoardState& board, const vector<int>& score_cols);
    Move find_mcts_move(const BoardState& board, const vector<int>& score_cols);


public:
    explicit StudentAgent(string s); 
    Move choose(const BoardState& board, int, int, const vector<int>& score_cols, float, float); 


    bool is_inside_board(int x, int y) const {
        return x >= 0 && x < board_cols && y >= 0 && y < board_rows;
    }

    bool present_in_scoring(int x, int y, const string& pid, const vector<int>& score_cols) const {
        if (!is_inside_board(x,y) || score_cols.empty()) return false;
        bool in_score_row = (pid == "circle") ? (y == 2) : (y == board_rows - 3);
        return in_score_row && find(score_cols.begin(), score_cols.end(), x) != score_cols.end();
    }

    void identify_river_motion(vector<Move>& moves, const BoardState& board, int start_x, int start_y, int curr_x, int curr_y, const string& pid, const vector<int>& score_cols, set<pair<int, int>>& visited) const {
        if (!is_inside_board(curr_x, curr_y) || present_in_scoring(curr_x, curr_y, (pid == side) ? opponent_side : side, score_cols)) return;
        visited.insert({curr_x, curr_y});
        const auto& cell = board[curr_y][curr_x];
        if (cell.empty() || get_key(cell, "side") != "river") return;
        
        string current_orientation = get_key(cell, "orientation");
        int dx = (current_orientation == "horizontal") ? 1 : 0;
        int dy = (current_orientation == "vertical") ? 1 : 0;
        
        for (int dir = -1; dir <= 1; dir += 2) {
            int next_x = curr_x + (dx * dir), next_y = curr_y + (dy * dir);
            while (is_inside_board(next_x, next_y)) {
                if (present_in_scoring(next_x, next_y, (pid == side) ? opponent_side : side, score_cols)) break;
                const auto& next_cell = board[next_y][next_x];
                if (next_cell.empty()) {
                    moves.push_back({"move", {start_x, start_y}, {next_x, next_y}, {}, ""});
                } else if (get_key(next_cell, "side") == "river") {
                    if (get_key(next_cell, "orientation") == current_orientation && visited.find({next_x, next_y}) == visited.end()) {
                        identify_river_motion(moves, board, start_x, start_y, next_x, next_y, pid, score_cols, visited);
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

    void find_river_moves(vector<Move>& moves, const BoardState& board, int start_x, int start_y, int curr_x, int curr_y, const string& pid, const vector<int>& score_cols) const {
        set<pair<int, int>> visited;
        identify_river_motion(moves, board, start_x, start_y, curr_x, curr_y, pid, score_cols, visited);
    }

    vector<Move> get_all_moves(const BoardState& board, const string& pid, const vector<int>& score_cols) const {
        vector<Move> moves;
        vector<pair<int, int>> dirs = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
        string current_opponent_side = (pid == this->side) ? this->opponent_side : this->side;

        set<pair<int, int>> fixed_positions;
        set<pair<int, int>> conditionally_fixed_positions;

        if (pid == "circle") {
            fixed_positions = {{3, 10}, {3, 11}, {8, 10}, {8, 11}};
            conditionally_fixed_positions = {{4, 9}, {5, 9}, {6, 9}, {7, 9}};
        } else {
            fixed_positions = {{3, 2}, {3, 1}, {8, 2}, {8, 1}};
            conditionally_fixed_positions = {{4, 3}, {5, 3}, {6, 3}, {7, 3}};
        }

        for (int y = 0; y < board_rows; ++y) {
            for (int x = 0; x < board_cols; ++x) {
                const auto& cell = board[y][x];
                if (cell.empty() || get_key(cell, "owner") != pid) continue;

                string piece_side = get_key(cell, "side");

                if (fixed_positions.count({x, y})) {


                    // MANUAL CHANGE 2;

                    // if (piece_side == "stone") {
                    //     moves.push_back({"flip", {x, y}, {x, y}, {}, "horizontal"});
                    //     moves.push_back({"flip", {x, y}, {x, y}, {}, "vertical"});
                    // } else {
                    //     moves.push_back({"flip", {x, y}, {x, y}, {}, ""});
                    //     moves.push_back({"rotate", {x, y}, {x, y}, {}, flip(get_key(cell, "orientation"))});
                    // }
                    continue;
                }

                if (conditionally_fixed_positions.count({x, y})) {

                    // MANUAL CHANGE 3

                    // vector<Move> scoring_moves;
                    // for (auto const& [dx, dy] : dirs) {
                    //     int nx = x + dx, ny = y + dy;
                    //     if(is_inside_board(nx, ny) && board[ny][nx].empty() && present_in_scoring(nx, ny, pid, score_cols) && !present_in_scoring(nx, ny, current_opponent_side, score_cols)){
                    //        scoring_moves.push_back({"move", {x, y}, {nx, ny}, {}, ""});
                    //     }
                    // }

                    // if(!scoring_moves.empty()){
                    //     moves.insert(moves.end(), scoring_moves.begin(), scoring_moves.end());
                    // } else { 
                    //     if (piece_side == "stone") {
                    //        moves.push_back({"flip", {x, y}, {x, y}, {}, "horizontal"});
                    //        moves.push_back({"flip", {x, y}, {x, y}, {}, "vertical"});
                    //     } else {
                    //        moves.push_back({"flip", {x, y}, {x, y}, {}, ""});
                    //        moves.push_back({"rotate", {x, y}, {x, y}, {}, ""});
                    //     }
                    // }
                    continue;
                }
                
                // moves for those peices that are already in the scoring area
                // flip if river
                // if stone move horizontally making sure it is in score area for me, not in opps score and board is empty 
                if (present_in_scoring(x, y, pid, score_cols)) {
                    if (piece_side == "river") {
                        moves.push_back({"flip", {x, y}, {x, y}, {}, ""});
                    } else if (piece_side == "stone") {
                        for (int dx : {-1, 1}) {
                            int nx = x + dx;
                            if (is_inside_board(nx, y) && present_in_scoring(nx, y, pid, score_cols) && !present_in_scoring(nx, y, current_opponent_side, score_cols) && board[y][nx].empty()) {
                                moves.push_back({"move", {x, y}, {nx, y}, {}, ""});
                            }
                        }
                    }
                    continue;
                }

                // does the river flow movements
                // ignores a new pos if 
                /*
                    not in valid boaard pos or is gonna enter opps score area or is already in my score area dont move
                */
                for (auto const& [dx, dy] : dirs) {
                    int nx = x + dx, ny = y + dy;


                    // MANUAL CHANGE 1

                    if (!is_inside_board(nx, ny) || present_in_scoring(nx, ny, current_opponent_side, score_cols) || present_in_scoring(nx, ny, pid, score_cols)) continue;
                    // if (!is_inside_board(nx, ny) || present_in_scoring(nx, ny, current_opponent_side, score_cols)) continue;
                    
                    
                    
                    const auto& next_cell = board[ny][nx];
                    // if (next_cell.empty()) moves.push_back({"move", {x, y}, {nx, ny}, {}, ""});
                    if (get_key(next_cell, "side") == "river") find_river_moves(moves, board, x, y, nx, ny, pid, score_cols);
                }

                for (auto const& [dx, dy] : dirs) {
                    int nx = x + dx, ny = y + dy;


                    // MANUAL CHANGE 1

                    // if (!is_inside_board(nx, ny) || present_in_scoring(nx, ny, current_opponent_side, score_cols) || present_in_scoring(nx, ny, pid, score_cols)) continue;
                    if (!is_inside_board(nx, ny) || present_in_scoring(nx, ny, current_opponent_side, score_cols)) continue;
                    
                    
                    
                    const auto& next_cell = board[ny][nx];
                    if (next_cell.empty()) moves.push_back({"move", {x, y}, {nx, ny}, {}, ""});
                }

                /*
                now for push
                we wanna check the following for the guy we wanna push
                    if i am a stone:
                        bro should not be a river
                        bro should not be in opps scoring area
                        bro in my scoring area cant help
                        pushing area must be empty 
                    if a river

                */
                for (auto const& [dx, dy] : dirs) {
                    int nx = x + dx, ny = y + dy;
                    if (
                        !is_inside_board(nx, ny) || 
                        board[ny][nx].empty() 
                    ) {
                            continue;
                        }

                    if (piece_side == "stone") {
                        int nx2 = x + 2*dx, ny2 = y + 2*dy;
                        if (
                            is_inside_board(nx2, ny2) && 
                            board[ny2][nx2].empty() && 
                            !(get_key(board[ny][nx], "owner")==current_opponent_side) && present_in_scoring(nx2, ny2, pid, score_cols) &&
                            !(get_key(board[ny][nx], "owner")==current_opponent_side) && present_in_scoring(nx2, ny2, current_opponent_side, score_cols) &&
                            !(get_key(board[ny][nx], "owner")==pid) && present_in_scoring(nx2, ny2, current_opponent_side, score_cols)
                        ){
                            moves.push_back({"push", {x, y}, {nx, ny}, {nx2, ny2}, ""});
                        }
                    } else {
                        if (get_key(board[ny][nx], "side") == "stone") {
                            string orientation = get_key(cell, "orientation");
                            int push_dx = (orientation == "horizontal") ? 1 : 0, push_dy = (orientation == "vertical") ? 1 : 0;
                            for (int dir = -1; dir <= 1; dir += 2) {
                                int cur_px = nx + push_dx * dir, cur_py = ny + push_dy * dir;
                                while(is_inside_board(cur_px, cur_py)) {
                                    if ( (get_key(board[ny][nx], "owner")==current_opponent_side) && present_in_scoring(cur_px, cur_py, pid, score_cols)) break;
                                    if ( (get_key(board[ny][nx], "owner")==current_opponent_side) && present_in_scoring(cur_px, cur_py, current_opponent_side, score_cols)) break;
                                    if ( (get_key(board[ny][nx], "owner")==pid) && present_in_scoring(cur_px, cur_py, current_opponent_side, score_cols)) break;
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


    optional<Move> find_immediate_flip_in_scoring_area(const BoardState& board, const vector<int>& score_cols) {
        for (int y = 0; y < board_rows; ++y) {
            for (int x = 0; x < board_cols; ++x) {
                if (!board[y][x].empty() && 
                    get_key(board[y][x], "owner") == this->side &&
                    get_key(board[y][x], "side") == "river" && 
                    present_in_scoring(x, y, this->side, score_cols)) 
                {
                    return Move{"flip", {x, y}, {x, y}, {}, ""};
                }
            }
        }
        return nullopt;
    }


    BoardState try_move(const BoardState& board, const Move& move, const vector<int>& score_cols) const {
        auto next_board = board;
        if (move.from.size() < 2) return next_board;
        int from_x = move.from[0], from_y = move.from[1];
        if (!is_inside_board(from_x, from_y) || next_board[from_y][from_x].empty()) return next_board;

        string owner = get_key(next_board[from_y][from_x], "owner");

        if (move.action == "move") {
            if (move.to.size() < 2) return next_board;
            int to_x = move.to[0], to_y = move.to[1];
            if (!is_inside_board(to_x, to_y)) return next_board;

            next_board[to_y][to_x] = next_board[from_y][from_x];
            next_board[from_y][from_x].clear();

            if (present_in_scoring(to_x, to_y, owner, score_cols)) {
                next_board[to_y][to_x]["side"] = "stone";
                next_board[to_y][to_x].erase("orientation");
            }

        } else if (move.action == "push") {
            if (move.to.size() < 2 || move.pushed_to.size() < 2) return next_board;
            int to_x = move.to[0], to_y = move.to[1];
            int p_x = move.pushed_to[0], p_y = move.pushed_to[1];
            if (!is_inside_board(to_x, to_y) || !is_inside_board(p_x, p_y)) return next_board;

            next_board[p_y][p_x] = next_board[to_y][to_x];
            string pushed_owner = get_key(next_board[p_y][p_x], "owner");

            if (present_in_scoring(p_x, p_y, pushed_owner, score_cols)) {
                next_board[p_y][p_x]["side"] = "stone";
                next_board[p_y][p_x].erase("orientation");
            }

            next_board[to_y][to_x] = next_board[from_y][from_x];

            if (present_in_scoring(to_x, to_y, owner, score_cols)) {
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

    string check_if_won(const BoardState& board, const vector<int>& score_cols) const {
        if (count_pieces_in_score_area(board, "circle", score_cols) >= 4) {
            // cout << "declare circle win" << endl;
            return "circle";
        }
        if (count_pieces_in_score_area(board, "square", score_cols) >= 4) {
            // cout << "declare square win" << endl;
            return "square";
        }
        return "";
    }



    optional<Move> find_direct_entry_path(const BoardState& board, const vector<int>& score_cols) {
        auto moves = get_all_moves(board, side, score_cols);
        if (moves.empty()) return nullopt;

        int scoring_row = (side == "circle") ? 2 : board_rows - 3;

        vector<int> gap_cols;
        for (int sx : score_cols) {
            if (is_inside_board(sx, scoring_row) && board[scoring_row][sx].empty()) {
                gap_cols.push_back(sx);
            }
        }

        auto dist_to_closest_gap = [&](int px, int py) {
            int min_dist = numeric_limits<int>::max();
            for (int gx : gap_cols) {
                min_dist = min(min_dist, abs(px - gx) + abs(py - scoring_row));
            }
            return min_dist;
        };

        auto is_valid_move = [&](const Move& move) -> bool {
            if (move.from.size() < 2 || move.to.size() < 2) return false;

            int fx = move.from[0], fy = move.from[1];
            int tx = move.to[0], ty = move.to[1];

            if (!is_inside_board(fx, fy) || !is_inside_board(tx, ty)) return false;

            if (board[fy][fx].empty() || get_key(board[fy][fx], "owner") != side) return false;

            auto sgn = [](int v) { return (v > 0) - (v < 0); };

            if (move.action == "move") {
                if (!board[ty][tx].empty()) return false;

                BoardState next = try_move(board, move, score_cols);
                return !next[ty][tx].empty() && get_key(next[ty][tx], "owner") == side;
            }

            if (move.action == "push") {
                // cout << "push" << endl;
                if (move.pushed_to.size() < 2) return false;
                int px = move.pushed_to[0], py = move.pushed_to[1];

                if (!is_inside_board(px, py)) return false;

                string piece_type = get_key(board[fy][fx], "type");  

                if (piece_type == "stone") {
                    int dx = tx - fx, dy = ty - fy;
                    if (abs(dx) + abs(dy) != 1) return false;

                    if (board[ty][tx].empty()) return false;

                    if (!is_inside_board(px, py)) return false;
                    if (!board[py][px].empty()) return false;

                    BoardState next = try_move(board, move, score_cols);

                    if (next[ty][tx].empty() || get_key(next[ty][tx], "owner") != side) return false;
                    if (next[py][px].empty()) return false;

                    return true;
                }


                if (piece_type == "river") {

                    if (board[ty][tx].empty() || get_key(board[ty][tx], "type") != "stone")
                        return false;

                    int dx = (px - tx == 0) ? 0 : (px - tx) / abs(px - tx);
                    int dy = (py - ty == 0) ? 0 : (py - ty) / abs(py - ty);


                    if (dx != 0 && dy != 0) return false;


                    int steps = max(abs(px - tx), abs(py - ty));
                    if (steps < 1) return false;
                    for (int i = 1; i <= steps; i++) {
                        int cx = tx + dx * i;
                        int cy = ty + dy * i;
                        if (!is_inside_board(cx, cy)) return false;
                        if (i < steps && !board[cy][cx].empty()) return false; 
                    }

                    BoardState next = try_move(board, move, score_cols);

                    if (next[ty][tx].empty() || get_key(next[ty][tx], "type") != "stone") return false;
                    if (get_key(next[ty][tx], "owner") != side) return false;


                    if (next[py][px].empty() || get_key(next[py][px], "type") != "stone") return false;



                    return true;
                }

                return false;
            }


            return false;
        };



        for (const auto& move : moves) {
            if ((move.action != "move" && move.action != "push") ||
                present_in_scoring(move.from[0], move.from[1], side, score_cols)) continue;

            if (!is_valid_move(move)) continue;

            int tx = move.to[0], ty = move.to[1];
            if (present_in_scoring(tx, ty, side, score_cols)) return move;
        }

        if (!gap_cols.empty()) {
            for (const auto& move : moves) {
                if ((move.action != "move" && move.action != "push") ||
                    present_in_scoring(move.from[0], move.from[1], side, score_cols)) continue;

                if (!is_valid_move(move)) continue;

                int old_dist = dist_to_closest_gap(move.from[0], move.from[1]);
                int new_dist = dist_to_closest_gap(move.to[0], move.to[1]);
                if (new_dist < old_dist) return move;
            }
        }
        // cout << "returning nullopt from enter scoring area" << endl;
        return nullopt;
    }

    double evaluate_position(const BoardState& board, const string& pid, const vector<int>& score_cols) const {
        int my_stones = count_pieces_in_score_area(board, pid, score_cols);
        int opp_stones = count_pieces_in_score_area(board, (pid == "circle") ? "square" : "circle", score_cols);
        int my_near = count_pieces_near_score_area(board, pid, score_cols);
        int opp_near = count_pieces_near_score_area(board, (pid == "circle") ? "square" : "circle", score_cols);
        double score = (my_stones * 10.0 + my_near * 2.0) - (opp_stones * 10.0 + opp_near * 2.0);
        return 0.5 + (score / 100.0);
    }


    int count_pieces_in_score_area(const BoardState& board, const string& pid, const vector<int>& score_cols) const {
        int count = 0;
        for (int y = 0; y < board_rows; ++y) {
            for (int x = 0; x < board_cols; ++x) {
                if (!board[y][x].empty() && 
                    get_key(board[y][x], "owner") == pid && 
                    get_key(board[y][x], "side") == "stone" &&
                    present_in_scoring(x, y, pid, score_cols)) {
                    count++;
                }
            }
        }
        return count;
    }

    int count_pieces_near_score_area(const BoardState& board, const string& pid, const vector<int>& score_cols) const {
        int count = 0;
        int target_y = (pid == "circle") ? 3 : board_rows - 4;
        for (int x : score_cols) {
            if (is_inside_board(x, target_y) && !board[target_y][x].empty() && 
                get_key(board[target_y][x], "owner") == pid) {
                count++;
            }
        }
        return count;
    }


};

StudentAgent::StudentAgent(string s) : side(move(s)), gen(rd()) {
    opponent_side = (side == "circle") ? "square" : "circle";
}


void print_board(const BoardState& board) {
    for (int y = 0; y < board.size(); ++y) {
        for (int x = 0; x < board[0].size(); ++x) {
            if (board[y][x].empty()) {
                // cout << ".(" << y << "," << x << ") ";
            } else {
                string owner = get_key(board[y][x], "owner");
                string type = get_key(board[y][x], "type");
                string orientation = get_key(board[y][x], "orientation");
                // cout << owner << endl;
                if (owner == "circle") {
                    // cout << "C"<< orientation << "(" << y << "," << x << ") ";
                } else {
                    // cout << "S"<< orientation << "(" << y << "," << x << ") ";
                }
            }
        }
        // cout << endl;
    }
}


int x;
Move StudentAgent::choose(const BoardState& board, int, int, const vector<int>& score_cols, float, float) {
    x=0;
    turn_count++;
    board_rows = board.size();
    if (board.empty()) return {};
    board_cols = board[0].size();

    if (turn_count <= 12) {
        // cout<<"opening"<<endl;
        return get_opening_move();
    }
    if (auto flip_move = find_immediate_flip_in_scoring_area(board, score_cols)) {
        // cout<<"flip"<<endl;

        return *flip_move;
    }
    if (optional<Move> enter_move = find_direct_entry_path(board, score_cols)) {
        // cout << "from " << (*enter_move).from[0] << "," << (*enter_move).from[1] << endl;
        // cout << "to " << (*enter_move).to[0] << "," << (*enter_move).to[1] << endl;
        // cout << "pushed to " << (*enter_move).pushed_to[0] << "," << (*enter_move).pushed_to[1] << endl;
        // cout << "ori " << (*enter_move).orientation << endl;
        // cout<<"enter_move"<<endl;

        return *enter_move;
    }
    if (auto win_move = find_immediate_win(board, score_cols)) {
                // cout<<"win_move"<<endl;

        return *win_move;
    }
    if (auto block_move = block_opponent_win(board, score_cols)) {
                // cout<<"block_opp"<<endl;

        return *block_move;
    }

    // cout<<"mcts"<<endl;
    // print_board(board);
    Move m_Ret = find_mcts_move(board, score_cols);
    // cout << "MCTS chose: " << m_Ret.action << " from (" << m_Ret.from[1] << "," << m_Ret.from[0] << ") to (" << m_Ret.to[1] << "," << m_Ret.to[0] << ") pushed_to (" << (m_Ret.pushed_to.empty() ? -1 : m_Ret.pushed_to[1]) << "," << (m_Ret.pushed_to.empty() ? -1 : m_Ret.pushed_to[0]) << ") orientation " << m_Ret.orientation << endl;
    return m_Ret;
}

//  MANUAL CHANGE 4

Move StudentAgent::get_opening_move() {
    if (side == "circle") {
        if (turn_count == 1) return {"push", {3, 8}, {3, 9}, {3,10}, ""}; 
        if (turn_count == 2) return {"push", {3, 9}, {3, 10}, {3,11}, ""};
        if (turn_count == 3) return {"push", {8, 8}, {8, 9}, {8,10}, ""};
        if (turn_count == 4) return {"push", {8, 9}, {8, 10}, {8,11}, ""};  
        if (turn_count == 5) return {"flip", {3, 10}, {3, 10}, {}, "vertical"}; 
        if (turn_count == 6) return {"flip", {8, 10}, {8, 10}, {}, "vertical"};
        if (turn_count == 7) return {"flip", {4, 9}, {4, 9}, {}, "horizontal"}; 
        if (turn_count == 8) return {"flip", {5, 9}, {5, 9}, {}, "horizontal"};
        if (turn_count == 9) return {"flip", {6, 9}, {6, 9}, {}, "horizontal"}; 
        if (turn_count == 10) return {"flip", {7, 9}, {7, 9}, {}, "horizontal"};
        if (turn_count == 11) return {"flip", {3, 11}, {3, 11}, {}, "vertical"}; 
        if (turn_count == 12) return {"flip", {8, 11}, {8, 11}, {}, "vertical"};
    } else {
        if (turn_count == 1) return {"push", {3, 4}, {3, 3}, {3, 2}, ""};
        if (turn_count == 2) return {"push", {3, 3}, {3, 2}, {3, 1}, ""};
        if (turn_count == 3) return {"push", {8, 4}, {8, 3}, {8, 2}, ""};
        if (turn_count == 4) return {"push", {8, 3}, {8, 2}, {8, 1}, ""};
        if (turn_count == 5) return {"flip", {3, 2}, {3, 2}, {}, "vertical"};
        if (turn_count == 6) return {"flip", {8, 2}, {8, 2}, {}, "vertical"};
        if (turn_count == 7) return {"flip", {4, 3}, {4, 3}, {}, "horizontal"}; 
        if (turn_count == 8) return {"flip", {5, 3}, {5, 3}, {}, "horizontal"};
        if (turn_count == 9) return {"flip", {6, 3}, {6, 3}, {}, "horizontal"}; 
        if (turn_count == 10) return {"flip", {7, 3}, {7, 3}, {}, "horizontal"};
        if (turn_count == 11) return {"flip", {3, 1}, {3, 1}, {}, "vertical"}; 
        if (turn_count == 12) return {"flip", {8, 1}, {8, 1}, {}, "vertical"};
    }
    return {};
}

optional<Move> StudentAgent::find_immediate_win(const BoardState& board, const vector<int>& score_cols) {
    auto moves = get_all_moves(board, side, score_cols);
    for (const auto& move : moves) {
        BoardState next_state = try_move(board, move, score_cols);
        if (check_if_won(next_state, score_cols) == side) {
            return move;
        }
    }
    return nullopt;
}

optional<Move> StudentAgent::block_opponent_win(const BoardState& board, const vector<int>& score_cols) {
    auto opponent_moves = get_all_moves(board, opponent_side, score_cols);
    for (const auto& opp_move : opponent_moves) {
        BoardState next_state = try_move(board, opp_move, score_cols);
        if (check_if_won(next_state, score_cols) == opponent_side) {
            vector<int> target_square = opp_move.action == "move" ? opp_move.to : opp_move.pushed_to;
            if (target_square.empty()) continue;

            auto my_moves = get_all_moves(board, side, score_cols);
            for (const auto& my_move : my_moves) {
                vector<int> my_target = my_move.action == "move" ? my_move.to : my_move.pushed_to;
                if (my_target == target_square) {
                    return my_move;
                }
            }
        }
    }
    return nullopt;
}




Move StudentAgent::find_mcts_move(const BoardState& board, const vector<int>& score_cols) {
    auto root_moves = get_all_moves(board, this->side, score_cols);
    // cout << root_moves.size() << " possible moves" << endl;
    if (root_moves.empty()) return {};
    
    auto root = make_unique<Node>();
    root->state = board;
    root->pid = this->side;
    root->untried_moves = root_moves;
    shuffle(root->untried_moves.begin(), root->untried_moves.end(), gen);
    
    string winner = check_if_won(board, score_cols);
    if (!winner.empty()) {
        root->is_terminal = true;
        root->terminal_result = winner;
    }
    
    auto start_time = chrono::steady_clock::now();
    while (chrono::duration<double>(chrono::steady_clock::now() - start_time).count() < TIME_LIMIT_SECONDS) {
        Node* leaf = mcts_select_init_node(root.get());
        
        if (leaf->is_terminal) {
            double result = (leaf->terminal_result == this->side) ? 1.0 : (leaf->terminal_result.empty() ? 0.5 : 0.0);
            backpropagate(leaf, result);
        } else {
            Node* child = mcts_expand_node(leaf, score_cols);
            if (child && child != leaf) {
                double result = simulate_playout(child, score_cols);
                backpropagate(child, result);
            } else if (!leaf->is_fully_expanded) {
                double result = simulate_playout(leaf, score_cols);
                backpropagate(leaf, result);
            }
        }
    }

    if (root->children.empty()) {
        // cout << "random move" << endl;
        return root_moves[0];
    }

    auto moves_equal = [&](const Move &a, const Move &b) -> bool {
        return a.action == b.action
            && a.orientation == b.orientation
            && a.from == b.from
            && a.to == b.to
            && a.pushed_to == b.pushed_to;
    };


    Node* best_child = nullptr;
    double best_win_rate = -1.0;

    for (const auto& child : root->children) {
        if (child->playouts > 0) {
            double win_rate = (double)child->wins / (double)child->playouts;
            if (win_rate > best_win_rate) {
                best_win_rate = win_rate;
                best_child = child.get();
            }
        }
    }
    
    if (best_child == nullptr) {
        int max_playouts = -1;
        for (const auto& child : root->children) {
            if (child->playouts > max_playouts) {
                max_playouts = child->playouts;
                best_child = child.get();
            }
        }
    }

    if (best_child != nullptr) {
        bool legal = false;
        for (const auto &rm : root_moves) {
            if (moves_equal(best_child->move, rm)) {
                legal = true;
                break;
            }
        }
        if (legal) {
            return best_child->move;
        } else {

            Node* alt_child = nullptr;
            int alt_playouts = -1;
            for (const auto& child : root->children) {
                for (const auto &rm : root_moves) {
                    if (moves_equal(child->move, rm)) {
                        if (child->playouts > alt_playouts) {
                            alt_playouts = child->playouts;
                            alt_child = child.get();
                        }
                        break;
                    }
                }
            }
            if (alt_child != nullptr) return alt_child->move;
        }
    }

    return root_moves[0];
}


string flip(string ori) {
    return (ori=="horizontal") ? "vertical" :  ori;
}


Node* StudentAgent::mcts_select_init_node(Node* root) {
    // cout << "in select node " << endl;
    Node* current = root;

    while (current->is_fully_expanded && !current->is_terminal) {
        Node* best_child = nullptr;
        double best_score = -numeric_limits<double>::infinity();

        for (const auto& child : current->children) {
            double uct_score;

            if (child->playouts == 0) {

                uct_score = numeric_limits<double>::infinity();
            } else {

                double parent_visits = static_cast<double>(max(1, current->playouts));
                double exploitation = static_cast<double>(child->wins) / static_cast<double>(child->playouts);
                double exploration = UCT_C * sqrt(log(parent_visits) / static_cast<double>(child->playouts));
                uct_score = exploitation + exploration;
            }

            if (uct_score > best_score) {
                best_score = uct_score;
                best_child = child.get();
            }
        }

        if (!best_child) break; // safety
        current = best_child;
        // cout << current->children.size() << " children" << endl;
    }
    // cout << "exit sekect bni" << endl;
    return current;
}


Node* StudentAgent::mcts_expand_node(Node* node, const vector<int>& score_cols) {
    if (node->untried_moves.empty()) {
        node->is_fully_expanded = true;
        return node;
    }
    Move move = node->untried_moves.back();
    node->untried_moves.pop_back();
    if (node->untried_moves.empty()) node->is_fully_expanded = true;
    
    BoardState new_state = try_move(node->state, move, score_cols);
    auto new_child = make_unique<Node>();
    new_child->state = new_state;
    new_child->parent = node;
    new_child->move = move;
    new_child->pid = (node->pid == "circle") ? "square" : "circle";
    
    string winner = check_if_won(new_state, score_cols);
    if (!winner.empty()) {
        new_child->is_terminal = true;
        new_child->terminal_result = winner;
    } else {
        new_child->untried_moves = get_all_moves(new_state, new_child->pid, score_cols);
        if (new_child->untried_moves.empty()) new_child->is_terminal = true;
    }
    
    Node* child_ptr = new_child.get();
    node->children.push_back(std::move(new_child));
    return child_ptr;
}

Move StudentAgent::find_playout_move(
    const vector<Move>& moves, const BoardState& board, 
    const string& pid, const vector<int>& score_cols) 
{
    if (moves.empty()) return {};

    vector<Move> scoring_moves, distance_reducing_moves, gap_moves;

    auto get_closest_score_dist = [&](int px, int py) {
        int min_dist = numeric_limits<int>::max();
        int scoring_row_for_current_player = (pid == "circle") ? 2 : board_rows - 3;
        for (int sx : score_cols) {
            min_dist = min(min_dist, abs(px - sx) + abs(py - scoring_row_for_current_player));
        }
        return min_dist;
    };

    int scoring_row_for_current_player = (pid == "circle") ? 2 : board_rows - 3;
    vector<int> gap_cols;
    for (int sx : score_cols) {
        if (is_inside_board(sx, scoring_row_for_current_player) && board[scoring_row_for_current_player][sx].empty()) {
            gap_cols.push_back(sx);
        }
    }

    auto get_closest_gap_dist = [&](int px, int py) {
        int min_dist = numeric_limits<int>::max();
        for (int gx : gap_cols) {
            min_dist = min(min_dist, abs(px - gx) + abs(py - scoring_row_for_current_player));
        }
        return min_dist;
    };

    for (const auto& move : moves) {
        if (move.action == "move" || move.action == "push") {
            if (move.from.empty() || move.to.empty()) continue;
            vector<int> start = move.from;
            vector<int> target = move.to;


            if (present_in_scoring(target[0], target[1], pid, score_cols)) {
                scoring_moves.push_back(move);
                continue;
            }


            int old_dist = get_closest_score_dist(start[0], start[1]);
            int new_dist = get_closest_score_dist(target[0], target[1]);
            if (new_dist < old_dist) {
                distance_reducing_moves.push_back(move);
                continue;
            }


            if (!gap_cols.empty()) {
                int old_gap_dist = get_closest_gap_dist(start[0], start[1]);
                int new_gap_dist = get_closest_gap_dist(target[0], target[1]);
                if (new_gap_dist < old_gap_dist) {
                    gap_moves.push_back(move);
                }
            }
        }
    }

    if (!scoring_moves.empty()) return scoring_moves[gen() % scoring_moves.size()];
    if (!distance_reducing_moves.empty()) return distance_reducing_moves[gen() % distance_reducing_moves.size()];
    if (!gap_moves.empty()) return gap_moves[gen() % gap_moves.size()];


    return moves[gen() % moves.size()];
}


double StudentAgent::simulate_playout(Node* node, const vector<int>& score_cols) {
    if (node->is_terminal) {
        if (node->terminal_result == this->side) return 1.0;
        if (node->terminal_result.empty()) return 0.5;
        return 0.0;
    }
    BoardState current_state = node->state;
    string current_player = node->pid;
    int moves_limit = 30;
    while (moves_limit-- > 0) {
        string winner = check_if_won(current_state, score_cols);
        if (!winner.empty()) return (winner == this->side) ? 1.0 : 0.0;
        auto moves = get_all_moves(current_state, current_player, score_cols);
        if (moves.empty()) return 0.5;
        Move move_to_play = find_playout_move(moves, current_state, current_player, score_cols);
        current_state = try_move(current_state, move_to_play, score_cols);
        current_player = (current_player == "circle") ? "square" : "circle";
    }
    return evaluate_position(current_state, this->side, score_cols);
}

void StudentAgent::backpropagate(Node* node, double result) {
    Node* current = node;
    while (current != nullptr) {
        current->playouts++;
        if (current->parent != nullptr) {
            if (current->parent->pid != this->side) current->wins += (1.0 - result);
            else current->wins += result;
        }
        current = current->parent;
    }
}

PYBIND11_MODULE(student_agent_module, m) {
    py::class_<Move>(m, "Move")
        .def_readonly("action", &Move::action)
        .def_readonly("from_pos", &Move::from)
        .def_readonly("to_pos", &Move::to)
        .def_readonly("pushed_to", &Move::pushed_to)
        .def_readonly("orientation", &Move::orientation);
    py::class_<StudentAgent>(m, "StudentAgent")
        .def(py::init<string>())
        .def("choose", &StudentAgent::choose);
}