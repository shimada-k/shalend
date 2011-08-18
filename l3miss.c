#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>	/* sleep(3) */
#include <syslog.h>

#include "shalen.h"
#include "lib/msr.h"

#include "lib/msr_address.h"

#define USE_NR_MSR	3	/* いくつのMSRを使ってイベントを計測するか */
#define MAX_RECORDS	900	/* 最大何回計測するか */

static FILE *tmp_fp[USE_NR_MSR];
static FILE *csv;

/* 初期化部分とループで読む部分を分ける */

/*
	前回のデータとの差分を取る関数 ライブラリ側で呼び出される
	msr_handleに格納される関数（scope==thread or core用）
	@handle_id mh_ctl.handles[]の添字
	@val MSRを計測した生データ
	return true/false
*/
bool sub_record_multi(int handle_id, u64 *val)
{
	int nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	int skip = 0;

	u64 val_last[nr_cpus];
	int i;
	int num;

	/*-- tmp_fpはcloseすると削除されるので、openとcloseはalloc,freeで行うこととする --*/

	fseek(tmp_fp[handle_id], 0, SEEK_SET);

	/* 過去のvalをtmp_fpから読み込む */
	if((num = fread(val_last, sizeof(u64), nr_cpus, tmp_fp[handle_id])) != nr_cpus){
		skip = 1;
	}

	fseek(tmp_fp[handle_id], 0, SEEK_SET);

	/* 現在のcpu_valを書き込む */
	fwrite(val, sizeof(u64), nr_cpus, tmp_fp[handle_id]);

	fseek(tmp_fp[handle_id], 0, SEEK_SET);

	for(i = 0; i < nr_cpus; i ++){
		/* MSRの回数は増えることはあっても減ることはないのでここは絶対0以上 */
		val[i] -= val_last[i];
	}

	if(skip){
		return false;
	}
	else{
		return true;
	}
}

bool l3miss_alloc_resources(void)
{
	int i;
	char path[STR_PATH_MAX];

#if 1
	snprintf(path, STR_PATH_MAX, "%s%s", wd_path, "l3miss.csv");

	if(!(csv = fopen(path, "w+"))){	/* l3miss.csvを作成 */
		;	/* エラー */
	}
#endif
	/* tempファイルをオープン */
	for(i = 0; i < USE_NR_MSR; i++){
		if((tmp_fp[i] = tmpfile()) == NULL){
			return false;
		}
	}

	return true;
}

bool l3miss_init(void)
{
	if((l3miss_alloc_resources()) == false){	/* tempファイルをオープンするだけ */
		syslog(LOG_ERR, "%s failed", log_err_prefix(l3miss_alloc_resources));
		return false;
	}

	if(init_handle_controller(csv, MAX_RECORDS, USE_NR_MSR) == false){
		syslog(LOG_ERR, "%s failed", log_err_prefix(init_handle_controller));
		return false;
	}

	/* MAX_RECORDS回、USE_NR_MSR個のMSRを使って計測する。という指定 */

	return true;
}

/*
	リソースの解放を行う関数
*/
bool l3miss_free_resources(void)
{
	int i;

	for(i = 0; i < USE_NR_MSR; i++){
		if(fclose(tmp_fp[i]) == EOF){
			return false;
		}
	}

	return true;
}


/*
	終了時に呼ばれる関数
*/
void l3miss_final(void *arg)
{
	if(l3miss_free_resources() == false){
		syslog(LOG_ERR, "%s failed", log_err_prefix(l3miss_free_resources));
		exit(EXIT_FAILURE);
	}

	/* 後始末 */
	term_handle_controller(NULL);
}

void *l3miss_worker(void *arg)
{
	int i;
	MHANDLE *handles[USE_NR_MSR];
	enum msr_scope scope = thread;

	if(l3miss_init() == false){
		syslog(LOG_ERR, "%s failed", log_err_prefix(l3miss_init));
	}

	for(i = 0; i < USE_NR_MSR; i++){
		if((handles[i] = alloc_handle()) == NULL){
			syslog(LOG_ERR, "%s failed", log_err_prefix(alloc_handle));
		}
	}

	/* PERF_GLOBAL_CTL、PERFEVTSELxはinit_msr側で設定済み */

	if(activate_handle(handles[0], "MEM_LOAD_RETIRED.MISS USER only", scope, IA32_PMC0, sub_record_multi) == false){
		syslog(LOG_ERR, "%s failed", log_err_prefix(activate_handle));
	}

	if(activate_handle(handles[1], "MEM_LOAD_RETIRED.MISS OS only", scope, IA32_PMC1, sub_record_multi) == false){
		syslog(LOG_ERR, "%s failed", log_err_prefix(activate_handle));
	}

	if(activate_handle(handles[2], "MEM_LOAD_RETIRED.MISS both ring", scope, IA32_PMC2, sub_record_multi) == false){
		syslog(LOG_ERR, "%s failed", log_err_prefix(activate_handle));
	}

	pthread_cleanup_push(l3miss_final, NULL);

	while(1){

		pthread_testcancel();

		sleep(1);

		if(read_msr() == false){	/* MAX_RECORDS以上計測した */
			puts("time over");
			break;
		}
	}

	pthread_exit(NULL);
	pthread_cleanup_pop(1);
}

