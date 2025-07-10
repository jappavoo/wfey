O:=3
CFLAGS += -g -O${O} -std=gnu99 -MD -MP -Wall -Werror
clinesize=$(shell cat /sys/devices/system/cpu/cpu0/cache/*/coherency_line_size | head -1)
CFLAGS += -D COHERENCY_LINE_SIZE=${clinesize} 
TARGETS = busypoll_nodb_wfey busypoll_db_wfey wfe_nodb_wfey wfe_db_nomon_wfey wfe_db_mon_wfey
.PHONY: clean

all: ${TARGETS}
###
busypoll_nodb_wfey: busypoll_nodb_wfey.o
	${CC} ${CFLAGS} -o $@ $^ -lpthread
busypoll_nodb_wfey.o:
	${CC} ${CFLAGS} -c -DBUSY_POLL -o $@ wfey.c 

###
busypoll_db_wfey: busypoll_db_wfey.o
	${CC} ${CFLAGS} -o $@ $^ -lpthread
busypoll_db_wfey.o:
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -DBUSY_POLL -o $@ wfey.c 

### use wfe but no doorbell to help filter suprious
wfe_nodb_wfey: wfe_nodb_wfey.o
	${CC} ${CFLAGS} -o $@ $^ -lpthread
wfe_nodb_wfey.o:
	${CC} ${CFLAGS} -c -o $@ wfey.c

### use wfe with doorbells but no monitor 
wfe_db_nomon_wfey: wfe_db_nomon_wfey.o
	${CC} ${CFLAGS} -o $@ $^ -lpthread
wfe_db_nomon_wfey.o:
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -o $@ wfey.c

### use wfe with doorbells and monitors
wfe_db_mon_wfey: wfe_db_mon_wfey.o
	${CC} ${CFLAGS} -o $@ $^ -lpthread
wfe_db_mon_wfey.o:
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -USE_MONITOR -o $@ wfey.c 

clean:
	rm -f $(wildcard *.o *.d ${TARGETS})
