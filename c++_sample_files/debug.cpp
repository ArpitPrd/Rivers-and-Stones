#include <iostream>
#include <vector>
#include <map>
#include <string>

// Include your agent's code directly.
// Note: You might need to adjust the path if your files are in different directories.
// For this to work, we will comment out the PyBind11 parts from the student_agent file.
#include "student_agent.cpp"

// Helper function to print the board state to the console
void print_board(const std::vector<std::vector<std::map<std::string, std::string>>>& board) {
    std::cout << "--- Current Board State ---" << std::endl;
    for (size_t y = 0; y < board.size(); ++y) {
        for (size_t x = 0; x < board[y].size(); ++x) {
            const auto& cell = board[y][x];
            if (cell.empty()) {
                std::cout << ". ";
            } else {
                std::cout << get_key(cell, "owner")[0] << (get_key(cell, "side") == "stone" ? 'S' : 'R') << " ";
            }
        }
        std::cout << std::endl;
    }
    std::cout << "--------------------------" << std::endl;
}

// Helper to print a move
void print_move(const Move& move) {
    std::cout << "Action: " << move.action
              << ", From: (" << move.from[0] << "," << move.from[1] << ")"
              << ", To: (" << move.to[0] << "," << move.to[1] << ")";
    if (!move.pushed_to.empty()) {
        std::cout << ", Pushed To: (" << move.pushed_to[0] << "," << move.pushed_to[1] << ")";
    }
    if (!move.orientation.empty()) {
        std::cout << ", Orientation: " << move.orientation;
    }
    std::cout << std::endl;
}


int main() {
    std::cout << "--- Running Standalone C++ Agent Debugger ---" << std::endl;

    // 1. Create a StudentAgent instance
    StudentAgent agent("circle");

    // 2. Set up a sample board state and game parameters
    int rows = 13;
    int cols = 12;
    std::vector<std::vector<std::map<std::string, std::string>>> board(rows, std::vector<std::map<std::string, std::string>>(cols));
    std::vector<int> score_cols = {4, 5, 6, 7}; // Example score columns

    // Add some pieces for testing
    board[8][5] = {{"owner", "circle"}, {"side", "stone"}};
    board[8][6] = {{"owner", "circle"}, {"side", "river"}, {"orientation", "horizontal"}};
    board[4][5] = {{"owner", "square"}, {"side", "stone"}};

    print_board(board);

    // 3. Test the heuristic function
    std::cout << "\n--- Testing heuristic() ---" << std::endl;
    float score = agent.heuristic(board, score_cols);
    std::cout << "Initial Heuristic Score: " << score << std::endl;

    // 4. Test the move generation function
    std::cout << "\n--- Testing generate_all_moves() ---" << std::endl;
    auto moves = agent.generate_all_moves(board, "circle", score_cols);
    std::cout << "Generated " << moves.size() << " moves for circle player." << std::endl;
    for(size_t i = 0; i < 5 && i < moves.size(); ++i) { // Print first 5 moves
        std::cout << "  Move " << i+1 << ": ";
        print_move(moves[i]);
    }

    // 5. Test the apply_move function
    if (!moves.empty()) {
        std::cout << "\n--- Testing apply_move() ---" << std::endl;
        Move move_to_apply = moves[0];
        std::cout << "Applying move: ";
        print_move(move_to_apply);
        auto next_board = agent.apply_move(board, move_to_apply);
        print_board(next_board);

        // Score the new board
        float new_score = agent.heuristic(next_board, score_cols);
        std::cout << "Heuristic score after move: " << new_score << std::endl;
    }

    // 6. Test the main choose function (which uses minimax)
    std::cout << "\n--- Testing choose() ---" << std::endl;
    Move best_move = agent.choose(board, rows, cols, score_cols, 60.0, 60.0);
    std::cout << "Minimax chose the following move: " << std::endl;
    print_move(best_move);

    std::cout << "\n--- Debugging session finished ---" << std::endl;

    return 0;
}
