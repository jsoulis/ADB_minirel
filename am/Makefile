# Makefile for AM layer
LIB	= am1
INCDIR	= ${MINIREL_HOME}/h
INCS	= 
SRCS	= am.c utils.c
TESTS	= amtest.c
OBJS	= ${SRCS:.c=.o}
LIBS	= lib${LIB}.a ../pf/libpf.a ../hf/libhf.a ../bf/libbf.a
MOCK_LIBS = lib${LIB}.a ../pf/libpf.a libhfmock.a ../bf/libbf.a

#############################################################################
# This macro definition can be overwritten by command-line definitions.
CC	= gcc
CFLAGS	= -g -ansi -pedantic 
#CFLAGS	= -O -ansi -pedantic
#############################################################################

all: lib${LIB}.a ${LIB}test own_test

own_test: libhfmock.a lib${LIB}.a
	$(CC) $(CFLAGS) own_test.c -o own_test -I$(INCDIR) $(MOCK_LIBS)

libhfmock.a: hfmock.c
	ar cr libhfmock.a hfmock.c
	ranlib libhfmock.a


${LIB}test: ${LIB}test.o lib${LIB}.a
	$(CC) $(CFLAGS) -o $@ $< ${LIBS}

lib${LIB}.a: $(OBJS)
	ar cr lib${LIB}.a $(OBJS)
	ranlib lib${LIB}.a

clean:
	rm -f lib${LIB}.a *.o ${LIB}test *.bak *~

.c.o:; $(CC) $(CFLAGS) -c $< -I. -I$(INCDIR)
