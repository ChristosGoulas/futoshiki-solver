/*
 * futoshiki.c -- a Futoshiki puzzle solver using simulated annealing.
 *
 * The puzzle is read from a file (or stdin), an initial randomized grid is
 * built with each row a permutation of 1..N (N is runtime-configurable via
 * --size), and the search iteratively swaps two non-fixed cells within a
 * row. The cost of a state is the number of violated inequality constraints
 * plus the number of duplicate values within any column; the search ends
 * when the cost reaches zero.
 *
 * Build:  cc -std=c11 -O2 -Wall -Wextra -Wpedantic -o futoshiki futoshiki.c -lm
 * Usage:  ./futoshiki [--quiet] [--seed N] [--size N] [puzzle-file]
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Configuration
 */

#define DEFAULT_GRID_SIZE 5 /* number of rows and columns unless --size is given */
#define MIN_GRID_SIZE 2     /* a smaller grid has no room for a swap move */
#define MAX_GRID_SIZE 50    /* sanity cap for --size to keep memory/time bounded */
#define INITIAL_TEMPERATURE 10.0f
#define COOLING_RATE 0.999f

/* Cooling is applied after this many rejected candidates, expressed in
   multiples of the number of free (non-fixed) cells. */
#define COOLING_INTERVAL_FACTOR 2

/* The search gives up after this many consecutive rejections, in the same
   multiples of the free-cell count. */
#define MAX_STAGNATION_FACTOR 40

/* Upper bound on retries while filling one row; a row that needs more
   attempts than this is almost certainly infeasible. */
#define MAX_FILL_TRIES 1000

/* If the search stagnates, restart from a fresh random grid this many times
   before giving up. */
#define MAX_RESTARTS 150

/* Rows and columns of the puzzle; set from --size, defaults to DEFAULT_GRID_SIZE. */
static int grid_size = DEFAULT_GRID_SIZE;

/*
 * Types
 */

typedef struct {
    int greater_row;
    int greater_column;
    int lesser_row;
    int lesser_column;
} Restriction;

typedef struct {
    int row;
    int column;
    int value;
} FixedCell;

/* Static puzzle data: the grid plus everything parsed from the input. */
typedef struct {
    int** grid;
    Restriction* restrictions;
    int restriction_count;
    FixedCell* fixed_cells;
    int fixed_cell_count;
    int free_cell_count;
} Puzzle;

/* Mutable state of the simulated-annealing search. */
typedef struct {
    float temperature;
    int current_cost;
    int candidate_cost;
    int rejected_since_cooling; /* candidates rejected since the last cooling */
    int stagnation_counter;     /* consecutive rejections overall */
    int steps;                  /* solver iterations performed */
    int verbose;                /* 1: print the search trace, 0: final grid only */
} Solver;

/*
 * Error handling
 */

/* Print an error message to stderr and exit. */
_Noreturn static void die(const char* message)
{
    fprintf(stderr, "Error: %s\n", message);
    exit(EXIT_FAILURE);
}

/*
 * Random helpers
 */

/* Return a random floating-point value in the range [0, 1]. */
static float random_float(void)
{
    int random_value = rand();

    return (float)random_value / (float)RAND_MAX;
}

/* Return a random column index in the range [1, grid_size]. */
static int random_column(void)
{
    return 1 + rand() % grid_size;
}

/* Return a random row index in the range [1, grid_size]. */
static int random_row(void)
{
    return 1 + rand() % grid_size;
}

/*
 * Grid management
 */

static int** grid_create(void)
{
    int** new_grid;
    int i, j;

    new_grid = malloc((size_t)grid_size * sizeof(*new_grid));
    if (new_grid == NULL) {
        die("cannot allocate the grid.");
    }
    for (i = 0; i < grid_size; i++) {
        new_grid[i] = malloc((size_t)grid_size * sizeof(*new_grid[i]));
        if (new_grid[i] == NULL) {
            die("cannot allocate a grid row.");
        }
    }
    for (i = 0; i < grid_size; i++) {
        for (j = 0; j < grid_size; j++) {
            new_grid[i][j] = 0;
        }
    }
    return new_grid;
}

static void grid_destroy(int** grid_to_free)
{
    int i;

    if (grid_to_free == NULL) {
        return;
    }
    for (i = 0; i < grid_size; i++) {
        free(grid_to_free[i]);
    }
    free(grid_to_free);
}

static void grid_copy(int** destination, int** source)
{
    int i, j;

    for (i = 0; i < grid_size; i++) {
        for (j = 0; j < grid_size; j++) {
            destination[i][j] = source[i][j];
        }
    }
}

/* Reset every cell to 0 so initialize_grid() can rebuild a fresh grid. */
static void grid_clear(int** grid_to_clear)
{
    int i, j;

    for (i = 0; i < grid_size; i++) {
        for (j = 0; j < grid_size; j++) {
            grid_to_clear[i][j] = 0;
        }
    }
}

static void grid_print(int** grid_to_print)
{
    int i, j;

    printf("+");
    for (j = 0; j < grid_size; j++) {
        printf("---+");
    }
    printf("\n");
    for (i = 0; i < grid_size; i++) {
        printf("|");
        for (j = 0; j < grid_size; j++) {
            printf(" %d |", grid_to_print[i][j]);
        }
        printf("\n+");
        for (j = 0; j < grid_size; j++) {
            printf("---+");
        }
        printf("\n");
    }
}

/*
 * Puzzle input
 */

/* Read a puzzle definition from a stream (file or stdin).

   Format, one directive per line; blank lines and '#' comments are ignored:
     fixed <row> <col> <value>   a given cell
     gt <r1> <c1> <r2> <c2>      cell (r1,c1) must be greater than cell (r2,c2)

   All coordinates are 0-based. Errors abort with the offending line number. */
static void puzzle_load(Puzzle* puzzle, FILE* fp, const char* source_name)
{
    char line[256];
    char keyword[32];
    int line_number = 0;
    int fixed_capacity = 0;
    int restriction_capacity = 0;
    int j, a, b, c, d;

    while (fgets(line, sizeof(line), fp) != NULL) {
        line_number++;

        /* Skip blank lines and comments. */
        if (sscanf(line, " %31s", keyword) != 1 || keyword[0] == '#') {
            continue;
        }

        if (strcmp(keyword, "fixed") == 0) {
            if (sscanf(line, "%*s %d %d %d", &a, &b, &c) != 3) {
                fprintf(stderr, "Error: %s:%d: expected 'fixed <row> <col> <value>'.\n",
                        source_name, line_number);
                exit(EXIT_FAILURE);
            }
            if (a < 0 || a >= grid_size || b < 0 || b >= grid_size) {
                fprintf(stderr, "Error: %s:%d: fixed cell coordinates out of range.\n", source_name,
                        line_number);
                exit(EXIT_FAILURE);
            }
            if (c < 1 || c > grid_size) {
                fprintf(stderr, "Error: %s:%d: fixed value must be in the range 1-%d.\n",
                        source_name, line_number, grid_size);
                exit(EXIT_FAILURE);
            }
            for (j = 0; j < puzzle->fixed_cell_count; j++) {
                if (puzzle->fixed_cells[j].row == a && puzzle->fixed_cells[j].column == b) {
                    fprintf(stderr, "Error: %s:%d: duplicate fixed cell.\n", source_name,
                            line_number);
                    exit(EXIT_FAILURE);
                }
                /* Duplicate givens in a row make the puzzle unsolvable: the
                   solver builds each row as a permutation of 1..grid_size. */
                if (puzzle->fixed_cells[j].row == a && puzzle->fixed_cells[j].value == c) {
                    fprintf(stderr, "Error: %s:%d: two fixed cells share the same row and value.\n",
                            source_name, line_number);
                    exit(EXIT_FAILURE);
                }
            }
            if (puzzle->fixed_cell_count == fixed_capacity) {
                fixed_capacity = fixed_capacity == 0 ? 8 : fixed_capacity * 2;
                puzzle->fixed_cells = realloc(
                    puzzle->fixed_cells, (size_t)fixed_capacity * sizeof(*puzzle->fixed_cells));
                if (puzzle->fixed_cells == NULL) {
                    die("could not grow the fixed cell storage.");
                }
            }
            puzzle->fixed_cells[puzzle->fixed_cell_count].row = a;
            puzzle->fixed_cells[puzzle->fixed_cell_count].column = b;
            puzzle->fixed_cells[puzzle->fixed_cell_count].value = c;
            puzzle->fixed_cell_count++;
        } else if (strcmp(keyword, "gt") == 0) {
            if (sscanf(line, "%*s %d %d %d %d", &a, &b, &c, &d) != 4) {
                fprintf(stderr, "Error: %s:%d: expected 'gt <r1> <c1> <r2> <c2>'.\n", source_name,
                        line_number);
                exit(EXIT_FAILURE);
            }
            if (a < 0 || a >= grid_size || b < 0 || b >= grid_size || c < 0 || c >= grid_size ||
                d < 0 || d >= grid_size) {
                fprintf(stderr, "Error: %s:%d: constraint coordinates out of range.\n", source_name,
                        line_number);
                exit(EXIT_FAILURE);
            }
            if (a == c && b == d) {
                fprintf(stderr, "Error: %s:%d: a constraint cannot compare a cell with itself.\n",
                        source_name, line_number);
                exit(EXIT_FAILURE);
            }
            if (puzzle->restriction_count == restriction_capacity) {
                restriction_capacity = restriction_capacity == 0 ? 8 : restriction_capacity * 2;
                puzzle->restrictions =
                    realloc(puzzle->restrictions,
                            (size_t)restriction_capacity * sizeof(*puzzle->restrictions));
                if (puzzle->restrictions == NULL) {
                    die("could not grow the constraint storage.");
                }
            }
            puzzle->restrictions[puzzle->restriction_count].greater_row = a;
            puzzle->restrictions[puzzle->restriction_count].greater_column = b;
            puzzle->restrictions[puzzle->restriction_count].lesser_row = c;
            puzzle->restrictions[puzzle->restriction_count].lesser_column = d;
            puzzle->restriction_count++;
        } else {
            fprintf(stderr,
                    "Error: %s:%d: unknown directive '%s' "
                    "(expected 'fixed' or 'gt').\n",
                    source_name, line_number, keyword);
            exit(EXIT_FAILURE);
        }
    }

    if (ferror(fp)) {
        fprintf(stderr, "Error: could not read %s.\n", source_name);
        exit(EXIT_FAILURE);
    }

    puzzle->free_cell_count = grid_size * grid_size - puzzle->fixed_cell_count;

    /* Shrink both arrays to their exact size (or release them when empty). */
    if (puzzle->fixed_cell_count == 0) {
        free(puzzle->fixed_cells);
        puzzle->fixed_cells = NULL;
    } else {
        puzzle->fixed_cells = realloc(puzzle->fixed_cells, (size_t)puzzle->fixed_cell_count *
                                                               sizeof(*puzzle->fixed_cells));
        if (puzzle->fixed_cells == NULL) {
            die("could not shrink the fixed cell storage.");
        }
    }
    if (puzzle->restriction_count == 0) {
        free(puzzle->restrictions);
        puzzle->restrictions = NULL;
    } else {
        puzzle->restrictions = realloc(puzzle->restrictions, (size_t)puzzle->restriction_count *
                                                                 sizeof(*puzzle->restrictions));
        if (puzzle->restrictions == NULL) {
            die("could not shrink the constraint storage.");
        }
    }
}

/*
 * Solver
 */

/* Return 1 when value already appears in the given row, 0 otherwise. */
static int row_contains(int** grid_to_search, int row, int value)
{
    int i;

    for (i = 0; i < grid_size; i++) {
        if (grid_to_search[row][i] == value) {
            return 1;
        }
    }
    return 0;
}

/* Build a valid initial grid while preserving fixed cells: every row ends
   up as a permutation of 1..grid_size. */
static void initialize_grid(Puzzle* puzzle)
{
    int i, j, value;
    int tries;

    for (i = 0; i < puzzle->fixed_cell_count; i++) {
        puzzle->grid[puzzle->fixed_cells[i].row][puzzle->fixed_cells[i].column] =
            puzzle->fixed_cells[i].value;
    }

    for (i = 0; i < grid_size; i++) {
        tries = 0;
        for (j = 0; j < grid_size; j++) {
            value = random_column(); /* [1, grid_size] */

            if (puzzle->grid[i][j] == 0) {
                if (row_contains(puzzle->grid, i, value) == 0) {
                    puzzle->grid[i][j] = value;
                } else {
                    j--; /* retry this cell */
                    if (++tries > MAX_FILL_TRIES) {
                        die("could not build an initial grid; "
                            "the fixed cells may be inconsistent.");
                    }
                }
            }
        }
    }
}

/* Number of duplicate values within columns: for each column and value,
   every occurrence past the first counts as one violation. */
static int count_column_violations(int** grid_to_score)
{
    int column, value, row, occurrences;
    int violation_count = 0;

    for (column = 0; column < grid_size; column++) {
        for (value = 1; value <= grid_size; value++) {
            occurrences = 0;
            for (row = 0; row < grid_size; row++) {
                if (grid_to_score[row][column] == value) {
                    occurrences++;
                }
            }
            if (occurrences > 1) {
                violation_count += occurrences - 1;
            }
        }
    }
    return violation_count;
}

/* Cost of a state: violated inequality constraints plus duplicate values
   within any column. Row uniqueness is maintained by construction, so it
   never contributes to the cost. */
static int count_violations(const Puzzle* puzzle, int** grid_to_score)
{
    int i;
    int violation_count = 0;

    for (i = 0; i < puzzle->restriction_count; i++) {
        if (grid_to_score[puzzle->restrictions[i].greater_row]
                         [puzzle->restrictions[i].greater_column] <=
            grid_to_score[puzzle->restrictions[i].lesser_row]
                         [puzzle->restrictions[i].lesser_column]) {
            violation_count++;
        }
    }
    violation_count += count_column_violations(grid_to_score);
    return violation_count;
}

/* Swap two non-fixed cells within one random row of the candidate grid. */
static void random_swap(const Puzzle* puzzle, int** candidate_grid)
{
    int row = 0, column1 = 0, column2 = 0, swapped_value = 0, f = 0;
    int touches_fixed = 1;

    while (column2 == column1 || touches_fixed == 1) {
        touches_fixed = 0;
        row = random_row() - 1;
        column1 = random_column() - 1;
        column2 = random_column() - 1;
        for (f = 0; f < puzzle->fixed_cell_count; f++) {
            if ((row == puzzle->fixed_cells[f].row && column1 == puzzle->fixed_cells[f].column) ||
                (row == puzzle->fixed_cells[f].row && column2 == puzzle->fixed_cells[f].column)) {
                touches_fixed = 1;
            }
        }
    }
    swapped_value = candidate_grid[row][column1];
    candidate_grid[row][column1] = candidate_grid[row][column2];
    candidate_grid[row][column2] = swapped_value;
}

/* Apply one simulated-annealing acceptance decision for the candidate.
   Returns 1 if the search has stagnated (caller should restart), 0 otherwise. */
static int simulated_annealing_step(Puzzle* puzzle, Solver* solver, int** candidate_grid)
{
    int cost_difference;
    float acceptance_probability = 0;
    int cooling_interval = COOLING_INTERVAL_FACTOR * puzzle->free_cell_count;
    int max_stagnation = MAX_STAGNATION_FACTOR * puzzle->free_cell_count;
    int** candidate_copy;

    cost_difference = solver->candidate_cost - solver->current_cost;

    candidate_copy = grid_create();
    grid_copy(candidate_copy, candidate_grid);

    if (cost_difference < 0) {
        if (solver->verbose) {
            printf("[%4d] improved: %d -> %d violated constraint(s)\n", solver->steps,
                   solver->current_cost, solver->candidate_cost);
            grid_print(puzzle->grid);
        }
        grid_copy(puzzle->grid, candidate_copy);
        solver->stagnation_counter = 0;
    } else { /* Worse candidates may still be accepted probabilistically. */
        if (solver->rejected_since_cooling > cooling_interval) {
            solver->rejected_since_cooling = 0;
            solver->temperature = COOLING_RATE * solver->temperature;
        }

        acceptance_probability = 1.0f / (1.0f + expf((float)cost_difference / solver->temperature));
        while (solver->rejected_since_cooling <= cooling_interval) {
            if (acceptance_probability > random_float()) {
                /* Any accepted move breaks the stagnation streak. */
                if (cost_difference != 0) {
                    solver->stagnation_counter = 0;
                }
                grid_copy(puzzle->grid, candidate_copy);
                break;
            }
            solver->stagnation_counter++;
            solver->rejected_since_cooling++;
            if (solver->stagnation_counter >= max_stagnation) {
                grid_destroy(candidate_copy);
                return 1;
            }
        }
    }
    grid_destroy(candidate_copy);
    return 0;
}

/* Runs the annealing search, restarting from a fresh random grid whenever it
   stagnates. Column uniqueness adds local minima that a single cooling run
   can get stuck in; restarts make the overall search robust to that. */
static void solve_futoshiki(Puzzle* puzzle, Solver* solver)
{
    int** candidate_grid;
    int restarts = 0;

    candidate_grid = grid_create();
    while (count_violations(puzzle, puzzle->grid) > 0) {
        solver->steps++;
        grid_copy(candidate_grid, puzzle->grid);
        random_swap(puzzle, candidate_grid);
        solver->current_cost = count_violations(puzzle, puzzle->grid);
        solver->candidate_cost = count_violations(puzzle, candidate_grid);
        if (simulated_annealing_step(puzzle, solver, candidate_grid)) {
            if (++restarts >= MAX_RESTARTS) {
                die("no solution found after several restarts; the puzzle may be infeasible.");
            }
            solver->temperature = INITIAL_TEMPERATURE;
            solver->stagnation_counter = 0;
            solver->rejected_since_cooling = 0;
            grid_clear(puzzle->grid);
            initialize_grid(puzzle);
        }
    }
    grid_destroy(candidate_grid);
}

/*
 * Entry point
 */

static void print_usage(const char* program_name)
{
    fprintf(stderr,
            "Usage: %s [options] [puzzle-file]\n"
            "\n"
            "Solves a Futoshiki puzzle with simulated annealing.\n"
            "Reads the puzzle from puzzle-file, or from stdin when no file is given.\n"
            "\n"
            "Options:\n"
            "  -q, --quiet       print only the final grid\n"
            "  -s, --seed SEED   seed the RNG for a reproducible run\n"
            "  -n, --size N      use an N x N grid (default %d, max %d)\n"
            "  -h, --help        show this help and exit\n"
            "\n"
            "Puzzle file format (one directive per line, '#' starts a comment):\n"
            "  fixed <row> <col> <value>   a given cell\n"
            "  gt <r1> <c1> <r2> <c2>      cell (r1,c1) must be greater than cell (r2,c2)\n"
            "All coordinates are 0-based.\n",
            program_name, DEFAULT_GRID_SIZE, MAX_GRID_SIZE);
}

int main(int argc, char* argv[])
{
    Puzzle puzzle = {0};
    Solver solver = {0};
    const char* input_path = NULL;
    unsigned int seed = 0;
    int seed_given = 0;
    int requested_size;
    int i;
    FILE* fp;

    solver.temperature = INITIAL_TEMPERATURE;
    solver.verbose = 1;

    /* Parse command-line options. */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            solver.verbose = 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--seed") == 0) {
            i++;
            if (i >= argc || sscanf(argv[i], "%u", &seed) != 1) {
                fprintf(stderr, "Error: option '%s' needs an unsigned integer seed.\n",
                        argv[i - 1]);
                return EXIT_FAILURE;
            }
            seed_given = 1;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--size") == 0) {
            i++;
            if (i >= argc || sscanf(argv[i], "%d", &requested_size) != 1) {
                fprintf(stderr, "Error: option '%s' needs an integer grid size.\n", argv[i - 1]);
                return EXIT_FAILURE;
            }
            if (requested_size < MIN_GRID_SIZE || requested_size > MAX_GRID_SIZE) {
                fprintf(stderr, "Error: --size must be between %d and %d.\n", MIN_GRID_SIZE,
                        MAX_GRID_SIZE);
                return EXIT_FAILURE;
            }
            grid_size = requested_size;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'.\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        } else if (input_path == NULL) {
            input_path = argv[i];
        } else {
            fprintf(stderr, "Error: unexpected extra argument '%s'.\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    /* Seed the PRNG: an explicit --seed makes runs reproducible, otherwise
       each run explores a different search trajectory. A fixed seed is also
       available for debugging: build with -DDEBUG_SEED. */
#ifdef DEBUG_SEED
    srand(1u);
    (void)seed_given; /* the explicit seed is ignored in debug builds */
#else
    srand(seed_given ? seed : (unsigned int)time(NULL));
#endif

    puzzle.grid = grid_create();

    /* Read the puzzle from the given file, or from stdin. */
    if (input_path == NULL) {
        puzzle_load(&puzzle, stdin, "<stdin>");
    } else {
        fp = fopen(input_path, "r");
        if (fp == NULL) {
            fprintf(stderr, "Error: cannot open '%s'.\n", input_path);
            return EXIT_FAILURE;
        }
        puzzle_load(&puzzle, fp, input_path);
        fclose(fp);
    }

    /* Build the initial randomized grid and solve. */
    initialize_grid(&puzzle);
    solve_futoshiki(&puzzle, &solver);

    /* Always show the solution, even when the initial grid was already valid
       or the trace is suppressed. */
    printf("\nSolved in %d step(s) (final temperature %.4g):\n", solver.steps,
           (double)solver.temperature);
    grid_print(puzzle.grid);

    grid_destroy(puzzle.grid);
    free(puzzle.restrictions);
    free(puzzle.fixed_cells);
    return EXIT_SUCCESS;
}
