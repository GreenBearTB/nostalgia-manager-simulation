#include "MatchEngine.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "../core/Formation.h"

namespace nm {

std::string ActionName(Action a) {
    switch (a) {
        case Action::Passing:  return "Passing";
        case Action::Longpass: return "Longpass";
        case Action::Move:     return "Move";
        case Action::Dribble:  return "Dribble";
        case Action::Longshot: return "Longshot";
        case Action::Finish:   return "Finish";
        case Action::Header:   return "Header";
    }
    return "?";
}

namespace {
std::string shirt(const Player& p) {
    return "#" + std::to_string(p.shirtNumber) + " " + p.name;
}
}  // namespace

// ---------------------------------------------------------------------------
// Setup & match loop
// ---------------------------------------------------------------------------
void MatchEngine::setup(Team& home, Team& away) {
    dir_[0] = +1;  // home attacks towards column 13
    dir_[1] = -1;  // away attacks towards column 1

    if (home.startingXI.empty()) home.autoSelectXI();
    if (away.startingXI.empty()) away.autoSelectXI();
    PlaceStartingXI(home, 1);
    PlaceStartingXI(away, 2);

    sidePlayers_[0].clear();
    sidePlayers_[1].clear();
    for (int pid : home.startingXI)
        if (Player* p = home.findPlayer(pid)) sidePlayers_[0].push_back(p);
    for (int pid : away.startingXI)
        if (Player* p = away.findPlayer(pid)) sidePlayers_[1].push_back(p);
}

void MatchEngine::kickoff(int controllingSide) {
    // Reset everyone to defensive (home) positions, ball on centre spot.
    for (int s = 0; s < 2; ++s)
        for (Player* p : sidePlayers_[s]) {
            p->pos = p->homePos;
            p->hasBall = false;
        }
    ball_ = CentreSpot();
    aerial_ = false;
    Player* nearest = nearestOpponent(ball_, controllingSide);  // nearest of controlling team
    if (nearest) giveBall(nearest, controllingSide);
}

MatchResult MatchEngine::simulate(Team& home, Team& away, bool verbose,
                                  const EventHook& hook) {
    MatchResult res;
    res.homeName = home.name;
    res.awayName = away.name;
    res_ = &res;
    verbose_ = verbose;
    hook_ = hook;

    setup(home, away);

    int firstBall = rng_.range(1, 2);  // Random(1-2) decides who starts
    int controlling = (firstBall == 1) ? 0 : 1;
    logEvent("Kickoff, " + (controlling == 0 ? home.name : away.name) +
                 " start with the ball",
             true);
    kickoff(controlling);

    runHalf(1);

    logEvent("Half time: " + home.name + " " + std::to_string(score_[0]) + " - " +
                 std::to_string(score_[1]) + " " + away.name,
             true);

    // Second half: other side kicks off.
    int secondControl = 1 - controlling;
    logEvent("Second half kickoff", true);
    kickoff(secondControl);
    runHalf(2);

    res.homeGoals = score_[0];
    res.awayGoals = score_[1];
    res.homeShots = shots_[0];
    res.awayShots = shots_[1];
    res.stats = stats_;
    logEvent("Full time: " + home.name + " " + std::to_string(score_[0]) + " - " +
                 std::to_string(score_[1]) + " " + away.name,
             true);
    res.finished = true;
    return res;
}

void MatchEngine::runHalf(int half) {
    half_ = half;
    // 45 minutes, 6 action rounds per minute.
    for (int minute = 0; minute < 45; ++minute) {
        for (int round = 0; round < 6; ++round) {
            clock_ = (half - 1) * 45.0 + minute + round / 6.0;
            resolveBallAction();
            moveOffBallPlayers();
            if (verbose_) logPlayerRound();
        }
    }
}

// ---------------------------------------------------------------------------
// Zones & allowed actions
// ---------------------------------------------------------------------------
int MatchEngine::zoneProgress(const Cell& c, int side) const {
    // Progress towards the opponent goal, expressed as 1..13.
    return (side == 0) ? c.col : (kCols + 1 - c.col);
}

std::string MatchEngine::zoneName(int progress) const {
    if (progress <= 4) return "Defensive";
    if (progress <= 9) return "Midfield";
    return "Attack";
}

std::vector<Action> MatchEngine::allowedActions(int progress) const {
    std::string z = zoneName(progress);
    if (z == "Defensive")
        return {Action::Passing, Action::Longpass, Action::Move, Action::Dribble};
    if (z == "Midfield")
        return {Action::Passing, Action::Longpass, Action::Move, Action::Dribble,
                Action::Longshot};
    return {Action::Passing, Action::Move, Action::Dribble, Action::Longshot,
            Action::Finish};
}

// ---------------------------------------------------------------------------
// Phase A: desire
// ---------------------------------------------------------------------------
double MatchEngine::desire(const Player& p, Action a, const std::string& zone,
                           bool opponentNearby) const {
    const std::string an = ActionName(a);
    double score = 0.0;
    for (const auto& stat : AttributeNames()) {
        double w = cfg_.get("desire." + an + "." + stat, 0.0);
        if (w != 0.0) score += w * p.norm(stat);
    }
    double zmod = cfg_.get("zonemod." + an + "." + zone, 1.0);
    score *= zmod;
    if (a == Action::Dribble && opponentNearby)
        score += cfg_.get("bonus.Dribble.opponentNearby", 0.0);
    // Tunable per-action multiplier (balancing knob, spec section 12).
    score *= cfg_.get("desire.scale." + an, 1.0);
    return score;
}

// ---------------------------------------------------------------------------
// Phase B: execution
// ---------------------------------------------------------------------------
double MatchEngine::execution(const Player& p, Action a) {
    const std::string an = ActionName(a);
    auto cat = [&](const char* c) {
        double v = 0.0;
        for (const auto& stat : AttributeNames()) {
            double w = cfg_.get("exec." + an + "." + std::string(c) + "." + stat, 0.0);
            if (w != 0.0) v += w * p.norm(stat);
        }
        return v;
    };
    double skill = cat("skill");
    double mental = cat("mental");
    double physical = cat("physical");
    double r = cfg_.get("exec.random", 0.15);
    double base = cfg_.get("exec.coef.skill", 0.75) * skill +
                  cfg_.get("exec.coef.mental", 0.15) * mental +
                  cfg_.get("exec.coef.physical", 0.10) * physical;
    // Tunable additive lift so realistic skill levels clear the spec thresholds
    // (balancing knob, spec section 12).
    base += cfg_.get("exec.base", 0.0);
    return base + rng_.real(-r, r);
}

double MatchEngine::pressure(const Cell& at, int defendingSide, int& defenderCount,
                             Player** best) {
    defenderCount = 0;
    double bestVal = 0.0;
    *best = nullptr;
    for (Player* d : sidePlayers_[defendingSide]) {
        if (CellDistance(d->pos, at) <= 1) {
            ++defenderCount;
            double v = 0.0;
            for (const auto& stat : AttributeNames()) {
                double w = cfg_.get("pressure." + stat, 0.0);
                if (w != 0.0) v += w * d->norm(stat);
            }
            if (v > bestVal) {
                bestVal = v;
                *best = d;
            }
        }
    }
    return bestVal;
}

std::pair<std::string, double> MatchEngine::threshold(Action a, bool pressured,
                                                      bool crowded) const {
    const std::string an = ActionName(a);
    switch (a) {
        case Action::Passing:
            return pressured ? std::make_pair(std::string("Hard"),
                                              cfg_.get("threshold.Passing.Hard"))
                             : std::make_pair(std::string("Medium"),
                                              cfg_.get("threshold.Passing.Medium"));
        case Action::Longpass:
            return {"Medium", cfg_.get("threshold.Longpass.Medium")};
        case Action::Move:
            return pressured ? std::make_pair(std::string("Pressured"),
                                              cfg_.get("threshold.Move.Pressured"))
                             : std::make_pair(std::string("Open"),
                                              cfg_.get("threshold.Move.Open"));
        case Action::Dribble:
            return crowded ? std::make_pair(std::string("Crowded"),
                                            cfg_.get("threshold.Dribble.Crowded"))
                           : std::make_pair(std::string("Normal"),
                                            cfg_.get("threshold.Dribble.Normal"));
        case Action::Longshot:
            return {"Normal", cfg_.get("threshold.Longshot.Normal")};
        case Action::Finish:
            return crowded ? std::make_pair(std::string("Tight"),
                                            cfg_.get("threshold.Finish.Tight"))
                           : std::make_pair(std::string("Good"),
                                            cfg_.get("threshold.Finish.Good"));
        case Action::Header:
            return {"Normal", cfg_.get("threshold.Header.Normal")};
    }
    return {"Medium", 0.6};
}

// ---------------------------------------------------------------------------
// The core decision: resolve the action of the player on the ball.
// ---------------------------------------------------------------------------
void MatchEngine::resolveBallAction() {
    if (!carrier_) return;
    Player& p = *carrier_;
    int side = carrierSide_;
    int defendingSide = 1 - side;

    ++stats_.possTicks[side];  // one ball-action tick of possession

    int progress = zoneProgress(ball_, side);
    std::string zone = zoneName(progress);

    int challenge = opponentsNear(ball_, defendingSide, 1);  // immediate pressure
    bool pressured = challenge > 0;
    bool crowded = challenge >= 2;
    bool opponentNearby = opponentsNear(ball_, defendingSide, 2) > 0;  // for dribble

    // Allowed actions (context rules: dribble needs an opponent; header only on
    // an aerial ball in the attacking zone).
    std::vector<Action> actions;
    for (Action a : allowedActions(progress)) {
        if (a == Action::Dribble && !opponentNearby) continue;
        actions.push_back(a);
    }
    if (aerial_ && zone == "Attack") actions.push_back(Action::Header);
    if (actions.empty()) actions.push_back(Action::Passing);

    // Phase A: weighted-random selection on desire (section 6).
    std::vector<double> weights;
    weights.reserve(actions.size());
    double total = 0.0;
    for (Action a : actions) {
        double w = std::max(0.01, desire(p, a, zone, opponentNearby));
        weights.push_back(w);
        total += w;
    }
    double pick = rng_.real(0.0, total);
    Action chosen = actions.back();
    double acc = 0.0;
    for (size_t i = 0; i < actions.size(); ++i) {
        acc += weights[i];
        if (pick <= acc) {
            chosen = actions[i];
            break;
        }
    }

    // Phase B: execution vs defensive pressure (sections 7-9).
    double exec = execution(p, chosen);
    int defCount = 0;
    Player* bestDef = nullptr;
    double pr = pressure(ball_, defendingSide, defCount, &bestDef);
    double mult = cfg_.get("pressure.multiplier", 0.35);
    double extraBonus = 0.0;
    if (defCount > 1)
        extraBonus = std::min(cfg_.get("pressure.extra.max", 0.15),
                              (defCount - 1) * cfg_.get("pressure.extra.bonus", 0.05));
    double finalScore = exec - mult * pr - extraBonus;

    auto thr = threshold(chosen, pressured, crowded);
    bool success = finalScore >= thr.second;

    // ---- Apply outcome (section 10) ----
    switch (chosen) {
        case Action::Passing:
        case Action::Longpass: {
            bool isLong = (chosen == Action::Longpass);
            ++stats_.passAtt[side];
            if (success) ++stats_.passOk[side];
            if (success) {
                Player* target = choosePassTarget(isLong);
                if (!target && isLong) target = choosePassTarget(false);
                if (!target) {  // no outlet: clear the ball upfield
                    ball_.col = clampCol(ball_.col + dir_[side] * 3);
                    p.pos = ball_;
                    aerial_ = true;
                    logEvent(shirt(p) + " clears the ball upfield to " + CellName(ball_));
                    break;
                }
                logEvent(shirt(p) + (isLong ? " plays a long ball to " : " passes to ") +
                         shirt(*target));
                aerial_ = isLong;  // long balls arrive in the air
                giveBall(target, side);
            } else {
                logEvent(shirt(p) + (isLong ? " over-hits a long ball" : " misplaces a pass") +
                         " - intercepted");
                turnover("interception");
            }
            break;
        }
        case Action::Move: {
            if (success) {
                int step = std::max(1, std::min(p.maxMovePerAction(), 2));
                ball_.col = clampCol(ball_.col + dir_[side] * step);
                if (rng_.chance(0.4)) ball_.row = clampRow(ball_.row + rng_.range(-1, 1));
                p.pos = ball_;
                aerial_ = false;
                logEvent(shirt(p) + " carries the ball forward to " + CellName(ball_));
            } else {
                logEvent(shirt(p) + " is slowed down and loses the ball");
                turnover("loss");
            }
            break;
        }
        case Action::Dribble: {
            if (success) {
                int step = std::max(1, std::min(p.maxMovePerAction(), 2));
                ball_.col = clampCol(ball_.col + dir_[side] * step);
                p.pos = ball_;
                aerial_ = false;
                logEvent(shirt(p) + " dribbles past " +
                         (bestDef ? shirt(*bestDef) : std::string("the defender")) +
                         " into " + CellName(ball_));
            } else {
                // A beaten defender concedes a foul some of the time, more often
                // the more aggressive they are (stat counter only - no free kick).
                double aggr = bestDef ? bestDef->norm("Aggression") : 0.5;
                if (rng_.chance(cfg_.get("foul.base", 0.05) +
                                cfg_.get("foul.aggression", 0.10) * aggr))
                    ++stats_.fouls[defendingSide];
                logEvent(shirt(p) + " tries to dribble past " +
                         (bestDef ? shirt(*bestDef) : std::string("the defender")) +
                         ", but loses possession");
                turnover("turnover");
            }
            break;
        }
        case Action::Longshot:
        case Action::Finish:
        case Action::Header: {
            ++shots_[side];
            ++stats_.shots[side];  // "Attempts" counter on the match screen
            onShot(chosen, finalScore, thr.second);
            break;
        }
    }
}

void MatchEngine::onShot(Action a, double finalScore, double thr) {
    int side = carrierSide_;
    int defendingSide = 1 - side;
    Player* shooter = carrier_;
    std::string kind = (a == Action::Longshot)
                           ? "lets fly from distance"
                           : (a == Action::Header ? "rises for a header" : "shoots");
    if (finalScore < thr) {
        logEvent(shirt(*shooter) + " " + kind + " but it is off target / blocked", true);
        goalKick(defendingSide);
        return;
    }
    ++stats_.onTarget[side];
    // On target -> goalkeeper save model.
    Player* gk = goalkeeper(defendingSide);
    double gkNorm = gk ? gk->norm("Goalkeeping") : 0.2;
    double margin = finalScore - thr;
    double saveChance = cfg_.get("gk.save.base", 0.30) +
                        cfg_.get("gk.save.skill", 0.45) * gkNorm - 0.20 * margin;
    saveChance = std::max(0.05, std::min(0.95, saveChance));
    if (gk && rng_.chance(saveChance)) {
        logEvent(shirt(*shooter) + " " + kind + " - SAVED by " + shirt(*gk), true);
        goalKick(defendingSide);
    } else {
        ++score_[side];
        logEvent("GOAL! " + shirt(*shooter) + " scores! (" +
                     std::to_string(score_[0]) + "-" + std::to_string(score_[1]) + ")",
                 true);
        kickoff(defendingSide);  // conceding side kicks off
    }
}

// ---------------------------------------------------------------------------
// Off-ball movement (section 5): players drift between their formation anchor
// and the ball depending on attacking/defending.
// ---------------------------------------------------------------------------
void MatchEngine::moveOffBallPlayers() {
    for (int s = 0; s < 2; ++s) {
        bool attacking = (s == carrierSide_);
        // How far the ball has advanced into this team's attacking half,
        // expressed in this side's own frame so the shape slides the right way
        // (towards the opponent goal when attacking, back when defending).
        int progress = zoneProgress(ball_, s) - 7;  // -6..+6
        // The two closest defenders also actively close down the ball.
        std::vector<std::pair<int, Player*>> byDist;
        if (!attacking)
            for (Player* p : sidePlayers_[s])
                byDist.emplace_back(CellDistance(p->pos, ball_), p);
        std::sort(byDist.begin(), byDist.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        for (Player* p : sidePlayers_[s]) {
            if (p == carrier_) continue;
            if (p->role == Role::GK) {
                // Keeper stays on its line near its own goal.
                p->pos.row = clampRow(p->homePos.row);
                p->pos.col = clampCol(p->homePos.col);
                continue;
            }

            // Team-shape target: anchor column slid by the ball advance, in the
            // team's attacking direction.
            double slide = attacking ? 1.0 : 0.6;
            int targetCol = clampCol(p->homePos.col + dir_[s] * static_cast<int>(progress * slide));
            int targetRow = p->homePos.row;

            // The nearest two defenders press the ball directly.
            bool presser = false;
            if (!attacking) {
                for (int i = 0; i < 2 && i < static_cast<int>(byDist.size()); ++i)
                    if (byDist[i].second == p) presser = true;
            }
            if (presser) {
                targetCol = ball_.col;
                targetRow = ball_.row;
            }

            int maxStep = p->maxMovePerAction();
            auto stepToward = [&](int delta) {
                if (delta > maxStep) return maxStep;
                if (delta < -maxStep) return -maxStep;
                return delta;
            };
            p->pos.row = clampRow(p->pos.row + stepToward(targetRow - p->pos.row));
            p->pos.col = clampCol(p->pos.col + stepToward(targetCol - p->pos.col));
        }
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
int MatchEngine::opponentsNear(const Cell& at, int defendingSide, int radius) const {
    int n = 0;
    for (Player* d : sidePlayers_[defendingSide])
        if (CellDistance(d->pos, at) <= radius) ++n;
    return n;
}

Player* MatchEngine::nearestOpponent(const Cell& at, int defendingSide) const {
    Player* best = nullptr;
    int bestDist = 1 << 30;
    for (Player* d : sidePlayers_[defendingSide]) {
        int dist = CellDistance(d->pos, at);
        if (dist < bestDist) {
            bestDist = dist;
            best = d;
        }
    }
    return best;
}

Player* MatchEngine::choosePassTarget(bool longPass) {
    int side = carrierSide_;
    std::vector<Player*> options;
    std::vector<double> weights;
    double total = 0.0;
    for (Player* t : sidePlayers_[side]) {
        if (t == carrier_) continue;
        int dist = CellDistance(t->pos, ball_);
        if (longPass) {
            if (dist < 4) continue;
        } else {
            if (dist < 1 || dist > 6) continue;
        }
        // Prefer teammates further forward and less crowded.
        int forward = (zoneProgress(t->pos, side) - zoneProgress(ball_, side));
        int markers = opponentsNear(t->pos, 1 - side, 1);
        double w = 1.0 + std::max(0, forward) * 0.4 - markers * 0.3;
        w = std::max(0.05, w);
        options.push_back(t);
        weights.push_back(w);
        total += w;
    }
    if (options.empty()) return nullptr;
    double pick = rng_.real(0.0, total);
    double acc = 0.0;
    for (size_t i = 0; i < options.size(); ++i) {
        acc += weights[i];
        if (pick <= acc) return options[i];
    }
    return options.back();
}

Player* MatchEngine::goalkeeper(int side) const {
    for (Player* p : sidePlayers_[side])
        if (p->role == Role::GK) return p;
    return sidePlayers_[side].empty() ? nullptr : sidePlayers_[side].front();
}

void MatchEngine::giveBall(Player* p, int side) {
    if (carrier_) carrier_->hasBall = false;
    carrier_ = p;
    carrierSide_ = side;
    p->hasBall = true;
    ball_ = p->pos;
}

void MatchEngine::turnover(const std::string& reason) {
    (void)reason;  // reason is reflected in the preceding narrated event
    int oldSide = carrierSide_;
    int newSide = 1 - carrierSide_;
    Player* loser = carrier_;
    Player* winner = nearestOpponent(ball_, newSide);
    aerial_ = false;
    if (winner) {
        winner->pos = ball_;
        giveBall(winner, newSide);
        // The dispossessed player has lost momentum: nudge them back towards
        // their own goal so the new carrier isn't instantly re-challenged at
        // point-blank range (which would cause endless ping-pong).
        if (loser) {
            loser->pos.col = clampCol(loser->pos.col - dir_[oldSide] * 2);
            loser->hasBall = false;
        }
    }
}

void MatchEngine::goalKick(int side) {
    // A goal kick / clearance launches the ball out to the midfield band so the
    // ball actually transitions away from the box (instead of being won back by
    // a camped attacker for another shot).
    aerial_ = true;
    Cell mid{4, 7};  // neutral zone, centre
    // Nudge it slightly towards the kicking team's own half so their player
    // collects it.
    mid.col = clampCol(7 - dir_[side] * 1);
    Player* receiver = nearestOpponent(mid, side);  // nearest of 'side'
    if (receiver) {
        receiver->pos = mid;
        giveBall(receiver, side);
    } else if (Player* gk = goalkeeper(side)) {
        giveBall(gk, side);
    }
}

std::string MatchEngine::renderPitch() const {
    // Build a grid; each cell holds a short token. Home players use their shirt
    // number, away players are shown in (parentheses). The ball carrier is *.
    std::string grid[kRows][kCols + 1];
    for (int r = 0; r < kRows; ++r)
        for (int c = 1; c <= kCols; ++c) grid[r][c] = " . ";

    for (int s = 0; s < 2; ++s) {
        for (Player* p : sidePlayers_[s]) {
            int r = clampRow(p->pos.row);
            int c = clampCol(p->pos.col);
            std::string num = std::to_string(p->shirtNumber);
            if (num.size() < 2) num = " " + num;
            std::string tok = (s == 0) ? (num + " ") : ("(" + num);
            if (p == carrier_) tok = (s == 0) ? (num + "*") : ("*" + num);
            grid[r][c] = tok;
        }
    }
    // Overlay ball position if empty.
    {
        int r = clampRow(ball_.row), c = clampCol(ball_.col);
        if (grid[r][c] == " . ") grid[r][c] = " o ";
    }

    std::ostringstream os;
    os << "     ";
    for (int c = 1; c <= kCols; ++c) {
        std::string h = std::to_string(c);
        if (h.size() < 2) h = " " + h;
        os << h << " ";
    }
    os << "\n";
    for (int r = 0; r < kRows; ++r) {
        os << "  " << RowLetter(r) << "  ";
        for (int c = 1; c <= kCols; ++c) os << grid[r][c];
        os << "\n";
    }
    os << "  Home: shirt number    Away: (number)    Ball carrier: *    Ball: o\n";
    return os.str();
}

void MatchEngine::logEvent(const std::string& text, bool key) {
    MatchEvent e;
    e.minute = clock_;
    e.half = half_;
    e.text = text;
    e.key = key;
    res_->events.push_back(e);
    if (hook_) hook_(e);
}

void MatchEngine::logPlayerRound() {
    std::ostringstream os;
    os << "[" << std::fixed;
    os.precision(1);
    os << clock_ << "] ";
    for (int s = 0; s < 2; ++s)
        for (Player* p : sidePlayers_[s]) {
            os << (p == carrier_ ? "*" : "") << "#" << p->shirtNumber << "@"
               << CellName(p->pos) << " ";
        }
    res_->fullLog.push_back(os.str());
}

}  // namespace nm
