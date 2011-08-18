# lib/Makefile

objs = msr.o bitops.o cpuid.o

all: $(objs)
	ar r libshalend.a $(objs)

# 依存関係のあるオブジェクトファイル
msr.o: msr.c msr.h
	cc -Wall -c msr.c

bitops.o: bitops.c
	cc -Wall -c bitops.c

cpuid.o: cpuid.c cpuid.h
	cc -Wall -c cpuid.c

clean:
	rm -f *.o libshalend.a
