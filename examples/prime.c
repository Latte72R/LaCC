
void printf();

// 素数を30個表示する
int main() {
  int prm[30];
  prm[0] = 2;
  int n = 1;
  for (int i = 3; n < 30; i++) {
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
  }
  printf("\n");
  return 0;
}
