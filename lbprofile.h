#define _IOC_NONE	0U

#define _IOC_NRBITS	8
#define _IOC_TYPEBITS	8
#define _IOC_SIZEBITS	14

#define _IOC_NRSHIFT	0
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)

#define _IOC(dir,type,nr,size) \
	(((dir)  << _IOC_DIRSHIFT) | \
	 ((type) << _IOC_TYPESHIFT) | \
	 ((nr)   << _IOC_NRSHIFT) | \
	 ((size) << _IOC_SIZESHIFT))
#define _IO(type,nr)		_IOC(_IOC_NONE,(type),(nr),0)

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


