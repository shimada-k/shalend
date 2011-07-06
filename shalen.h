#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>	/* calloc(3), exit(3), EXIT_SUCCESS */
#include <pthread.h>	/* pthread_exit(3), pthread_join(3), pthread_create(3) */
#include <unistd.h>	/* daemon(3), sleep(3) */
#include <sys/types.h>
#include <sys/stat.h>	/* mkdir(2) */
#include <signal.h>	/* getpid(2) */

#define PARAMS	"/proc/params"
#define DEFAULT_WD	"/home/shimada/"

#define DATA_LINE_MAX	160	/* データ行を格納するバッファの最大バイト数 */
#define AXIS_LINE_MAX	64	/* セクション名行を格納するバッファの最大バイト数 */
#define PATH_MAX		64

#define MAX_DOMAIN_LV	2

#define MAX_RECORD	100	/* 1つの実験が1000秒 == MAX_RECORD * RERIOD */
#define PERIOD	10 	/* periodic operation rate [second] */

/*
* 新しい項目を追加する時は付け足す。削除する時はコメントアウト
* ただし、/proc/paramsの項目数とsheramの項目数は等しくすること
* ここに列挙している項目はループの制御に使われる
* 最終的にcsvに書き込まれるセクションネームは/proc/paramsの#から始まる文字列
*/
enum sheram {
	RQ_RUNNING,
	MIN_VRUNTIME,
	ROOT_VRUNTIME,
	MAX_VRUNTIME,
	SUB_VRUNTIME,
	RQ_IQR,
	LOAD_WEIGHT,
	CPU_LOAD0,
	CPU_LOAD2,
	SMT_DOM__TOTAL_LOAD,
	SMT_DOM__TOTAL_PWR,
	SMT_DOM__AVG_LOAD,
	SMT_DOM__THIS_LOAD,
	SMT_DOM__THIS_LOAD_PER_TASK,
	SMT_DOM__THIS_NR_RUNNING,
	SMT_DOM__MAX_LOAD,
	SMT_DOM__BUSIEST_LOAD_PER_TASK,
	SMT_DOM__BUSIEST_NR_RUNNING,
	SMT_DOM__BUSIEST_GROUP_CAPACITY,

	MC_DOM__TOTAL_LOAD,
	MC_DOM__TOTAL_PWR,
	MC_DOM__AVG_LOAD,
	MC_DOM__THIS_LOAD,
	MC_DOM__THIS_LOAD_PER_TASK,
	MC_DOM__THIS_NR_RUNNING,
	MC_DOM__MAX_LOAD,
	MC_DOM__BUSIEST_LOAD_PER_TASK,
	MC_DOM__BUSIEST_NR_RUNNING,
	MC_DOM__BUSIEST_GROUP_CAPACITY,

	LB_COUNT_SMT,
	LB_COUNT_MC,
	MAX_PARAM,
	DIVIDER = LB_COUNT_SMT	/* DIVIDER以下は最後に1回readするもの */
};

extern FILE *csv;
extern int nr_cpus;
extern char axes[MAX_PARAM][AXIS_LINE_MAX];

enum thread_operation_status{
	SIGTERM_RECEPT,
	SHERAM_STOPPED,
	LBPROFILE_STOPPED,
	NR_STATUS_MAX,
};

extern enum thread_operation_status tos;

#define log_err_prefix(func_name)	__FILE__ "::" #func_name

