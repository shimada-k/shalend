#include <fcntl.h>
#include <sys/ioctl.h>

#include "shalen.h"
#include "lbprofile.h"

FILE *flb;
int dev;
struct lbprofile *hndlr_buf;
struct lbprofile_hdr hdr;

extern char *wd_path;

int lbprofile_alloc_resources(void)
{
	char path[PATH_MAX];

	snprintf(path, PATH_MAX, "%s%s", wd_path, "output.lb");

	if(!(flb = fopen(path, "w+"))){	/* output file */
		return 1;
	}
	if((dev = open("/dev/lbprofile", O_RDONLY)) < 0){
		return 1;
	}

	if(!(hndlr_buf = calloc(GRAN_LB, sizeof(struct lbprofile)))){
		return 1;
	}

	return 0;
}

void lbprofile_free_resources(void)
{
	close(dev);
	fclose(flb);
	free(hndlr_buf);
}

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

void lbprofile_final(void)
{
	unsigned int i, piece;
	ssize_t r_size;

	if(ioctl(dev, IOC_USEREND_NOTIFY, &piece) < 0){
		syslog(LOG_ERR, "%s ioctl(2) failed", log_err_prefix(lbprofile_final));
		lbprofile_free_resources();
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

		for(i = 0; i < tip; i++){
			syslog(LOG_DEBUG, "pid:%d, src_cpu:%d, dst_cpu:%d", hndlr_buf[i].pid, 
				hndlr_buf[i].src_cpu, hndlr_buf[i].dst_cpu);
		}

		if(fwrite(hndlr_buf, sizeof(struct lbprofile), tip, flb) != tip){
			syslog(LOG_ERR, "%s fwrite(3) failed.", log_err_prefix(lbprofile_final));
		}
		hdr.nr_lbprofile += tip;
	}

	hdr.nr_cpus = nr_cpus;

	put_hdr(&hdr);

	lbprofile_free_resources();
	//syslog(LOG_DEBUG, "lbprofile_free_resources() successfully done\n");
}

void lbprofile_handler(int sig)
{
	int i;
	ssize_t s_read;

	if((s_read = read(dev, hndlr_buf, sizeof(struct lbprofile) * GRAN_LB)) != sizeof(struct lbprofile) * GRAN_LB){
		syslog(LOG_ERR, "%s read(2) failed. lbentries was not loaded s_read = %d\n", log_err_prefix(lbprofile_handler), (int)s_read);
		lbprofile_free_resources();
		exit(EXIT_FAILURE);
	}

	if(fwrite(hndlr_buf, sizeof(struct lbprofile), GRAN_LB, flb) != GRAN_LB){
		lbprofile_free_resources();
		syslog(LOG_ERR, "%s fwrite(3) failed", log_err_prefix(lbprofile_handler));
		exit(EXIT_FAILURE);
	}

	hdr.nr_lbprofile += GRAN_LB;

	lseek(dev, 0, SEEK_SET);
}

/* 返り値　成功：0　失敗：1 */
int lbprofile_init(void)
{
	if(lbprofile_alloc_resources()){
		return 1;
	}

	fseek(flb, (long)sizeof(struct lbprofile_hdr), SEEK_SET);	/* make a header space */

	signal(SIGUSR1, lbprofile_handler);
	syslog(LOG_DEBUG, "SETHNDLR:%d IOC_SETPID:%d\n", IOC_SETSIGNO, IOC_SETPID);

	if(ioctl(dev, IOC_SETSIGNO, SIGUSR1) < 0){
		syslog(LOG_ERR, "%s IOC_SETSIGNO", log_err_prefix(lbprofile_init));
		return 1;
	}

	if(ioctl(dev, IOC_SETGRAN, GRAN_LB) < 0){
		syslog(LOG_ERR, "%s IOC_SETGRAN", log_err_prefix(lbprofile_init));
		return 1;
	}

	if(ioctl(dev, IOC_SETPID, (int)getpid()) < 0){
		syslog(LOG_ERR, "%s IOC_SETPID", log_err_prefix(lbprofile_init));
		return 1;
	}

	return 0;
}

void *lbprofile_worker(void *arg)
{
	if(lbprofile_init()){
		syslog(LOG_ERR, "%s lbprofile_init() failed", log_err_prefix(lbprofile_worker));
		goto finalize;
	}

	while(1){
		if(tos == SIGTERM_RECEPT){
			break;
		}
		/* will recieve signal SIGUSR1, and call lbprofile_handler() */
	}

finalize:
	syslog(LOG_NOTICE, "stopping lbprofile");
	lbprofile_final();
	tos = LBPROFILE_STOPPED;

	pthread_exit(NULL);
}





