
#define DEFAULT_WD	"/home/shimada/"

#define STR_PATH_MAX	128

#define MAX_DOMAIN_LV	2

#define MAX_RECORD	100	/* 1つの実験が1000秒 == MAX_RECORD * RERIOD */
#define PERIOD	10 	/* periodic operation rate [second] */

extern int nr_cpus;

extern char *wd_path;

#define log_err_prefix(func_name)	__FILE__ "::" #func_name

