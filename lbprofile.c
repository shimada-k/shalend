#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>	/* calloc(3), exit(3), EXIT_SUCCESS */
#include <pthread.h>	/* pthread_exit(3), pthread_join(3), pthread_create(3) */
#include <unistd.h>	/* daemon(3), sleep(3) */
#include <sys/types.h>
#include <sys/stat.h>	/* mkdir(2) */
#include <signal.h>	/* getpid(2) */

#include <fcntl.h>
#include <sys/ioctl.h>

#include <stdbool.h>

#include "shalen.h"
#include "lbprofile.h"

FILE *flb;
int dev;
struct lbprofile *hndlr_buf;
struct lbprofile_hdr hdr;

/*
	リソースを確保する関数
*/
bool lbprofile_alloc_resources(void)
{
	char path[STR_PATH_MAX];

	snprintf(path, STR_PATH_MAX, "%s%s", wd_path, "lbprofile.lb");

	if(!(flb = fopen(path, "w+b"))){	/* 出力するファイルを作成 */
		return false;
	}
	if((dev = open("/dev/lbprofile", O_RDONLY)) < 0){
		return false;
	}

	if(!(hndlr_buf = calloc(GRAN_LB, sizeof(struct lbprofile)))){
		return false;
	}

	return true;
}

/*
	リソースを解放する関数
	@called 呼び出し元の関数名
*/
bool lbprofile_free_resources(const char *called)
{
	syslog(LOG_NOTICE, "%s calls lbprofile_free_resources()\n", called);

	if(close(dev) < 0){
		return false;
	}

	if(fclose(flb) != 0){
		return false;
	}

	free(hndlr_buf);

	return true;
}


/*
	ヘッダを挿入する関数
	@hdr ヘッダのデータのアドレス
*/
void put_hdr(struct lbprofile_hdr *hdr)
{
	long pos;

	pos = ftell(flb);	/* カレントオフセットを記録 */
	fseek(flb, 0L, SEEK_SET);

	if(fwrite(hdr, sizeof(struct lbprofile_hdr), 1, flb) != 1){
		syslog(LOG_ERR, "%s fwrite(3) failed", log_err_prefix(put_hdr));
	}

	fseek(flb, pos, SEEK_SET);	/* 記録されたオフセットに戻す */
}

/*
	終了時に呼ばれる関数
	@arg 引数 pthread_cleanup_pushで指定される
*/
void lbprofile_final(void *arg)
{
	unsigned int piece;
	ssize_t r_size;

	if(ioctl(dev, IOC_USEREND_NOTIFY, &piece) < 0){
		syslog(LOG_ERR, "%s ioctl(2) failed", log_err_prefix(lbprofile_final));
		lbprofile_free_resources(__func__);
		exit(EXIT_FAILURE);
	}
	else{
		unsigned int nr_locked_be, tip;

		syslog(LOG_NOTICE, "piece = %d\n", piece);

		if((nr_locked_be = piece / GRAN_LB)){	/* nr_locked_be回だけread(2)を回さないといけない */
			unsigned int i;

			syslog(LOG_NOTICE, "nr_locked_be = %d\n", nr_locked_be);

			for(i = 0; i < nr_locked_be; i++){
				if((r_size = read(dev, hndlr_buf, sizeof(struct lbprofile) * GRAN_LB)) != sizeof(struct lbprofile) * GRAN_LB){
					syslog(LOG_ERR, "%s read(2) failed. lbentries was not loaded r_size is %d\n",
						log_err_prefix(lbprofile_final), (int)r_size);
				}

				if(fwrite(hndlr_buf, sizeof(struct lbprofile), GRAN_LB, flb) != GRAN_LB){
					syslog(LOG_ERR, "%s fwrite(3) failed.", log_err_prefix(lbprofile_final));
				}
				hdr.nr_lbprofile += GRAN_LB;
			}

			tip = piece % GRAN_LB;
		}
		else{
			tip = piece;
		}

		/* 端数の分だけ追加でread(2)する */
		if((r_size = read(dev, hndlr_buf, sizeof(struct lbprofile) * tip)) != sizeof(struct lbprofile) * tip){
			syslog(LOG_ERR, "%s read(2) failed. lbentries was not loaded r_size is %d\n", log_err_prefix(lbprofile_final), (int)r_size);
		}

		if(fwrite(hndlr_buf, sizeof(struct lbprofile), tip, flb) != tip){
			syslog(LOG_ERR, "%s fwrite(3) failed.", log_err_prefix(lbprofile_final));
		}
		hdr.nr_lbprofile += tip;
	}

	hdr.nr_cpus = nr_cpus;

	put_hdr(&hdr);

	if(lbprofile_free_resources(__func__) == false){
		exit(EXIT_FAILURE);
	}
}

/*
	カーネルからのシグナルのハンドラ関数
	@sig 仕様
*/
void lbprofile_handler(int sig)
{
	ssize_t s_read;

	if((s_read = read(dev, hndlr_buf, sizeof(struct lbprofile) * GRAN_LB)) != sizeof(struct lbprofile) * GRAN_LB){
		syslog(LOG_ERR, "%s read(2) failed. lbentries was not loaded s_read = %d\n", log_err_prefix(lbprofile_handler), (int)s_read);
		lbprofile_free_resources(__func__);
		exit(EXIT_FAILURE);
	}

	if(fwrite(hndlr_buf, sizeof(struct lbprofile), GRAN_LB, flb) != GRAN_LB){
		lbprofile_free_resources(__func__);
		syslog(LOG_ERR, "%s fwrite(3) failed", log_err_prefix(lbprofile_handler));
		exit(EXIT_FAILURE);
	}

	hdr.nr_lbprofile += GRAN_LB;

	lseek(dev, 0, SEEK_SET);
}

/*
	初期化関数
	返り値　成功：true　失敗：false
*/
bool lbprofile_init(void)
{
	if(lbprofile_alloc_resources() == false){
		syslog(LOG_ERR, "%s failed", log_err_prefix(lbprofile_alloc_resources));
		return false;
	}

	fseek(flb, (long)sizeof(struct lbprofile_hdr), SEEK_SET);	/* ヘッダの分を空けておく */

	signal(SIGUSR1, lbprofile_handler);
	syslog(LOG_DEBUG, "IOC_SETSIGNO:%d IOC_SETGRAN:%d IOC_SETPID:%d\n", IOC_SETSIGNO, IOC_SETGRAN, IOC_SETPID);

	if(ioctl(dev, IOC_SETSIGNO, SIGUSR1) < 0){
		syslog(LOG_ERR, "%s IOC_SETSIGNO", log_err_prefix(lbprofile_init));
		return false;
	}

	if(ioctl(dev, IOC_SETGRAN, GRAN_LB) < 0){
		syslog(LOG_ERR, "%s IOC_SETGRAN", log_err_prefix(lbprofile_init));
		return false;
	}

	if(ioctl(dev, IOC_SETPID, (int)getpid()) < 0){
		syslog(LOG_ERR, "%s IOC_SETPID", log_err_prefix(lbprofile_init));
		return false;
	}

	return true;
}

/*
	lbprofileのスレッドルーチン
	@arg 引数
*/
void *lbprofile_worker(void *arg)
{
	if(lbprofile_init() == false){
		syslog(LOG_ERR, "%s failed", log_err_prefix(lbprofile_init));
	}

	pthread_cleanup_push(lbprofile_final, NULL);

	while(1){
		sleep(1);
		pthread_testcancel();
		/* will recieve signal SIGUSR1, and call lbprofile_handler() */
	}

	syslog(LOG_NOTICE, "lbprofile loop breaked\n");

	pthread_exit(NULL);
	pthread_cleanup_pop(1);

}

