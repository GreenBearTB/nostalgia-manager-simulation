# Nostalgia Manager Simulation

A console-based football (soccer) management game written in C++17, implementing
the **GBNFM match engine** specification. You pick a club, set your tactics, and
the action-by-action match engine simulates the game on a 9×13 pitch grid.

The project ships as a **Visual Studio solution** (`NostalgiaManager.sln`) and
also builds with a portable `Makefile` (g++/clang).

## Menu flow

Modelled on the design flow chart (`Flow.drawio`):

```
Start menu
 ├── Friendly match ──► team select ──► opponent select ──► tactics ──► Match Engine
 ├── Career game ─────► choose club ──► league season (game day ► tactics ► match) ──► save
 ├── Load game
 ├── Edit database ───► search players / teams
 └── Exit
```

## Building

### Visual Studio (Windows)
1. Open `NostalgiaManager.sln`.
2. Select the `Release` / `x64` configuration.
3. Build & run (`Ctrl+F5`). The working directory is the project folder, so the
   `data/` files are found automatically.

### Command line (Linux / macOS)
```sh
make            # builds build/NostalgiaManager
make run        # builds and runs with the bundled data
```

### Useful flags
```sh
./build/NostalgiaManager <dataDir>              # run the game
./build/NostalgiaManager <dataDir> --dump-config  # write the engine weights to data/engine.cfg
./build/NostalgiaManager <dataDir> --trace        # simulate one match, print full commentary
./build/NostalgiaManager <dataDir> --bench 100    # simulate N matches, print avg goals/shots
```

## The match engine (GBNFM)

Each match is two 45-minute halves, **6 action rounds per minute**. Every round
the player on the ball acts first, then the other 21 players move. Each decision
has two phases:

* **Phase A – Action Desire** — among the actions allowed in the current zone
  (Defensive / Midfield / Attacking), a weighted-random choice is made using the
  player's attributes (`desire.*` weights).
* **Phase B – Execution** — `ExecutionScore = 0.75·Skill + 0.15·Mental +
  0.10·Physical + Random`, reduced by **defensive pressure**
  (`FinalScore = Execution − 0.35·Pressure`), then compared to the action's
  threshold to decide success or failure.

Outcomes (pass / dribble / move / shot) update the ball, possession and score,
and every ball action is narrated into the match commentary.

### Everything is configurable

Per the spec ("keep weights configurable, NOT hardcoded"), all desire weights,
execution weights, thresholds, pressure weights and balancing knobs live in
[`NostalgiaManager/data/engine.cfg`](NostalgiaManager/data/engine.cfg) (built-in
defaults are used if the file is absent). Tuning examples:

* Too many shots → raise `threshold.Finish.*` or lower `desire.scale.Finish`.
* Too many goals → raise `gk.save.*` or the finish thresholds.
* Too many failed passes → lower `threshold.Passing.*` or raise `exec.base`.

## Data files

* `data/TeamsDB.csv` — `id,name,league,formation,mentality`
* `data/PlayersDB.csv` — `id,name,teamId,role,number,<19 attributes 1–20>`

The bundled data has 2 leagues × 6 clubs with full squads. Replace these CSVs to
use your own database (the in-game **Edit database** screen lets you search it).

## Project layout

```
NostalgiaManager/
  src/
    core/    Player, Team, Formation, Pitch, Attributes
    data/    CSV loader + Database
    engine/  Config (weights), Rng, MatchEngine (GBNFM)
    ui/      Console helpers + Game (menu flow, career, save/load)
    main.cpp
  data/      TeamsDB.csv, PlayersDB.csv, engine.cfg
  saves/     career save files (*.nms)
```

## Status

* **Working:** full GBNFM match engine, friendly matches with live commentary &
  pitch view, league career mode (fixtures, table, save/load), tactics &
  substitutions, database search.
* **Skeleton / next:** the transfer market screen is a stub; player editing in
  "Edit database" is read-only search for now.
