
void printf();

// 素数を30個表示する
int main() {
  int prm[30];
  prm[0] = 2;
  int i = 3;
  int n = 1;
  while (n < 30) {
    for (int j = 0; j < n; j++) {
      if (i % prm[j] == 0)
        break;
      if (j == n - 1) {
        prm[n] = i;
        n++;
        printf("%d ", i);
        break;
      }
    }
    i++;
  }
  printf("\n");
  return 0;
}
