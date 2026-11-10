# Makes defaults.cpp (the standard levels) from the plans below.
# Jack jumps up to 3 squares (4 with the spring boots) and across gaps of 3.
import sys

HEADER = """; Jack Volt levels. Edit them in Notepad: one character for each map square.
;
;   #  wall               =  girder (solid)      -  ledge (jump up through it)
;   %  cracked block (shoot it to break it)     ^  spikes
;   R G B Y  doors: red, green, blue, yellow    r g b y  their keys
;   p q l  pipes and a lamp, just for looks
;   P  where Jack starts                        X  the exit (press Up there)
;   E walker   J hopper   F flyer   T turret   K Professor Gearhart (drops the yellow key)
;   $ gem 200    o cog 1000    h battery (+1 health)    + medkit (+4 health)
;   f fire power (one more shot at once)      j spring boots (jump higher)
;   0 empty crate   1 crate with a gem   2 with a battery   3 with fire power
;   4 with a medkit   5 with a cog
;
; TILESET and BACKDROP: 1 city, 2 factory, 3 fortress.   MUSIC: 1, 2 or 3.
; A map can be up to 128 squares wide and 64 high, and needs one P.
; Things that stand (robots, crates, the exit, Jack) go in the square they stand in.
"""

class Map:
    def __init__(s, w, h):
        s.w, s.h = w, h
        s.g = [['.'] * w for _ in range(h)]
        s.fill(0, 0, w - 1, 0, '#'); s.fill(0, h - 1, w - 1, h - 1, '#')
        s.fill(0, 0, 0, h - 1, '#'); s.fill(w - 1, 0, w - 1, h - 1, '#')
    def fill(s, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                s.g[y][x] = c
    def put(s, x, y, c):
        for i, ch in enumerate(c):
            s.g[y][x + i] = ch
    def text(s):
        return ["".join(r) for r in s.g]

levels = []

# ---------------------------------------------------------------------------
# 1. Steel City Streets: walkers, crates, a cracked wall, a red key and door
m = Map(100, 24)
m.fill(1, 22, 98, 22, '#')
m.put(3, 21, 'P')
for x in (5, 31, 47, 69, 86): m.put(x, 18, 'l')
m.put(9, 21, '1'); m.put(12, 21, '$$$')
m.fill(16, 19, 20, 19, '='); m.put(17, 18, '$$$')
m.put(23, 21, 'E')
m.fill(28, 1, 28, 19, '#'); m.put(28, 20, '%'); m.put(28, 21, '%')         # shoot through
m.put(33, 21, '^^'); m.put(33, 18, '$$')                                 # jump the spikes
m.fill(38, 19, 45, 21, '#'); m.put(42, 18, 'E')                            # a block to climb
m.fill(46, 16, 50, 16, '='); m.put(47, 15, '$')
m.fill(52, 13, 56, 13, '='); m.put(53, 12, 'r'); m.put(55, 12, 'o')        # the red key, up high
m.put(50, 21, '2'); m.put(58, 21, 'E'); m.put(62, 21, '3')
m.fill(66, 1, 66, 19, '#'); m.put(66, 20, 'R'); m.put(66, 21, 'R')         # the red door
m.put(70, 21, '0'); m.put(72, 21, '4')
m.put(78, 21, 'T')
m.fill(82, 19, 85, 19, '-'); m.put(83, 18, '$')
m.fill(87, 16, 90, 16, '-'); m.put(88, 15, '$')
m.fill(92, 13, 98, 13, '='); m.put(96, 12, 'X'); m.put(93, 12, '$')
m.put(93, 21, '$$$'); m.put(91, 21, 'E')
for y in range(2, 13): m.put(76, y, 'p')
levels.append(("Steel City Streets", 1, 1, 1, m))

# ---------------------------------------------------------------------------
# 2. The Gearworks: three floors joined by ledges; green and blue keys
m = Map(96, 36)
m.fill(1, 34, 94, 34, '#')
m.fill(1, 23, 82, 23, '=')                    # the middle floor
m.fill(12, 12, 94, 12, '=')                   # the top floor...
m.fill(86, 12, 88, 12, '.')                   # ...with a hole to drop through
# lower floor
m.put(3, 33, 'P')
m.put(10, 33, '1'); m.put(14, 33, '$$')
m.put(20, 33, 'J')
m.fill(25, 31, 28, 31, '='); m.put(25, 30, '$$$$')
m.put(32, 33, '3')
m.fill(38, 24, 38, 31, '#'); m.put(38, 32, '%'); m.put(38, 33, '%')
m.put(45, 33, 'J'); m.put(50, 33, '$$')
m.put(55, 33, '2')
m.put(58, 33, '^^')
m.put(66, 33, 'E'); m.put(72, 33, '$$$')
for y in range(24, 33): m.put(78, y, 'p')
# the right-hand ledges up to the middle floor
for y in (31, 28, 25): m.fill(84, y, 89, y, '-')
m.put(86, 30, '$'); m.put(86, 27, '$')
# the exit, behind the blue door
m.fill(91, 13, 91, 31, '#'); m.put(91, 32, 'B'); m.put(91, 33, 'B')
m.put(93, 33, 'X')
# middle floor, running left
m.put(76, 22, 'E')
m.put(70, 22, '4')
m.put(60, 19, 'F')
m.put(52, 22, '^^'); m.put(52, 19, '$$')
m.put(45, 22, 'T')
m.put(30, 19, 'F')
m.put(24, 22, '$$$'); m.put(15, 22, 'h')
m.put(2, 22, 'g')                             # the green key
for x in (20, 40, 64): m.put(x, 15, 'l')
# the left-hand ledges up to the top floor
for y in (20, 17, 14): m.fill(3, y, 8, y, '-')
m.put(5, 16, '$'); m.put(5, 13, '$')
# top floor, running right
m.put(20, 11, 'o'); m.put(24, 11, '$$')
m.put(30, 11, 'T')
m.put(40, 11, '5')
m.fill(50, 1, 50, 9, '#'); m.put(50, 10, 'G'); m.put(50, 11, 'G')
m.put(56, 11, '$$$')
m.put(60, 11, 'J'); m.put(68, 8, 'F'); m.put(75, 11, 'J')
m.put(80, 11, '1')
m.put(91, 11, 'b')                            # the blue key, past the hole
m.put(92, 11, '$')
levels.append(("The Gearworks", 2, 2, 2, m))

# ---------------------------------------------------------------------------
# 3. Skyline Towers: up and up, with the spring boots
m = Map(48, 62)
m.fill(1, 60, 46, 60, '#')
m.put(40, 59, 'P')
m.put(3, 59, 'j')                             # the boots: you need them
m.put(8, 59, '$$$'); m.put(30, 59, 'E'); m.put(15, 59, '2')
rows = [56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12]
for k, y in enumerate(rows):
    if k % 2 == 0: m.fill(21, y, 46, y, '=')
    else:          m.fill(1, y, 26, y, '=')
def on(k): return rows[k] - 1
m.put(36, on(0), '$$$'); m.put(44, on(0), '3')
m.put(8, on(1), '$$'); m.put(4, on(1), 'J')
m.put(38, on(2), 'E'); m.put(30, on(2), '$$')
m.put(3, on(3), 'T'); m.put(14, on(3), '$$$')
m.put(40, on(4), 'J'); m.put(44, on(4), 'h')
m.put(2, on(5), 'r'); m.put(10, on(5), 'E'); m.put(16, on(5), 'o')
m.put(36, on(6), '$$$'); m.put(44, on(6), 'T')
m.put(6, on(7), '4'); m.put(12, on(7), 'J')
m.put(34, on(8), 'E'); m.put(40, on(8), '$$')
m.put(4, on(9), '$$'); m.put(14, on(9), 'J'); m.put(2, on(9), '5')
m.put(30, on(10), '1'); m.put(38, on(10), 'E')
m.put(23, 42, 'F'); m.put(24, 26, 'F'); m.put(23, 14, 'F')
# the top: the red door, then the exit
m.fill(10, 1, 10, 9, '#'); m.put(10, 10, 'R'); m.put(10, 11, 'R')
m.put(4, on(11), 'X'); m.put(7, on(11), '$')
m.put(18, on(11), '$$')
for y in range(2, 11): m.put(30, y, 'p')
levels.append(("Skyline Towers", 1, 1, 1, m))

# ---------------------------------------------------------------------------
# 4. Gearhart's Fortress: everything at once, then the professor himself
m = Map(110, 30)
m.fill(1, 28, 108, 28, '#')
m.put(3, 27, 'P')
m.put(8, 27, '3'); m.put(11, 27, '$$')
m.put(15, 27, 'E')
m.fill(20, 1, 20, 25, '#'); m.put(20, 26, '%'); m.put(20, 27, '%')
m.put(24, 27, 'J'); m.put(27, 27, '2')
for (x0, y) in ((30, 25), (35, 22), (30, 19)): m.fill(x0, y, x0 + 3, y, '-')
m.put(31, 24, '$'); m.put(36, 21, '$'); m.put(31, 18, '$')
m.fill(36, 16, 70, 16, '=')                   # the high walkway
m.put(40, 15, '$$'); m.put(46, 15, 'E'); m.put(52, 15, 'T')
m.put(58, 15, '1'); m.put(62, 12, 'F')
m.put(68, 15, 'b'); m.put(66, 15, 'o')        # the blue key
m.put(38, 27, 'F'); m.put(42, 27, '$$')
m.put(45, 27, '^^')
m.put(50, 27, 'J'); m.put(54, 27, '4')
m.put(58, 27, '^^')
m.put(63, 27, 'T'); m.put(67, 27, 'E')
m.fill(72, 1, 72, 25, '#'); m.put(72, 26, '%'); m.put(72, 27, '%')
m.put(75, 27, '2')
m.fill(78, 1, 78, 25, '#'); m.put(78, 26, 'B'); m.put(78, 27, 'B')
# the arena
m.fill(83, 25, 86, 25, '-'); m.fill(93, 25, 96, 25, '-')
m.fill(88, 22, 91, 22, '-')
m.put(80, 27, '4'); m.put(89, 21, 'h')
m.put(97, 27, 'K')
m.fill(101, 1, 101, 25, '#'); m.put(101, 26, 'Y'); m.put(101, 27, 'Y')
m.put(105, 27, 'X'); m.put(103, 27, '$$')
for x in (10, 44, 84, 95): m.put(x, 10, 'l')
levels.append(("Gearhart's Fortress", 3, 3, 3, m))

# ---------------------------------------------------------------------------
lines = HEADER.rstrip("\n").split("\n") + [""]
for name, ts, bd, mu, m in levels:
    lines += ["LEVEL " + name, "TILESET %d" % ts, "BACKDROP %d" % bd, "MUSIC %d" % mu, "MAP"]
    lines += m.text()
    lines += ["END", ""]

def cstr(name, lines):
    out = ["const char %s[] =" % name]
    for l in lines:
        out.append('    "%s\\n"' % l.replace('\\', '\\\\').replace('"', '\\"'))
    out[-1] += ';'
    return "\n".join(out) + "\n"

open(sys.argv[1] if len(sys.argv) > 1 else "defaults.cpp", "w").write(
    "// defaults.cpp - the standard levels, made by gen/make_levels.py.\n"
    "// The game writes these out as levels.txt the first time it runs.\n\n"
    '#include "volt.h"\n\n' + cstr("defaultLevels", lines))
open("levels_preview.txt", "w").write("\n".join(lines))
print("ok", len(levels))
