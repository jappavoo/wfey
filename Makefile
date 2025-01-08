O:=0
CFLAGS += -g -O${O} -std=gnu99 -MD -MP -Wall -Werror
clinesize=$(shell cat /sys/devices/system/cpu/cpu0/cache/*/coherency_line_size | head -1)
CFLAGS += -D COHERENCY_LINE_SIZE=${clinesize} 

.PHONY: clean

wfey: wfey.o
	${CC} ${CFLAGS} -o $@ $^ -lpthread	


clean:
	rm -f $(wildcard *.o wfey)
