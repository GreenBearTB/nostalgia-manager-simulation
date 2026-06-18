#pragma once
#include <vector>

#include "Pitch.h"
#include "Player.h"
#include "Team.h"

namespace nm {

// Maps a role to its X column based on team side and mentality, exactly as in
// the "X Positions" table of the design doc.
inline int RoleColumn(Role role, int side /*1 or 2*/, Mentality m) {
    // [side][mentality][role index GK,D,DM,M,AM,F]
    static const int table[2][3][6] = {
        // Team 1
        {
            {1, 2, 3, 4, 5, 6},     // Defensive
            {2, 4, 5, 6, 8, 10},    // Standard
            {3, 7, 8, 9, 11, 12},   // Attack
        },
        // Team 2 (mirrored)
        {
            {13, 12, 11, 10, 9, 8}, // Defensive
            {12, 10, 9, 8, 6, 4},   // Standard
            {11, 6, 5, 4, 3, 2},    // Attack
        },
    };
    int s = (side == 2) ? 1 : 0;
    int mi = (m == Mentality::Defensive) ? 0 : (m == Mentality::Attack ? 2 : 1);
    int ri = static_cast<int>(role);
    return table[s][mi][ri];
}

// Spread players of the same role evenly across the 9 Y rows (sticking to
// Right / Center / Left bands).
inline void PlaceStartingXI(Team& team, int side) {
    // Group starting players by role.
    std::vector<Role> roleOrder = {Role::GK, Role::D, Role::DM,
                                   Role::M,  Role::AM, Role::F};
    for (Role role : roleOrder) {
        std::vector<Player*> group;
        for (int pid : team.startingXI) {
            Player* p = team.findPlayer(pid);
            if (p && p->role == role) group.push_back(p);
        }
        int col = RoleColumn(role, side, team.mentality);
        int n = static_cast<int>(group.size());
        for (int i = 0; i < n; ++i) {
            int row;
            if (role == Role::GK) {
                row = 4;  // E line
            } else if (n == 1) {
                row = 4;
            } else {
                // Distribute from row 1 (B) to row 7 (H) across the band.
                row = 1 + static_cast<int>((6.0 * i) / (n - 1) + 0.5);
            }
            group[i]->homePos = Cell{clampRow(row), clampCol(col)};
            group[i]->pos = group[i]->homePos;
            group[i]->hasBall = false;
        }
    }
}

}  // namespace nm
