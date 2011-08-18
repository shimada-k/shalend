# Makefile
objs = shalen.o kpreport.o lbprofile.o l3miss.o
lib_path = ./lib
cmd_path = ./cmd


# shalend 関連のオブジェクトファイル
shalend: Makefile shalen.h $(objs) $(lib_path)/libshalend.a
	cc -Wall -o shalend $(objs) $(lib_path)/libshalend.a -lpthread

shalen.o: shalen.c
	cc -Wall -c shalen.c

kpreport.o: kpreport.c
	cc -Wall -c kpreport.c

lbprofile.o: lbprofile.c
	cc -Wall -c lbprofile.c

l3miss.o: l3miss.c
	cc -Wall -c l3miss.c

$(lib_path)/libshalend.a:
	cd $(lib_path); make

.PHONY: cmd
cmd:
	cd $(cmd_path); make

# 各種操作
install: shalend
	scp shalend shimada@192.168.141.250:/home/shimada/shalend/

install-all: shalend analb
	scp shalend shimada@192.168.141.250:/home/shimada/shalend/
	scp analb shimada@192.168.141.250:/home/shimada/shalend/

clean:
	rm -f shalend *.o *~

clean-all:
	rm -f shalend *.o
	cd $(cmd_path); make clean
	cd $(lib_path); make clean

