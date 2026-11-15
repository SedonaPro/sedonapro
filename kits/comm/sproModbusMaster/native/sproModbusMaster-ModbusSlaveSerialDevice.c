//
// Copyright 2016 Andrey Skvortsov <andrej.skvortzov@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "sedona.h"

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <modbus.h>

/* #define NATIVE_DEBUG */

#include "sproModbusMaster_list.h"
#include "sproModbusMaster_common.h"
#include "sproModbusMaster_ModbusSlaveSerialDevice.h"
#include "sproUtil.h"
#include "sproUtil_SerialCommon.h"

struct modbus_master_serial_cfg {
	int port;
	int baudrate;
	int databits;
	int parity;
	int stopbits;
	int rtsMode;
};

struct modbus_master_serial_thread {
	struct modbus_master_thread thread;

	struct modbus_master_serial_cfg sedona;
	struct modbus_master_serial_cfg backend_copy;
};



static void  modbus_master_serial_init(struct modbus_master_thread *thread)
{
	struct modbus_master_serial_thread *ptr;
	ptr = container_of(thread, struct modbus_master_serial_thread, thread);

	pthread_mutex_lock_checked(&ptr->thread.master_mutex, __func__);
	ptr->backend_copy = ptr->sedona;
	ptr->thread.update = false;
	pthread_mutex_unlock(&ptr->thread.master_mutex);

	int max_port = serialMaxPortNum;
	if (ptr->backend_copy.port > max_port)
		ptr->backend_copy.port = 0;

	ptr->thread.modbus_ctx = modbus_new_rtu(
		serial_ports_filenames[ptr->backend_copy.port],
		baudrates[ptr->backend_copy.baudrate],
		parity[   ptr->backend_copy.parity],
		ptr->backend_copy.databits,
		ptr->backend_copy.stopbits);
}

static int   modbus_master_serial_after_connect(struct modbus_master_thread *thread)
{
	struct modbus_master_serial_thread *ptr;
	ptr = container_of(thread, struct modbus_master_serial_thread, thread);

#if LIBMODBUS_VERSION_CHECK(3, 1, 0)
	int rc;
	if (rc = modbus_rtu_set_rts(ptr->thread.modbus_ctx, ptr->backend_copy.rtsMode)) {
		if (rc == -1) {
			ERROR("Unable to set RTS mode - %s (%d)\n", modbus_strerror(errno), errno);
			return -1;
		}
	}
#else
#warning "There is no RS485 support!"
#endif
	return 0;
}

static void   modbus_master_serial_free(struct modbus_master_thread *thread)
{
	struct modbus_master_serial_thread *ptr;
	ptr = container_of(thread, struct modbus_master_serial_thread, thread);
	memset(ptr, 0, sizeof(*ptr));
	free(ptr);
}


static void   modbus_master_serial_print_cfg(struct modbus_master_serial_cfg* cfg)
{
	printf("port:     %d\n", cfg->port);
	printf("baudrate: %d\n", cfg->baudrate);
	printf("databits: %d\n", cfg->databits);
	printf("parity:   %d\n", cfg->parity);
	printf("stopbits: %d\n", cfg->stopbits);
	printf("rtsMode:  %d\n", cfg->rtsMode);
}

static void   modbus_master_serial_print(struct modbus_master_thread *thread)
{
	struct modbus_master_serial_thread *ptr;
	ptr = container_of(thread, struct modbus_master_serial_thread, thread);
	printf("sedona config\n");
	modbus_master_serial_print_cfg(&ptr->sedona);
	printf("backend config\n");
	modbus_master_serial_print_cfg(&ptr->backend_copy);
}





static const struct modbus_master_backend_ops modbus_master_backend_serial_ops = {
	.init          = modbus_master_serial_init,
	.after_connect = modbus_master_serial_after_connect,
	.print         = modbus_master_serial_print,
	.free          = modbus_master_serial_free
} ;


Cell sproModbusMaster_ModbusSlaveSerialAsyncNetwork_addNetwork(SedonaVM* vm, Cell* params)
{
	int ret;
	uint8_t* self = params[0].aval;
	Cell ret_cell;

	struct modbus_master_serial_thread *ptr = malloc(sizeof(*ptr));

	if (!ptr) {
		ERROR("failed allocate memory\n");
		return zeroCell;
	}
	memset(ptr, 0, sizeof(*ptr));
	ret_cell.aval = &ptr->thread;

	ptr->thread.type = RTU;
	ptr->thread.ops = &modbus_master_backend_serial_ops;
	ptr->thread.component = self;

	INIT_LIST_HEAD(&ptr->thread.threads);
	INIT_LIST_HEAD(&ptr->thread.regs);
	pthread_mutex_init(&ptr->thread.master_mutex, NULL);

	ptr->sedona.port     = params[1].ival;
	ptr->sedona.baudrate = params[2].ival;
	ptr->sedona.databits = params[3].ival;
	ptr->sedona.parity   = params[4].ival;
	ptr->sedona.stopbits = params[5].ival;
	ptr->sedona.rtsMode  = params[6].ival;

	ret = pthread_create(&ptr->thread.thread_id, NULL, modbus_master_thread, &ptr->thread);
	if (ret) {
		modbus_master_set_status(&ptr->thread, STATUS_FAILED);
		ERROR("failed to create thread (%s)\n", strerror(ret));
	}
	register_modbus_master(&ptr->thread);
	return ret_cell;
}


Cell sproModbusMaster_ModbusSlaveSerialAsyncNetwork_updateSerialSettings(SedonaVM* vm, Cell* params)
{
	uint8_t* self = params[0].aval;
	struct modbus_master_thread *thread = params[1].aval;
	struct modbus_master_serial_thread *ptr;

	ptr = container_of(thread, struct modbus_master_serial_thread, thread);


	pthread_mutex_lock_checked(&ptr->thread.master_mutex, __func__);

	ptr->sedona.port     = params[2].ival;
	ptr->sedona.baudrate = params[3].ival;
	ptr->sedona.databits = params[4].ival;
	ptr->sedona.parity   = params[5].ival;
	ptr->sedona.stopbits = params[6].ival;
	ptr->sedona.rtsMode  = params[7].ival;

	thread->update = true;
	pthread_mutex_unlock(&ptr->thread.master_mutex);
	return zeroCell;
}
