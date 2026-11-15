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

#include "sproUtil.h"
#include "sproModbusMaster_list.h"
#include "sproModbusMaster_common.h"
#include "sproModbusMaster_ModbusSlaveTcpDevice.h"
#include "svm_config.h"

static LIST_HEAD(modbus_master_threads);
static pthread_mutex_t  modbus_master_threads_mutex = PTHREAD_MUTEX_INITIALIZER;


static bool check_modbus_handle(const uint8_t *self, const struct modbus_master_thread *thread)
{
	bool ret = false;
	if (!thread)
		return false;

	if (thread->component != self) {
		ERROR("inconsistent data in modbus_thread\n");
		return false;
	}

	struct modbus_master_thread *ptr;
	list_for_each_entry(ptr, &modbus_master_threads, threads) {
		// check correctness of data in the list
		if (ptr == thread) {
			ret = true;
			break;
		}
	}

	if (!ret)
		ERROR("handle is not registered\n");

	return ret;
}


void register_modbus_master(struct modbus_master_thread* thread)
{
	FTRACE();

	pthread_mutex_lock_checked(&modbus_master_threads_mutex, __func__);
	list_add(&thread->threads, &modbus_master_threads);
	pthread_mutex_unlock(&modbus_master_threads_mutex);
}


void unregister_modbus_master(struct modbus_master_thread* thread)
{
	FTRACE();

	if (check_modbus_handle(thread->component, thread)) {
		pthread_mutex_lock_checked(&modbus_master_threads_mutex, __func__);
		list_del(&thread->threads);
		pthread_mutex_unlock(&modbus_master_threads_mutex);
	}
}






static void  modbus_master_set_update(struct modbus_master_thread *ptr,bool val)
{
	FTRACE();

	pthread_mutex_lock_checked(&ptr->master_mutex, __func__);
	ptr->update = val;
	pthread_mutex_unlock(&ptr->master_mutex);
}


static bool  modbus_master_get_update(struct modbus_master_thread *ptr)
{
	/* FTRACE(); */

	bool ret;
	pthread_mutex_lock_checked(&ptr->master_mutex, __func__);
	ret = ptr->update;
	pthread_mutex_unlock(&ptr->master_mutex);
	return ret;
}



static void  modbus_master_set_enable(struct modbus_master_thread *ptr,bool enable)
{
	FTRACE();

	pthread_mutex_lock_checked(&ptr->master_mutex, __func__);
	ptr->enable = enable;
	if (enable)
		pthread_cond_signal(&ptr->en_cond);
	else
		ptr->update = true;
	pthread_mutex_unlock(&ptr->master_mutex);
}

static bool  modbus_master_is_enabled(struct modbus_master_thread *ptr)
{
	FTRACE();

	bool ret;
	pthread_mutex_lock_checked(&ptr->master_mutex, __func__);
	if (!ptr->enable)
		pthread_cond_wait(&ptr->en_cond, &ptr->master_mutex);
	ret = ptr->enable;
	pthread_mutex_unlock(&ptr->master_mutex);
	return ret;
}



void  modbus_master_set_status(struct modbus_master_thread *ptr, int status)
{
	/* FTRACE(); */

	bool ret;
	pthread_mutex_lock_checked(&ptr->master_mutex, __func__);
	ptr->status = status;
	pthread_mutex_unlock(&ptr->master_mutex);
}

static int  modbus_master_get_status(struct modbus_master_thread *ptr)
{
	/* FTRACE(); */

	int ret;
	pthread_mutex_lock_checked(&ptr->master_mutex, __func__);
	ret = ptr->status;
	pthread_mutex_unlock(&ptr->master_mutex);
	return ret;
}


static void modbus_master_set_quit(struct modbus_master_thread *ptr, bool val)
{
	FTRACE();

	pthread_mutex_lock_checked(&ptr->master_mutex, __func__);
	ptr->quit = val;
	if (val && !ptr->enable)
		pthread_cond_signal(&ptr->en_cond);
	pthread_mutex_unlock(&ptr->master_mutex);
}


static bool modbus_master_get_quit(struct modbus_master_thread *ptr)
{
	/* FTRACE(); */

	bool ret;
	pthread_mutex_lock_checked(&ptr->master_mutex, __func__);
	ret = ptr->quit;
	pthread_mutex_unlock(&ptr->master_mutex);
	return ret;
}




static void modbus_master_make_request(struct modbus_master_thread* thread, struct modbus_reg *reg)
{
	FTRACE();

	int rc;
	int errno_tmp;
	struct modbus_req_data* ptr = &reg->backend_copy;
	bool read = false;
	modbus_t *ctx = thread->modbus_ctx;

	modbus_set_slave(ctx, ptr->dev_addr);
	switch(ptr->func) {
	case 	MODBUS_READ_COILS :
		ptr->data = 0;
		rc = modbus_read_bits(ctx, ptr->addr, 1, (uint8_t*)&ptr->data);
		read = true;
		break;
	case 	MODBUS_READ_DISCRETE_INPUTS :
		ptr->data = 0;
		rc = modbus_read_input_bits(ctx, ptr->addr, 1, (uint8_t*)&ptr->data);
		read = true;
		break;
	case 	MODBUS_WRITE_SINGLE_COIL :
		rc = modbus_write_bit(ctx, ptr->addr, ptr->data);
		break;
	case 	MODBUS_WRITE_MULTIPLE_COILS :
		rc = modbus_write_bits(ctx, ptr->addr, 1, (uint8_t*)&ptr->data);
		break;
	case 	MODBUS_READ_HOLDING_REGS :
		ptr->data = 0;
		rc = modbus_read_registers(ctx, ptr->addr, ptr->len, (uint16_t*)&ptr->data);
		read = true;
		break;
	case 	MODBUS_READ_INPUT_REGS :
		ptr->data = 0;
		rc = modbus_read_input_registers(ctx, ptr->addr, ptr->len, (uint16_t*)&ptr->data);
		read = true;
		break;
	case 	MODBUS_WRITE_SINGLE_HOLDING_REG :
	{
		int tmp = ptr->data;
		int addr = ptr->addr;
		rc = modbus_write_register(ctx, addr, (uint16_t)(tmp & 0xFFFF));
		if (!rc && (ptr->len > 1))
			rc = modbus_write_register(ctx, ++addr, (uint16_t)((tmp >> 16) & 0xFFFF));
		break;
	}
	case 	MODBUS_WRITE_MULTIPLE_HOLDING_REGS :
		rc = modbus_write_registers(ctx, ptr->addr, ptr->len, (uint16_t*)&ptr->data);
		break;
	default:
		ERROR("unsupported function (%d)\n", ptr->func);
		errno = 0;
		rc = -1;
		break;
	}
	ptr->request = false;
	ptr->status = (rc == -1) ? STATUS_FAILED : STATUS_OK;
	ptr->done = true;	
	if (rc == -1) {
		int errno_tmp = errno;		
		switch(errno_tmp) {
		case EPIPE:
			modbus_master_set_update(thread, true);			
			break;
		case ETIMEDOUT:
			ptr->status = STATUS_DOWN;
			break;
		}

		/* ERROR("modbus request failed\n",modbus_strerror(errno_tmp)); */
	}


	/* copy results back to sedona */
	pthread_mutex_lock_checked(&reg->reg_mutex, __func__);
	if (svmConfig.modbusSafe) {
		/* results only: dev_addr/func/addr/len changed by Sedona
		 * (updateRegisterInfo) while the request was in flight are kept */
		reg->sedona.data    = reg->backend_copy.data;
		reg->sedona.request = reg->backend_copy.request;
		reg->sedona.done    = reg->backend_copy.done;
		reg->sedona.status  = reg->backend_copy.status;
	} else {
		reg->sedona = reg->backend_copy;
	}
	pthread_mutex_unlock(&reg->reg_mutex);
}



static bool modbus_master_backend_init(struct modbus_master_thread* thread)
{
	FTRACE();
	
	const bool mb_debug = 
#ifdef NATIVE_DEBUG
		true;
#else
		false;
#endif

        const int retry_delay = 2;
	struct timeval response_timeout;

	
	thread->ops->init(thread);
	if (!thread->modbus_ctx) {
		ERROR("failed to create libmodbus context\n");
		modbus_master_set_status(thread, STATUS_FAILED);
		sleep(retry_delay);
		return false;
	}
	

	modbus_set_debug(thread->modbus_ctx, mb_debug);

	response_timeout.tv_sec = 0;
	response_timeout.tv_usec = 100*1000; /* 100 ms */
#if LIBMODBUS_VERSION_CHECK(3,1,0)
	modbus_set_response_timeout(thread->modbus_ctx, response_timeout.tv_sec, response_timeout.tv_usec);
#else
	modbus_set_response_timeout(thread->modbus_ctx, &response_timeout);
#endif
	if (modbus_connect(thread->modbus_ctx) == -1) {
		ERROR("connection failed: %s\n", modbus_strerror(errno));		
		goto error;
	}

	if (thread->ops->after_connect(thread) == -1) {
		modbus_close(thread->modbus_ctx);
		goto error;
	}
	
	INFO("init done");
	return true;
	
error:
	modbus_free(thread->modbus_ctx);
	thread->modbus_ctx = NULL;
	modbus_master_set_status(thread, STATUS_FAILED);
	sleep(retry_delay);
	return false;;	
}

/*
 * Free a register whose point was removed (del_flag, only set with
 * modbus.safe=1).  Called by the backend thread without reg_mutex held:
 * Sedona dropped its handle when it set del_flag, so nobody else uses the
 * register, and master_mutex is never taken while holding reg_mutex
 * (the print functions take master_mutex first).
 */
static void modbus_master_free_register(struct modbus_master_thread *thread, struct modbus_reg* reg)
{
	FTRACE();

	pthread_mutex_lock_checked(&thread->master_mutex, __func__);
	list_del(&reg->list);
	pthread_mutex_unlock(&thread->master_mutex);

	pthread_mutex_destroy(&reg->reg_mutex);
	memset(reg, 0, sizeof(*reg));
	free(reg);
}

static void modbus_master_process_register(struct modbus_master_thread *thread, struct modbus_reg *reg)
{
	bool del;

	pthread_mutex_lock_checked(&reg->reg_mutex, __func__);
	reg->backend_copy = reg->sedona;
	del = reg->del_flag;
	pthread_mutex_unlock(&reg->reg_mutex);

	if (del) {
		modbus_master_free_register(thread, reg);
		return;
	}

	if (reg->backend_copy.request) {
		INFO("next request\n");
		modbus_master_make_request(thread, reg);
	}
}

int modbus_master_process_all_registers(struct modbus_master_thread *thread)
{
	uint8_t quit_loop = 0;

	// process modbus requests
	while(!quit_loop) {
		bool unlock = true;
		struct modbus_reg *reg;
		struct modbus_reg *reg_tmp;

		pthread_mutex_lock_checked(&thread->master_mutex, __func__);
		list_for_each_entry_safe(reg, reg_tmp, &thread->regs, list) {
			pthread_mutex_unlock(&thread->master_mutex);

			modbus_master_process_register(thread, reg);

			if (	modbus_master_get_quit(thread) ||
				modbus_master_get_update(thread)) {
				quit_loop = 1;
				unlock = false;
				break;
			}

			pthread_mutex_lock_checked(&thread->master_mutex, __func__);
		}
		if (unlock)
			pthread_mutex_unlock(&thread->master_mutex);

		if (!quit_loop)
			usleep(1000);
	};

	return quit_loop;
}

void * modbus_master_thread(void* arg)
{
	FTRACE();

	struct modbus_master_thread *thread = arg;
	while(!modbus_master_get_quit(thread)) {
		if (!modbus_master_is_enabled(thread)) {
			modbus_master_set_status(thread, STATUS_DOWN);
			sleep(2);
			continue;
		}

		if (!modbus_master_backend_init(thread))
			continue;

		modbus_master_set_status(thread, STATUS_OK);
		modbus_master_process_all_registers(thread);
		modbus_close(thread->modbus_ctx);
		modbus_free(thread->modbus_ctx);
		thread->modbus_ctx = NULL;
	};
	modbus_master_set_quit(thread, false);

	if (svmConfig.modbusSafe) {
		/* registers of removed points are no longer referenced; the
		 * others still belong to live points and must stay allocated */
		struct modbus_reg *reg, *reg_tmp;
		list_for_each_entry_safe(reg, reg_tmp, &thread->regs, list) {
			bool del;
			pthread_mutex_lock_checked(&reg->reg_mutex, __func__);
			del = reg->del_flag;
			pthread_mutex_unlock(&reg->reg_mutex);
			if (del)
				modbus_master_free_register(thread, reg);
		}
	}
	pthread_mutex_destroy(&thread->master_mutex);
	unregister_modbus_master(thread);
	thread->ops->free(thread);
	return 0;
}





static void modbus_master_print_modbus_req_data(struct modbus_req_data* ptr)
{
	printf("dev_addr: %d\n", ptr->dev_addr);
	printf("func:     %d\n", ptr->func);
	printf("addr:     %d\n", ptr->addr);
	printf("data:     %d\n", ptr->data);
	printf("len:      %d\n", ptr->len);
	printf("request:  %d\n", ptr->request);
	printf("done:     %d\n", ptr->done);
	printf("status:   %d\n", ptr->status);
}

static void modbus_master_print_modbus_reg(struct modbus_reg* ptr)
{
	printf("*** modbus_reg at 0x%X ***\n", ptr);	
	pthread_mutex_lock_checked(&ptr->reg_mutex, __func__);
	printf("component: 0x%X\n", ptr->component);
	printf("del_flag:  0x%X\n", ptr->del_flag);
	printf("sedona\n");
	modbus_master_print_modbus_req_data(&ptr->sedona);
	printf("backend_copy\n");
	modbus_master_print_modbus_req_data(&ptr->backend_copy);
	pthread_mutex_unlock(&ptr->reg_mutex);
}


static void modbus_master_print_modbus_master_thread(struct modbus_master_thread* thread)
{
	pthread_mutex_lock_checked(&thread->master_mutex, __func__);
	printf("*** modbus_master_thread at 0x%X ***\n", thread);
	printf("component:  0x%X\n", thread->component);
	printf("enable:     %d\n",   thread->enable);
	printf("modbus_ctx: 0x%X\n", thread->modbus_ctx);
	printf("status:     %d\n",   thread->status);
	printf("update:     %d\n",   thread->update);
	printf("quit:       %d\n",   thread->quit);
	printf("type:       %d\n",   thread->type);
	printf("backend info\n");
	thread->ops->print(thread);
	printf("regs (start)\n");
	struct modbus_reg *reg;
	list_for_each_entry(reg, &thread->regs, list) 
		modbus_master_print_modbus_reg(reg);
	printf("regs (end)\n");
	pthread_mutex_unlock(&thread->master_mutex);
}

static void modbus_master_print_modbus_master_threads(void)
{
	/* FTRACE(); */
	
	struct modbus_master_thread *thread;
	pthread_mutex_lock_checked(&modbus_master_threads_mutex, __func__);			
	list_for_each_entry(thread, &modbus_master_threads, threads)
		modbus_master_print_modbus_master_thread(thread);
	pthread_mutex_unlock(&modbus_master_threads_mutex);

}

Cell sproModbusMaster_ModbusPoint_addRegister(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self                       = params[0].aval;
	struct modbus_master_thread* modbus = params[1].aval;
	Cell ret_cell;

	struct modbus_reg *ptr = malloc(sizeof(*ptr));
	if (!ptr) {
		ERROR("failed allocate memory\n");
		return zeroCell;
	}
	memset(ptr, 0, sizeof(*ptr));
	ret_cell.aval = ptr;

	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&ptr->reg_mutex, &mutexattr);
	pthread_mutexattr_destroy(&mutexattr);

	ptr->sedona.dev_addr  = params[2].ival;
	ptr->sedona.func      = params[3].ival;
	ptr->sedona.addr      = params[4].ival;
	ptr->sedona.len       = params[5].ival;

	pthread_mutex_lock_checked(&modbus->master_mutex, __func__);
	list_add(&ptr->list, &modbus->regs);
	pthread_mutex_unlock(&modbus->master_mutex);

	INFO("added\n");
	return ret_cell;
}


Cell sproModbusMaster_ModbusPoint_removeRegister(SedonaVM* vm, Cell* params)
{
	FTRACE();

	/* legacy: registers of removed points are never freed */
	if (!svmConfig.modbusSafe)
		return zeroCell;

	struct modbus_reg* ptr = params[2].aval;
	if (!ptr)
		return zeroCell;

	/* the backend thread frees it; Sedona drops its handle now */
	pthread_mutex_lock_checked(&ptr->reg_mutex, __func__);
	ptr->del_flag = true;
	pthread_mutex_unlock(&ptr->reg_mutex);
	return zeroCell;
}



Cell sproModbusMaster_ModbusPoint_updateRegisterInfo(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_reg* ptr = params[1].aval;

	pthread_mutex_lock_checked(&ptr->reg_mutex, __func__);
	ptr->sedona.dev_addr = params[2].ival;
	ptr->sedona.func     = params[3].ival;
	ptr->sedona.addr     = params[4].ival;
	ptr->sedona.len      = params[5].ival;
	pthread_mutex_unlock(&ptr->reg_mutex);

	return zeroCell;
}

Cell sproModbusMaster_ModbusPoint_requestInProgress(SedonaVM* vm, Cell* params)
{
	/* FTRACE(); */

	uint8_t* self = params[0].aval;
	struct modbus_reg* ptr = params[1].aval;
	Cell ret_cell;

	pthread_mutex_lock_checked(&ptr->reg_mutex, __func__);
	ret_cell.ival         = ptr->sedona.request || ptr->sedona.done;
	pthread_mutex_unlock(&ptr->reg_mutex);

	return ret_cell;
}

Cell sproModbusMaster_ModbusPoint_getDone(SedonaVM* vm, Cell* params)
{
	/* FTRACE(); */

	uint8_t* self = params[0].aval;
	struct modbus_reg* ptr = params[1].aval;
	Cell ret_cell;

	pthread_mutex_lock_checked(&ptr->reg_mutex, __func__);
	*(int*)params[2].aval = ptr->sedona.status;
	*(int*)params[3].aval = ptr->sedona.data;
	ret_cell.ival         = ptr->sedona.done;
	ptr->sedona.done      = 0;
	pthread_mutex_unlock(&ptr->reg_mutex);

	return ret_cell;
}

Cell sproModbusMaster_ModbusPoint_setRequest(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_reg* ptr = params[1].aval;

	pthread_mutex_lock_checked(&ptr->reg_mutex, __func__);
	ptr->sedona.done    = false;
	ptr->sedona.request = params[2].ival;
	ptr->sedona.data    = params[3].ival;
	pthread_mutex_unlock(&ptr->reg_mutex);

	return zeroCell;
}


Cell sproModbusMaster_ModbusPoint_printInternalStateNative(SedonaVM* vm, Cell* params)
{
	FTRACE();

	struct modbus_reg* ptr = params[1].aval;
	modbus_master_print_modbus_reg(ptr);
	return zeroCell;
}












Cell sproModbusMaster_ModbusSlaveNetwork_setQuit(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_master_thread* ptr = params[1].aval;

	modbus_master_set_quit(ptr, true);
	return zeroCell;
}

Cell sproModbusMaster_ModbusSlaveNetwork_getStatus(SedonaVM* vm, Cell* params)
{
	/* FTRACE(); */

	uint8_t* self = params[0].aval;
	struct modbus_master_thread* ptr = params[1].aval;
	Cell ret;
	ret.ival = modbus_master_get_status(ptr);
	return ret;
}

Cell sproModbusMaster_ModbusSlaveNetwork_setEnable(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_master_thread* ptr = params[1].aval;
	modbus_master_set_enable(ptr, params[2].ival);
	return zeroCell;
}

Cell sproModbusMaster_ModbusSlaveNetwork_printInternalStateNative(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_master_thread* ptr = params[1].aval;

	modbus_master_print_modbus_master_thread(ptr);
	return zeroCell;
}

Cell sproModbusMaster_ModbusSlaveNetwork_printInternalStateNativeAll(SedonaVM* vm, Cell* params)
{
	FTRACE();

	modbus_master_print_modbus_master_threads();
	return zeroCell;
}
