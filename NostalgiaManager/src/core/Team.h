#pragma once
#include <string>
#include <vector>

#include "Player.h"

namespace nm {

enum class Mentality { Defensive, Standard, Attack };

inline std::string MentalityName(Mentality m) {
    switch (m) {
        case Mentality::Defensive: return "Defensive";
        case Mentality::Attack:    return "Attack";
        default:                   return "Standard";
    }
}

struct Team {
    int id = 0;
    std::string name;
    std::string league;
    std::string formation = "4-4-2";
    Mentality mentality = Mentality::Standard;

    std::vector<Player> squad;
    std::vector<int> startingXI;  // player ids
    std::vector<int> subsUsed;    // player ids brought on

    Player* findPlayer(int playerId) {
        for (auto& p : squad)
            if (p.id == playerId) return &p;
        return nullptr;
    }

    // Best 11 by aggregate ability, always including a goalkeeper, used when no
    // explicit XI is configured.
    void autoSelectXI();

    double averageAbility() const;
};

inline double PlayerAbility(const Player& p) {
    double sum = 0.0;
    int n = 0;
    for (const auto& name : AttributeNames()) {
        if (name == "Goalkeeping" && p.role != Role::GK) continue;
        sum += p.attr.get(name);
        ++n;
    }
    return n ? sum / n : 0.0;
}

}  // namespace nm
