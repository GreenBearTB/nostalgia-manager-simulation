#pragma once
#include <string>

#include "Attributes.h"
#include "Pitch.h"

namespace nm {

// Role on the team sheet. Drives the default X column in a formation.
enum class Role { GK, D, DM, M, AM, F };

inline std::string RoleName(Role r) {
    switch (r) {
        case Role::GK: return "GK";
        case Role::D:  return "D";
        case Role::DM: return "DM";
        case Role::M:  return "M";
        case Role::AM: return "AM";
        case Role::F:  return "F";
    }
    return "?";
}

inline Role RoleFromString(const std::string& s) {
    if (s == "GK") return Role::GK;
    if (s == "DM") return Role::DM;
    if (s == "M")  return Role::M;
    if (s == "AM") return Role::AM;
    if (s == "F")  return Role::F;
    return Role::D;
}

struct Player {
    int id = 0;
    std::string name;
    int teamId = 0;
    Role role = Role::M;
    int shirtNumber = 0;
    Attributes attr;

    // Live match state (reset each match).
    Cell pos = CentreSpot();
    Cell homePos = CentreSpot();  // formation anchor for this match
    bool hasBall = false;

    double norm(const std::string& stat) const { return attr.norm(stat); }

    // Maximum fields a player may move per action = 1 + pace/10 (spec section 5),
    // returned as an integer field count (1.1 -> 1, 2.0 -> 2, 3.0 -> 3).
    int maxMovePerAction() const {
        double pace = static_cast<double>(attr.get("Pace"));
        return static_cast<int>(1.0 + pace / 10.0);
    }
};

}  // namespace nm
