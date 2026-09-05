# Design notes

Details on the solving algorithm and compile-time configuration, split out of
the main [README](../README.md#how-it-works) to keep it skimmable.

## How the solver works

| Aspect | Approach |
|---|---|
| State | Grid where every row is a permutation of 1..N; fixed cells are pinned |
| Cost function | Violated inequality constraints, plus one point per duplicate value within a column (0 = solved) |
| Neighbor move | Swap two non-fixed cells within a random row |
| Acceptance | Improving moves always accepted; worsening moves with probability $1 / (1 + e^{\Delta C / T})$ |
| Cooling schedule | Geometric: $T \leftarrow 0.999\,T$ after every $2 \cdot N_{\text{free}}$ rejected candidates |
| Stagnation / restart | After $40 \cdot N_{\text{free}}$ consecutive rejections, the search restarts from a fresh random grid (temperature and counters reset); it gives up after `MAX_RESTARTS` such restarts |

Because row permutations are maintained by construction, row uniqueness never contributes to
the cost function — only column duplicates and inequality constraints do. Restarting on
stagnation exists because column uniqueness introduces local minima that a single cooling run
can get stuck in; larger `--size` values increase both the free-cell count and the difficulty
of escaping those minima, so they may take noticeably longer (or occasionally exhaust
`MAX_RESTARTS`) compared to the default 5×5 grid.

## Configuration

Compile-time constants at the top of `futoshiki.c`:

| Constant | Default | Meaning |
|---|---|---|
| `DEFAULT_GRID_SIZE` | `5` | Grid size used unless `--size` is given |
| `MIN_GRID_SIZE` / `MAX_GRID_SIZE` | `2` / `50` | Valid range for `--size` |
| `INITIAL_TEMPERATURE` | `10.0` | Starting annealing temperature |
| `COOLING_RATE` | `0.999` | Geometric cooling factor |
| `MAX_STAGNATION_FACTOR` | `40` | Rejections (× free cells) before a restart |
| `MAX_RESTARTS` | `150` | Restarts allowed before giving up |

The grid size itself (`grid_size`) is a runtime variable set from `--size`, not a compile-time
constant — see [Options](../README.md#options) in the README.

## Project layout

```text
futoshiki-solver/
├── futoshiki.c            # complete solver (single translation unit)
├── Makefile               # build / format / lint / test / clean
├── README.md
├── LICENSE                # MIT
├── .clang-format
├── .gitignore
├── docs/DESIGN.md          # this file
├── examples/              # sample puzzles (.fut files)
├── tests/run_tests.sh     # shell test suite (make test)
└── .github/
    ├── CODEOWNERS
    └── workflows/ci.yml   # build + test on Linux/macOS, gcc/clang
```
