
int printf();

int main() {
  int prm[30];
  prm[0] = 2;
  int i = 3;
  int n = 1;
  int j;
  while (1) {
    for (j = 0; j < n; j = j + 1) {
      if (i % prm[j] == 0) {
        break;
      }
      if (j == n - 1) {
        prm[n] = i;
        n = n + 1;
        printf("%d ", i);
        break;
      }
    }
    if (n == 30) {
      break;
    }
    i = i + 1;
  }
  printf("\n");
  return 0;
}
