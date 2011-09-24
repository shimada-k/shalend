
#define DEFAULT_WD	"/home/shimada/"

#define STR_PATH_MAX	128

#define MAX_DOMAIN_LV	2

#define MAX_RECORD	100	/* 1つの実験が1000秒 == MAX_RECORD * RERIOD */
#define PERIOD	10 	/* periodic operation rate [second] */

extern int nr_cpus;

extern char *wd_path;

#define log_err_prefix(func_name)	__FILE__ "::" #func_name

#ifdef DEBUG
#define ERR_MESSAGE(...)		\
	printf(__VA_ARGS__)
#else
#define ERR_MESSAGE(...)		\
	syslog(LOG_ERR, __VA_ARGS__)
#endif

#ifdef DEBUG
#define NOTICE_MESSAGE(...)		\
	printf(__VA_ARGS__)
#else
#define NOTICE_MESSAGE(...)		\
	syslog(LOG_NOTICE, __VA_ARGS__)
#endif
