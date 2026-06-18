#include "Config.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace nm {

void Config::loadDefaults() {
    auto& v = values_;

    // ---- Phase A: Action Desire weights (spec section 5) ----
    // Passing
    v["desire.Passing.Passing"] = 0.40;
    v["desire.Passing.Technique"] = 0.20;
    v["desire.Passing.Creativity"] = 0.15;
    v["desire.Passing.Positioning"] = 0.10;
    v["desire.Passing.OffTheBall"] = 0.10;
    v["desire.Passing.Determination"] = 0.05;
    v["zonemod.Passing.Defensive"] = 1.15;
    v["zonemod.Passing.Midfield"] = 1.00;
    v["zonemod.Passing.Attack"] = 0.90;

    // Longpass
    v["desire.Longpass.Passing"] = 0.35;
    v["desire.Longpass.Technique"] = 0.20;
    v["desire.Longpass.Creativity"] = 0.20;
    v["desire.Longpass.Flair"] = 0.10;
    v["desire.Longpass.Influence"] = 0.10;
    v["desire.Longpass.Determination"] = 0.05;

    // Move
    v["desire.Move.Pace"] = 0.30;
    v["desire.Move.OffTheBall"] = 0.20;
    v["desire.Move.Positioning"] = 0.20;
    v["desire.Move.Stamina"] = 0.15;
    v["desire.Move.Technique"] = 0.10;
    v["desire.Move.Determination"] = 0.05;

    // Dribble (+0.10 bonus when an opponent is nearby, applied in code)
    v["desire.Dribble.Dribbling"] = 0.35;
    v["desire.Dribble.Flair"] = 0.20;
    v["desire.Dribble.Technique"] = 0.15;
    v["desire.Dribble.Pace"] = 0.15;
    v["desire.Dribble.Creativity"] = 0.10;
    v["desire.Dribble.Determination"] = 0.05;
    v["bonus.Dribble.opponentNearby"] = 0.10;

    // Longshot
    v["desire.Longshot.Shooting"] = 0.35;
    v["desire.Longshot.Technique"] = 0.25;
    v["desire.Longshot.Flair"] = 0.15;
    v["desire.Longshot.Creativity"] = 0.10;
    v["desire.Longshot.Determination"] = 0.10;
    v["desire.Longshot.Influence"] = 0.05;

    // Finish (shot)
    v["desire.Finish.Shooting"] = 0.45;
    v["desire.Finish.Technique"] = 0.20;
    v["desire.Finish.OffTheBall"] = 0.10;
    v["desire.Finish.Positioning"] = 0.10;
    v["desire.Finish.Determination"] = 0.10;
    v["desire.Finish.Flair"] = 0.05;

    // Finish (header)
    v["desire.Header.Heading"] = 0.30;
    v["desire.Header.Jumping"] = 0.20;
    v["desire.Header.Strength"] = 0.20;
    v["desire.Header.Positioning"] = 0.15;
    v["desire.Header.OffTheBall"] = 0.10;
    v["desire.Header.Determination"] = 0.05;

    // ---- Phase B: Execution model (spec sections 7 & 8) ----
    v["exec.coef.skill"] = 0.75;
    v["exec.coef.mental"] = 0.15;
    v["exec.coef.physical"] = 0.10;
    v["exec.random"] = 0.15;
    // Balancing knob: additive lift to every execution score so realistic skill
    // levels clear the thresholds and matches produce sensible stat lines.
    v["exec.base"] = 0.11;

    // Balancing knob: per-action desire multipliers (lower = less frequently
    // attempted). Shots are scaled down so teams don't shoot every other action.
    v["desire.scale.Longshot"] = 0.15;
    v["desire.scale.Finish"] = 0.32;

    // Passing execution
    v["exec.Passing.skill.Passing"] = 0.55;
    v["exec.Passing.skill.Technique"] = 0.25;
    v["exec.Passing.skill.Creativity"] = 0.20;
    v["exec.Passing.mental.Positioning"] = 0.50;
    v["exec.Passing.mental.Determination"] = 0.50;
    v["exec.Passing.physical.Stamina"] = 0.50;
    v["exec.Passing.physical.Pace"] = 0.50;
    v["threshold.Passing.Easy"] = 0.50;
    v["threshold.Passing.Medium"] = 0.60;
    v["threshold.Passing.Hard"] = 0.68;

    // Longpass execution (reuse passing skill profile; own threshold)
    v["exec.Longpass.skill.Passing"] = 0.55;
    v["exec.Longpass.skill.Technique"] = 0.25;
    v["exec.Longpass.skill.Creativity"] = 0.20;
    v["exec.Longpass.mental.Positioning"] = 0.50;
    v["exec.Longpass.mental.Determination"] = 0.50;
    v["exec.Longpass.physical.Stamina"] = 0.50;
    v["exec.Longpass.physical.Pace"] = 0.50;
    v["threshold.Longpass.Medium"] = 0.66;

    // Move execution
    v["exec.Move.skill.Technique"] = 0.50;
    v["exec.Move.skill.Dribbling"] = 0.50;
    v["exec.Move.mental.Positioning"] = 0.50;
    v["exec.Move.mental.OffTheBall"] = 0.50;
    v["exec.Move.physical.Pace"] = 0.60;
    v["exec.Move.physical.Stamina"] = 0.40;
    v["threshold.Move.Open"] = 0.45;
    v["threshold.Move.Pressured"] = 0.58;

    // Dribble execution
    v["exec.Dribble.skill.Dribbling"] = 0.60;
    v["exec.Dribble.skill.Technique"] = 0.25;
    v["exec.Dribble.skill.Flair"] = 0.15;
    v["exec.Dribble.mental.Determination"] = 0.50;
    v["exec.Dribble.mental.Creativity"] = 0.50;
    v["exec.Dribble.physical.Pace"] = 0.60;
    v["exec.Dribble.physical.Strength"] = 0.40;
    v["threshold.Dribble.Normal"] = 0.60;
    v["threshold.Dribble.Crowded"] = 0.72;

    // Longshot execution
    v["exec.Longshot.skill.Shooting"] = 0.60;
    v["exec.Longshot.skill.Technique"] = 0.25;
    v["exec.Longshot.skill.Flair"] = 0.15;
    v["exec.Longshot.mental.Determination"] = 0.50;
    v["exec.Longshot.mental.Influence"] = 0.50;
    v["exec.Longshot.physical.Strength"] = 0.50;
    v["exec.Longshot.physical.Stamina"] = 0.50;
    v["threshold.Longshot.Normal"] = 0.72;

    // Finish (shot) execution
    v["exec.Finish.skill.Shooting"] = 0.60;
    v["exec.Finish.skill.Technique"] = 0.25;
    v["exec.Finish.skill.Flair"] = 0.15;
    v["exec.Finish.mental.Determination"] = 0.50;
    v["exec.Finish.mental.Positioning"] = 0.50;
    v["exec.Finish.physical.Strength"] = 0.50;
    v["exec.Finish.physical.Pace"] = 0.50;
    v["threshold.Finish.Good"] = 0.62;
    v["threshold.Finish.Tight"] = 0.74;

    // Finish (header) execution
    v["exec.Header.skill.Heading"] = 0.60;
    v["exec.Header.skill.Technique"] = 0.40;
    v["exec.Header.mental.Determination"] = 0.50;
    v["exec.Header.mental.Positioning"] = 0.50;
    v["exec.Header.physical.Jumping"] = 0.50;
    v["exec.Header.physical.Strength"] = 0.50;
    v["threshold.Header.Normal"] = 0.68;

    // ---- Defensive pressure (spec section 9) ----
    v["pressure.Tackling"] = 0.30;
    v["pressure.Marking"] = 0.25;
    v["pressure.Positioning"] = 0.20;
    v["pressure.Strength"] = 0.15;
    v["pressure.Aggression"] = 0.10;
    v["pressure.multiplier"] = 0.35;
    v["pressure.extra.bonus"] = 0.05;
    v["pressure.extra.max"] = 0.15;

    // ---- Goalkeeper save model (shot that beats outfield defenders) ----
    v["gk.save.base"] = 0.30;
    v["gk.save.skill"] = 0.45;  // weight of GK Goalkeeping in save chance
}

bool Config::loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // strip comments
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t");
            if (a == std::string::npos) return std::string();
            size_t b = s.find_last_not_of(" \t");
            return s.substr(a, b - a + 1);
        };
        key = trim(key);
        val = trim(val);
        if (key.empty() || val.empty()) continue;
        try {
            values_[key] = std::stod(val);
        } catch (...) {
        }
    }
    return true;
}

bool Config::saveFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << "# Nostalgia Manager Simulation - match engine config\n";
    out << "# Edit these to tune balance (see spec section 12).\n";
    std::vector<std::pair<std::string, double>> sorted(values_.begin(), values_.end());
    std::sort(sorted.begin(), sorted.end());
    for (const auto& kv : sorted) out << kv.first << " = " << kv.second << "\n";
    return true;
}

}  // namespace nm
