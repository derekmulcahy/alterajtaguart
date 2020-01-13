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
 * The device node is printed. You can connect to the pty with:
 * $ picocom /dev/pts/10
 *
 * At present doesn't receive cable/device/instance on command line
 * so only one UART device is supported
 */


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

#ifdef DEFAULT_CABLE
    const char* cable = NULL;
    const int device = -1;
    const int instance = -1;
#else
    const char* cable = "1";
    const int device = 1;
    const int instance = 1;
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

        ssize_t termCount = read(ptyDescriptor, termBuffer, sizeof termBuffer);

        if (termCount > 0) {
            jtagatlantic_write(atlantic, termBuffer, termCount);
#ifdef DEBUG
            printf("tx%ld\n", termCount);
#endif
            jtagatlantic_flush(atlantic);
        }
        
    }

}
