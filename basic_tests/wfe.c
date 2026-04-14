#include <math.h>
#include <stdio.h>

int main() {
  
  while (1) {
    asm volatile("wfe \n\t");
  }
	
  return 1;
}

