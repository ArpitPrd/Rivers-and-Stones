#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

bool is_valid(int x, int y){
    int board_cols = 12;
    int board_rows = 13;
    return x >= 0 && x < board_cols && y >= 0 && y < board_rows;
}

bool is_in_score_area(int x, int y, const std::string& player_side, const std::vector<int>& score_cols) {
    int board_cols = 12;
    int board_rows = 13;
    if (!is_valid(x,y) || score_cols.empty()) return false;
    bool in_score_row = (player_side == "circle") ? (y == 2) : (y == board_rows - 3);
    return in_score_row && std::find(score_cols.begin(), score_cols.end(), x) != score_cols.end();
}

int main() {
    cout << is_in_score_area(10, 5, "square", {4, 5, 6, 7}) << endl;
    return 0;
}