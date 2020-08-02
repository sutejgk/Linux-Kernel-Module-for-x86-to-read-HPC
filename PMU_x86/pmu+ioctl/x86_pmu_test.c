#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define EVTCOUNT 4
#define START_PMU 1
#define END_PMU 0

#define WR_VALUE _IOW('a','a',int32_t*)
#define RD_VALUE _IOR('a','b',int32_t*)

int32_t value = 0;
unsigned int mem_evtsel_low[EVTCOUNT] = {0};
unsigned int mem_evtsel_high[EVTCOUNT] = {0};

dev_t dev = 0;
static struct class *dev_class;
static struct cdev pmu_cdev;

static void pmu_end(void);
static int pmu_start(void);
static void set_pce(void *arg);
static int __init pmu_driver_init(void);
static void __exit pmu_driver_exit(void);
static int pmu_open(struct inode *inode, struct file *file);
static int pmu_release(struct inode *inode, struct file *file);
static ssize_t pmu_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t pmu_write(struct file *filp, const char *buf, size_t len, loff_t * off);
static long pmu_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static struct file_operations fops =
{
	.owner          = THIS_MODULE,
	.read           = pmu_read,
	.write          = pmu_write,
	.open           = pmu_open,
	.unlocked_ioctl = pmu_ioctl,
	.release        = pmu_release,
};
static int pmu_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "Device File Opened...!!!\n");
	return 0;
}

static int pmu_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "Device File Closed...!!!\n");
	return 0;
}

static ssize_t pmu_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	printk(KERN_INFO "Read Function\n");
	return 0;
}
static ssize_t pmu_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	printk(KERN_INFO "Write function\n");
	return 0;
}

static long pmu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int msr;
	unsigned int low, high;
	unsigned int i=0;
	low = 0, high = 0;
         
	switch(cmd) {
		case WR_VALUE:
			copy_from_user(&value ,(int32_t*) arg, sizeof(value));
			printk(KERN_INFO "Value = %d\n", value);
			if(value == START_PMU) {
				printk(KERN_INFO "pmu started\n");
				pmu_start();
			}
			else if(value == END_PMU) {
				pmu_end();
				printk(KERN_INFO "pmu ended\n");

			}
			break;

		case RD_VALUE:
			/*******************************************
			* IA32_PMCx reading
			*******************************************/
			msr = 0xc1;
			for(i=0; i<EVTCOUNT; i++) {
					rdmsr(msr, low, high);
					printk(KERN_INFO "##READ PMU Result IA32_PMC%d 0x%02x: [low]%u, [high]%u\n", i, msr, low, high);
					++msr;
			}
			copy_to_user((int32_t*) arg, &value, sizeof(value));
			break;
	}
	return 0;
}

static int __init pmu_driver_init(void)
{
	/*Allocating Major number*/
	if((alloc_chrdev_region(&dev, 0, 1, "pmu_Dev")) <0){
			printk(KERN_INFO "Cannot allocate major number\n");
			return -1;
	}
	printk(KERN_INFO "Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));

	/*Creating cdev structure*/
	cdev_init(&pmu_cdev,&fops);

	/*Adding character device to the system*/
	if((cdev_add(&pmu_cdev,dev,1)) < 0){
		printk(KERN_INFO "Cannot add the device to the system\n");
		goto r_class;
	}

	/*Creating struct class*/
	if((dev_class = class_create(THIS_MODULE,"pmu_class")) == NULL){
		printk(KERN_INFO "Cannot create the struct class\n");
		goto r_class;
	}

	/*Creating device*/
	if((device_create(dev_class,NULL,dev,NULL,"pmu_device")) == NULL){
		printk(KERN_INFO "Cannot create the Device 1\n");
		goto r_device;
	}
	printk(KERN_INFO "Device Driver Insert...Done!!!\n");
    return 0;

r_device:
	class_destroy(dev_class);
r_class:
	unregister_chrdev_region(dev,1);
	return -1;
}

void __exit pmu_driver_exit(void)
{
	device_destroy(dev_class,dev);
	class_destroy(dev_class);
	cdev_del(&pmu_cdev);
	unregister_chrdev_region(dev, 1);
    printk(KERN_INFO "Device Driver Remove...Done!!!\n");
}


static void set_pce(void *arg)
{
	int to_val = (arg != 0);
	unsigned int cr4_val;

	cr4_val = __read_cr4();
	printk("cr4_val: %08x\n", cr4_val);
	if(to_val) {
		cr4_val |= X86_CR4_PCE;
	} else {
		cr4_val &= ~X86_CR4_PCE;
	}
	printk("cr4_val: %08x\n", cr4_val);

	__write_cr4(cr4_val);
}


static int pmu_start(void){
	unsigned int msr;
	unsigned int low, high;
	unsigned int i=0;

	unsigned int num_cpus=0;
	int cpu;

	num_cpus = num_online_cpus();

	printk(KERN_INFO "Enabling RDPMC %d CPUs\n", num_cpus);
	for(cpu = 0; cpu<num_cpus; cpu++) {
		smp_call_function_single(cpu, set_pce, (void *)1, 1);
	}

	/*******************************************
	* IA32_PERF_GLOBAL_CTRL setting
	*******************************************/
	msr = 0x38F;
	rdmsr(msr, low, high);
	printk("##READ BEFORE SETTING## IA32_PERF_GLOBAL_CTRL 0x38F: [low]%08x, [high]%08x\n", low, high);

	low |= 0xF;
	wrmsr(msr, low, high);
	printk("##WRITE## IA32_PERF_GLOBAL_CTRL 0x38F: [low]%08x, [high]%08x\n", (low), (high));

	rdmsr(msr, low, high);
	printk("##READ## IA32_PERF_GLOBAL_CTRL 0x38F: [low]%08x, [high]%08x\n", (low), (high));


	//		[PERF_COUNT_HW_CPU_CYCLES]              = 0x003c,
	//		[PERF_COUNT_HW_INSTRUCTIONS]            = 0x00c0,
	//		[PERF_COUNT_HW_CACHE_REFERENCES]        = 0x4f2e,
	//		[PERF_COUNT_HW_CACHE_MISSES]            = 0x412e,
	//		[PERF_COUNT_HW_BRANCH_INSTRUCTIONS]     = 0x00c4,
	//		[PERF_COUNT_HW_BRANCH_MISSES]           = 0x00c5,
	//		[PERF_COUNT_HW_BUS_CYCLES]              = 0x013c,
	//		[PERF_COUNT_HW_REF_CPU_CYCLES]          = 0x0300, /* pseudo-encoding */
	     
	/***********************************************
	* IA32_PERFEVTSELx MSR setting Starting point
	************************************************/


	/*******************************************
	* IA32_PERFEVTSEL0: instructions
	*******************************************/
	msr = 0x186;
	rdmsr(msr, mem_evtsel_low[0], mem_evtsel_high[0]);
	printk("##READ BEFORE SETTING## IA32_PERFEVTSEL0 0x186: [low]%08x, [high]%08x\n", (mem_evtsel_low[0]), (mem_evtsel_high[0]));
	low = 0x4300c0;
	high = mem_evtsel_high[0] | 0x0;
	wrmsr(msr, low, high);
	printk("##WRITE## IA32_PERFEVTSEL0 0x186: [low]%08x, [high]%08x\n", (low), (high));
	
	rdmsr(msr, low, high);
	printk("##READ## IA32_PERFEVTSEL0 0x186: [low]%08x, [high]%08x\n", (low), (high));
	     
	/*******************************************
	* IA32_PERFEVTSEL1: Cache References
	*******************************************/
	msr = 0x187;
	rdmsr(msr, mem_evtsel_low[1], mem_evtsel_high[1]);
	printk("##READ BEFORE SETTING## IA32_PERFEVTSEL1 0x187: [low]%08x, [high]%08x\n", (mem_evtsel_low[1]), (mem_evtsel_high[1]));
	low = 0x434f2e;
	high = mem_evtsel_high[1] | 0x0;
	wrmsr(msr, low, high);
	printk("##WRITE## IA32_PERFEVTSEL1 0x187: [low]%08x, [high]%08x\n", (low), (high));
	
	rdmsr(msr, low, high);
	printk("##READ## IA32_PERFEVTSEL1 0x187: [low]%08x, [high]%08x\n", (low), (high));

	     
	/*******************************************
	* IA32_PERFEVTSEL2: Branch Misses
	*******************************************/
	msr = 0x188;
	rdmsr(msr, mem_evtsel_low[2], mem_evtsel_high[2]);
	printk("##READ BEFORE SETTING## IA32_PERFEVTSEL2 0x188: [low]%08x, [high]%08x\n", (mem_evtsel_low[2]), (mem_evtsel_high[2]));
	low = 0x4300c5;
	high = mem_evtsel_high[2] | 0x0;
	wrmsr(msr, low, high);
	printk("##WRITE## IA32_PERFEVTSEL2 0x188: [low]%08x, [high]%08x\n", (low), (high));
	
	rdmsr(msr, low, high);
	printk("##READ## IA32_PERFEVTSEL2 0x188: [low]%08x, [high]%08x\n", (low), (high));


	/*******************************************
	* IA32_PERFEVTSEL3: Cache Misses
	*******************************************/
	msr = 0x189;
	rdmsr(msr, mem_evtsel_low[3], mem_evtsel_high[3]);
	printk("##READ BEFORE SETTING## IA32_PERFEVTSEL3 0x189: [low]%08x, [high]%08x\n", (mem_evtsel_low[3]), (mem_evtsel_high[3]));
	low = 0x43412e;
	high = mem_evtsel_high[3] | 0x0;
	wrmsr(msr, low, high);
	printk("##WRITE## IA32_PERFEVTSEL3 0x189: [low]%08x, [high]%08x\n", (low), (high));
	
	rdmsr(msr, low, high);
	printk("##READ## IA32_PERFEVTSEL3 0x189: [low]%08x, [high]%08x\n", (low), (high));
	     


	/*******************************************
	* IA32_PMCx reset to 0 to start counting
	*******************************************/
	msr = 0xC1;
	low = 0x0;
	high = 0x0;

	for(i=0; i<EVTCOUNT; i++) {
		wrmsr(msr, low, high);
		++msr;
	}
  
 	return  0;

}

static void pmu_end(void)
{
	unsigned int msr;
	unsigned int low, high;
	unsigned int i=0;
	low = 0, high = 0;

	/*******************************************
	* IA32_PERF_EVTSELx register
	* BIT22: Enable Counters (Disabled)
	*******************************************/
	msr = 0x186; // EVTSEL register
	for(i=0; i<EVTCOUNT; i++) {
		wrmsr(msr, mem_evtsel_low[i], mem_evtsel_high[i]);
		++msr;
	}

	/***************************************************
	* IA32_PERF_GLOBAL_CONTROL register: PMC disable
	****************************************************/
	 msr = 0x38F;
	 wrmsr(msr, low, high);

#if 0
	 /*******************************************
	 * IA32_PMCx reading
	 *******************************************/
	 msr = 0xc1;
	 for(i=0; i<EVTCOUNT; i++) {
		rdmsr(msr, low, high);
		printk("##READ ##3## IA32_PMC%d 0x%02x: [low]%u, [high]%u\n", i, msr, low, high);
		++msr;
	 }
#endif

	 msr = 0x186;
	 for(i=0; i<EVTCOUNT; i++) {
		rdmsr(msr, low, high);
		printk("@@CLEAR BITS@@ IA32_PERF_EVTSEL%d 0x%03x: [low]%08x, [high]%08x\n", i, msr, low, high);
	 }

	 /****************************************************
	  * IA32_PERF_GLOBAL_STATUS check PMCs overflow
	  ****************************************************/
 	  msr = 0x38E;
	  rdmsr(msr, low, high);
	  printk("##CHECK OVERFLOW## IA32_PERF_GLOBAL_STATUS 0x38F: [low]%08x, [high]%08x\n", low, high);

	 /****************************************************
	  * IA32_PERF_GLOBAL_CONTROL check reset bits
	  ****************************************************/
	  msr = 0x38F; // GLOBAL_CTRL register
	  rdmsr(msr, low, high);
	  printk("@@CLEAR BITS@@ IA32_PERF_GLOBAL_CTRL 0x38F: [low]%08x, [high]%08x\n", (low), (high));

}

module_init(pmu_driver_init);
module_exit(pmu_driver_exit);

MODULE_LICENSE("GPL");
