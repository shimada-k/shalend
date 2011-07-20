#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>	/* calloc(3), exit(3), EXIT_SUCCESS */
#include <pthread.h>	/* pthread_exit(3), pthread_join(3), pthread_create(3) */
#include <unistd.h>	/* daemon(3), sleep(3) */
#include <sys/types.h>
#include <sys/stat.h>	/* mkdir(2) */
#include <signal.h>	/* getpid(2) */

#include <dirent.h>
#include <stdbool.h>

#include "shalen.h"

#define STR_TAG_MAX	32

#define STR_ENTRY_MAX	160

#define BASE_PATH	"/sys/kernel/kpreport"

#define NR_PRE_RECORD_ENTRY	2

int interrupt;

unsigned long long *flat_records;	/* vruntimeが入るので64bit整数 */

struct kp_entry {
	FILE *fp;
	char tag[STR_TAG_MAX];
	void (*pre_closure)(unsigned long long cpu_val[], int idx);
};

struct kp_entry_controller{
	int nr_entries;
	FILE *tmp_fp;
	unsigned long long *cpu_values;
	struct kp_entry *entries;
};

struct kp_entry_controller kp_entry_ctl;

/*
	前回のデータとの差分を取る関数
	kp_entryに格納される関数
*/
void sub_record(unsigned long long cpu_val[], int idx)
{
	unsigned long long cpu_val_last[nr_cpus];
	int i;
	int num;

	/*-- tmp_fpはcloseすると削除されるので、openとcloseはalloc,freeで行うこととする --*/

	fseek(kp_entry_ctl.tmp_fp, idx * nr_cpus * sizeof(unsigned long long), SEEK_SET);

	/* 過去のcpu_valをtmp_fpから読み込む */
	if((num = fread(cpu_val_last, sizeof(unsigned long long), nr_cpus, kp_entry_ctl.tmp_fp)) != nr_cpus){
		/* 最初の1回だけ何も書かれてないので読み込み失敗 */
		for(i = 0; i < nr_cpus; i++){
			cpu_val_last[i] = 0;
		}
	}

	fseek(kp_entry_ctl.tmp_fp, idx * nr_cpus * sizeof(unsigned long long), SEEK_SET);

	/* 現在のcpu_valを書き込む */
	fwrite(cpu_val, sizeof(unsigned long long), nr_cpus, kp_entry_ctl.tmp_fp);

	fseek(kp_entry_ctl.tmp_fp, 0, SEEK_SET);

	for(i = 0; i < nr_cpus; i ++){
		/* ロードバランスの回数は増えることはあっても減ることはないのでここは絶対0以上 */
		cpu_val[i] -= cpu_val_last[i];
	}
}

void records2csv(void);

/*
	BASE_PATH以下を探索してファイルの数を返す関数
	@base_path 起点のディレクトリ 絶対パスで指定
	@search_or_buildin 動作モード 0:探索のみ 1:構造体に組み込み
*/
int search_sysfs_f(char *base_path, int searched)
{
	DIR *dirp;
	struct dirent *p;
	int f = 0;
	static int entry_idx = 0;

	dirp = opendir(base_path);

	while((p= readdir(dirp)) != NULL){

		/* 隠しファイルだったらスルー */
		if(strcmp(p->d_name, ".") == 0 || strcmp(p->d_name, "..") == 0 || p->d_name[0] == '.'){
			continue;
		}

		struct stat buf;
		char full_path[STR_PATH_MAX];

		sprintf(full_path, "%s/%s", base_path, p->d_name);	/* フルパスの文字列を生成 */

		stat(full_path, &buf);
		//printf("p->d_name:%s mode:%d\n", p->d_name, buf.st_mode);

		if((buf.st_mode & S_IFMT) == S_IFDIR){	/* ディレクトリだったら再帰 */
			//printf("\tp->d_name:%s mode:%d\n", p->d_name, buf.st_mode);
			f += search_sysfs_f(full_path, searched);
		}
		else{
			if(searched){
				if(strcmp(p->d_name, "nr_lb_mc") == 0 || strcmp(p->d_name, "nr_lb_smt") == 0){
					snprintf(kp_entry_ctl.entries[entry_idx].tag, sizeof(char) * STR_TAG_MAX, "%s.sub", p->d_name);
					kp_entry_ctl.entries[entry_idx].pre_closure = sub_record;
				}
				else{
					strncpy(kp_entry_ctl.entries[entry_idx].tag, p->d_name, STR_TAG_MAX);
					kp_entry_ctl.entries[entry_idx].pre_closure = NULL;
				}

				kp_entry_ctl.entries[entry_idx].fp = fopen(full_path, "r");

				setbuf(kp_entry_ctl.entries[entry_idx].fp, NULL);	/* バッファリングをしない */

				entry_idx++;
			}
			f++;
		}
	}

	closedir(dirp);

	return f;

}

/*
	リソースの確保を行う関数
*/
bool kpreport_alloc_resources(void)
{
	if((kp_entry_ctl.tmp_fp = tmpfile()) == NULL){
		return false;
	}

	if((kp_entry_ctl.entries = calloc(kp_entry_ctl.nr_entries, sizeof(struct kp_entry))) == NULL){
		return false;
	}

	if((kp_entry_ctl.cpu_values = calloc(NR_PRE_RECORD_ENTRY * nr_cpus, sizeof(unsigned long long))) == NULL){
		return false;
	}

	if((flat_records = calloc(kp_entry_ctl.nr_entries * nr_cpus * MAX_RECORD, sizeof(unsigned long long))) == NULL){
		return false;
	}



	search_sysfs_f(BASE_PATH, 1);

	return true;
}

/*
	初期化関数
*/
void kpreport_init(void)
{
	int i;

	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);	/* CPU数を取得 */
	kp_entry_ctl.nr_entries = search_sysfs_f(BASE_PATH, 0);

	if(kpreport_alloc_resources() == false){
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < kp_entry_ctl.nr_entries; i++){	/* debug */
		puts(kp_entry_ctl.entries[i].tag);
	}

	if(kp_entry_ctl.entries == NULL){
		exit(EXIT_FAILURE);
	}
}

/*
	リソースの解放を行う関数
*/
bool kpreport_free_resources(void)
{
	int i;

	if(fclose(kp_entry_ctl.tmp_fp) != 0){
		return false;
	}

	for(i = 0; i < kp_entry_ctl.nr_entries; i++){
		if(fclose(kp_entry_ctl.entries[i].fp) != 0){
			return false;
		}
	}

	free(kp_entry_ctl.entries);

	free(kp_entry_ctl.cpu_values);

	free(flat_records);

	return true;
}


/*
	終了時に呼ばれる関数
*/
void kpreport_final(void *arg)
{

	records2csv();
	
	if(kpreport_free_resources() == false){
		exit(EXIT_FAILURE);
	}
}

/*
	コンマ区切りの文字列をCPUごとの数値の配列に格納する関数
	@cpu_val[] 格納先の配列 呼び出し元での宣言はcpu_val[nr_cpus]
*/
void buf2cpu_val(char *buf, unsigned long long cpu_val[])
{
	char *p;
	int idx = 0;

	p = strtok(buf, ",");
	cpu_val[idx] = atoll(p);
	idx++;

	while (p != NULL && idx < nr_cpus) {
		p = strtok(NULL, ",");
		if(p != NULL){
			cpu_val[idx] = atoll(p);
			idx++;
		}
	}
}

/*
	コンマ区切り文字列データを数値に変換してflat_recordsに代入する関数
	@buf ファイルの生データを格納してあるバッファ
	@entry_idx 読み込んだファイルの種類 *entriesの添字
*/
void add_record(char *buf, int entry_idx)
{
	unsigned long long cpu_val[nr_cpus];
	unsigned int i;
	unsigned long long (*nested_records)[MAX_RECORD][nr_cpus] = (unsigned long long (*)[MAX_RECORD][nr_cpus])flat_records;	/* 1次元配列を3次元配列に変換 */

	buf2cpu_val(buf, cpu_val);	/* split buf and substitute cpu_val[] */

	if(kp_entry_ctl.entries[entry_idx].pre_closure){
		if(strcmp(kp_entry_ctl.entries[entry_idx].tag, "nr_lb_smt.sub") == 0){
			kp_entry_ctl.entries[entry_idx].pre_closure(cpu_val, 0);
		}
		else if(strcmp(kp_entry_ctl.entries[entry_idx].tag, "nr_lb_mc.sub") == 0){
			kp_entry_ctl.entries[entry_idx].pre_closure(cpu_val, 1);
		}
	}

	for(i = 0; i < nr_cpus; i++){
		if(interrupt >= MAX_RECORD){	/* flat_recordsがオーバーフロー */
			records2csv();
			kpreport_final((void *)NULL);
			puts("timeout");
			exit(EXIT_SUCCESS);
		}
		else{
			nested_records[entry_idx][interrupt][i] = cpu_val[i];
		}
	}
}

/*
	kpreport_workerから定期的に呼び出される関数
*/
void read_periodic(void)
{
	int i;
	char buf[STR_ENTRY_MAX];

	for(i = 0; i < kp_entry_ctl.nr_entries; i++){
		fgets(buf, STR_ENTRY_MAX, kp_entry_ctl.entries[i].fp);
		add_record(buf, i);
		fseek(kp_entry_ctl.entries[i].fp, 0, SEEK_SET);	/* 次回のために頭出ししておく */
	}
}

/*
	BASE_PATH以下のファイルを読んで溜まったデータをCSVに書き出す関数
*/
void records2csv(void)
{
	int i, j, k;
	unsigned long long (*nested_records)[MAX_RECORD][nr_cpus] = (unsigned long long (*)[MAX_RECORD][nr_cpus])flat_records;	/* 1次元配列を3次元配列にキャスト */
	FILE *csv;

	csv = fopen("kpreport.csv", "w");

	for(i = 0; i < kp_entry_ctl.nr_entries; i++){
		fprintf(csv, "%s,,,\n", kp_entry_ctl.entries[i].tag);	/* タグ名を書き込む */
		for(j = 0; j < nr_cpus; j++){
			fprintf(csv, ",CPU%d", j);
		}
		fprintf(csv, "\n");

		for(j = 0; j < interrupt; j++){
			fprintf(csv, "%d", j * PERIOD);	/* 時間軸を書き込む */
			for(k = 0; k < nr_cpus; k++){
				fprintf(csv, ",%llu", nested_records[i][j][k]);	/* 値を書き込む */
			}
			fprintf(csv, "\n");
		}
		fprintf(csv, "\n\n");
	}

	fclose(csv);
}

void *kpreport_worker(void *arg)
{
	kpreport_init();

	pthread_cleanup_push(kpreport_final, NULL);

	while(1){
		interrupt++;
		read_periodic();
		sleep(PERIOD);
	}

	pthread_exit(NULL);
	pthread_cleanup_pop(1);
}

#if 0
int main(int argc, char *argv[])
{
	kpreport_init();

	while(1){
		interrupt++;
		read_periodic();
		sleep(PERIOD);
	}

	records2csv();

	kpreport_final();

	return 0;
}
#endif

