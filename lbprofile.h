#include <linux/ioctl.h>

#define IO_MAGIC				'k'
#define IOC_USEREND_NOTIFY			_IO(IO_MAGIC, 0)
#define IOC_SIGRESET_REQUEST		_IO(IO_MAGIC, 1)
#define IOC_SETSIGNO				_IO(IO_MAGIC, 2)
#define IOC_SETGRAN				_IO(IO_MAGIC, 3)
#define IOC_SETPID				_IO(IO_MAGIC, 4)

#define GRAN_LB	240
#define MAX_NR_HOLD	5

struct lbprofile_hdr{
	int nr_cpus;
	unsigned int nr_lbprofile;
};

struct lbprofile {
	pid_t pid;
	int src_cpu, dst_cpu;
};

struct pid_loop{
	unsigned int loop;
	int src_cpu, dst_cpu;
};


