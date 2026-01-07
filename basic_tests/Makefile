O:=0
CFLAGS += -g -O${O} -std=gnu99 -Wall -Werror
TARGETS = wfe sev busy

.PHONY: clean

all: ${TARGETS}

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(wildcard *.o *.txt ${TARGETS})
