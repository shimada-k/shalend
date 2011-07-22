# Makefile
objs = shalen.o kpreport.o lbprofile.o

shalend: Makefile shalen.h $(objs)
	cc -Wall -o shalend $(objs) -lpthread

shalen.o: shalen.c
	cc -Wall -c shalen.c

kpreport.o: kpreport.c
	cc -Wall -c kpreport.c

lbprofile.o: lbprofile.c
	cc -Wall -c lbprofile.c

analb: analb.c
	cc -o analb analb.c

install: shalend
	scp shalend shimada@192.168.141.250:/home/shimada/shalend/

install-all: shalend analb
	scp shalend shimada@192.168.141.250:/home/shimada/shalend/
	scp analb shimada@192.168.141.250:/home/shimada/shalend/

clean:
	rm -f shalend *.o *~
