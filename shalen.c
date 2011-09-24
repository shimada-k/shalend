#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>	/* calloc(3), exit(3), EXIT_SUCCESS */
#include <pthread.h>	/* pthread_exit(3), pthread_join(3), pthread_create(3) */
#include <unistd.h>	/* daemon(3), sleep(3) */
#include <sys/types.h>
#include <sys/stat.h>	/* mkdir(2) */
#include <signal.h>	/* getpid(2) */

#include <stdbool.h>

#include "shalen.h"

/*
	DEBUGオプションを実装する
	MakefileでDEBUGオプション付きでコンパイルすると標準出力にメッセージを出すようにする
	逆に何もなかったら、デーモンになって、syslogに出力する
*/

char *wd_path;
int nr_cpus;

void *kpreport_worker(void *arg);
void *lbprofile_worker(void *arg);
void *l3miss_worker(void *arg);

/*
	リソースを確保する関数
*/
bool shalen_alloc_resources(void)
{
	return true;
}

/*
	リソースを解放する関数
	@called 呼び出し元の関数名
*/
void shalen_free_resources(const char *called)
{
	NOTICE_MESSAGE("%s calls shalen_free_resources()\n", called);
	closelog();
}

/*
	初期化関数
	return　成功：true　失敗：false
*/
bool shalen_init(void)
{
	FILE *f = NULL;
	char path[STR_PATH_MAX];

	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);	/* CPU数を取得 */

	openlog("shalend", LOG_PID, LOG_DAEMON);

#ifndef DEBUG
	daemon(0, 0);	/* デバッグモードでなければデーモン化　これ以降は子プロセスで実行される */
#endif

	snprintf(path, STR_PATH_MAX, "%s%s", wd_path, "shalend.pid");

	if(!(f = fopen(path, "w"))){	/* .pidファイルを作成 */
		ERR_MESSAGE("%s cannot open pid file", log_err_prefix(shalen_init));
		return false;
	}

	fprintf(f, "%d", getpid());	/* ここはdaemon(3)の後に実行されないといけない */
	fclose(f);		/* *.pidファイルをクローズ */

	if(shalen_alloc_resources() == false){
		return false;
	}

	NOTICE_MESSAGE("starting shalend.");
	//syslog(LOG_NOTICE, "starting shalend.");

	return true;
}

/*
	終了時に呼び出される関数
*/
void shalen_final(void)
{
	char pid_path[STR_PATH_MAX];

	snprintf(pid_path, STR_PATH_MAX, "%sshalend.pid", wd_path);
	remove(pid_path);	/* remove shalend.pid */

	shalen_free_resources(__func__);
}

/*
	main関数 シグナルはメインスレッドが受信する
*/
int main(int argc, char *argv[])
{
	int ret, signo;
	sigset_t ss;
	pthread_t kpreport, lbprofile, l3miss;

	if(argc == 1){
		wd_path = DEFAULT_WD;

		if(shalen_init() == false){
			ERR_MESSAGE("%s failed", log_err_prefix(shalen_init));
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

		if(shalen_init() == false){
			ERR_MESSAGE("%s shalen_init() failed", log_err_prefix(shalen_init));
			exit(EXIT_FAILURE);
		}
	}
	else{
		ERR_MESSAGE("%s invalid argument", log_err_prefix(main));
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
		ERR_MESSAGE("%s pthread_create() failed", log_err_prefix(main));
	}

	if(pthread_create(&lbprofile, NULL, lbprofile_worker, NULL) != 0){
		ERR_MESSAGE("%s pthread_create() failed", log_err_prefix(main));
	}

	if(pthread_create(&l3miss, NULL, l3miss_worker, NULL) != 0){
		ERR_MESSAGE("%s pthread_create() failed", log_err_prefix(main));
	}

	while(1){
		if(sigwait(&ss, &signo) == 0){
			if(signo == SIGTERM){
				NOTICE_MESSAGE("shalend:sigterm recept\n");
				break;
			}
		}
	}

	pthread_cancel(kpreport);
	pthread_cancel(lbprofile);
	pthread_cancel(l3miss);

	pthread_join(kpreport, NULL);
	pthread_join(lbprofile, NULL);
	pthread_join(l3miss, NULL);

	NOTICE_MESSAGE("stopping shalend");

	shalen_final();

	return 0;
}


