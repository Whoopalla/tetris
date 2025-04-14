#define NOB_IMPLEMENTATION

#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  nob_mkdir_if_not_exists(BUILD_FOLDER);

  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", BUILD_FOLDER "tetris",
                 SRC_FOLDER "main.c");
  // Raylib
  nob_cmd_append(&cmd, "-I", "./include/",
                 "-L"
                 "./lib/",
                 "-lraylib");
#ifdef _WIN32
  nob_cmd_append(&cmd, "-lopengl32", "-lgdi32", "-lwinmm");
#endif
#ifdef __linux__
  nob_cmd_append($cmd, "-lGL",
                 "-ldl"
                 "-lpthread"
                 "-lX11");
#endif
  // Nob_String_Builder sb = {0};

  // nob_cmd_render(cmd, &sb);
  if (!nob_cmd_run_sync(cmd))
    return 1;
  return 0;
}
