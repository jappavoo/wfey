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
busypoll_nodb: busypoll_nodb.o wfey_hwmon.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
busypoll_nodb.o: wfey.c
	${CC} ${CFLAGS} -c -DBUSY_POLL -o $@ wfey.c 

###
busypoll_db: busypoll_db.o wfey_hwmon.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
busypoll_db.o: wfey.c
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -DBUSY_POLL -o $@ wfey.c 

### use wfe but no doorbell to help filter suprious
wfe_nodb: wfe_nodb.o wfey_hwmon.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
wfe_nodb.o: wfey.c
	${CC} ${CFLAGS} -c -o $@ wfey.c

### use wfe with doorbells but no monitor 
wfe_db_nomon: wfe_db_nomon.o wfey_hwmon.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
wfe_db_nomon.o: wfey.c
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -o $@ wfey.c

### use wfe with doorbells and monitors
wfe_db_mon: wfe_db_mon.o wfey_hwmon.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBRARIES}
wfe_db_mon.o: wfey.c
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -DUSE_MONITOR -o $@ wfey.c 

wfey_hwmon.o: wfey_hwmon.c ${INCLUDES}/wfey_hwmon.h
	${CC} ${CFLAGS} -c $^ 
hwmon_test: wfey_hwmon.c ${INCLUDES}/wfey_hwmon.h
	${CC} -D __STAND_ALONE__ ${CFLAGS} -o $@ $^

gpu_source.o: gpu_source.cu include/gpu_source.h
	nvcc -O0 -g -std=c++17 -c -o $@ gpu_source.cu

wfe_db_mon_gpu_src.o: wfey.c include/gpu_source.h
	${CC} ${CFLAGS} -c -DUSE_DOORBELL -DUSE_MONITOR -DGPU_SOURCE -o $@ wfey.c

wfe_db_mon_gpu_src: wfe_db_mon_gpu_src.o wfey_hwmon.o gpu_source.o
	nvcc -o $@ $^ ${LIBRARIES}

cuda_doorbell_smoke: cuda_doorbell_smoke.cu
	nvcc -O0 -g -std=c++17 -o $@ $<

cuda_wfe_doorbell_smoke: cuda_wfe_doorbell_smoke.cu
	nvcc -O0 -g -std=c++17 -Xcompiler -pthread -o $@ $<

clean:
	rm -f $(wildcard *~ *.o *.d *.h.gch *.out ${TARGETS} wfe_db_mon_gpu_src cuda_doorbell_smoke cuda_wfe_doorbell_smoke)

-include $(wildcard *.d)
