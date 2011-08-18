# cmd/Makefile

objs = analb init_msr
lib_path = ../lib


all: $(objs)

# コマンドの実行ファイル
analb: analb.c
	cc -o analb analb.c

init_msr: init_msr.c ../lib/libshalend.a
	cc -o init_msr init_msr.c ../lib/libshalend.a

clean:
	rm -f $(objs) 

