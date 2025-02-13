
int _pc(int c);
int _pd(int d);

int main() {
  int number = 30;
  int i;
  for (i = 1; i < number + 1; i = i + 1) {
    if (i % 15 == 0) {
      _pc(70);
      _pc(105);
      _pc(122);
      _pc(122);
      _pc(66);
      _pc(117);
      _pc(122);
      _pc(122);
      _pc(32);
    } else if (i % 3 == 0) {
      _pc(70);
      _pc(105);
      _pc(122);
      _pc(122);
      _pc(32);
    } else if (i % 5 == 0) {
      _pc(66);
      _pc(117);
      _pc(122);
      _pc(122);
      _pc(32);
    } else {
      _pd(i);
      _pc(32);
    }
  }
  _pc(10);
  return 0;
}
