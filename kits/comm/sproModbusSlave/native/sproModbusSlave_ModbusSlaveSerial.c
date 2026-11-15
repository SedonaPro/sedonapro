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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

/* #define NATIVE_DEBUG */

#include "sproUtil.h"
#include "sproModbusSlave_common.h"
#include "sproUtil_SerialCommon.h"

struct modbus_slave_serial_thread {
	struct modbus_slave_thread thread;

	int port;
	int baudrate;
	int databits;
	int parity;
	int stopbits;
	int rtsMode;
};


static void   modbus_slave_serial_print(struct modbus_slave_thread *thread)
{
	struct modbus_slave_serial_thread *ptr;
	ptr = container_of(thread, struct modbus_slave_serial_thread, thread);
	printf("port:     %d\n", ptr->port);
	printf("baudrate: %d\n", ptr->baudrate);
	printf("databits: %d\n", ptr->databits);
	printf("parity:   %d\n", ptr->parity);
	printf("stopbits: %d\n", ptr->stopbits);
	printf("rtsMode:  %d\n", ptr->rtsMode);
}

static void   modbus_slave_serial_break_receive(struct modbus_slave_thread *thread)
{
	FTRACE();

	struct modbus_slave_serial_thread *ptr;
	ptr = container_of(thread, struct modbus_slave_serial_thread, thread);
	if (thread->enable) {
		modbus_close(thread->modbus_ctx);
		pthread_kill(thread->thread_id, SIGUSR1);
	}
}



static const struct modbus_slave_backend_ops modbus_slave_backend_serial_ops = {
	.print = modbus_slave_serial_print,
	.break_receive = modbus_slave_serial_break_receive
};





bool  modbus_slave_serial_init(struct modbus_slave_serial_thread* ptr)
{
	FTRACE();
	const bool mb_debug = 
#ifdef NATIVE_DEBUG
		true;
#else
		false;
#endif
	
	const int sleep_after_error_s = 2;


	pthread_mutex_lock_checked(&ptr->thread.mutex, __func__);

	int max_port = serialMaxPortNum;
	if (ptr->port > max_port)
		ptr->port = 0;

	if (!modbus_slave_signal_setup()) {
		ptr->thread.status = STATUS_FAILED;
		pthread_mutex_unlock(&ptr->thread.mutex);
		sleep(sleep_after_error_s);
		return false;
	}

	ptr->thread.modbus_ctx = modbus_new_rtu(
		serial_ports_filenames[ptr->port],
		baudrates[ptr->baudrate],
		parity[   ptr->parity],
		ptr->databits,
		ptr->stopbits);

	if (!ptr->thread.modbus_ctx) {
		ERROR("failed to create libmodbus context\n");
		ptr->thread.status = STATUS_FAILED;
		pthread_mutex_unlock(&ptr->thread.mutex);
		sleep(sleep_after_error_s);
		return false;
	}

	modbus_set_debug(ptr->thread.modbus_ctx, mb_debug);	
	modbus_set_slave(ptr->thread.modbus_ctx, ptr->thread.mb_slave_addr);
	ptr->thread.update = false;
	ptr->thread.requests = 0;

	pthread_mutex_unlock(&ptr->thread.mutex);
	if (modbus_connect(ptr->thread.modbus_ctx)) {
		ERROR("connection failed: %s\n",modbus_strerror(errno));
		modbus_free(ptr->thread.modbus_ctx);
		ptr->thread.modbus_ctx = NULL;
		ptr->thread.status = STATUS_FAILED;

		sleep(sleep_after_error_s);
		return false;
	}


#if LIBMODBUS_VERSION_CHECK(3, 1, 0)
	// set rs485 mode
	pthread_mutex_lock_checked(&ptr->thread.mutex, __func__);
	int rc;
	if (rc = modbus_rtu_set_rts(ptr->thread.modbus_ctx, ptr->rtsMode)) {
		if (rc == -1) {
			ERROR("Unable to set RTS mode - %s (%d)\n", modbus_strerror(errno), errno);
			modbus_close(ptr->thread.modbus_ctx);
			modbus_free(ptr->thread.modbus_ctx);
			ptr->thread.modbus_ctx = NULL;
			ptr->thread.status = STATUS_FAILED;
			pthread_mutex_unlock(&ptr->thread.mutex);			
			sleep(sleep_after_error_s);
			return false;
		}
	}	
	pthread_mutex_unlock(&ptr->thread.mutex);
#else
#warning "There is no RS485 support!"
#endif
	
	INFO("init_done");
	return true;	
}

void modbus_slave_work_loop(struct modbus_slave_thread *thread)
{
	FTRACE();

	int requests = 0;
	modbus_slave_set_status(thread, STATUS_OK);
	for(;;) {
		int rc;
		uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

		rc = modbus_receive(thread->modbus_ctx, query);
		INFO("after_receive");
		if (rc > 0) {
			modbus_slave_set_requests(thread, requests++);
			modbus_slave_set_status(thread, STATUS_OK);
			if (thread->mb_mapping)
				usleep(1000);
			pthread_mutex_lock_checked(&thread->mutex, __func__);
			if (thread->mb_mapping)
				modbus_reply(thread->modbus_ctx, query, rc, thread->mb_mapping);
			pthread_mutex_unlock(&thread->mutex);

		} else if (rc  == -1) {
			/* Connection closed by the client or error */
			modbus_slave_set_status(thread, STATUS_FAILED);
			ERROR("Error on request receive: %s\n", modbus_strerror(errno));
			sleep(2);
			break;
		}

		if (modbus_slave_get_quit(thread))
			break;

		if (modbus_slave_get_update(thread))
			break;
	}
}

void* modbus_slave_serial_thread(void* arg)
{
	FTRACE();

	struct modbus_slave_serial_thread *ptr = arg;
	struct modbus_slave_thread *thread = &ptr->thread;

	modbus_slave_signal_started_work_thread(&ptr->thread);
	while(!modbus_slave_get_quit(thread)) {
		if (!modbus_slave_is_enable(thread))
			continue;

		if (!modbus_slave_serial_init(ptr))
			continue;

		modbus_slave_work_loop(thread);

		modbus_close(thread->modbus_ctx);
		modbus_free(thread->modbus_ctx);
		modbus_slave_set_status(thread, STATUS_DOWN);
	}
	modbus_slave_clear(thread);
	INFO("quit");
	return 0;
}


Cell sproModbusSlave_ModbusSlaveSerialService_addSlaveService(SedonaVM* vm, Cell* params)
{
	Cell ret_cell;
	uint8_t* self = params[0].aval;

	struct modbus_slave_serial_thread *ptr = malloc(sizeof(*ptr));
	if (!ptr) {
		ERROR("failed allocate memory\n");
		return zeroCell;
	}
	memset(ptr, 0, sizeof(*ptr));


	ptr->thread.type = RTU;
	ptr->thread.ops  = &modbus_slave_backend_serial_ops;
	ptr->thread.component = self;

	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&ptr->thread.mutex, &mutexattr);
	pthread_mutexattr_destroy(&mutexattr);

	pthread_cond_init(&ptr->thread.en_cond, NULL);

	ptr->port     = params[1].ival;
	ptr->baudrate = params[2].ival;
	ptr->databits = params[3].ival;
	ptr->parity   = params[4].ival;
	ptr->stopbits = params[5].ival;
	
	ptr->thread.mb_slave_addr  = params[6].ival;
	
	modbus_slave_update_mapping(&ptr->thread,
				params[7].ival,
				params[8].ival,
				params[9].ival,
				params[10].ival);

	ptr->rtsMode = params[11].ival;
	
	ret_cell.aval = &ptr->thread;

	int ret;

	ret = pthread_create(&ptr->thread.thread_id, NULL, modbus_slave_serial_thread, ptr);
	if (ret) {

		ptr->thread.status = STATUS_FAILED;
		ERROR("failed to create thread (%s)\n", strerror(ret));
	}
	register_modbus_slave(&ptr->thread);
	return ret_cell;
}



Cell sproModbusSlave_ModbusSlaveSerialService_getMaxPortNum(SedonaVM* vm, Cell* params)
{
	Cell ret_cell;
	ret_cell.ival = serialMaxPortNum;
	return ret_cell;
}


Cell sproModbusSlave_ModbusSlaveSerialService_updateSerialSettings(SedonaVM* vm, Cell* params)
{
	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* thread = params[1].aval;
	struct modbus_slave_serial_thread* ptr;

	ptr = container_of(thread, struct modbus_slave_serial_thread, thread);

	pthread_mutex_lock_checked(&thread->mutex, __func__);

	ptr->port     = params[2].ival;
	ptr->baudrate = params[3].ival;
	ptr->databits = params[4].ival;
	ptr->parity   = params[5].ival;
	ptr->stopbits = params[6].ival;
	ptr->rtsMode  = params[7].ival;

	modbus_slave_serial_break_receive(thread);
	pthread_mutex_unlock(&thread->mutex);
	return zeroCell;
}
