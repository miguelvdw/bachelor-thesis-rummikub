//Rummikub puzzle solver
//Miguel van der Wekken, bachelor thesis "Algorithms for Rummikub Puzzles",
//LIACS, Leiden University, 2025.
//
//Finds the maximum number of points that can be scored by laying out tiles
//of a Rummikub puzzle in valid groups and runs.
//   - 4 colors (b, g, r, y), tile values 1-100, at most 2 copies per tile
//   - groups: at least 3 tiles of the same value, all different colors
//   - runs:   at least 3 consecutive values of the same color
//   - a tile scores its value, unused tiles score nothing
//
//The puzzle is a grid of rows (tile values) and columns (colors). The solver
//recurses from the highest row (highest tile value) down: in every row it tries each combination
//of runs ending in that row, puts the remaining tiles of the row into groups,
//and memoizes the result on an encoding of the runs that are still "open"
//in the last four rows.
//
//Usage:
//   ./rummikub <puzzles.in> print the max score per puzzle
//   ./rummikub <puzzles.in> --features <out.csv> also write difficulty features
//
//Input format: the number of puzzles, then per puzzle a line with the number
//of tiles and a line with the tiles, e.g. "3" / "1b 2b 3b".

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

enum colors { black, green, red, yellow };

const int n_colors = 4;   //number of colors (columns)
const int n_rows = 100;   //number of tile values (rows)
const int max_copies = 2; //max copies of each tile

//the run combinations that can end in a row, as run lengths (one run per copy)
//runs longer than 5 are made by chaining runs, so 3, 4 and 5 are enough
const int n_patterns = 10;
const int RUN_PATTERNS[n_patterns][max_copies] = {
  {0, 0}, {3, 0}, {4, 0}, {5, 0},
  {3, 3}, {4, 3}, {4, 4}, {5, 3}, {5, 4}, {5, 5}
};

//a column state is how many runs start in each of the last 4 rows of one color
//all these runs cover the current row, so they add up to at most max_copies:
//C(max_copies + 4, 4) = 15 states (thesis, Section 5.1)
const int column_states = 15;
const int max_code = column_states * column_states * column_states * column_states; //50,625
const uint32_t NOT_COMPUTED = UINT32_MAX;

//the columns have to be in the same order as write_features() writes them
const char* FEATURES_HEADER =
  "bound_gap,tile_surplus,pct_0_copies,pct_1_copy,pct_2_copies,tiles_in_run_and_group,execution_time";

uint8_t puzzle[n_rows][n_colors];   //number of copies of each tile
uint8_t run_ends[n_rows][n_colors]; //number of chosen runs that start in this row
uint32_t value[n_rows][max_code];   //memoized best value of rows 0-row per state

//state_index[a][b][c][d] is the number 0 till 14 of the column state, or -1
int state_index[max_copies + 1][max_copies + 1][max_copies + 1][max_copies + 1];

int total_tiles;       //tiles in the puzzle (after removing isolated tiles)
int max_points_puzzle; //sum of all tile values (after removing isolated tiles)
int easyrows;          //rows 0-easyrows are all clearable
int lower, upper;      //bounds on the maximum score
int tiles_in_runs;     //tiles used by the run-only lower bound
int tiles_in_groups;   //tiles used by the group-only lower bound

//numbers all the valid column states, returns false when the count does not
//match column_states, which can only happen after changing max_copies
bool init_state_index() {
  int count = 0;
  for (int a = 0; a <= max_copies; a++)
    for (int b = 0; b <= max_copies; b++)
      for (int c = 0; c <= max_copies; c++)
        for (int d = 0; d <= max_copies; d++)
          state_index[a][b][c][d] = (a + b + c + d <= max_copies) ? count++ : -1;
  return count == column_states;
}

//returns the max amount of tiles of a row that we can put in groups, with the
//closed formula from the thesis (Section 4.4)
//sort the colors on their amount of tiles c_1 <= ... <= c_k and take T = c_1 + ... + c_k-2
//if T <= c_k-1 every tile of the k-2 smallest colors makes a group of 3
//else we use T + c_k-1 tiles plus one tile of the biggest color per group,
//and there are at most (T + c_k-1) / 2 groups
int group_value(const int row) {
  int count[n_colors];
  for (int color = 0; color < n_colors; color++) {
    count[color] = puzzle[row][color];
  }
  std::sort(count, count + n_colors);
  int sum = 0;
  for (int color = 0; color < n_colors - 2; color++) {
    sum += count[color];
  }
  if (sum <= count[n_colors - 2]) {
    return 3 * sum;
  }
  sum += count[n_colors - 2];
  return sum + std::min(count[n_colors - 1], sum / 2);
}

//can we put all the tiles of a row in groups?
bool clearable(const int row) {
  int tiles = 0;
  for (int color = 0; color < n_colors; color++) {
    tiles += puzzle[row][color];
  }
  return group_value(row) == tiles;
}

bool row_is_empty(const int row) {
  for (int color = 0; color < n_colors; color++) {
    if (puzzle[row][color] != 0) {
      return false;
    }
  }
  return true;
}

//marks in possible[] which run patterns can end in this row and color
//the i-th row from the top needs one copy for every run in the pattern that
//is longer than i
void possible_runs(bool possible[], const int row, const int color) {
  for (int p = 0; p < n_patterns; p++) {
    possible[p] = true;
    for (int i = 0; i < 5 && possible[p]; i++) {
      int needed = 0;
      for (int r = 0; r < max_copies; r++) {
        if (RUN_PATTERNS[p][r] > i) {
          needed++;
        }
      }
      if (needed > 0 && (row - i < 0 || puzzle[row - i][color] < needed)) {
        possible[p] = false;
      }
    }
  }
}

//gets rid of the runs of a pattern that end in (row, color) and returns their points
int eliminate_runs(const int row, const int pattern, const int color) {
  int points = 0;
  for (int r = 0; r < max_copies && RUN_PATTERNS[pattern][r] > 0; r++) {
    int length = RUN_PATTERNS[pattern][r];
    run_ends[row - length + 1][color]++;
    for (int i = 0; i < length; i++) {
      puzzle[row - i][color]--;
      points += row - i + 1;
    }
  }
  return points;
}

//undo eliminate_runs by putting the tiles back
void undo_runs(const int row, const int pattern, const int color) {
  for (int r = 0; r < max_copies && RUN_PATTERNS[pattern][r] > 0; r++) {
    int length = RUN_PATTERNS[pattern][r];
    run_ends[row - length + 1][color]--;
    for (int i = 0; i < length; i++) {
      puzzle[row - i][color]++;
    }
  }
}

//how many runs of 3 can the tile (row, color) be part of: with the 2 rows below,
//the 2 rows above, or 1 row on each side, and twice when both neighbours have
//2 copies
int run_neighbours(const int row, const int color) {
  int runs = 0;
  if (row - 2 >= 0 && puzzle[row - 1][color] != 0 && puzzle[row - 2][color] != 0) {
    runs += (puzzle[row - 1][color] > 1 && puzzle[row - 2][color] > 1) ? 2 : 1;
  }
  if (row + 2 < n_rows && puzzle[row + 1][color] != 0 && puzzle[row + 2][color] != 0) {
    runs += (puzzle[row + 1][color] > 1 && puzzle[row + 2][color] > 1) ? 2 : 1;
  }
  if (row - 1 >= 0 && row + 1 < n_rows && puzzle[row + 1][color] != 0 && puzzle[row - 1][color] != 0) {
    runs += (puzzle[row + 1][color] > 1 && puzzle[row - 1][color] > 1) ? 2 : 1;
  }
  return runs;
}

//lower bound when we only make groups, gives the points and the tiles we use
void groups_lower_bound(int& points, int& tiles) {
  points = 0;
  tiles = 0;
  for (int h = 0; h < n_rows; h++) {
    int rowvalue = group_value(h);
    points += rowvalue * (h + 1);
    tiles += rowvalue;
  }
}

//lower bound when we only make runs, gives the points and the tiles we use
//goes over every color from the last row up and takes every sequence of 3 or
//more non empty rows, with the second copies that fit (thesis, Section 6.2)
void runs_lower_bound(int& points, int& tiles) {
  points = 0;
  tiles = 0;
  for (int color = 0; color < n_colors; color++) {
    for (int i = n_rows - 1; i >= 0; i--) {
      if (puzzle[i][color] == 0) { //skip empty cells
        continue;
      }
      if ((i - 2 >= 0) && (puzzle[i - 1][color] > 0) && (puzzle[i - 2][color] > 0)) { //at least one run of 3
        while (i >= 0 && puzzle[i][color] > 0) {
          if (puzzle[i][color] == 2) {
            if (i - 2 >= 0 && puzzle[i - 1][color] == 2 && puzzle[i - 2][color] == 2) { //3 or more consecutive 2's
              while (i >= 0 && puzzle[i][color] == 2) {
                points += (i + 1) * 2;
                tiles += 2;
                i--;
              }
              continue;
            }
            if (i - 2 >= 0 && i + 1 < n_rows && puzzle[i + 1][color] > 0 && puzzle[i - 1][color] == 2 && puzzle[i - 2][color] > 0) { //two consecutive 2's
              points += (4 * i) + 2;
              tiles += 4;
              i -= 2;
              continue;
            }
            if (i - 2 >= 0 && i + 2 < n_rows && puzzle[i + 1][color] > 0 && puzzle[i + 2][color] > 0 && puzzle[i - 1][color] > 0 && puzzle[i - 2][color] > 0) { //single 2
              points += i + 1;
              tiles++;
            }
          }
          points += i + 1;
          tiles++;
          i--;
        }
      }
    }
  }
}

//counts the tiles that can be in a group and in a run
int in_run_and_in_group() {
  int tiles_in_both = 0;
  for (int i = n_rows - 1; i >= 0; i--) {
    bool clear = clearable(i);
    int group = group_value(i);
    for (int color = 0; color < n_colors; color++) {
      if (group != 0 && puzzle[i][color] != 0) {
        int tilesg = clear ? puzzle[i][color] : 1;
        int tilesr = run_neighbours(i, color);
        tiles_in_both += (tilesg < tilesr) ? tilesg : tilesr;
      }
    }
  }
  return tiles_in_both;
}

//removes the copies that can never be part of a group or a run
void remove_isolated_tiles() {
  for (int i = n_rows - 1; i >= 0; i--) {
    int group = group_value(i);
    bool clear = clearable(i);
    for (int color = 0; color < n_colors; color++) {
      if (!clear && puzzle[i][color] != 0) {
        int tilesg = (group > 0) ? 1 : 0;
        int tilesr = run_neighbours(i, color);
        int isolated = puzzle[i][color] - tilesg - tilesr;
        if (isolated > 0) {
          max_points_puzzle -= isolated * (i + 1);
          puzzle[i][color] -= isolated;
          total_tiles -= isolated;
        }
      }
    }
  }
}

//are the rows 0 till row all clearable right now?
//the runs we chose sofar only took tiles from the 4 rows under the row they end
//in, so everything under row - 3 is untouched and easyrows still holds there,
//the rows row-3 till row we check again
bool rest_is_clearable(const int row) {
  if (row - 4 > easyrows) {
    return false;
  }
  for (int r = std::max(0, row - 3); r <= row; r++) {
    if (!clearable(r)) {
      return false;
    }
  }
  return true;
}

//key for the memory: the column states of the 4 colors in base 15
//under row 0 there are no run ends
int state_code(const int row) {
  int code = 0;
  for (int color = n_colors - 1; color >= 0; color--) {
    int bottom = (row >= 3) ? run_ends[row - 3][color] : 0;
    code = code * column_states
         + state_index[bottom][run_ends[row - 2][color]][run_ends[row - 1][color]][run_ends[row][color]];
  }
  return code;
}

//recursive function that computes the best score for the rows 0 till row
//currentamount is what we scored in the rows above, maxpoints is the best total sofar
int max_points(const int row, int& maxpoints, int currentamount) {
  //base case: no run can end in row 1 or row 0, so we only make groups
  if (row == 1) {
    currentamount += group_value(0) + group_value(1) * 2;
    if (currentamount > maxpoints) {
      maxpoints = currentamount;
    }
    return currentamount;
  }

  if (row_is_empty(row)) {
    return max_points(row - 1, maxpoints, currentamount);
  }

  //everything that is left is clearable, so all these tiles score
  if (rest_is_clearable(row)) {
    for (int r = 0; r <= row; r++) {
      for (int color = 0; color < n_colors; color++) {
        currentamount += (r + 1) * puzzle[r][color];
      }
    }
    if (currentamount > maxpoints) {
      maxpoints = currentamount;
    }
    return currentamount;
  }

  //did we see this state before?
  int code = state_code(row);
  if (value[row][code] != NOT_COMPUTED) {
    return value[row][code] + currentamount;
  }

  bool possible[n_colors][n_patterns];
  for (int color = 0; color < n_colors; color++) {
    possible_runs(possible[color], row, color);
  }

  int maxscorerow = 0;

  //try every combination of run patterns and put the rest of the row in groups
  for (int i = 0; i < n_patterns; i++) {
    if (!possible[black][i])
      continue;
    int pointsblack = eliminate_runs(row, i, black);
    currentamount += pointsblack;
    for (int j = 0; j < n_patterns; j++) {
      if (!possible[green][j])
        continue;
      int pointsgreen = eliminate_runs(row, j, green);
      currentamount += pointsgreen;
      for (int k = 0; k < n_patterns; k++) {
        if (!possible[red][k])
          continue;
        int pointsred = eliminate_runs(row, k, red);
        currentamount += pointsred;
        for (int l = 0; l < n_patterns; l++) {
          if (!possible[yellow][l])
            continue;
          int pointsyellow = eliminate_runs(row, l, yellow);
          currentamount += pointsyellow;

          int grouppoints = group_value(row) * (row + 1);
          currentamount += grouppoints;
          int amount = max_points(row - 1, maxpoints, currentamount);
          if (amount > maxscorerow) {
            maxscorerow = amount;
          }
          if (maxpoints == upper) { //we reached the upper bound, we can stop
            undo_runs(row, l, yellow);
            undo_runs(row, k, red);
            undo_runs(row, j, green);
            undo_runs(row, i, black);
            return maxpoints;
          }
          currentamount -= grouppoints;
          undo_runs(row, l, yellow);
          currentamount -= pointsyellow;
        }
        undo_runs(row, k, red);
        currentamount -= pointsred;
      }
      undo_runs(row, j, green);
      currentamount -= pointsgreen;
    }
    undo_runs(row, i, black);
    currentamount -= pointsblack;
  }
  value[row][code] = maxscorerow - currentamount;
  if (maxscorerow > maxpoints) {
    maxpoints = maxscorerow;
  }
  return maxscorerow;
}

//the max score of the puzzle we prepared
int solve() {
  if (lower == upper)
    return lower;
  int maxpoints = 0;
  max_points(n_rows - 1, maxpoints, 0);
  return maxpoints;
}

//reads one puzzle into puzzle[][], returns false when the input is not valid
bool read_puzzle(std::istream& in) {
  std::memset(puzzle, 0, sizeof(puzzle));
  if (!(in >> total_tiles) || total_tiles < 0) {
    std::cerr << "Error: expected the number of tiles" << std::endl;
    return false;
  }
  max_points_puzzle = 0;
  std::string tile;
  for (int j = 0; j < total_tiles; j++) {
    if (!(in >> tile) || tile.size() < 2) {
      std::cerr << "Error: expected " << total_tiles << " tiles" << std::endl;
      return false;
    }
    char color = tile.back();
    std::string digits = tile.substr(0, tile.size() - 1);
    if (digits.find_first_not_of("0123456789") != std::string::npos || digits.size() > 3) {
      std::cerr << "Error: invalid tile '" << tile << "'" << std::endl;
      return false;
    }
    int number = std::stoi(digits);
    int c;
    switch (color) {
      case 'b': c = black; break;
      case 'g': c = green; break;
      case 'r': c = red; break;
      case 'y': c = yellow; break;
      default:
        std::cerr << "Error: invalid color in tile '" << tile << "'" << std::endl;
        return false;
    }
    if (number < 1 || number > n_rows) {
      std::cerr << "Error: tile value must be between 1 and " << n_rows << ": '" << tile << "'" << std::endl;
      return false;
    }
    if (puzzle[number - 1][c] == max_copies) {
      std::cerr << "Error: more than " << max_copies << " copies of tile '" << tile << "'" << std::endl;
      return false;
    }
    puzzle[number - 1][c]++;
    max_points_puzzle += number;
  }
  return true;
}

//pre-processing: removes the isolated tiles, resets the memory and computes
//easyrows and the lower and upper bound
void prepare_puzzle() {
  remove_isolated_tiles();
  std::memset(run_ends, 0, sizeof(run_ends));
  std::memset(value, 0xFF, sizeof(value)); //sets every entry to NOT_COMPUTED
  easyrows = n_rows - 1;
  for (int h = 0; h < n_rows; h++) {
    if (!clearable(h)) {
      easyrows = h - 1;
      break;
    }
  }
  int only_runs, only_groups;
  runs_lower_bound(only_runs, tiles_in_runs);
  groups_lower_bound(only_groups, tiles_in_groups);
  lower = (only_runs > only_groups ? only_runs : only_groups);
  upper = (max_points_puzzle < (only_runs + only_groups) ? max_points_puzzle : only_runs + only_groups);
}

//writes the difficulty features of the puzzle as one line of csv
//we can compute them before solving, execution_time is the label
void write_features(std::ostream& out, double seconds, int tiles_in_both) {
  int minimal_tiles = (tiles_in_groups > tiles_in_runs ? tiles_in_groups : tiles_in_runs);
  out << upper - lower << "," << total_tiles - minimal_tiles << ",";

  int frequency_copies[max_copies + 1] = {0};
  for (int i = 0; i < n_rows; i++) {
    for (int j = 0; j < n_colors; j++) {
      frequency_copies[puzzle[i][j]]++;
    }
  }
  for (int i = 0; i <= max_copies; i++) {
    out << (double)frequency_copies[i] / (n_rows * n_colors) * 100 << ",";
  }
  out << tiles_in_both << "," << seconds << std::endl;
}

int main(int argc, char* argv[]) {
  bool with_features = argc == 4 && std::string(argv[2]) == "--features";
  if (argc != 2 && !with_features) {
    std::cerr << "Usage: " << argv[0] << " <puzzles.in> [--features <out.csv>]" << std::endl;
    return 1;
  }
  if (!init_state_index()) {
    std::cerr << "Error: column_states does not match max_copies" << std::endl;
    return 1;
  }
  std::ifstream in(argv[1]);
  if (!in) {
    std::cerr << "Error: could not open " << argv[1] << std::endl;
    return 1;
  }
  std::ofstream features;
  if (with_features) {
    features.open(argv[3]);
    if (!features) {
      std::cerr << "Error: could not open " << argv[3] << std::endl;
      return 1;
    }
    features << FEATURES_HEADER << std::endl;
  }

  int puzzle_count;
  if (!(in >> puzzle_count)) {
    std::cerr << "Error: expected the number of puzzles" << std::endl;
    return 1;
  }
  for (int p = 0; p < puzzle_count; p++) {
    if (!read_puzzle(in)) {
      std::cerr << "  in puzzle " << p + 1 << std::endl;
      return 1;
    }
    prepare_puzzle();
    int tiles_in_both = with_features ? in_run_and_in_group() : 0;

    auto start = std::chrono::steady_clock::now();
    int result = solve();
    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

    std::cout << result << "\n";
    if (with_features) {
      write_features(features, elapsed.count(), tiles_in_both);
    }
  }
  return 0;
}
