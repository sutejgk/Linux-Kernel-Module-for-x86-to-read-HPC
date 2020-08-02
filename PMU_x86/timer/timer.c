#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#define MASK 0
#define START_PMU 1
#define END_PMU 0

#define WR_VALUE _IOW('a','a',int32_t*)
#define RD_VALUE _IOR('a','b',int32_t*)

static int fd;
static int status;
int timer_count = 0;

static void TimerHandler(int sig, siginfo_t *si, void *uc)
{
	int32_t number = END_PMU;
	ioctl(fd, WR_VALUE, (int32_t*) &number);
	++timer_count;
	
	if(timer_count >= 1) {
		printf("Here\n");
	}
}

int main()
{
	/***********************
	 * ioctl variables
	 ***********************/
	int32_t value, number;

	/***********************
	 * timer variables
	 ***********************/
	struct sigevent sev;
	struct sigaction sa;
	struct itimerspec its;
	struct itimerspec curr_its;
	timer_t timerid;
	sigset_t mask;

	printf("\nOpening Driver\n");
	fd = open("/dev/pmu_device", O_RDWR);
	if(fd < 0) {
		printf("Cannot open device file...\n");
	}

	sa.sa_flags = 0;
	sa.sa_handler = TimerHandler;
	sigemptyset(&sa.sa_mask);
	if(sigaction(SIGRTMIN, &sa, NULL) == -1) {
		printf("sigaction\n");
		return;
	}

#if MASK
	printf("Blocking signal %d\n", SIGRTMIN);
	sigemptyset(&mask);
	sigaddset(&mask, SIGRTMIN);
	if(sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
		printf("sigprocmask\n");
		return;
	}
#endif

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGRTMIN;
	sev.sigev_value.sival_ptr = &timerid;
	if(timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
		printf("timer_create\n");
		return;
	}
	
	printf("timer ID is 0x%lx\n", (long)timerid);


	/************************************
	* Ioctl
	************************************/
	number = START_PMU;
	ioctl(fd, WR_VALUE, (int32_t*) &number);

	// Timer time
	// 100000000 100ms
	// 1000000 1ms
	// 50000  50us
	its.it_value.tv_nsec = 50000;
	its.it_value.tv_sec = 0;
	// Interval for starting again
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	if(timer_settime(timerid, 0, &its, NULL) == -1) {
		printf("timer_settime\n");
		return;
	}

	// program running
	int a=0;
	for(int i=0; i<10000; i++)
		a += i;

	a=0;
	for(int i=0; i<10000; i++)
		a += i;

	a=0;
	for(int i=0; i<10000; i++)
		a += i;

	a=0;
	for(int i=0; i<10000; i++)
		a += i;

	printf("Reading Value from Driver\n");
	ioctl(fd, RD_VALUE, (int32_t*) &value);
	

#if MASK
	printf("Unblocking signal %d\n", SIGRTMIN);
	if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == 1) {
		printf("sigprocmask unblock\n");
		return;
	}
#endif

	if(timer_delete(timerid) == 0)
		printf("delete timer SUCCESS\n");
	else
		printf("delete timer FAIL: %d\n", errno);

	return 0;
}
