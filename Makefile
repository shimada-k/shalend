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

clean:
	rm -f shalend *.o *~
