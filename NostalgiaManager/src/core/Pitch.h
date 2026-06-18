#pragma once
#include <string>

namespace nm {

// Pitch matrix per the design doc: rows A-I (9 rows, the Y axis) and columns
// 1-13 (the X axis). E7 is the centre spot. Team 1 defends columns 1-6 and
// attacks towards column 13; Team 2 is mirrored.
struct Cell {
    int row = 4;  // 0..8  -> A..I
    int col = 7;  // 1..13

    bool operator==(const Cell& o) const { return row == o.row && col == o.col; }
};

constexpr int kRows = 9;
constexpr int kCols = 13;

inline char RowLetter(int row) { return static_cast<char>('A' + row); }

inline std::string CellName(const Cell& c) {
    return std::string(1, RowLetter(c.row)) + std::to_string(c.col);
}

inline Cell CentreSpot() { return Cell{4, 7}; }  // E7

inline int clampRow(int r) { return r < 0 ? 0 : (r >= kRows ? kRows - 1 : r); }
inline int clampCol(int c) { return c < 1 ? 1 : (c > kCols ? kCols : c); }

// Manhattan distance between two cells (used for pressure / pass range checks).
inline int CellDistance(const Cell& a, const Cell& b) {
    int dr = a.row - b.row;
    int dc = a.col - b.col;
    if (dr < 0) dr = -dr;
    if (dc < 0) dc = -dc;
    return dr + dc;
}

}  // namespace nm
