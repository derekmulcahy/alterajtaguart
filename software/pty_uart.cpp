#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#include "jtag_atlantic.h"
#include "common.h"

int main(void)
{
    char termBuffer[16384];
    char uartBuffer[16384];

    // open a pty and print the device node
    int ptyDescriptor = posix_openpt(O_RDWR | O_NOCTTY);

    int result = grantpt(ptyDescriptor);
    result = unlockpt(ptyDescriptor);

    char* deviceName = ptsname(ptyDescriptor);
    puts(deviceName);

    // set it nonblocking
    int flags = fcntl (ptyDescriptor, F_GETFL);
    if (flags < 0) {
        fprintf(stderr, "Couldn't set pty nonblocking\n");
        exit(1);
    }
    flags |= O_NONBLOCK;
    if (fcntl (ptyDescriptor, F_SETFL, flags) < 0) {
        fprintf(stderr, "Couldn't set pty nonblocking\n");
        exit(1);
    }


    // open the JTAG UART
    JTAGATLANTIC *atlantic = jtagatlantic_open(NULL, -1, -1, "pty_uart");
    if(!atlantic) {
        show_err();
        return 1;
    }
    show_info(atlantic);
    fprintf(stderr, "Unplug the cable or press ^C to stop.\n");

    // poll each end for data, send to the other side
    while(1) {

        ssize_t uartCount = jtagatlantic_bytes_available(atlantic);
        if (uartCount > 0) {
            jtagatlantic_read(atlantic, uartBuffer, uartCount);
            ssize_t writeCount = write(ptyDescriptor, uartBuffer, uartCount);
            printf("rx%ld,%ld\n", uartCount, writeCount);
            fsync(ptyDescriptor);
        }

        ssize_t termCount = read(ptyDescriptor, termBuffer, sizeof termBuffer);

        if (termCount > 0) {
            jtagatlantic_write(atlantic, termBuffer, termCount);
            printf("tx%ld\n", termCount);
            jtagatlantic_flush(atlantic);
        }
        
    }

}
