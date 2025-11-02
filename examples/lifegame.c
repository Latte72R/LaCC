// Edited by uint256_t

// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>

int printf(char *msg);
int usleep(int time);

int count_nbr(int (*grid)[20], int i, int j, int size);

void init_grid(int (*grid)[20], int *grid_binaries) {
  for (int i = 0; i < 20; i = i + 1) {
    for (int j = 0; j < 20; j = j + 1) {
      grid[i][j] = (grid_binaries[i] >> (19 - j)) & 1;
    }
  }
}

int N_STEP = 30000;

int main() {
  int neighbour_count[20][20];
  int grid[20][20];
  int grid_binaries[20] = {
      0b00000000000000000000, 0b00000000000000000000, 0b00000000000000000000, 0b00000000000000000000,
      0b00000000000000000000, 0b00000000000000000000, 0b00000000000000000000, 0b00000111111111100000,
      0b00000000000000000000, 0b00000000000000000000, 0b00000000000000000000, 0b00000000000000000000,
      0b00000000000000000000, 0b00000000000000000000, 0b00111100000000000000, 0b01000100000000000000,
      0b00000100000000000000, 0b01001000000000000000, 0b00000000000000000000, 0b00000000000000000000};
  init_grid(grid, grid_binaries);
  int i;
  int j;
  int steps;

  for (steps = 0; steps < N_STEP; steps = steps + 1) {
    printf("\033[0;0H");
    for (i = 0; i < 20; i = i + 1) {
      printf("\n");
      for (j = 0; j < 20; j = j + 1) {
        if (grid[i][j] == 1)
          printf("\033[42m  \033[m");
        else
          printf("\033[47m  \033[m");
        neighbour_count[i][j] = count_nbr(grid, i, j, 20);
      }
    }

    for (i = 0; i < 20; i = i + 1) {
      for (j = 0; j < 20; j = j + 1) {
        if (grid[i][j] >= 1) {
          if (neighbour_count[i][j] <= 1) {
            grid[i][j] = 0;
          } else if (neighbour_count[i][j] >= 4)
            grid[i][j] = 0;
        } else if (neighbour_count[i][j] == 3)
          grid[i][j] = 1;
      }
    }

    usleep(100000);
  }

  return 0;
}

int count_nbr(int (*grid)[20], int i, int j, int size) {
  int n_count = 0;
  if (i - 1 >= 0) {
    if (j - 1 >= 0) {
      if (grid[i - 1][j - 1] >= 1)
        n_count = n_count + 1;
    }
  }

  if (i - 1 >= 0) {
    if (grid[i - 1][j] >= 1)
      n_count = n_count + 1;
  }

  if (i - 1 >= 0) {
    if (j + 1 < size) {
      if (grid[i - 1][j + 1] >= 1)
        n_count = n_count + 1;
    }
  }

  if (j - 1 >= 0) {
    if (grid[i][j - 1] >= 1)
      n_count = n_count + 1;
  }

  if (j + 1 < size) {
    if (grid[i][j + 1] >= 1)
      n_count = n_count + 1;
  }

  if (i + 1 < size) {
    if (j - 1 >= 0) {
      if (grid[i + 1][j - 1] >= 1)
        n_count = n_count + 1;
    }
  }

  if (i + 1 < size) {
    if (grid[i + 1][j] >= 1)
      n_count = n_count + 1;
  }

  if (i + 1 < size) {
    if (j + 1 < size) {
      if (grid[i + 1][j + 1] >= 1)
        n_count = n_count + 1;
    }
  }

  return n_count;
}
