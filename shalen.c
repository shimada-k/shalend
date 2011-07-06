#include "shalen.h"

FILE *csv;
char *wd_path;
int nr_cpus;

enum thread_operation_status tos = NR_STATUS_MAX;
sigset_t ss;

void *sheram_worker(void *arg);
void *lbprofile_worker(void *arg);

/* initialize data_per_time[] */
void shalen_alloc_resources(void)
{
	char path[PATH_MAX];

	snprintf(path, PATH_MAX, "%s%s", wd_path, "output.csv");

	puts(path);

	if(!(csv = fopen(path, "w"))){	/* open output.csv */
		syslog(LOG_ERR, "%s cannot open csv file", log_err_prefix(shalen_alloc_resources));
		exit(EXIT_FAILURE);
	}
}

void shalen_free_resources(void)
{
	closelog();

	if(fclose(csv) == EOF){
		exit(EXIT_FAILURE);
	}
}

int shalen_init(void)
{
	FILE *f = NULL;
	char path[PATH_MAX];

	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);	/* CPU数を取得 */

	openlog("shalend", LOG_PID, LOG_DAEMON);

	daemon(0, 0);	/* デーモン化　これ以降は子プロセスで実行される */

	snprintf(path, PATH_MAX, "%s%s", wd_path, "shalend.pid");

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
	char pid_path[PATH_MAX];

	snprintf(pid_path, PATH_MAX, "%sshalend.pid", wd_path);
	remove(pid_path);	/* remove shalend.pid */

	shalen_free_resources();

	syslog(LOG_NOTICE, "SIGTERM accepted.");
}

int main(int argc, char *argv[])
{
	int ret, signo;
	pthread_t sheram, lbprofile;

	if(argc == 1){
		wd_path = DEFAULT_WD;

		if(shalen_init()){
			syslog(LOG_ERR, "%s shalen_init() failed", log_err_prefix(main));
			exit(EXIT_FAILURE);
		}
	}
	else if(argc == 2){

		FILE *wd = NULL;

		if((wd = fopen(&argv[1][8], "r"))){
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

	if(pthread_create(&sheram, NULL, sheram_worker, NULL) != 0){
		syslog(LOG_ERR, "%s pthread_create() failed", log_err_prefix(main));
	}

	if(pthread_create(&lbprofile, NULL, lbprofile_worker, NULL) != 0){
		syslog(LOG_ERR, "%s pthread_create() failed", log_err_prefix(main));
	}

	while(1){
		if(sigwait(&ss, &signo) == 0){
			if(signo == SIGTERM){
				syslog(LOG_NOTICE, "shalend:sigterm recept");
				tos = SIGTERM_RECEPT;
				break;
			}
		}
	}

	pthread_join(sheram, NULL);
	pthread_join(lbprofile, NULL);

	if(tos == LBPROFILE_STOPPED){
		syslog(LOG_NOTICE, "stopping shalend");
		shalen_final();
	}

	return 0;
}


