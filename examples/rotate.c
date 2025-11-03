
#include <poll.h>
#include <stdio.h>
#include <string.h>

int m(int a, int b) { return (a * b + 5000) / 10000; }
void a(int *c, int *s, int d, int t) {
  int k = m(*c, d) - m(*s, t);
  int l = m(*s, d) + m(*c, t);
  *c = k;
  *s = l;
}
int main() {
  int z[1760];
  char b[1760];
  printf("\033[2J");
  int q = 10000;
  int r = 0;
  int u = 10000;
  int v = 0;
  for (;;) {
    memset(b, 32, 1760);
    memset(z, 0, 1760 * sizeof(int));
    int l = 0;
    int p = 10000;
    for (int i = 0; i < 88; i = i + 1) {
      int w = 0;
      int e = 10000;
      for (int j = 0; j < 314; j = j + 1) {
        int f = p + 20000;
        int g = 100000000 / (m(m(w, f), r) + m(l, q) + 50000);
        int t = m(m(w, q), f) - m(l, r);
        int x = 40 + 30 * m(g, m(m(e, u), f) - m(t, v)) / 10000;
        int y = 12 + 15 * m(g, m(m(e, v), f) + m(t, u)) / 10000;
        int o = x + 80 * y;
        int N = 8 * (m(m(l, r) - m(m(w, q), p), u) - m(m(w, r), p) - m(l, q) - m(m(e, v), p)) / 10000;
        if (22 > y) {
          if (y > 0) {
            if (x > 0) {
              if (80 > x) {
                if (g > z[o]) {
                  z[o] = g;
                  if (N >= 1) {
                    b[o] = ".,-~:;=!*#$@"[N];
                  } else {
                    b[o] = ".,-~:;=!*#$@"[0];
                  }
                }
              }
            }
          }
        }
        a(&e, &w, 9998, 200);
      }
      a(&p, &l, 9974 + i % 2, 714);
    }
    printf("\033[H");
    for (int k = 0; k < 1761; k = k + 1) {
      if (k % 80) {
      } else {
        putchar(10);
      }
      putchar(b[k]);
    }
    a(&q, &r, 9992, 400);
    a(&u, &v, 9998, 200);
    poll(NULL, 0, 50);
  }
  return 0;
}
