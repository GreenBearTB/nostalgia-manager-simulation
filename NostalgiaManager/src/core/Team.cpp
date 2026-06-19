#include "Team.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace nm {

namespace {
// "4-4-2" -> {defenders, midfielders, forwards}; the first number is the
// defensive line, the last the forward line, everything between is midfield.
void parseFormation(const std::string& f, int& nd, int& nm, int& nf) {
    std::vector<int> parts;
    std::string num;
    for (char c : f) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            num += c;
        } else if (!num.empty()) {
            parts.push_back(std::stoi(num));
            num.clear();
        }
    }
    if (!num.empty()) parts.push_back(std::stoi(num));

    nd = 4;
    nm = 4;
    nf = 2;
    if (parts.size() >= 2) {
        nd = parts.front();
        nf = parts.back();
        nm = 0;
        for (size_t i = 1; i + 1 < parts.size(); ++i) nm += parts[i];
    }
    if (nd + nm + nf != 10) nm = std::max(0, 10 - nd - nf);
}
}  // namespace

void Team::autoSelectXI() {
    startingXI.clear();

    int nd, nm, nf;
    parseFormation(formation, nd, nm, nf);

    std::vector<const Player*> gks, defs, mids, fwds, outfield;
    for (const auto& p : squad) {
        if (p.role == Role::GK) {
            gks.push_back(&p);
            continue;
        }
        outfield.push_back(&p);
        if (p.role == Role::D)
            defs.push_back(&p);
        else if (p.role == Role::F)
            fwds.push_back(&p);
        else
            mids.push_back(&p);  // DM / M / AM
    }

    auto byAbility = [](const Player* a, const Player* b) {
        return PlayerAbility(*a) > PlayerAbility(*b);
    };
    std::sort(gks.begin(), gks.end(), byAbility);
    std::sort(defs.begin(), defs.end(), byAbility);
    std::sort(mids.begin(), mids.end(), byAbility);
    std::sort(fwds.begin(), fwds.end(), byAbility);
    std::sort(outfield.begin(), outfield.end(), byAbility);

    std::unordered_set<int> chosen;
    auto take = [&](const std::vector<const Player*>& pool, int count) {
        for (const Player* p : pool) {
            if (count <= 0) break;
            if (chosen.insert(p->id).second) {
                startingXI.push_back(p->id);
                --count;
            }
        }
    };

    if (!gks.empty()) {
        startingXI.push_back(gks.front()->id);
        chosen.insert(gks.front()->id);
    }
    take(defs, nd);
    take(mids, nm);
    take(fwds, nf);

    // Fill any leftover slots (squad imbalance) with the best available outfield.
    for (const Player* p : outfield) {
        if (static_cast<int>(startingXI.size()) >= 11) break;
        if (chosen.insert(p->id).second) startingXI.push_back(p->id);
    }
}

double Team::averageAbility() const {
    if (squad.empty()) return 0.0;
    double sum = 0.0;
    for (const auto& p : squad) sum += PlayerAbility(p);
    return sum / squad.size();
}

}  // namespace nm
