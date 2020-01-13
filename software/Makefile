CXX=g++
CC=$(CXX)
CXXFLAGS=-O2 -Wall
LDFLAGS=-L${QUARTUS_ROOTDIR}/linux64
# -Wl,-rpath=.
LDLIBS=-ljtag_atlantic -ljtag_client
all: echohello counterperf jtaguart_pty
echohello: echohello.o common.o
counterperf: counterperf.o common.o
jtaguart_pty: jtaguart_pty.o common.o
clean:
	rm -f echohello echohello.o counterperf counterperf.o common.o jtaguart_pty jtaguart_pty.o
