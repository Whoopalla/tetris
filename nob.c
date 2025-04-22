#include <stddef.h>
#define NOB_IMPLEMENTATION

#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"

#define PATH_TO_EMSCRIPTEN_SDK "C:/emsdk/"

#define PLATFORM "-DPLATFORM_DESKTOP"

#if defined(_WIN32)
#define STATIC_LIB_NAME "raylib.lib"
#elif defined(__linux__)
#define STATIC_LIB_NAME "libraylib.a"
#else
#error "Unsupported platform: STATIC_LIB_NAME not defined"
#endif

#define RELEASE_FLAG "-release"
#define PLATFORM_FLAG_PREFIX "-DPLATFORM_"

#define WEB_CC "emcc"
#define DEFAULT_CC "cc"

#define DESKTOP_FLAGS "-DSUPPORT_WINMM"
#define WEB_FLAGS "-Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2"

char *platforms[] = {"-DPLATFORM_DESKTOP", "-DPLATFORM_WEB"};
char platform[30] = PLATFORM;

bool release = false;
bool web = false;

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  for (size_t i = 0; i < argc; i++) {
    if (strcmp(RELEASE_FLAG, argv[i]) == 0) {
      nob_log(NOB_INFO, "RELEASE Flag Detected");
      release = true;
    }
    if (strncmp(PLATFORM_FLAG_PREFIX, argv[i], strlen(PLATFORM_FLAG_PREFIX)) ==
        0) {
      for (size_t p = 0; p < sizeof(platforms) / sizeof(char *); p++) {
        if (strcmp(platforms[p], argv[i]) == 0) {
          nob_log(NOB_INFO, "Selected Platform: %s\n", argv[i]);
          strcpy(platform, platforms[p]);
          if (strcmp(platform, "-DPLATFORM_WEB") == 0) {
            web = true;
            nob_log(NOB_INFO, "Compiling for web");
          }
        }
      }
      nob_log(NOB_INFO, "Default Platform: %s\n", PLATFORM);
    }
  }

  nob_mkdir_if_not_exists(BUILD_FOLDER);

  Nob_Cmd cmd = {0};

#define RAYLIB_OBJ_COUNT 6
  char *raylib_headers[RAYLIB_OBJ_COUNT] = {
      "third_party/raylib/src/rcore.c",     "third_party/raylib/src/rshapes.c",
      "third_party/raylib/src/rtextures.c", "third_party/raylib/src/rtext.c",
      "third_party/raylib/src/rglfw.c",     "third_party/raylib/src/utils.c"};

  char *raylib_build_object_files[RAYLIB_OBJ_COUNT] = {
      BUILD_FOLDER "rcore.o",     BUILD_FOLDER "rshapes.o",
      BUILD_FOLDER "rtextures.o", BUILD_FOLDER "rtext.o",
      BUILD_FOLDER "rglfw.o",     BUILD_FOLDER "utils.o"};

  if (web) {
    // Setup emscripten environment
#ifdef _WIN32
    nob_cmd_append(&cmd, "cd", PATH_TO_EMSCRIPTEN_SDK);
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;
    nob_cmd_append(&cmd, "./emcmdprompt.bat");
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;
#endif
#ifdef __linux__
    nob_cmd_append(&cmd, "source", PATH_TO_EMSCRIPTEN_SDK "/emsdk_env.sh");
#endif
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;
  }

  if (!nob_file_exists("./build/raylib.lib") || release) {
    // Raylib

    for (size_t i = 0; i < RAYLIB_OBJ_COUNT; i++) {
      nob_cmd_append(&cmd, web ? WEB_CC : DEFAULT_CC, PLATFORM,
                     web ? WEB_FLAGS : DESKTOP_FLAGS,
                     "-I./third_party/raylib/src/", "-c", raylib_headers[i],
                     "-I./third_party/raylib/src/external/glfw/include/", "-o",
                     raylib_build_object_files[i]);
      if (release && !web)
        nob_cmd_append(&cmd, "-O3");
      if (!nob_cmd_run_sync_and_reset(&cmd))
        return 1;
    }

    if (web) {
      nob_cmd_append(&cmd, "emar", "rcs", BUILD_FOLDER STATIC_LIB_NAME);
    } else {
      nob_cmd_append(&cmd, "ar", "rcs", BUILD_FOLDER STATIC_LIB_NAME);
    }

    for (size_t i = 0; i < RAYLIB_OBJ_COUNT; i++) {
      nob_cmd_append(&cmd, raylib_build_object_files[i]);
    }
    if (!nob_cmd_run_sync_and_reset(&cmd))
      return 1;

    for (size_t i = 0; i < RAYLIB_OBJ_COUNT; i++) {
      nob_cmd_append(&cmd, "rm");
      nob_cmd_append(&cmd, raylib_build_object_files[i]);
      if (!nob_cmd_run_sync_and_reset(&cmd))
        return 1;
    }
  }

  if (web) {
    nob_cmd_append(&cmd, WEB_CC, "-o", BUILD_FOLDER "tetris.html",
                   SRC_FOLDER "main.c", "-Os", "-Wall", "-s", "USE_GLFW3",
                   "--shell-file", "./third_party/raylib/src/shell.html",
                   platform);
    if (!nob_cmd_run_sync(cmd))
      return 1;
    return 0;
  }
  nob_cmd_append(&cmd, "cc", "-o", BUILD_FOLDER "tetris", SRC_FOLDER "main.c");

  if (!release) {
    nob_cmd_append(&cmd, "-g", "-ggdb", "-Wall", "-Wextra");
  }
  if (release) {
    nob_cmd_append(&cmd, "-O3");
  }
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
