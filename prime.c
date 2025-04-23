
int printf();

// 素数を30個表示する
int main() {
  int prm[30];
  prm[0] = 2;
  int i = 3;
  int n = 1;
  int j;
  while (1) {
    for (j = 0; j < n; j++) {
      if (i % prm[j] == 0)
        break;
      if (j == n - 1) {
        prm[n] = i;
        n++;
        printf("%d ", i);
        break;
      }
    }
    if (n == 30)
      break;
    i++;
  }
  printf("\n");
  return 0;
}
