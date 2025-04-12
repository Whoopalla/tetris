#include <stdio.h>
#include <stdlib.h>

#define FLAGS "-g -ggdb -Wall -Wextra -Wpedantic -L ./lib -I ./include -lopengl32  -lraylib -lm -lgdi32 -lwinmm -mwindows -lUser32 -mconsole"

int main(void) {
  printf("...Building\n");
#ifdef _WIN32
  if (system("IF NOT EXIST build mkdir build") != 0) {
    printf("Error while creating the folder stucture\n");
    return 1;
  }

  if (system("IF EXIST source_files.txt del source_files.txt") != 0) {
    printf("Error deletint source_files.txt\n");
    return 1;
  }

  if (system("for %f in (./src/*.c) do @echo ./src/%f >> source_files.txt "
             "&& gcc @source_files.txt -o ./build/tetris " FLAGS " "
             "&& del source_files.txt") != 0) {
    printf("***Error\n");
    return 1;
  }
#endif
#ifdef __linux__
  if (system("mkdir -p ./build")) {
    printf("Error while creating the folder stucture\n");
    return 1;
  }

  if (system("rm ./source_files.txt")) {
    printf("Error deletint source_files.txt\n");
    return 1;
  }

  if (system("gcc ./src/*.c -o ./build/tetris") != 0) {
    printf("***Error\n");
    return 1;
  }
#endif
  printf("...Done!\n");
  return 0;
}
