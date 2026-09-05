# Design notes

Details on the solving algorithm and compile-time configuration, split out of
the main [README](../README.md#how-it-works) to keep it skimmable.

## How the solver works

| Aspect | Approach |
|---|---|
| State | Grid where every row is a permutation of 1..N; fixed cells are pinned |
| Cost function | Number of violated inequality constraints (0 = solved) |
| Neighbor move | Swap two non-fixed cells within a random row |
| Acceptance | Improving moves always accepted; worsening moves with probability $1 / (1 + e^{\Delta C / T})$ |
| Cooling schedule | Geometric: $T \leftarrow 0.999\,T$ after every $2 \cdot N_{\text{free}}$ rejected candidates |
| Giving up | Aborts after $6 \cdot N_{\text{free}}$ consecutive rejections |

Because row permutations are maintained by construction, only the inequality constraints appear in the cost function.

## Configuration

Compile-time constants at the top of `futoshiki.c`:

| Constant | Default | Meaning |
|---|---|---|
| `GRID_SIZE` | `5` | Rows and columns of the puzzle |
| `INITIAL_TEMPERATURE` | `10.0` | Starting annealing temperature |
| `COOLING_RATE` | `0.999` | Geometric cooling factor |

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
