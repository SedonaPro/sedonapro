/*
 * Copyright © 2008-2014 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <modbus.h>

#if defined(_WIN32)
#define close closesocket
#endif

enum {
    TCP,
    RTU
};

int main(int argc, char *argv[])
{
    int s = -1;
    int cnt = 0;
    modbus_t *ctx = NULL;
    modbus_mapping_t *mb_mapping = NULL;
    int rc;
    int use_backend;

     /* TCP */
    if (argc > 1) {
        if (strcmp(argv[1], "tcp") == 0) {
            use_backend = TCP;
        } else if (strcmp(argv[1], "rtu") == 0) {
            use_backend = RTU;
        } else {
            printf("Usage:\n  %s [tcp|rtu] - Modbus client to measure data bandwith\n\n", argv[0]);
            exit(1);
        }
    } else {
        /* By default */
        use_backend = TCP;
    }

    for(;;) {
	    if (use_backend == TCP) {
		    ctx = modbus_new_tcp("127.0.0.1", 1502);
		    s = modbus_tcp_listen(ctx, 1);
		    modbus_tcp_accept(ctx, &s);

	    } else {
		    ctx = modbus_new_rtu("/dev/ttyUSB0", 115200, 'N', 8, 1);
		    modbus_set_slave(ctx, 1);
		    modbus_connect(ctx);
	    }

	    mb_mapping = modbus_mapping_new(MODBUS_MAX_READ_BITS, 0,
					    MODBUS_MAX_READ_REGISTERS, 0);
	    if (mb_mapping == NULL) {
		    fprintf(stderr, "Failed to allocate the mapping: %s\n",
			    modbus_strerror(errno));
		    modbus_free(ctx);
		    return -1;
	    }

	    for(;;) {
		    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

		    rc = modbus_receive(ctx, query);
		    if (rc > 0) {
			    modbus_reply(ctx, query, rc, mb_mapping);
		    } else if (rc  == -1) {
			    /* Connection closed by the client or error */
			    break;
		    }
		    cnt++;
		    int f = 291*cnt;
		    mb_mapping->tab_registers[0] = cnt++;
		    mb_mapping->tab_registers[1] = (f) & 0xFFFF;
		    mb_mapping->tab_registers[2] = (f >> 16) & 0xFFFF;
		    mb_mapping->tab_registers[3] = 42;

		    float x = 42.42;
		    int   x_int = *(int*)&x;
		    mb_mapping->tab_registers[4] = x_int & 0xFFFF;
		    mb_mapping->tab_registers[5] = (x_int>>16) & 0xFFFF;

		    mb_mapping->tab_registers[6] = (x_int>>16) & 0xFFFF;
		    mb_mapping->tab_registers[7] = x_int & 0xFFFF;

		    mb_mapping->tab_registers[8] = 2;
		    mb_mapping->tab_registers[9] = 4;
	    }

	    printf("Quit the loop: %s\n", modbus_strerror(errno));

	    modbus_mapping_free(mb_mapping);
	    if (s != -1) {
		    close(s);
	    }
	    /* For RTU, skipped by TCP (no TCP connect) */
	    modbus_close(ctx);
	    modbus_free(ctx);
    }
    return 0;
}
