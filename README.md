# Futoshiki Solver

[![CI](https://github.com/ChristosGoulas/futoshiki-solver/actions/workflows/ci.yml/badge.svg)](https://github.com/ChristosGoulas/futoshiki-solver/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A dependency-free command-line solver for [Futoshiki](https://en.wikipedia.org/wiki/Futoshiki) puzzles, written in C11.
The puzzle is solved with **simulated annealing**: the search starts from a randomized valid grid and iteratively swaps cells, accepting worsening moves with a temperature-dependent probability until every inequality constraint is satisfied.

> **Why this project?** I wanted a small, self-contained C project to practice metaheuristic search
> outside of a textbook example. Simulated annealing was chosen over backtracking to explore local-search
> tradeoffs (tuning cooling schedules, stagnation detection) rather than pure constraint propagation.

## Contents

- [Rules of Futoshiki](#rules-of-futoshiki)
- [Features](#features)
- [Getting started](#getting-started)
- [Puzzle file format](#puzzle-file-format)
- [Example session](#example-session)
- [Options](#options)
- [How it works](#how-it-works)
- [Development](#development)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

## Rules of Futoshiki

Futoshiki (不等式, "inequality") is a logic puzzle played on an N×N grid:

- Fill every cell with a digit from 1 to N.
- Each row and each column must contain every digit exactly once.
- Inequality signs (`>` / `<`) placed between cells must be respected.
- Some cells may be pre-filled ("givens") and must not change.

> **Note:** row uniqueness is enforced by construction (every row is built as a
> permutation of 1..N), while column uniqueness and the inequality constraints are
> enforced by the search itself. If the search stagnates it restarts from a fresh
> random grid (see [docs/DESIGN.md](docs/DESIGN.md)); very large `--size` values may
> take noticeably longer, or occasionally fail, to converge.

## Features

- Full Latin-square rules (row *and* column uniqueness) plus arbitrary inequality constraints
- Runtime-configurable grid size via `--size` (default 5×5, up to 50×50)
- Fixed (given) cells with full input validation
- Simulated-annealing solver with geometric cooling, stagnation detection, and automatic restarts
- Single-file, standard C11 — no dependencies beyond libc and libm

## Getting started

### Prerequisites

- A C11 compiler (`cc`, `gcc`, or `clang`)
- `make`

### Build

```sh
make
```

### Run

```sh
./futoshiki examples/puzzle1.fut           # solve from a file
./futoshiki < examples/puzzle1.fut         # or from stdin
./futoshiki --quiet examples/puzzle1.fut   # final grid only
./futoshiki --seed 42 examples/puzzle1.fut # reproducible run
./futoshiki --size 4                       # solve an unconstrained 4x4 grid
./futoshiki --help                         # all options
```

### Puzzle file format

One directive per line; blank lines and `#` comments are ignored:

```text
fixed <row> <col> <value>   a given cell
gt <r1> <c1> <r2> <c2>      cell (r1,c1) must be greater than cell (r2,c2)
```

All coordinates are 0-based, i.e. in the range `0`–`4` for the default 5×5 grid (or `0`–`N-1` when `--size N` is given). Example ([examples/puzzle1.fut](examples/puzzle1.fut)):

```text
fixed 0 4 5
gt 0 4 0 3
gt 1 0 2 0
```

The puzzle file itself doesn't encode a grid size — it's always the `--size` option (default 5).
[examples/puzzle4.fut](examples/puzzle4.fut) is a 4×4 puzzle and needs `--size 4` to solve:

```sh
./futoshiki --size 4 examples/puzzle4.fut
```

### Example session

```text
$ ./futoshiki --quiet examples/puzzle1.fut

Solved in 104 step(s) (final temperature 9.99):
+---+---+---+---+---+
| 4 | 2 | 1 | 3 | 5 |
+---+---+---+---+---+
| 3 | 4 | 1 | 5 | 2 |
+---+---+---+---+---+
| 2 | 1 | 5 | 4 | 3 |
+---+---+---+---+---+
| 4 | 5 | 2 | 3 | 1 |
+---+---+---+---+---+
| 3 | 5 | 4 | 2 | 1 |
+---+---+---+---+---+
```

Without `--quiet`, the trace shows every improving state as it is found.

### Options

| Option | Effect |
|---|---|
| `-q`, `--quiet` | Print only the final grid |
| `-s`, `--seed N` | Seed the RNG for a reproducible run |
| `-n`, `--size N` | Solve an N×N grid instead of the default 5×5 (2 ≤ N ≤ 50) |
| `-h`, `--help` | Show usage and exit |

## How it works

The solver's cost function, acceptance rule, cooling schedule, compile-time constants, and
project layout are documented in [docs/DESIGN.md](docs/DESIGN.md).

## Development

```sh
make format   # clang-format
make lint     # -Wall -Wextra -Wpedantic, clang-tidy, cppcheck
make test     # run the sample puzzles
make clean
```

## Roadmap

- [x] Read puzzles from a file / stdin instead of interactive prompts
- [x] Enforce column uniqueness in the cost function (full Latin-square rules)
- [x] Pretty grid output with `--quiet` / `--verbose` flags
- [x] Runtime-configurable grid size

## Contributing

Issues and pull requests are welcome. Please run `make format lint test` before submitting.

## License

Distributed under the MIT License. See [LICENSE](LICENSE) for details.
