obj-m += x86_pmu_test.o

all: x86_pmu_test.ko

x86_pmu_test.ko: x86_pmu_test.c
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

.PHONY: clean
clean:
	 make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	 -rm x86_pmu_test
