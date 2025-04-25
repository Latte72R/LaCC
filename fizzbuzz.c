
void printf();

// FizzBuzz
int main() {
  int number = 30;
  int i;
  for (i = 1; i < number + 1; i = i + 1) {
    if (i % 15 == 0) {
      printf("FizzBuzz ");
    } else if (i % 3 == 0) {
      printf("Fizz ");
    } else if (i % 5 == 0) {
      printf("Buzz ");
    } else {
      printf("%d ", i);
    }
  }
  printf("\n");
  return 0;
}
