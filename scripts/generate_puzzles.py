"""Makes random Rummikub puzzles in the input format of the solver.

Every tile (value 1 till rows, color b/g/r/y) gets 0, 1 or 2 copies, with
probabilities p0 and p1 in percent for 0 and 1 copies, the rest gets 2.

There are two modes:
  fixed  every puzzle uses the same p0 and p1
  mixed  p0 and p1 are drawn per puzzle, the way I made the training data for
         the difficulty classifier: after puzzle 10,000 and 100,000 the
         puzzles get more copies and become harder

Examples:
  python scripts/generate_puzzles.py fixed --count 100 --p0 5 --p1 5 -o puzzles.in
  python scripts/generate_puzzles.py mixed --count 1000 --seed 1 -o puzzles.in
"""

import argparse
import random
import sys

COLORS = ["b", "g", "r", "y"]


def make_puzzle(rows, tile_prob0, tile_prob1):
    """Returns the tiles of one puzzle, for example ['1b', '1b', '2g'].
       Args:
        rows: The highest tile value in the puzzle.
        tile_prob0: The probability in percent of zero copies.
        tile_prob1: The probability in percent of one copy."""
    tiles = []
    for number in range(1, rows + 1):
        for color in COLORS:
            rand_num = random.uniform(0, 100)
            if rand_num < tile_prob0:  # 0 copies
                continue
            elif rand_num < tile_prob0 + tile_prob1:  # 1 copy
                tiles.append(str(number) + color)
            else:  # 2 copies
                tiles.append(str(number) + color)
                tiles.append(str(number) + color)
    return tiles


def mixed_probabilities(i):
    """Returns the copy probabilities for puzzle i of the training data."""
    tile_prob0 = random.uniform(0, 100)
    tile_prob1 = random.uniform(0, 100 - tile_prob0)
    if i > 10000:
        tile_prob0 = random.uniform(0, 50)
        tile_prob1 = random.uniform(0, 100 - tile_prob0)
    if i > 100000:
        tile_prob0 = random.uniform(0, 10)
        tile_prob1 = random.uniform(0, 10 - tile_prob0)
    return tile_prob0, tile_prob1


def write_puzzles(out, count, rows, probabilities):
    """Writes count puzzles to out, probabilities is a function of the puzzle number."""
    out.write(str(count) + "\n")
    for i in range(count):
        tiles = make_puzzle(rows, *probabilities(i))
        out.write(str(len(tiles)) + "\n")
        out.write(" ".join(tiles) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("mode", choices=["fixed", "mixed"])
    parser.add_argument("--count", type=int, default=100, help="number of puzzles (default 100)")
    parser.add_argument("--rows", type=int, default=100, help="highest tile value, 1-100 (default 100)")
    parser.add_argument("--p0", type=float, default=10.0, help="fixed mode: %% of tiles with 0 copies")
    parser.add_argument("--p1", type=float, default=10.0, help="fixed mode: %% of tiles with 1 copy")
    parser.add_argument("--seed", type=int, help="random seed for reproducible output")
    parser.add_argument("-o", "--output", help="output file (default: stdout)")
    args = parser.parse_args()

    if not 1 <= args.rows <= 100:
        parser.error("--rows must be between 1 and 100 (the solver supports values up to 100)")
    if args.mode == "fixed" and not (args.p0 >= 0 and args.p1 >= 0 and args.p0 + args.p1 <= 100):
        parser.error("--p0 and --p1 must be non-negative and sum to at most 100")

    random.seed(args.seed)
    if args.mode == "fixed":
        probabilities = lambda i: (args.p0, args.p1)
    else:
        probabilities = mixed_probabilities

    if args.output:
        with open(args.output, "w") as out:
            write_puzzles(out, args.count, args.rows, probabilities)
    else:
        write_puzzles(sys.stdout, args.count, args.rows, probabilities)


if __name__ == "__main__":
    main()
