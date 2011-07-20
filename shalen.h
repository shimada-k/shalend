

#define barrier() __asm__ __volatile__("": : :"memory")	/* メモリバリア */

#define mb() \
	__asm__ __volatile__("mb": : :"memory")

#define rmb() \
	__asm__ __volatile__("mb": : :"memory")

#define wmb() \
	__asm__ __volatile__("wmb": : :"memory")

#define DEFAULT_WD	"/home/shimada/"

#define STR_PATH_MAX	128

#define MAX_DOMAIN_LV	2

#define MAX_RECORD	100	/* 1つの実験が1000秒 == MAX_RECORD * RERIOD */
#define PERIOD	10 	/* periodic operation rate [second] */

extern FILE *csv;
extern int nr_cpus;

extern char *wd_path;

enum thread_operation_status{
	SIGTERM_RECEPT,
	SHERAM_STOPPED,
	LBPROFILE_STOPPED,
	NR_STATUS_MAX,
};

#define log_err_prefix(func_name)	__FILE__ "::" #func_name

