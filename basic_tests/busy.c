#include <math.h>
#include <stdio.h>

volatile int val = 0;

int main() {
  double ss=0;
  
  while (!val) {
    ss += sin(3.1444);
  }
  
  return 1;
}

