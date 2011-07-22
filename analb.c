#include <stdio.h>
#include <stdlib.h>	/* exit(3) */
#include <unistd.h>	/* sysconf(3) */
#include <string.h>	/* memset(3) */

#include "lbprofile.h"

struct lbprofile_hdr hdr;
int nr_cpus;

/* たらい回し現象をCSVに出力 */
void output_csv_ho(FILE *csv, unsigned long long hold[])
{
	int i;

	fprintf(csv, "hold_officer\n\n");

	for(i = 0; i < MAX_NR_HOLD; i++){
		fprintf(csv, "%llu,", hold[i]);
	}
	fprintf(csv, "\n\n");
}

/* ピンポンタスク現象をCSVに出力 */
void output_csv_pp(FILE *csv, unsigned long long map[][nr_cpus])
{
	int i, j;

	puts("hoge");

	fprintf(csv, "ping_pong\n\n");

	puts("hoge2");

	for(i = 0; i < nr_cpus; i++){
		fprintf(csv, ",CPU%d", i);
	}
	fprintf(csv, "\n");

	for(i = 0; i < nr_cpus; i++){
		fprintf(csv, "CPU%d,", i);

		for(j = 0; j < nr_cpus; j++){
			fprintf(csv, "%llu,", map[i][j]);
			printf("%llu,", map[i][j]);
		}

		fprintf(csv, "\n");
		putchar('\n');
	}
	fprintf(csv, "\n");
}

/* *.lbファイルからstruct lbprofileの配列にデータを取り込む関数 */
void analyze_lb_and_store(FILE *flb, unsigned long long pp[][nr_cpus], unsigned long long ho[])
{
	struct lbprofile lb, nearly;
	struct pid_loop lo_pp, lo_ho;

	memset(pp, 0, nr_cpus * nr_cpus * sizeof(unsigned long long));
	memset(ho, 0, MAX_NR_HOLD * sizeof(unsigned long long));

	lo_pp.loop = 0;
	lo_ho.loop = 0;

	fseek(flb, 0L, SEEK_SET);	/* 先頭までseek */

	if(fread(&hdr, sizeof(struct lbprofile_hdr), 1, flb) == 1){
		printf("nr_cpus = %d, nr_lbprofile = %u\n", hdr.nr_cpus, hdr.nr_lbprofile);	/* ヘッダ情報を出力 */
	}

	while(fread(&lb, sizeof(struct lbprofile), 1, flb) == 1){	/* struct lbprofileが読み込める限りloop */
		if(nearly.pid == lb.pid){
			if(nearly.src_cpu == lb.dst_cpu && nearly.dst_cpu == lb.src_cpu){	/* ピンポンタスク現象 */
				if(lo_pp.loop == 0){
					lo_pp.src_cpu = nearly.src_cpu;
					lo_pp.dst_cpu = nearly.dst_cpu;
				}
				lo_pp.loop++;
			}
			else{	/* たらい回し現象 */
				if(lo_ho.loop == 0){
					lo_ho.src_cpu = nearly.src_cpu;
					lo_ho.dst_cpu = nearly.dst_cpu;
				}
				lo_ho.loop++;
			}
		}
		else{
			if(lo_pp.loop != 0){
				pp[lo_pp.src_cpu][lo_pp.dst_cpu] += lo_pp.loop;	/* ループした数だけ増やしていく */
				lo_pp.loop = 0;
			}
			if(lo_ho.loop != 0){
				ho[lo_ho.loop - 1]++;	/* 配列の添字は0から */
				lo_ho.loop = 0;
			}
		}
		nearly = lb;
	}
}

int main(int argc, char *argv[])
{
	nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	//nr_cpus = 8;
	unsigned long long pp[nr_cpus][nr_cpus], ho[MAX_NR_HOLD];
	FILE *csv = NULL, *flb = NULL;

	if((csv = fopen("kpreport.csv", "a")) == NULL){
		exit(EXIT_FAILURE);
	}

	fprintf(csv, "hogepiyo\n\n");

	if((flb = fopen("lbprofile.lb", "rb")) == NULL){
		exit(EXIT_FAILURE);
	}

	//analyze_lb_and_store(flb, pp, ho);

	fprintf(csv, "hogehoge\n\n");

	puts("analyze_lb_and_store() successfully done");

	output_csv_pp(csv, pp);
	puts("output_csv_pp() successfully done");
	output_csv_ho(csv, ho);
	puts("output_csv_ho() successfully done");

	fclose(flb);
	fclose(csv);

	return 0;
}

