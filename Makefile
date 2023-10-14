
CFLAGS = -Wall -Wextra -Wconversion -Werror -g -O2 -std=gnu++20 -l pthread -lboost_program_options  -Wl,-rpath -Wl,/opt/gcc-11.2/lib64
CC = /opt/gcc-11.2/bin/g++-11.2

robots-client: robots-client.o
	$(CC) $(CFLAGS) -o $@ robots-client.o

clean:
	-rm -f *.o robots-client

.cpp.o:
	$(CC) $(CFLAGS) -c $<
