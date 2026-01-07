#include <math.h>
#include <stdio.h>

//volatile int val = 0;

int main() {
  //double ss=0;

  //  while (!val) {
    while (1) {
    asm volatile("wfe \n\t");
  }
	
  return 1;
}

