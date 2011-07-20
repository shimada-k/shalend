#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>	/* calloc(3), exit(3), EXIT_SUCCESS */
#include <pthread.h>	/* pthread_exit(3), pthread_join(3), pthread_create(3) */
#include <unistd.h>	/* daemon(3), sleep(3) */
#include <sys/types.h>
#include <sys/stat.h>	/* mkdir(2) */
#include <signal.h>	/* getpid(2) */

#include "shalen.h"

FILE *csv;
char *wd_path;
int nr_cpus;

sigset_t ss;
int debug;	/* デバッグモードなら1 */

pthread_mutex_t	mx;	/* thread_operation_status用のmutex */

void *kpreport_worker(void *arg);
void *lbprofile_worker(void *arg);

/* initialize data_per_time[] */
void shalen_alloc_resources(void)
{
	char path[STR_PATH_MAX];

	snprintf(path, STR_PATH_MAX, "%s%s", wd_path, "output.csv");

	puts(path);

	if(!(csv = fopen(path, "w"))){	/* open output.csv */
		syslog(LOG_ERR, "%s cannot open csv file", log_err_prefix(shalen_alloc_resources));
		exit(EXIT_FAILURE);
	}
}

void shalen_free_resources(const char *called)
{
	syslog(LOG_NOTICE, "%s calls shalen_free_resources()\n", called);
	closelog();

	if(fclose(csv) == EOF){
		exit(EXIT_FAILURE);
	}
}

int shalen_init(void)
{
	FILE *f = NULL;
	char path[STR_PATH_MAX];

	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);	/* CPU数を取得 */

	openlog("shalend", LOG_PID, LOG_DAEMON);

	if(debug == 0){
		daemon(0, 0);	/* デーモン化　これ以降は子プロセスで実行される */
	}

	snprintf(path, STR_PATH_MAX, "%s%s", wd_path, "shalend.pid");

	if(!(f = fopen(path, "w"))){	/* .pidファイルを作成 */
		syslog(LOG_ERR, "%s cannot open pid file", log_err_prefix(shalen_init));
		return 1;
	}

	fprintf(f, "%d", getpid());	/* ここはdaemon(3)の後に実行されないといけない */
	fclose(f);		/* *.pidファイルをクローズ */

	shalen_alloc_resources();

	syslog(LOG_NOTICE, "starting shalend.");

	return 0;
}

void shalen_final(void)
{
	char pid_path[STR_PATH_MAX];

	snprintf(pid_path, STR_PATH_MAX, "%sshalend.pid", wd_path);
	remove(pid_path);	/* remove shalend.pid */

	shalen_free_resources(__func__);
}

int main(int argc, char *argv[])
{
	int ret, signo;
	pthread_t kpreport, lbprofile;

	if(argc == 1){
		wd_path = DEFAULT_WD;

		if(shalen_init()){
			syslog(LOG_ERR, "%s shalen_init() failed", log_err_prefix(main));
			exit(EXIT_FAILURE);
		}
	}
	else if(argc == 2){

		FILE *wd = NULL;

		if((wd = fopen(&argv[1][8], "r"))){	/* work_dir="xxx" */
			fclose(wd);	/* working directry is already exist */
		}
		else{
			if(mkdir(&argv[1][8], 0777) == -1)	/* open working directory */
				exit(EXIT_FAILURE);
		}
		wd_path = &argv[1][8];

		if(shalen_init()){
			syslog(LOG_ERR, "%sshalen_init() failed", log_err_prefix(main));
			exit(EXIT_FAILURE);
		}
	}
	else if(argc == 3){

		FILE *wd = NULL;

		if((wd = fopen(&argv[1][8], "r"))){	/* work_dir="xxx" */
			fclose(wd);	/* working directry is already exist */
		}
		else{
			if(mkdir(&argv[1][8], 0777) == -1)	/* open working directory */
				exit(EXIT_FAILURE);
		}
		wd_path = &argv[1][8];

		if(strcmp(argv[2], "--debug") == 0){
			debug = 1;
		}
		if(shalen_init()){
			syslog(LOG_ERR, "%sshalen_init() failed", log_err_prefix(main));
			exit(EXIT_FAILURE);
		}
	}
	else{
		syslog(LOG_ERR, "%s invalid argument", log_err_prefix(main));
		exit(EXIT_FAILURE);
	}

	sigemptyset(&ss);

	/* block SIGTERM */
	ret = sigaddset(&ss, SIGTERM);

	if(ret != 0){
		return 1;
	}

	ret = sigprocmask(SIG_BLOCK, &ss, NULL);

	if(ret != 0){
		return 1;
	}

	/* スレッドの生成 */

	if(pthread_create(&kpreport, NULL, kpreport_worker, NULL) != 0){
		syslog(LOG_ERR, "%s pthread_create() failed", log_err_prefix(main));
	}

	if(pthread_create(&lbprofile, NULL, lbprofile_worker, NULL) != 0){
		syslog(LOG_ERR, "%s pthread_create() failed", log_err_prefix(main));
	}

	while(1){
		if(sigwait(&ss, &signo) == 0){
			if(signo == SIGTERM){
				syslog(LOG_NOTICE, "shalend:sigterm recept\n");
				break;
			}
		}
	}

	//syslog(LOG_NOTICE, "shalen loop breaked tos:%d\n", tos);

	pthread_cancel(kpreport);
	pthread_cancel(lbprofile);

	pthread_join(kpreport, NULL);
	pthread_join(lbprofile, NULL);

	syslog(LOG_NOTICE, "stopping shalend");
	shalen_final();

	return 0;
}


