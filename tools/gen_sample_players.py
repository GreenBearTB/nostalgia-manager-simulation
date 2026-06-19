#!/usr/bin/env python3
"""Regenerate the bundled sample PlayersDB.csv (legacy format) with realistic,
role-flavoured 1-20 attributes so the demo data plays a balanced match under the
tuned engine thresholds. Not used at runtime; kept for reproducibility."""
import os
import random

random.seed(42)
ATTRS = ["passing", "shooting", "technique", "dribbling", "heading", "pace",
         "stamina", "strength", "jumping", "positioning", "offtheball",
         "marking", "tackling", "creativity", "determination", "influence",
         "aggression", "flair", "goalkeeping"]
HEADER = ["id", "name", "teamId", "role", "number"] + ATTRS

TEAMS = list(range(1, 13))
# squad: 1 GK, 5 D, 2 DM, 4 M, 2 AM, 2 F
COMP = ["GK"] + ["D"] * 5 + ["DM"] * 2 + ["M"] * 4 + ["AM"] * 2 + ["F"] * 2
FIRST = ["Jack", "Tom", "Liam", "Owen", "Leo", "Max", "Sam", "Dan", "Joe",
         "Alex", "Ben", "Carl", "Eric", "Finn", "Hugo", "Ivan", "Karl", "Luca",
         "Marc", "Omar", "Paul", "Rory", "Theo", "Cole", "Reed", "Noah", "Zane",
         "Drew", "Niko", "Quin"]
LAST = ["Hill", "Stone", "Ward", "Brooks", "Hart", "Cross", "Lane", "Frost",
        "Vance", "Quinn", "Banks", "Reid", "Snow", "Wells", "Lowe", "Dale",
        "Knox", "Webb", "Holt", "Finch", "Boyd", "Nash", "Pike", "Pratt",
        "Doyle", "Hayes", "Tate", "Vaughn"]


def g(a, b):
    return random.randint(a, b)


def clamp(v):
    return max(1, min(20, v))


rows = []
pid = 1
for t in TEAMS:
    base = g(-1, 3)  # team quality offset
    num = 1
    for role in COMP:
        a = {k: g(14, 18) + base // 2 for k in ATTRS}
        a["determination"] = g(11, 17)
        a["stamina"] = g(12, 17)
        a["goalkeeping"] = g(2, 5)
        if role == "GK":
            a = {k: g(6, 10) for k in ATTRS}
            a["goalkeeping"] = g(13, 17) + base // 2
            a["jumping"] = g(13, 17)
            a["strength"] = g(12, 16)
            a["positioning"] = g(13, 17)
        elif role == "D":
            for k in ["tackling", "marking", "positioning", "strength",
                      "heading", "jumping"]:
                a[k] = g(14, 18) + base // 2
        elif role == "DM":
            for k in ["tackling", "passing", "stamina", "positioning",
                      "strength"]:
                a[k] = g(13, 17) + base // 2
        elif role == "M":
            for k in ["passing", "technique", "creativity", "stamina",
                      "offtheball", "dribbling"]:
                a[k] = g(14, 18) + base // 2
        elif role == "AM":
            for k in ["passing", "technique", "creativity", "dribbling",
                      "flair", "offtheball", "pace"]:
                a[k] = g(15, 19) + base // 2
        elif role == "F":
            for k in ["shooting", "technique", "pace", "offtheball",
                      "dribbling", "heading"]:
                a[k] = g(15, 19) + base // 2
        a = {k: clamp(v) for k, v in a.items()}
        name = random.choice(FIRST) + " " + random.choice(LAST)
        rows.append([str(pid), name, str(t), role, str(num)] +
                    [str(a[k]) for k in ATTRS])
        pid += 1
        num += 1

out = os.path.join(os.path.dirname(__file__), "..",
                   "NostalgiaManager", "data", "PlayersDB.csv")
with open(out, "w") as f:
    f.write(",".join(HEADER) + "\n")
    for r in rows:
        f.write(",".join(r) + "\n")
print("wrote", len(rows), "players to", os.path.normpath(out))
