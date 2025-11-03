int comma_basic() {
  return (1, 2);
}

int comma_assignments() {
  int x = 0;
  int y = (x = 3, x + 4);
  return x + y;
}

int comma_chained_side_effects() {
  int x = 1;
  int y = 2;
  int result = (x += y, y += x, y - x);
  return result + x + y;
}

int comma_in_for_loop() {
  int sum = 0;
  int i = 0;
  int j = 0;
  for (i = 0, j = 0; i < 4; i++, j += 2) {
    sum += i + j;
  }
  return sum;
}
