#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#define NOB_IMPLEMENTATION

#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"

#define PATH_TO_EMSCRIPTEN_SDK "C:/emsdk"

#define PLATFORM "-DPLATFORM_DESKTOP"

#if defined(_WIN32)

#define STATIC_LIB_NAME "raylib.lib"
#include <direct.h>
#define getcwd _getcwd

#elif defined(__linux__)
#include <unistd.h>
#define STATIC_LIB_NAME "libraylib.a"
#endif

#define RELEASE_FLAG "-release"
#define PLATFORM_FLAG_PREFIX "-DPLATFORM_"

#define WEB_CC "emcc"
#define DEFAULT_CC "gcc"

#define DESKTOP_FLAGS "-DSUPPORT_WINMM"
#define WEB_FLAGS "-Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2"

char *platforms[] = {"-DPLATFORM_DESKTOP", "-DPLATFORM_WEB"};
char platform[30] = PLATFORM;
char working_dir[1024];
Nob_String_Builder sb = {0};
Nob_String_View sv = {0};

bool release = false;
bool web = false;

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  if (getcwd(working_dir, sizeof(working_dir)) == NULL) {
    nob_log(NOB_INFO, "getcwd()\n");
    return 1;
  }

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
#define RGLFW_INDEX 4
  char *raylib_headers[RAYLIB_OBJ_COUNT] = {
      "third_party/raylib/src/rcore.c",     "third_party/raylib/src/rshapes.c",
      "third_party/raylib/src/rtextures.c", "third_party/raylib/src/rtext.c",
      "third_party/raylib/src/rglfw.c",     "third_party/raylib/src/utils.c"};

  char *raylib_build_object_files[RAYLIB_OBJ_COUNT] = {
      BUILD_FOLDER "rcore.o",     BUILD_FOLDER "rshapes.o",
      BUILD_FOLDER "rtextures.o", BUILD_FOLDER "rtext.o",
      BUILD_FOLDER "rglfw.o",     BUILD_FOLDER "utils.o"};

  if (web || !nob_file_exists("./build/" STATIC_LIB_NAME) || release) {
    // Raylib
    nob_log(NOB_INFO, "Building raylib.lib");

    for (size_t i = 0; i < RAYLIB_OBJ_COUNT; i++) {
      if (web) {
        // emcc provides its own iplementation?
        if (i == RGLFW_INDEX)
          continue;
        nob_cmd_append(&cmd, WEB_CC, platform, "-I./third_party/raylib/src/",
                       "-Os", "-DPLATFORM_WEB", "-DGRAPHICS_API_OPENGL_ES2",
                       "-c", raylib_headers[i],
                       "-I./third_party/raylib/src/external/glfw/include/",
                       "-o", raylib_build_object_files[i]);
        // For some reason nob doesn't see emcc

        sb.count = 0;
        nob_cmd_render(cmd, &sb);
        nob_sb_append_null(&sb);
        sv = nob_sb_to_sv(sb);
        printf("System(%s)\n", sv.data);
        if (system(sv.data) != 0) {
          exit(1);
        };
        cmd.count = 0;
        continue;
      }

      nob_cmd_append(&cmd, DEFAULT_CC, platform, "-I",
                     "./third_party/raylib/src/", "-c", raylib_headers[i], "-I",
                     "./third_party/raylib/src/external/glfw/include/", "-o",
                     raylib_build_object_files[i]);

#ifdef __linux__
      nob_cmd_append(&cmd, "-D_GLFW_X11");
#endif
      if (release)
        nob_cmd_append(&cmd, "-O3");

      if (!nob_cmd_run_sync_and_reset(&cmd))
        return 1;
    }

    nob_log(NOB_INFO, "Linking .o's");
    if (web) {
      cmd.count = 0;
      nob_cmd_append(&cmd, "emar", "rcs", BUILD_FOLDER STATIC_LIB_NAME);
    } else {
      nob_cmd_append(&cmd, "ar", "rcs", BUILD_FOLDER STATIC_LIB_NAME);
    }

    for (size_t i = 0; i < RAYLIB_OBJ_COUNT; i++) {
      if (web && i == RGLFW_INDEX)
        continue;
      nob_cmd_append(&cmd, raylib_build_object_files[i]);
    }

    if (web) {
      sb.count = 0;
      nob_cmd_render(cmd, &sb);
      nob_sb_append_null(&sb);
      sv = nob_sb_to_sv(sb);
      printf("System(%s)\n", sv.data);
      if (system(sv.data) != 0) {
        exit(1);
      };
      cmd.count = 0;
      sb.count = 0;
    } else {
      if (!nob_cmd_run_sync_and_reset(&cmd))
        return 1;
    }

    for (size_t i = 0; i < RAYLIB_OBJ_COUNT; i++) {
      if (web && i == RGLFW_INDEX)
        continue;
      nob_cmd_append(&cmd, "rm");
      nob_cmd_append(&cmd, raylib_build_object_files[i]);
      if (!nob_cmd_run_sync_and_reset(&cmd))
        return 1;
    }
  }

  nob_log(NOB_INFO, "Building the game");
  if (web) {
    nob_cmd_append(&cmd, WEB_CC, "-o", BUILD_FOLDER "tetris.html",
                   SRC_FOLDER "main.c", "-Os", "-Wall",
                   BUILD_FOLDER STATIC_LIB_NAME, "-s", "USE_GLFW=3", "-I",
                   "./third_party/raylib/src/", "-L",
                   "./" BUILD_FOLDER "raylib.lib", platform);
    nob_cmd_render(cmd, &sb);
    nob_sb_append_null(&sb);
    sv = nob_sb_to_sv(sb);
    printf("%s\n", sv.data);
    if (system(sb.items) != 0) {
      exit(1);
    }
    // if (!nob_cmd_run_sync_and_reset(&cmd))
    //   return 1;
    return 0;
  }
  nob_cmd_append(&cmd, DEFAULT_CC, "-o", BUILD_FOLDER "tetris",
                 SRC_FOLDER "main.c");

  if (!release) {
    nob_cmd_append(&cmd, "-g", "-ggdb", "-Wall", "-Wextra");
  }
  if (release) {
    nob_cmd_append(&cmd, "-O3");
  }
  nob_cmd_append(&cmd, "-I", "./third_party/raylib/src/", "-L", "./build/",
                 "-lraylib");
#ifdef _WIN32
  nob_cmd_append(&cmd, "-lopengl32", "-lgdi32", "-lwinmm");
#endif
#ifdef __linux__
  nob_cmd_append(&cmd, "-lGL", "-ldl", "-lpthread", "-lX11", "-lm");
#endif

#ifdef _WIN32
  nob_cmd_append(&cmd, "-static");
#endif

  // nob_cmd_render(cmd, &sb);
  if (!nob_cmd_run_sync(cmd))
    return 1;

  return 0;
}
