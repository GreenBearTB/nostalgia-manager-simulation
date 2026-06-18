#pragma once
#include <array>
#include <string>
#include <unordered_map>

namespace nm {

// The canonical list of player attributes (1-20 scale) used by the match
// engine. Kept as a list of names so that desire/execution formulas can be
// expressed in a fully data-driven config file (NOT hardcoded).
inline const std::array<std::string, 19>& AttributeNames() {
    static const std::array<std::string, 19> names = {
        // Technical
        "Passing", "Shooting", "Technique", "Dribbling", "Heading",
        // Physical
        "Pace", "Stamina", "Strength", "Jumping",
        // Tactical
        "Positioning", "OffTheBall", "Marking", "Tackling",
        // Mental
        "Creativity", "Determination", "Influence", "Aggression", "Flair",
        // Goalkeeping (only meaningful for goalkeepers)
        "Goalkeeping"};
    return names;
}

// Holds the raw 1-20 attribute values for a single player.
struct Attributes {
    std::unordered_map<std::string, int> values;

    Attributes() {
        for (const auto& n : AttributeNames()) values[n] = 10;
    }

    int get(const std::string& name) const {
        auto it = values.find(name);
        return it == values.end() ? 0 : it->second;
    }

    void set(const std::string& name, int v) {
        if (v < 1) v = 1;
        if (v > 20) v = 20;
        values[name] = v;
    }

    // Norm(stat) = stat / 20  (spec section 3)
    double norm(const std::string& name) const {
        return static_cast<double>(get(name)) / 20.0;
    }
};

}  // namespace nm
