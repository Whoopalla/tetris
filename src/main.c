#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef __EMSCRIPTEN__
#include "favicon.h"
#endif

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// TODO: Maybe implement kick rotations (Check if rotation is possible if you
// move the piece away from the wall)
//
// TODO: Pouse menu
// TODO: Why after game over tetromino is so low?
// TODO: Sound?

#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define BOARD_HEIGHT_EXTRA 2
#define CELL_WIDTH_RATIO 0.05
#define INIT_TICK 0.8f
#define TICK_INC 0.05
#define FAST_TICK_HORIZONTAL .06f
#define FAST_TICK_VERTICAL 15.0f
#define SINGLE_TAP_DELAY 0.15f

#define NEW_LEVEL_POINTS 50
#define CLEAR_LINE_POINTS 10
#define CLEAR_ANIMATION_DURATION .3f
#define CLEAR_ANIMATION_SWITCH_DURATION CLEAR_ANIMATION_DURATION / 3.0f

#define GAME_OVER_ANIMATION_CELL_TIME .07f

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

typedef struct {
  int score_points;
  float tick;
  Color empty_cell_color;
  Color alive_cell_color;
  Color background_color;
} Level;

Color empty_cell_colors[] = {
    {0xFB, 0xF8, 0xCC, 0xFF}, {0xFD, 0xE4, 0xCF, 0xFF},
    {0xFF, 0xCF, 0xD2, 0xFF}, {0xF1, 0xC0, 0xE8, 0xFF},
    {0xCF, 0xBA, 0xF0, 0xFF}, {0xA3, 0xC4, 0xF3, 0xFF},
    {0x90, 0xDB, 0xF4, 0xFF}, {0x8E, 0xEC, 0xF5, 0xFF},
    {0x98, 0xF5, 0xE1, 0xFF}, {0xB9, 0xFB, 0xC0, 0xFF}};

Color alive_cell_colors[] = {{0xB7, 0x09, 0x4C, 0xFF}, {0xA0, 0x1A, 0x58, 0xFF},
                             {0x89, 0x2B, 0x64, 0xFF}, {0x72, 0x3C, 0x70, 0xFF},
                             {0x5C, 0x4D, 0x7D, 0xFF}, {0x45, 0x5E, 0x89, 0xFF},
                             {0x2E, 0x6F, 0x95, 0xFF}};

Color background_colors[] = {
    {0x04, 0x15, 0x1F, 0xFF}, {0x18, 0x3A, 0x37, 0xFF},
    {0x7B, 0x90, 0x4B, 0xFF}, {0xA0, 0x6D, 0x26, 0xFF},
    {0xC4, 0x49, 0x00, 0xFF}, {0x43, 0x25, 0x34, 0xFF}};

bool board[BOARD_WIDTH][BOARD_HEIGHT + BOARD_HEIGHT_EXTRA] = {0};
Tetromino tetromino_bag[7] = {0};
int tetromino_bag_used = 0;

static int screen_width;
static int screen_height;

Color empty_cell_color;
Color alive_cell_color;
Color background_color;

#define TOUCH_FAST_DOWN_HEIGHT_RATIO 0.2f
#define MAX_TOUCH_COUNT 2
Vector2 touch_pos[2];
int gesture;
int touch_points_count;

float last_tick_time = 0;
float delta_time = 0;
bool tick_time = 0;
float current_time = 0;
float last_tap_time = 0;

bool clear_animation = false;
int clear_lowest_y = 0;
int clear_shift_amount = 0;
float clear_animation_time = 0;
float clear_animation_switch_time = 0;

bool game_over_animation = false;
float game_over_animation_time = 0;
int game_over_animation_x;
int game_over_animation_y;

float last_horizontal_tick;
bool horizontal_move = false;
bool continuous_horizontal = false;

bool fast_shift_left = false;
bool fast_shift_right = false;
bool fast_shift_down = false;
bool fast_rotate = false;

Tetromino tetromino;
int current_level_num = 1;
Level init_level = {.tick = INIT_TICK,
                    .score_points = NEW_LEVEL_POINTS,
                    .empty_cell_color = (Color){0x1B, 0x49, 0x65, 0xFF},
                    .alive_cell_color = (Color){0x5F, 0xA8, 0xD3, 0xFF},
                    .background_color = BLACK};
Level current_level;

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
  game_points += clear_shift_amount * CLEAR_LINE_POINTS;
  printf("Game Points: %lld\n", game_points);
  clear_lowest_y = 0;
  clear_shift_amount = 0;
}

bool clear_animation_done(void) {
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
  current_level = init_level;
  current_level_num = 1;
  game_points = 0;
  game_over_animation = true;
}

bool game_over_animation_done(void) {
  if (!game_over_animation)
    return true;
  int x, y;
  game_over_animation_time += delta_time;
  for (y = game_over_animation_y; y < BOARD_HEIGHT + BOARD_HEIGHT_EXTRA; y++) {
    for (x = game_over_animation_x; x < BOARD_WIDTH; x++) {
      if (game_over_animation_time < GAME_OVER_ANIMATION_CELL_TIME)
        return false;
      if (board[x][y]) {
        board[x][y] = false;
        game_over_animation_time = 0;
        return false;
      } else {
        continue;
      }
    }
  }
  game_over();
  game_over_animation = false;
  return true;
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

Level create_random_level(void) {
  srand(time(NULL));
  return (Level){.tick = current_level.tick - TICK_INC,
                 .score_points = NEW_LEVEL_POINTS,
                 .empty_cell_color =
                     empty_cell_colors[rand() % (sizeof(empty_cell_colors) /
                                                 sizeof(Color))],
                 .alive_cell_color =
                     alive_cell_colors[rand() % (sizeof(alive_cell_colors) /
                                                 sizeof(Color))],
                 .background_color =
                     background_colors[rand() % (sizeof(background_colors) /
                                                 sizeof(Color))]};
}

void UpdateDrawFrame() {
  if (last_tick_time >= current_level.tick) {
    last_tick_time = 0.0;
    tick_time = true;
  }

  gesture = GetGestureDetected();
  touch_pos[0] = GetTouchPosition(0);
  touch_pos[1] = GetTouchPosition(1);
  touch_points_count = GetTouchPointCount();

  current_time = GetTime();
  delta_time = GetFrameTime();
  last_tick_time += delta_time;

  screen_width = GetScreenWidth();
  screen_height = GetScreenHeight();

  int cell_width = screen_height * CELL_WIDTH_RATIO;

  int x0 = (screen_width - cell_width * BOARD_WIDTH) / 2;
  int y0 = screen_height - cell_width * BOARD_HEIGHT -
           cell_width * BOARD_HEIGHT_EXTRA;

  // Clear up: remove clear_animation check. make like with game over
  if (clear_animation) {
    if (clear_animation_done()) {
      if (game_points / current_level_num == NEW_LEVEL_POINTS) {
        current_level = create_random_level();
        current_level_num++;
        printf("Level UP!\n");
        printf("Tick time: %f\n", current_level.tick);
      }
    };
    goto _draw;
  }

  if (!game_over_animation_done()) {
    goto _draw;
  }

  if (tick_time) {
    if (tetromino_grounded()) {
      printf("Grounded! Type: %d\n", tetromino.type);

      for (size_t i = 0; i < BOARD_WIDTH; i++) {
        if (board[i][BOARD_HEIGHT_EXTRA]) {
          game_over_animation = true;
          game_over_animation_y = BOARD_HEIGHT_EXTRA;
          game_over_animation_x = 0;
          goto _draw;
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

  if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_UP) ||
      ((gesture == GESTURE_SWIPE_DOWN || gesture == GESTURE_SWIPE_UP) &&
       touch_pos[0].y <
           screen_height - screen_height * TOUCH_FAST_DOWN_HEIGHT_RATIO) ||
      !FloatEquals(GetMouseWheelMove(), 0.0f)) {
    rotate_tetromino();
    last_tap_time = current_time;
    goto _draw;
  }

  if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT) ||
      (gesture == GESTURE_TAP &&
       current_time - last_tap_time > SINGLE_TAP_DELAY &&
       touch_pos[0].y <
           screen_height - screen_height * TOUCH_FAST_DOWN_HEIGHT_RATIO &&
       touch_pos[0].x < screen_width / 2.0f)) {
    last_horizontal_tick = 0;
    move_tetromino(Left);
    last_tap_time = current_time;
  }

  if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT) ||
      (gesture == GESTURE_TAP &&
       current_time - last_tap_time > SINGLE_TAP_DELAY &&
       touch_pos[0].y <
           screen_height - screen_height * TOUCH_FAST_DOWN_HEIGHT_RATIO &&
       touch_pos[0].x >= screen_width / 2.0f)) {
    last_horizontal_tick = 0;
    move_tetromino(Right);
    last_tap_time = current_time;
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

  if ((IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) ||
      (gesture == GESTURE_HOLD &&
       touch_pos[0].y >=
           screen_height - screen_height * TOUCH_FAST_DOWN_HEIGHT_RATIO)) {
    last_tick_time += FAST_TICK_VERTICAL * delta_time;
  }

_draw:
  BeginDrawing();
  ClearBackground(current_level.background_color);
  for (size_t y = BOARD_HEIGHT_EXTRA; y < BOARD_HEIGHT + BOARD_HEIGHT_EXTRA;
       y++) {
    for (size_t x = 0; x < BOARD_WIDTH; x++) {
      if (board[x][y]) {
        DrawRectangle(
            x0 + x * cell_width, y0 + y * cell_width, cell_width - 5,
            cell_width - 5,
            current_level.alive_cell_color); // - 5 maybe something else
      } else {
        DrawRectangle(
            x0 + x * cell_width, y0 + y * cell_width, cell_width - 5,
            cell_width - 5,
            current_level.empty_cell_color); // - 5 maybe something else
      }
    }
  }
  EndDrawing();
}

int main(void) {
  InitWindow(800, 900, "tetris");
#ifndef __EMSCRIPTEN__
  Image icon = {.data = icon_rgba,
                .width = 32,
                .height = 32,
                .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                .mipmaps = 1};
  SetWindowIcon(icon);
#endif

  SetWindowState(FLAG_WINDOW_RESIZABLE);

#ifdef __EMSCRIPTEN__
  SetWindowState(FLAG_WINDOW_MAXIMIZED);
#endif

  SetGesturesEnabled(GESTURE_TAP | GESTURE_HOLD | GESTURE_SWIPE_DOWN |
                     GESTURE_SWIPE_UP);

  empty_cell_color = GetColor(0x1b4965FF);
  alive_cell_color = GetColor(0x5fa8d3FF);
  background_color = BLACK;

  last_tick_time = 0;
  last_horizontal_tick = 0;

  current_level = init_level;
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
