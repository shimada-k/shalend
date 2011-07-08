#include <time.h>
#include "shalen.h"

int interrupt;	/* elapsed time == PERIOD * interrupt [second] */
unsigned long long *flat_records;	/* vruntimeが入るので64bit整数 */
char axes[MAX_PARAM][AXIS_LINE_MAX];

void nr_load_balance2csv(void);
void set_axes(char table[][AXIS_LINE_MAX]);
void records2csv(void);

int sheram_alloc_resources(void)
{
	if(!(flat_records = calloc(DIVIDER * nr_cpus * MAX_RECORD, sizeof(unsigned long long)))){
		syslog(LOG_ERR, "%s cannot alloc flat_records", log_err_prefix(sheram_alloc_resources));
		return 1;
	}
	return 0;
}

void sheram_free_resources(const char *called)
{
	syslog(LOG_NOTICE, "%s calls sheram_free_resources()\n", called);
	free(flat_records);
}

int sheram_init(void)
{
	char axis_table[MAX_PARAM][AXIS_LINE_MAX];

	if(sheram_alloc_resources()){
		return 1;
	}
	set_axes(axis_table);	/* axes = axis_table; */

	return 0;
}

void sheram_final(void)
{
	records2csv();
	nr_load_balance2csv();

	sheram_free_resources(__func__);
}

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

/* コンマ区切り文字列データを数値に変換してflat_recordsに代入する関数 */
void add_record(char *buf, enum sheram param)
{
	unsigned long long cpu_val[nr_cpus];
	unsigned int i;
	unsigned long long (*nested_records)[MAX_RECORD][nr_cpus] = (unsigned long long (*)[MAX_RECORD][nr_cpus])flat_records;	/* 1次元配列を3次元配列に変換 */

	buf2cpu_val(buf, cpu_val);	/* split buf and substitute cpu_val[] */

	for(i = 0; i < nr_cpus; i++){
		if(interrupt >= MAX_RECORD){	/* flat_recordsがオーバーフロー */
			syslog(LOG_ERR, "%s array has overflowed", log_err_prefix(add_record));
			tos = SIGTERM_RECEPT;
		}
		else{
			nested_records[param][interrupt][i] = cpu_val[i];
		}
	}
}

/* /proc/paramsからデータをとってくる関数。/proc/paramsはこの関数の中でopen(2) & close(2) */
void poll_data(char table[][DATA_LINE_MAX])
{
	int i = RQ_RUNNING;
	size_t len;
	char buf[DATA_LINE_MAX];
	FILE *prms = NULL;

	if(!(prms = fopen(PARAMS, "r"))){
		syslog(LOG_ERR, "%s cannot open /proc/params", log_err_prefix(poll_data));
		exit(EXIT_FAILURE);
	}

	while(i < DIVIDER){
		fgets(buf, DATA_LINE_MAX, prms);

		if(buf[0] == '#' && buf[1] != '#'){
			fgets(buf, DATA_LINE_MAX, prms);	/* 「;」から始まる行をスキップ */
			fgets(buf, DATA_LINE_MAX, prms);	/* データ行を取得 */

			strncpy(table[i], buf, DATA_LINE_MAX);
			len = strlen(table[i]);
			table[i][len - 1] = '\0';	/* parge '\n' */
			i++;
		}
	}

	if(fclose(prms) == EOF){
		syslog(LOG_ERR, "%s cannot close /proc/params", log_err_prefix(poll_data));
		exit(EXIT_FAILURE);
	}
	puts("poll_data done");
}

/* /proc/paramsからパラメータ名を取得する関数 */
void set_axes(char table[][AXIS_LINE_MAX])
{
	int i = RQ_RUNNING;
	size_t len;
	char buf[AXIS_LINE_MAX];
	FILE *prms = NULL;

	if(!(prms = fopen(PARAMS, "r"))){
		syslog(LOG_ERR, "%s cannot open /proc/params", log_err_prefix(set_axes));
		exit(EXIT_FAILURE);
	}

	while(fgets(buf, AXIS_LINE_MAX, prms) != NULL){
		
		if(buf[0] == '#'){
			strncpy(table[i], buf, AXIS_LINE_MAX);
			len = strlen(table[i]);
			table[i][len - 1] = '\0';	/* parge '\n' */

			if(buf[1] != '#'){	/* 時間軸で追っていかないといけないエントリ。table[i]がcsvファイルに書き込まれる */
				char tmp[len - 1];
				int j;
				for(j = 0; j < len - 1; j++){	/* parge '#' */
					tmp[j] = table[i][j + 1];
				}
				for(j = 0; j < len - 1; j++){
					table[i][j] = tmp[j];
				}
				puts(table[i]);
				i++;
			}
			else{	/* sheramの終了時に読めばいいエントリ */
				char tmp[len - 1];
				int j;
				for(j = 0; j < len - 2; j++){	/* parge double '#' */
					tmp[j] = table[i][j + 2];
				}
				for(j = 0; j < len - 2; j++){
					table[i][j] = tmp[j];
				}
				//syslog(LOG_DEBUG, "table[%d] = %d", i, table[i]);
				i++;
			}
		}
	}

	for(i = RQ_RUNNING; i < MAX_PARAM; i++){	/* copy addreses */
		strncpy(axes[i], table[i], AXIS_LINE_MAX);
		//syslog(LOG_DEBUG, "%s\n", axes[i]);
	}

	if(fclose(prms) == EOF){
		syslog(LOG_ERR, "%s cannot close /proc/params", log_err_prefix(set_axes));
		exit(EXIT_FAILURE);
	}
}

void get_lb_data(char table[][DATA_LINE_MAX])
{
	int i = 0;
	size_t len;
	char buf[DATA_LINE_MAX];
	FILE *prms = NULL;

	if(!(prms = fopen(PARAMS, "r"))){
		syslog(LOG_ERR, "%s cannot open /proc/params", log_err_prefix(get_lb_data));
		exit(EXIT_FAILURE);
	}

	while(i < MAX_PARAM - DIVIDER){
		fgets(buf, DATA_LINE_MAX, prms);

		if(buf[0] == '#' && buf[1] == '#'){
			fgets(buf, DATA_LINE_MAX, prms);	/* 「;」から始まる行をスキップ */
			fgets(buf, DATA_LINE_MAX, prms);
			strncpy(table[i], buf, DATA_LINE_MAX);
			len = strlen(table[i]);
			table[i][len - 1] = '\0';	/* parge '\n' */
			i++;
		}
	}

	if(fclose(prms) == EOF){
		syslog(LOG_ERR, "%s cannot close /proc/params", log_err_prefix(get_lb_data));
		exit(EXIT_FAILURE);
	}
}

/* ロードバランスの回数をCSVファイルに出力 */
void nr_load_balance2csv(void)
{
	unsigned long long cpu_val[MAX_PARAM - DIVIDER][nr_cpus];
	int i, j;
	char data_table[MAX_PARAM - DIVIDER][DATA_LINE_MAX];

	get_lb_data(data_table);	/* /proc/params 2 data_table[][] */

	for(i = 0; i < MAX_PARAM - DIVIDER; i++){
		buf2cpu_val(data_table[i], cpu_val[i]);
	}

	for(i = 0; i < MAX_PARAM - DIVIDER; i++){
		switch(i % MAX_DOMAIN_LV){
			case 0:
				fprintf(csv, "SMT-domain\n");
				break;
			case 1:
				fprintf(csv, "MC-domain\n");
				break;
		}
		fprintf(csv, "cpu-no,%s\n", axes[i + DIVIDER]);

		for(j = 0; j < nr_cpus; j++){
			fprintf(csv, "cpu%d,%llu\n", j, cpu_val[i][j]);
		}
		fprintf(csv, "\n\n");
	}
}

/* /proc/paramsを読んで溜まったデータをCSVに書き出す関数 */
void records2csv(void)
{
	int i, j, k;
	unsigned long long (*nested_records)[MAX_RECORD][nr_cpus] = (unsigned long long (*)[MAX_RECORD][nr_cpus])flat_records;	/* 1次元配列を3次元配列にキャスト */

	for(i = RQ_RUNNING; i < DIVIDER; i++){
		fprintf(csv, "%s,,,\n", axes[i]);	/* セクション名を書き込む */
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
}

void *sheram_worker(void *arg)
{
	int i;
	char data_table[DIVIDER][DATA_LINE_MAX];

	if(sheram_init()){
		syslog(LOG_ERR, "%s sheram_init() failed", log_err_prefix(sheram_worker));
		goto finalize;
	}

	syslog(LOG_NOTICE, "sheram loop starting tos:%d\n", tos);

	while(1){
		//if(tos == SIGTERM_RECEPT){
		if(death_flag == 1){
			break;
		}
		else{
			poll_data(data_table);

			for(i = RQ_RUNNING; i < DIVIDER; i++){
				add_record(data_table[i], i);
			}
			interrupt++;

			sleep(PERIOD);	/* PERIOD秒スリープ */
		}
	}

	syslog(LOG_NOTICE, "sheram loop breaked tos:%d\n", tos);

finalize:
	syslog(LOG_NOTICE, "stopping sheram");
	sheram_final();
	tos = SHERAM_STOPPED;
	barrier();

	pthread_exit(NULL) ;
}

