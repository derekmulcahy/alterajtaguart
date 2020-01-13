CXX=g++
CC=$(CXX)
CXXFLAGS=-O2 -Wall
LDFLAGS=-L${QUARTUS_ROOTDIR}/linux64
# -Wl,-rpath=.
LDLIBS=-ljtag_atlantic -ljtag_client
all: echohello counterperf pty_uart
echohello: echohello.o common.o
counterperf: counterperf.o common.o
pty_uart: pty_uart.o common.o
clean:
	rm -f echohello echohello.o counterperf counterperf.o common.o pty_uart.o pty_uart
