#include <cstdio>
#include <cstring>

void func2(int *p) noexcept {
  printf("p in func2 points to %p\n", p);
  // *p = 2;
  return;
}

void change(int *&p) noexcept {
  printf("before change, p points to: %p\n", p);
  p = new int[2];
  printf("after change, p points to: %p\n", p);
}

void func1(int *p) noexcept {
  printf("p in func1 points to %p\n", p);
  func2(p);
  return;
}

int main() {
  int *p = nullptr;
  p = new int[2];
  p[0] = 1;
  printf("p in main points to %p\n", p);
  func1(p);
  change(p);
  printf("p in main points to %p\n", p);
  func1(p);
  return 0;
}
