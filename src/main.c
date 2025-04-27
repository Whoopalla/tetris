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

// Testing ci
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define BOARD_HEIGHT_EXTRA 2
#define CELL_WIDTH_RATIO 0.05
#define TICK 0.8f
#define FAST_TICK_HORIZONTAL .1f
#define FAST_TICK_VERTICAL 15.0f

#define CLEAR_LINE_POINTS 10
#define CLEAR_ANIMATION_DURATION .3f
#define CLEAR_ANIMATION_SWITCH_DURATION CLEAR_ANIMATION_DURATION / 3.0f

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

int tet_max_widths[] = {
    [I] = 4, [L] = 3, [J] = 3, [T] = 3, [S] = 3, [Z] = 3, [O] = 2};

int tet_state_count[] = {
    [I] = TET_I_STATES, [L] = TET_L_STATES, [J] = TET_J_STATES,
    [T] = TET_T_STATES, [S] = TET_S_STATES, [Z] = TET_Z_STATES,
    [O] = TET_O_STATES};

#define TET_START_OFFSET BOARD_WIDTH / 2.0f
// TODO: make them all horizontal so that they take only 2 vertical cells
Parts tet_states[O + 1] = {
    (Vector2){0, 0},  (Vector2){1, 0}, (Vector2){2, 0}, (Vector2){3, 0}, // I
    (Vector2){1, -1}, (Vector2){1, 0}, (Vector2){1, 1}, (Vector2){1, 2},

    (Vector2){0, 1},  (Vector2){1, 1}, (Vector2){2, 1}, (Vector2){2, 0},
    (Vector2){0, 0},  (Vector2){1, 0}, (Vector2){1, 1}, (Vector2){1, 2},
    (Vector2){0, 2},  (Vector2){0, 1}, (Vector2){1, 1}, (Vector2){2, 1},
    (Vector2){1, 0},  (Vector2){1, 1}, (Vector2){1, 2}, (Vector2){2, 2}, // L

    (Vector2){0, 0},  (Vector2){0, 1}, (Vector2){1, 1}, (Vector2){2, 1}, // J
    (Vector2){1, 0},  (Vector2){1, 1}, (Vector2){1, 2}, (Vector2){0, 2},
    (Vector2){0, 1},  (Vector2){1, 1}, (Vector2){2, 1}, (Vector2){2, 2},
    (Vector2){1, 0},  (Vector2){2, 0}, (Vector2){1, 1}, (Vector2){1, 2},

    (Vector2){0, 1},  (Vector2){1, 1}, (Vector2){2, 1}, (Vector2){1, 2}, // T
    (Vector2){1, 0},  (Vector2){1, 1}, (Vector2){1, 2}, (Vector2){2, 1},
    (Vector2){1, 0},  (Vector2){0, 1}, (Vector2){1, 1}, (Vector2){2, 1},
    (Vector2){1, 0},  (Vector2){1, 1}, (Vector2){1, 2}, (Vector2){0, 1},

    (Vector2){1, 0},  (Vector2){2, 0}, (Vector2){1, 1}, (Vector2){0, 1}, // S
    (Vector2){1, -1}, (Vector2){1, 0}, (Vector2){2, 0}, (Vector2){2, 1},

    (Vector2){0, 0},  (Vector2){1, 0}, (Vector2){1, 1}, (Vector2){2, 1}, // Z
    (Vector2){2, -1}, (Vector2){1, 0}, (Vector2){1, 1}, (Vector2){2, 0},

    (Vector2){0, 0},  (Vector2){1, 0}, (Vector2){0, 1}, (Vector2){1, 1}, // O
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

bool clear_animation = false;
int clear_lowest_y;
int clear_shift_amount;
float clear_animation_time = 0;
float clear_animation_switch_time = 0;

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

void dump_board(void) {
  printf("BOARD DUMP\n");
  for (int y = 0; y < BOARD_HEIGHT + BOARD_HEIGHT_EXTRA; y++) {
    for (int x = 0; x < BOARD_WIDTH; x++) {
      printf("%s", board[x][y] ? "[*]" : "[ ]");
    }
    printf("\n");
  }
}

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
    if (!within_board(new_part_pos) ||
        board[(int)new_part_pos.x][(int)new_part_pos.y]) {
      // printf("No rotation!\n");
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

bool full_lines(void) {
  clear_lowest_y = 0;
  clear_shift_amount = 0;
  int y, x;
  for (y = BOARD_HEIGHT + BOARD_HEIGHT_EXTRA - 1; y > BOARD_HEIGHT_EXTRA; y--) {
    for (x = 0; x < BOARD_WIDTH; x++) {
      if (!board[x][y]) {
        goto _out_loop;
      }
    }
    if (y > clear_lowest_y)
      clear_lowest_y = y;
    clear_shift_amount++;
  _out_loop:
  }
  clear_animation = clear_lowest_y;
  return clear_lowest_y != 0;
}

void clear_full_lines(void) {
  printf("clear_lowest_y: %d clear_shift_amount: %d\n", clear_lowest_y,
         clear_shift_amount);
  // dump_board();
  int x, y;
  if (clear_lowest_y != 0) {
    for (y = clear_lowest_y; y >= BOARD_HEIGHT_EXTRA + clear_shift_amount;
         y--) {
      for (x = 0; x < BOARD_WIDTH; x++) {
        assert(y - clear_shift_amount > BOARD_HEIGHT_EXTRA - 1 &&
               "You are stupid");
        board[x][y] = board[x][y - clear_shift_amount];
      }
    }
  }
  // dump_board();
  clear_lowest_y = 0;
  clear_shift_amount = 0;
  game_points += clear_shift_amount * CLEAR_LINE_POINTS;
  printf("Game Points: %lld\n", game_points);
}

bool clear_animation_do(void) {
  int x, y;
  clear_animation_time += delta_time;
  clear_animation_switch_time += delta_time;

  if (clear_animation_time >= CLEAR_ANIMATION_DURATION) {
    printf("Animating clear is DONE\n");
    clear_animation = false;
    clear_animation_time = 0.0;
    clear_animation_switch_time = 0.0;

    clear_full_lines();
    return true;
  }

  if (clear_lowest_y != 0) {
    for (y = clear_lowest_y; y > clear_lowest_y - clear_shift_amount; y--) {
      for (x = 0; x < BOARD_WIDTH; x++) {
        // printf("clear_animation_switch_time: %f\n",
        //        clear_animation_switch_time);
        // printf("Clear anim setting: %s\n",
        //        clear_animation_switch_time >= CLEAR_ANIMATION_SWITCH_DURATION
        //            ? "true"
        //            : "false");
        board[x][y] =
            clear_animation_switch_time >= CLEAR_ANIMATION_SWITCH_DURATION
                ? true
                : false;
      }
    }
    if (clear_animation_switch_time >= CLEAR_ANIMATION_SWITCH_DURATION) {
      clear_animation_switch_time = 0.0f;
    }
  }
  return false;
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
  for (size_t i = 0; i < 4; i++) {
    tetromino.pos.x = TET_START_OFFSET - tet_max_widths[tetromino.type] / 2.0f;
    tetromino.parts[i].x +=
        TET_START_OFFSET - tet_max_widths[tetromino.type] / 2.0f;
  }
}

void UpdateDrawFrame() {
  if (last_tick_time >= TICK) {
    last_tick_time = 0.0;
    tick_time = true;
  }

  delta_time = GetFrameTime();
  last_tick_time += delta_time;

  screen_width = GetScreenWidth();
  screen_height = GetScreenHeight();

  int cell_width = screen_height * CELL_WIDTH_RATIO;

  int x0 = (screen_width - cell_width * BOARD_WIDTH) / 2;
  int y0 = screen_height - cell_width * BOARD_HEIGHT -
           cell_width * BOARD_HEIGHT_EXTRA;

  if (clear_animation) {
    clear_animation_do();
    goto _draw;
  }

  if (tick_time) {
    if (tetromino_grounded()) {
      // Spawn new one
      printf("Grounded! Type: %d\n", tetromino.type);

      for (size_t i = 0; i < BOARD_WIDTH; i++) {
        if (board[i][BOARD_HEIGHT_EXTRA]) {
          game_over();
        }
      }

      if (!clear_animation && !full_lines()) {
        spawn_tetromino();
      }
    } else {
      move_tetromino(Down);
      tick_time = false;
      goto _draw;
    }
  }

  if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
    last_horizontal_tick = 0;
    move_tetromino(Left);
  }
  if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
    last_horizontal_tick = 0;
    move_tetromino(Right);
  }
  if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_UP)) {
    rotate_tetromino();
  }

  // Continuous press

  // Both directions are pressed
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

  if ((IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))) {
    last_tick_time += FAST_TICK_VERTICAL * delta_time;
  }

_draw:
  BeginDrawing();
  ClearBackground(background_color);
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
