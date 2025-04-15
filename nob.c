#define NOB_IMPLEMENTATION

#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"

#define PLATFORM "-DPLATFORM_DESKTOP"

#ifdef _WIN32
#define STATIC_LIB_NAME "raylib.lib"
#endif
#ifdef __linux__
#define STATIC_LIB_NAME "libraylib.a"
#endif

// TODO: add release/debug flags
int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  nob_mkdir_if_not_exists(BUILD_FOLDER);

  Nob_Cmd cmd = {0};

#define RAYLIB_OBJ_COUNT 6
  char *raylib_object_files[RAYLIB_OBJ_COUNT] = {
      BUILD_FOLDER "rcore.o",     BUILD_FOLDER "rshapes.o",
      BUILD_FOLDER "rtextures.o", BUILD_FOLDER "rtext.o",
      BUILD_FOLDER "rglfw.o",     BUILD_FOLDER "utils.o"};

  if (!nob_file_exists("./build/raylib.lib")) {
    // Raylib

    // TODO: make a loop
    nob_cmd_append(&cmd, "cc", PLATFORM, "-DSUPPORT_WINMM",
                   "-I./third_party/raylib/src/", "-c",
                   "third_party/raylib/src/rcore.c",
                   "-I./third_party/raylib/src/external/glfw/include/", "-o",
                   "./build/rcore.o");
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;
    nob_cmd_append(&cmd, "cc", PLATFORM, "-DSUPPORT_WINMM", "-I",
                   "./third_party/raylib/src/", "-c",
                   "third_party/raylib/src/rshapes.c", "-o",
                   "./build/rshapes.o");
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;
    nob_cmd_append(&cmd, "cc", PLATFORM, "-DSUPPORT_WINMM", "-I",
                   "./third_party/raylib/src/", "-c",
                   "third_party/raylib/src/rtextures.c", "-o",
                   "./build/rtextures.o");
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;
    nob_cmd_append(&cmd, "cc", PLATFORM, "-DSUPPORT_WINMM", "-I",
                   "./third_party/raylib/src/", "-c",
                   "third_party/raylib/src/rtext.c", "-o", "./build/rtext.o");
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;
    nob_cmd_append(&cmd, "cc", PLATFORM, "-DSUPPORT_WINMM", "-I",
                   "./third_party/raylib/src/", "-c",
                   "third_party/raylib/src/rglfw.c", "-o", "./build/rglfw.o");
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;
    nob_cmd_append(&cmd, "cc", PLATFORM, "-DSUPPORT_WINMM", "-I",
                   "./third_party/raylib/src/", "-c",
                   "third_party/raylib/src/utils.c", "-o", "./build/utils.o");
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;

    nob_cmd_append(&cmd, "ar", "rcs", BUILD_FOLDER STATIC_LIB_NAME);
    for (size_t i = 0; i < RAYLIB_OBJ_COUNT; i++) {
      nob_cmd_append(&cmd, raylib_object_files[i]);
    }
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;

    for (size_t i = 0; i < RAYLIB_OBJ_COUNT; i++) {
      nob_cmd_append(&cmd, "rm");
      nob_cmd_append(&cmd, raylib_object_files[i]);
      nob_cmd_run_sync_and_reset(&cmd);
    }
  }

  nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", BUILD_FOLDER "tetris",
                 SRC_FOLDER "main.c");

  nob_cmd_append(&cmd, "-I./third_party/raylib/src", "-L./build/", "-lraylib");
#ifdef _WIN32
  nob_cmd_append(&cmd, "-lopengl32", "-lgdi32", "-lwinmm");
#endif
#ifdef __linux__
  nob_cmd_append($cmd, "-lGL",
                 "-ldl"
                 "-lpthread"
                 "-lX11");
#endif

  nob_cmd_append(&cmd, "-static");

  // nob_cmd_render(cmd, &sb);
  if (!nob_cmd_run_sync(cmd))
    return 1;
  return 0;
}
