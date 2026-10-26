# space-mining-calc

A calculator for the **Space Elevator mining module** in GregTech: New Horizons. It finds the
module settings that mine a chosen resource best. The module picks a random asteroid on every
operation, so choosing distance, drones, plasma and overdrive is an optimization problem.

> **Status:** asteroid data is extracted from the mod sources. The first piece of math picks the
> distance for chosen asteroids, in a simplified model (see [Command line](#command-line)).

## Plan

The math comes in two stages:

1. **Brute force.** Enumerate every combination of distance, drone set, plasma tier and
   overdrive, and evaluate the expected yield of each. The search space is small (about 301
   distances × 2¹³ drone sets × 5 plasmas), so this gives exact answers and a reference to
   test against.
2. **Something smarter.** Solve the same problem with optimization methods, for example
   Dinkelbach's method for the drone set and a closed form for overdrive, and check the
   results against stage 1.

## How the module works

Read from `TileEntityModuleMiner.java` and `SpaceMiningRecipes.java` in
[GT5-Unofficial](../gt5unofficial). Not verified in game.

**Recipes.** Each asteroid becomes one recipe per drone tier from `droneTiers[0]` to
`droneTiers[1]`. With `k` = drone tier index − start index:

| Value | For drone tier k |
| --- | --- |
| sizes | `minSize + 2^k − 1`, `maxSize + 2^k − 1` |
| duration | `ceil(duration / √(k+1))` ticks |
| EU/t | `ceil(eut · √(k+1))` if `scaleEUt`, else `eut` |
| inputs | the drone (not consumed), 4 drill tips and 4 rods of the tier material |

**One operation.**

1. Candidates are the recipes whose drone, drills and rods are in the input buses, with
   `minModuleTier` ≤ module tier, `minDistance` ≤ distance ≤ `maxDistance`, and EU/t ≤ the
   module's voltage. The module has no energy input of its own: the voltage is set by its tier,
   UV / UHV / UEV (524 288 / 2 097 152 / 8 388 608 EU/t) for T1 / T2 / T3.
2. One candidate is picked at random, with probability proportional to `weight`. An asteroid
   reachable with two supplied drone tiers counts twice.
3. Parallels = min(setting, module max of 2 / 4 / 8 for T1 / T2 / T3, plasma / use per
   parallel, computation / asteroid computation, inputs).
4. `maxSize · P` draws are made. The first `minSize · P` always happen; each further one
   happens with the bonus chance `min((T/6)³ · (2 − overdrive), 0.75)`, where T is the plasma
   tier 1–5. The game keeps the chance as a whole number out of 10000, rounded down. A draw
   yields output j with probability `chance_j / Σ chance` (the first output gets 1 more, the
   last 1 less, out of Σ chance); the sum is not always 10000.
5. Time = `duration · max((1 − 0.1 · (T − 1)) / overdrive, 0.5)` ticks, rounded down. An
   overdrive of 0 passes the game's check but gives an infinite time.
6. Plasma per parallel: 825 / 550 / 375 / 250 / 150 mB of helium / bismuth / radon /
   technetium / plutonium-241 plasma.

**Long-run rate.** The asteroid is drawn anew every operation. So the yield of output j per
unit of time is a ratio of weighted sums, not a sum of ratios:

```
R_j = Σ_r w_r · E_j(r) / Σ_r w_r · t_r
```

`E_j(r)` is the expected amount of j per operation with recipe r, and `t_r` its time.

## Command line

```sh
smc                                  # summary of the data
smc --list                           # every asteroid with its outputs
smc --show coalAsteroid tinAsteroid  # the named asteroids
smc --at 50                          # the asteroids reachable at distance 50
smc --best-distance saltAsteroid --drones LV MV  # the best distances with these drones
smc --drone-list                     # the drone tier names
```

`--best-distance` prints every distance in the targets' common range where they come up most
often per tick, as ranges (`201-250`, or `1-3, 7-10`), and the drone tiers that reach all the
targets.

With `--drones`, the module may pick the recipes of these drone tiers: an asteroid that two of
them reach is drawn twice, each time with its own duration. With the drones fixed and every
target reachable, the best distances are where Σ weight · time of the candidate recipes is
least (see the long-run rate above). The module is taken as T3, which in the current data
filters nothing but by the drones. Tier names are case-insensitive.

Without `--drones` the answer is a rough estimate: each asteroid counts once with its base
duration, which no real drone set gives. For salt it says 1, while with an LV drone alone the
best is 201-250.

Exit codes: 0 done, 1 the command failed (unknown asteroid, unreadable data, targets the drones
do not mine), 2 a bad command line.

## Data

[data/asteroids.json](data/asteroids.json) holds every asteroid as `addAsteroids()` passes it to
`addRecipesToDrones`, before the per drone tier expansion above. It is generated, so do not edit
it by hand:

```sh
tools/extract_asteroids.py            # reads ../gt5unofficial, writes data/asteroids.json
```

The script understands only the expression forms the source uses today. If an update brings a
new one, the script stops and names it.

The build embeds the file into the programs, so the desktop and the browser read the same data
the same way; editing it rebuilds one generated source. The command line tool can read another
file instead: `smc --data path/to/asteroids.json`.

| Field | Meaning |
| --- | --- |
| `name` | Asteroid id in the source, e.g. `coalAsteroid` |
| `outputs[]` | `name` (material or item id), `prefix` (ore prefix, for ore outputs), `amount` per draw, `chance` out of 10000 |
| `minSize`, `maxSize` | Draws per parallel, before drone tier scaling |
| `minDistance`, `maxDistance` | Distance range, inclusive |
| `computation` | Computation per second, per parallel |
| `minModuleTier` | Lowest mining module tier (1–3) |
| `duration` | Ticks, before drone tier scaling |
| `eut` | EU/t, before drone tier scaling |
| `scaleEUt` | Whether EU/t grows with the drone tier: true for ore outputs, false for the item ones |
| `droneTiers` | First and last drone tier, names as in the top-level `droneTiers` list |
| `weight` | Recipe weight for the random pick |
| `requiresMod` | Present when the asteroid exists only with that mod |

The top-level `source.gt5u` records the GT5U version the data comes from. The game server may
run a different version: check before trusting the numbers.

## Build

CMake ≥ 3.30, Ninja and a compiler with C++26 support. Dependencies come through
[CPM](https://github.com/cpm-cmake/CPM.cmake), SDL3 included; the desktop UI also needs OpenGL and
the development files SDL builds against (X11 and/or Wayland).

Build trees go to `build/<platform>/<preset>`, installs to `install/<platform>/<preset>`.
Native presets are named after their platform (`linux_*`) and show up only on that host.

```sh
cmake --list-presets             # the presets for this host
cmake --preset linux_clang_debug
cmake --build --preset linux_clang_debug
./build/linux/linux_clang_debug/smc                       # command line tool
./build/linux/linux_clang_debug/ui/space_mining_calc_ui   # desktop UI
```

### Tests

Unit tests use [GoogleTest](https://github.com/google/googletest) and build natively, on by
default (`-DSMC_BUILD_TESTS=OFF` turns them off). Every native preset has a test preset:

```sh
ctest --preset linux_clang_debug
./build/linux/linux_clang_debug/tests/smc_tests --gtest_also_run_disabled_tests
```

Tests named `DISABLED_` describe behaviour that is wanted but not written yet.

### Browser (WebAssembly)

Needs [Emscripten](https://emscripten.org). Point `EMSCRIPTEN_ROOT` at its directory:
`/usr/lib/emscripten` for the Arch package, `<emsdk>/upstream/emscripten` with emsdk.

```sh
export EMSCRIPTEN_ROOT=/usr/lib/emscripten
cmake --preset wasm_release       # or wasm_debug
cmake --build --preset wasm_release
python3 -m http.server -d build/wasm/wasm_release/ui
# open http://localhost:8000/space_mining_calc_ui.html
```

The browser build has the UI only: the command line tool needs a terminal.

## License

[Apache License 2.0](../LICENSE)
