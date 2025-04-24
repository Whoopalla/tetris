#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// TODO: Add window icon
// TODO: Maybe implement kick rotations (Check if rotation is possible if you
// move the piece away from the wall)

// TODO: BUG when the last line is filled it takes some time

// Testing CI
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define BOARD_HEIGHT_EXTRA 2
#define CELL_WIDTH_RATIO 0.05
#define TICK 0.8f
#define FAST_TICK_HORIZONTAL .2f
#define FAST_TICK_VERTICAL .1f

#define CLEAR_LINE_POINTS 10

#define TET_I_STATES 2
#define TET_L_STATES 4
#define TET_J_STATES 4
#define TET_T_STATES 4
#define TET_S_STATES 2
#define TET_Z_STATES 2
#define TET_O_STATES 0

typedef Vector2 Parts[4];

typedef enum { Down, Left, Right } Direction;

typedef enum {
  I = 0,
  L = I + TET_I_STATES, // 2
  J = L + TET_L_STATES, // 6
  T = J + TET_J_STATES, // 10
  S = T + TET_T_STATES, // 14
  Z = S + TET_S_STATES, // 16
  O = Z + TET_Z_STATES, // 18
} Tet_Type;

int tetromino_types[] = {I, L, J, T, S, Z, O};

int tet_state_count[O + 1] =
    (int[]){[I] = TET_I_STATES, [L] = TET_L_STATES, [J] = TET_J_STATES,
            [T] = TET_T_STATES, [S] = TET_S_STATES, [Z] = TET_Z_STATES,
            [O] = TET_O_STATES};

// TODO: make them all horizontal so that they take only 2 vertical cells
Parts tet_states[O + 1] = (Parts[]){
    (Vector2){0, 0},  (Vector2){1, 0},  (Vector2){2, 0}, (Vector2){3, 0}, // I
    (Vector2){1, -1}, (Vector2){1, 0},  (Vector2){1, 1}, (Vector2){1, 2},

    (Vector2){0, 0},  (Vector2){0, 1},  (Vector2){0, 2}, (Vector2){1, 2}, // L
    (Vector2){-1, 1}, (Vector2){0, 1},  (Vector2){1, 1}, (Vector2){1, 0},
    (Vector2){-1, 0}, (Vector2){0, 0},  (Vector2){0, 1}, (Vector2){0, 2},
    (Vector2){-1, 2}, (Vector2){-1, 1}, (Vector2){0, 1}, (Vector2){1, 1},

    (Vector2){0, 0},  (Vector2){0, 1},  (Vector2){1, 1}, (Vector2){2, 1}, // J
    (Vector2){1, 0},  (Vector2){1, 1},  (Vector2){1, 2}, (Vector2){0, 2},
    (Vector2){0, 1},  (Vector2){1, 1},  (Vector2){2, 1}, (Vector2){2, 2},
    (Vector2){1, 0},  (Vector2){2, 0},  (Vector2){1, 1}, (Vector2){1, 2},

    (Vector2){0, 1},  (Vector2){1, 1},  (Vector2){2, 1}, (Vector2){1, 2}, // T
    (Vector2){1, 0},  (Vector2){1, 1},  (Vector2){1, 2}, (Vector2){2, 1},
    (Vector2){1, 0},  (Vector2){0, 1},  (Vector2){1, 1}, (Vector2){2, 1},
    (Vector2){1, 0},  (Vector2){1, 1},  (Vector2){1, 2}, (Vector2){0, 1},

    (Vector2){1, 0},  (Vector2){2, 0},  (Vector2){1, 1}, (Vector2){0, 1}, // S
    (Vector2){1, -1}, (Vector2){1, 0},  (Vector2){2, 0}, (Vector2){2, 1},

    (Vector2){0, 0},  (Vector2){1, 0},  (Vector2){1, 1}, (Vector2){2, 1}, // Z
    (Vector2){2, -1}, (Vector2){1, 0},  (Vector2){1, 1}, (Vector2){2, 0},

    (Vector2){0, 0},  (Vector2){1, 0},  (Vector2){0, 1}, (Vector2){1, 1}, // O
};

typedef struct {
  Vector2 pos;
  Parts parts;
  Tet_Type type;
  int state;
} Tetromino;

bool board[BOARD_WIDTH][BOARD_HEIGHT + BOARD_HEIGHT_EXTRA] = {0};
Tetromino tetromino_bag[7] = {0};
int tetromino_bag_used = 0;

static int screen_width;
static int screen_height;

Color empty_cell_color;
Color alive_cell_color;
Color background_color;

float last_tick_time;
float delta_time;
bool tick_time;

float last_horizontal_tick;
bool horizontal_move = false;
bool continuous_horizontal = false;

bool fast_shift_left = false;
bool fast_shift_right = false;
bool fast_shift_down = false;
bool fast_rotate = false;

Tetromino tetromino;

size_t game_points = 0;

void UpdateDrawFrame(void);

bool is_tetromino_at(int part_index, Vector2 index) {
  for (int i = 0; i < 4; i++) {
    if (i == part_index)
      continue;
    if (Vector2Equals(tetromino.parts[i], index)) {
      return true;
    }
  }
  return false;
}

void board_remove_tetromino(void) {
  for (size_t i = 0; i < 4; i++) {
    board[(int)tetromino.parts[i].x][(int)tetromino.parts[i].y] = false;
  }
}

void board_add_tetromino(void) {
  for (size_t i = 0; i < 4; i++) {
    board[(int)tetromino.parts[i].x][(int)tetromino.parts[i].y] = true;
  }
}

bool within_board(Vector2 index) {
  bool is_within = index.x >= 0 && index.x < BOARD_WIDTH && index.y >= 0 &&
                   index.y < BOARD_HEIGHT + BOARD_HEIGHT_EXTRA;
  return is_within;
}

void move_tetromino(Direction dir) {
  board_remove_tetromino();
  switch (dir) {
  case Down:
    for (size_t i = 0; i < 4; i++) {
      if (!within_board(Vector2Add(tetromino.parts[i], (Vector2){0, 1})) ||
          board[(int)tetromino.parts[i].x][(int)tetromino.parts[i].y + 1]) {
        goto _no_move;
      }
    }

    tetromino.pos.y++;
    for (size_t i = 0; i < 4; i++) {
      tetromino.parts[i].y += 1;
    }
    break;
  case Left:
    for (size_t i = 0; i < 4; i++) {
      if (!within_board(Vector2Add(tetromino.parts[i], (Vector2){-1, 0})) ||
          board[(int)tetromino.parts[i].x - 1][(int)tetromino.parts[i].y]) {
        goto _no_move;
      }
    }

    tetromino.pos.x--;
    for (size_t i = 0; i < 4; i++) {
      tetromino.parts[i].x -= 1;
    }
    break;
  case Right:
    for (size_t i = 0; i < 4; i++) {
      if (!within_board(Vector2Add(tetromino.parts[i], (Vector2){1, 0})) ||
          board[(int)tetromino.parts[i].x + 1][(int)tetromino.parts[i].y]) {
        goto _no_move;
      }
    }

    tetromino.pos.x++;
    for (size_t i = 0; i < 4; i++) {
      tetromino.parts[i].x += 1;
    }
    break;
  }
_no_move:
  board_add_tetromino();
}

void rotate_tetromino(void) {
  int new_state = tetromino.state;
  board_remove_tetromino();

  new_state++;
  if (new_state >= tet_state_count[tetromino.type]) {
    new_state = 0;
  }
  // printf("Tetromino new state: %d\n", new_state);
  for (size_t i = 0; i < 4; i++) {
    Vector2 new_part_pos =
        Vector2Add(tet_states[tetromino.type + new_state][i], tetromino.pos);
    printf("new_part_pos: %f %f\n", new_part_pos.x, new_part_pos.y);
    printf("tetromino.pos: %f %f\n", tetromino.pos.x, tetromino.pos.x);
    if (!within_board(new_part_pos) ||
        board[(int)new_part_pos.x][(int)new_part_pos.y]) {
      printf("No rotation!\n");
      goto _no_rotation;
    }
  }
  tetromino.state = new_state;
  for (size_t i = 0; i < 4; i++) {
    Vector2 new_part_pos =
        Vector2Add(tet_states[tetromino.type + new_state][i], tetromino.pos);
    tetromino.parts[i] = new_part_pos;
  }

_no_rotation:
  board_add_tetromino();
}

void clear_full_line(void) {
  int lowest_y = 0;
  int shift_amount = 0;
  int y, x;
  for (y = BOARD_HEIGHT + BOARD_HEIGHT_EXTRA - 1; y > BOARD_HEIGHT_EXTRA; y--) {
    for (x = 0; x < BOARD_WIDTH; x++) {
      if (!board[x][y]) {
        goto _out_loop;
      }
    }
    if (y > lowest_y)
      lowest_y = y;
    // for (size_t x = 0; x < BOARD_WIDTH; x++) {
    //   board[x][y] = false;
    // }
    shift_amount++;
  _out_loop:
  }

  printf("lowest_y: %d shift_amount: %d\n", lowest_y, shift_amount);
  if (lowest_y != 0) {
    for (y = lowest_y; y >= BOARD_HEIGHT_EXTRA + shift_amount; y--) {
      for (x = 0; x < BOARD_WIDTH; x++) {
        assert(y - shift_amount > BOARD_HEIGHT_EXTRA - 1 && "You are stupid");
        board[x][y] = board[x][y - shift_amount];
      }
    }
  }
  game_points += shift_amount * CLEAR_LINE_POINTS;
  printf("Game Points: %lld\n", game_points);
}

void game_over(void) {
  game_points = 0;
  for (size_t y = BOARD_HEIGHT + BOARD_HEIGHT_EXTRA - 1;
       y >= BOARD_HEIGHT_EXTRA; y--) {
    for (size_t x = 0; x < BOARD_WIDTH; x++) {
      board[x][y] = false;
    }
  }
}

bool tetromino_grounded(void) {
  for (size_t i = 0; i < 4; i++) {
    Vector2 cell_below = {tetromino.parts[i].x, tetromino.parts[i].y + 1};
    // On the ground
    if (!within_board(cell_below)) {
      // assert(false && "kill tetromino");
      return true;
    }

    // On the pile of dead tetrominos
    if (!is_tetromino_at(i, cell_below) &&
        board[(int)cell_below.x][(int)cell_below.y]) {
      // assert(false && "kill tetromino");
      return true;
    }
  }
  return false;
}

void refill_tetromino_bag(void) {
  Tetromino t;
  int r;
  for (size_t i = 0; i < 7; i++) {
    tetromino_bag[i] = (Tetromino){
        .type = tetromino_types[i], .state = 0, .pos = (Vector2){0.0f, 0.0f}};
    memcpy(tetromino_bag[i].parts, tet_states[tetromino_types[i]],
           sizeof(Parts));
  }

  srand(time(NULL));
  for (size_t i = 6; i >= 1; i--) {
    r = rand() % i;
    t = tetromino_bag[r];
    tetromino_bag[r] = tetromino_bag[i];
    tetromino_bag[i] = t;
  }
  tetromino_bag_used = 0;
}

void spawn_tetromino(void) {
  if (tetromino_bag_used == 7) {
    refill_tetromino_bag();
  }
  tetromino = tetromino_bag[tetromino_bag_used++];
}

void UpdateDrawFrame() {
  if (last_tick_time >= TICK) {
    last_tick_time = 0.0;
    tick_time = true;
  }

  delta_time = GetFrameTime();
  last_tick_time += delta_time;

  BeginDrawing();

  screen_width = GetScreenWidth();
  screen_height = GetScreenHeight();

  int cell_width = screen_height * CELL_WIDTH_RATIO;

  int x0 = (screen_width - cell_width * BOARD_WIDTH) / 2;
  int y0 = screen_height - cell_width * BOARD_HEIGHT -
           cell_width * BOARD_HEIGHT_EXTRA;

  if (tick_time) {
    if (tetromino_grounded()) {
      // Spawn new one
      printf("Grounded! Type: %d\n", tetromino.type);

      for (size_t i = 0; i < BOARD_WIDTH; i++) {
        if (board[i][BOARD_HEIGHT_EXTRA]) {
          game_over();
        }
      }

      clear_full_line();
      spawn_tetromino();
    }
    move_tetromino(Down);
    tick_time = false;
  }

  if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
    last_horizontal_tick = 0;
    move_tetromino(Left);
  }
  if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
    last_horizontal_tick = 0;
    move_tetromino(Right);
  }
  if (IsKeyPressed(KEY_R)) {
    rotate_tetromino();
  }

  // Continuous press

  // Both are directions pressed
  if ((IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) &&
      (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))) {
    last_horizontal_tick = 0;
  }

  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
    last_horizontal_tick += delta_time;

    if (last_horizontal_tick >= FAST_TICK_HORIZONTAL) {
      last_horizontal_tick = 0;
      move_tetromino(Left);
    }
  }

  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
    last_horizontal_tick += delta_time;

    if (last_horizontal_tick >= FAST_TICK_HORIZONTAL) {
      last_horizontal_tick = 0;
      move_tetromino(Right);
    }
  }

  // Check grounded to not wait a whole tick for a new spawn
  if (!tetromino_grounded() && (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))) {
    last_tick_time += delta_time;

    if (last_tick_time >= FAST_TICK_VERTICAL) {
      last_tick_time = 0;
      move_tetromino(Down);
      if (tetromino_grounded()) {
        last_tick_time = TICK - .1; // Couse new spawn
      }
    }
  }

  for (size_t y = BOARD_HEIGHT_EXTRA; y < BOARD_HEIGHT + BOARD_HEIGHT_EXTRA;
       y++) {
    for (size_t x = 0; x < BOARD_WIDTH; x++) {
      if (board[x][y]) {
        DrawRectangle(x0 + x * cell_width, y0 + y * cell_width, cell_width - 5,
                      cell_width - 5,
                      alive_cell_color); // - 5 maybe something else
      } else {
        DrawRectangle(x0 + x * cell_width, y0 + y * cell_width, cell_width - 5,
                      cell_width - 5,
                      empty_cell_color); // - 5 maybe something else
      }
    }
  }
  ClearBackground(background_color);
  EndDrawing();
}

int main(void) {
  InitWindow(800, 900, "tetris");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
#ifdef __EMSCRIPTEN__
  SetWindowState(FLAG_WINDOW_MAXIMIZED);
#endif
  empty_cell_color = GetColor(0x1b4965FF);
  alive_cell_color = GetColor(0x5fa8d3FF);
  background_color = BLACK;

  last_tick_time = 0;
  last_horizontal_tick = 0;

  printf("I: %d L: %d J: %d T: %d S: %d Z: %d O: %d", I, L, J, T, S, Z, O);

  refill_tetromino_bag();
  spawn_tetromino();

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
  while (!WindowShouldClose()) {
    UpdateDrawFrame();
  }
#endif
  CloseWindow();
  return 0;
}
