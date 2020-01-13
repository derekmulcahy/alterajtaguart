/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 A. Theodore Markettos
 * All rights reserved.
 *
 * This software was developed by SRI International, the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology), and ARM Research under DARPA contract HR0011-18-C-0016
 * ("ECATS"), as part of the DARPA SSITH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Create a pseudo-tty (pty) and connect it to a JTAG UART stream
 * via JTAG Atlantic
 *
 * The device node is printed. You can connect to the pty with
 * (for example):
 * $ picocom /dev/pts/10
 *
 * Example:
 * jtaguart_pty --cable "USB-Blaster [5-1.3]" --device 1 --instance 0
 */


#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <poll.h>
#include <getopt.h>
#include <string.h>

#include "jtag_atlantic.h"
#include "common.h"

// scan JTAG every N milliseconds if no pty activity
#define POLL_TIMEOUT 20

int main(int argc, char *argv[])
{
    char termBuffer[16384];
    char uartBuffer[16384];

    char* cable = NULL;
    int device = -1;
    int instance = -1;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"cable", required_argument, 0, 'c'},
        {"device", required_argument, 0, 'd'},
        {"instance", required_argument, 0, 'i'},
        {0, 0, 0, 0}
    };
    int option_index = 0;
    int c = 0;

    while ((c = getopt_long(argc, argv, "hc:d:i:",
            long_options, &option_index)) != -1) {
//        int this_option_optind = optind ? optind : 1;
        switch (c) {
            case 'c':
                cable = strdup(optarg);
                break;
            case 'd':
                device = atoi(optarg);
                break;
            case 'i':
                instance = atoi(optarg);
                break;
            case 'h':
            default:
                fprintf(stderr, "Syntax:\n");
                fprintf(stderr, "%s [--cable <cable>] [--device <device>] [--instance <instance>] [--help]\n", argv[0]);
                exit(0);
        }
    }

#ifdef DEBUG
    fprintf(stderr,"cable = '%s', device = %d, instance = %d\n", cable, device, instance);
#endif

    // open a pty and print the device node
    int ptyDescriptor = posix_openpt(O_RDWR | O_NOCTTY);

    int result = grantpt(ptyDescriptor);
    if (!result) {
        result = unlockpt(ptyDescriptor);
    }
    if (result) {
        fprintf(stderr, "Couldn't open pty\n");
        exit(2);
    }

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
    JTAGATLANTIC *atlantic = jtagatlantic_open(cable, device, instance, "pty_uart");
    if(!atlantic) {
        show_err();
        return 1;
    }
#ifdef DEBUG
    show_info(atlantic);
    fprintf(stderr, "Unplug the cable or press ^C to stop.\n");
#endif

    // get ready for poll()
    struct pollfd pollfds[1];
    pollfds[0].fd = ptyDescriptor;
    pollfds[0].events = POLLIN;


    // poll each end for data, send to the other side
    while(1) {

        ssize_t uartCount = jtagatlantic_bytes_available(atlantic);
        if (uartCount > 0) {
            jtagatlantic_read(atlantic, uartBuffer, uartCount);
            ssize_t writeCount = write(ptyDescriptor, uartBuffer, uartCount);
#ifdef DEBUG
            printf("rx%ld,%ld\n", uartCount, writeCount);
#else
            writeCount = writeCount; // silence compiler warning
#endif
            fsync(ptyDescriptor);
        }

        // look for activity on the pty
        poll(pollfds, 1, POLL_TIMEOUT /* ms */);

        if (pollfds[0].revents & POLLIN) {
            pollfds[0].revents = 0;
            ssize_t termCount = read(pollfds[0].fd, termBuffer, sizeof termBuffer);

            if (termCount > 0) {
                jtagatlantic_write(atlantic, termBuffer, termCount);
#ifdef DEBUG
                printf("tx%ld\n", termCount);
#endif
                jtagatlantic_flush(atlantic);
            }
        }
        
    }

}
