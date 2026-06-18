#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace nm {

// All match-engine tuning lives here as data (spec section 12: "Keep weights
// configurable, NOT hardcoded"). Defaults are built in code and can be
// overridden from data/engine.cfg using "dotted.key = value" lines.
//
// Key conventions:
//   desire.<Action>.<Stat>        weight of a stat in an action's desire score
//   zonemod.<Action>.<Zone>       multiplier applied to desire by zone
//   exec.<Action>.<cat>.<Stat>    cat in {skill,mental,physical}
//   exec.coef.<cat>               0.75 / 0.15 / 0.10 blend (section 7)
//   exec.random                   +/- random swing (0.15)
//   threshold.<Action>.<context>  success threshold (section 8)
//   pressure.<Stat>               defensive pressure weights (section 9)
//   pressure.multiplier           0.35 (section 9)
//   pressure.extra.bonus / .max   extra-defender bonus +0.05 each, cap 0.15
class Config {
public:
    Config() { loadDefaults(); }

    bool loadFile(const std::string& path);  // override defaults
    bool saveFile(const std::string& path) const;

    double get(const std::string& key, double def = 0.0) const {
        auto it = values_.find(key);
        return it == values_.end() ? def : it->second;
    }
    void set(const std::string& key, double v) { values_[key] = v; }

    const std::unordered_map<std::string, double>& all() const { return values_; }

private:
    void loadDefaults();
    std::unordered_map<std::string, double> values_;
};

}  // namespace nm
