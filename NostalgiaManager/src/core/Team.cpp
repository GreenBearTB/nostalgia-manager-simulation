#include "Team.h"

#include <algorithm>

namespace nm {

void Team::autoSelectXI() {
    startingXI.clear();
    std::vector<const Player*> gks, outfield;
    for (const auto& p : squad) {
        if (p.role == Role::GK)
            gks.push_back(&p);
        else
            outfield.push_back(&p);
    }
    std::sort(gks.begin(), gks.end(), [](const Player* a, const Player* b) {
        return PlayerAbility(*a) > PlayerAbility(*b);
    });
    std::sort(outfield.begin(), outfield.end(), [](const Player* a, const Player* b) {
        return PlayerAbility(*a) > PlayerAbility(*b);
    });

    if (!gks.empty()) startingXI.push_back(gks.front()->id);
    for (const Player* p : outfield) {
        if (static_cast<int>(startingXI.size()) >= 11) break;
        startingXI.push_back(p->id);
    }
}

double Team::averageAbility() const {
    if (squad.empty()) return 0.0;
    double sum = 0.0;
    for (const auto& p : squad) sum += PlayerAbility(p);
    return sum / squad.size();
}

}  // namespace nm
