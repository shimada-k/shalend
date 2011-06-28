# Makefile
objs = shalen.o sheram.o lbprofile.o

shalend: Makefile shalen.h $(objs)
	cc -Wall -o shalend $(objs) -lpthread

shalen.o: shalen.c
	cc -Wall -c shalen.c

sheram.o: sheram.c
	cc -Wall -c sheram.c

lbprofile.o: lbprofile.c
	cc -Wall -c lbprofile.c

clean:
	rm -f shalend *.o *~
