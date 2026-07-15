O:=0
CFLAGS += -g -O${O} -std=gnu99 -MD -MP -Wall -Werror
clinesize=$(shell cat /sys/devices/system/cpu/cpu0/cache/*/coherency_line_size | head -1)
CFLAGS += -D COHERENCY_LINE_SIZE=${clinesize}
LIBRARIES = -lpthread -lm
INCLUDES = include

TARGETS = busypoll_nodb busypoll_db wfe_nodb wfe_db_nomon wfe_db_mon hwmon_test
.PHONY: clean

all: ${TARGETS}
###
busypoll_nodb: busypoll_nodb.o wfey_hwmon.o logging.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
busypoll_nodb.o: wfey.c
	${CC} ${CFLAGS} -c -DBUSY_POLL -o $@ wfey.c 

###
busypoll_db: busypoll_db.o wfey_hwmon.o logging.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
busypoll_db.o: wfey.c
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -DBUSY_POLL -o $@ wfey.c 

### use wfe but no doorbell to help filter suprious
wfe_nodb: wfe_nodb.o wfey_hwmon.o logging.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
wfe_nodb.o: wfey.c
	${CC} ${CFLAGS} -c -o $@ wfey.c

### use wfe with doorbells but no monitor 
wfe_db_nomon: wfe_db_nomon.o wfey_hwmon.o logging.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
wfe_db_nomon.o: wfey.c
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -o $@ wfey.c

### use wfe with doorbells and monitors
wfe_db_mon: wfe_db_mon.o wfey_hwmon.o logging.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
wfe_db_mon.o: wfey.c
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -DUSE_MONITOR -o $@ wfey.c 

logging.o: logging.c ${INCLUDES}/logging.h
	${CC} ${CFLAGS} -c $^

wfey_hwmon.o: wfey_hwmon.c ${INCLUDES}/wfey_hwmon.h
	${CC} ${CFLAGS} -c $^ 
hwmon_test: wfey_hwmon.c ${INCLUDES}/wfey_hwmon.h
	${CC} -D __STAND_ALONE__ ${CFLAGS} -o $@ $^

clean:
	rm -f $(wildcard *~ *.o *.d *.h.gch *.out ${TARGETS})
