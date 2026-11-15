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
#include "svm_config.h"

#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>

/* #define NATIVE_DEBUG */

#include "sproModbusSlave_common.h"
#include "sproUtil.h"


static LIST_HEAD(modbus_slave_threads);
static pthread_mutex_t  modbus_slave_threads_mutex =  PTHREAD_MUTEX_INITIALIZER;


static void modbus_slave_set_slave_addr(struct modbus_slave_thread *ptr, int slave_addr)
{
	/* FTRACE(); */

	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	ptr->mb_slave_addr = slave_addr;
	ptr->update = true;
	pthread_mutex_unlock(&ptr->mutex);
}


static void modbus_slave_set_quit(struct modbus_slave_thread *ptr)
{
	/* FTRACE(); */

	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	ptr->quit = true;
	if (!ptr->enable)
		pthread_cond_signal(&ptr->en_cond);
	ptr->ops->break_receive(ptr);
	pthread_mutex_unlock(&ptr->mutex);
}


bool modbus_slave_get_quit(struct modbus_slave_thread *ptr)
{
	/* FTRACE(); */

	bool ret;
	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	ret = ptr->quit;
	pthread_mutex_unlock(&ptr->mutex);
	return ret;
}

void  modbus_slave_set_status(struct modbus_slave_thread *ptr, int status)
{
	/* FTRACE(); */

	bool ret;
	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	ptr->status = status;
	pthread_mutex_unlock(&ptr->mutex);
}

static int  modbus_slave_get_status(struct modbus_slave_thread *ptr)
{
	/* FTRACE(); */

	int ret;
	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	ret = ptr->status;
	pthread_mutex_unlock(&ptr->mutex);
	return ret;
}

static int  modbus_slave_get_requests(struct modbus_slave_thread *ptr)
{
	/* FTRACE(); */

	int ret;
	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	ret = ptr->requests;
	pthread_mutex_unlock(&ptr->mutex);
	return ret;
}

void  modbus_slave_set_requests(struct modbus_slave_thread *ptr, int requests)
{
	/* FTRACE(); */

	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	ptr->requests = requests;
	pthread_mutex_unlock(&ptr->mutex);
}



bool  modbus_slave_get_update(struct modbus_slave_thread *ptr)
{
	/* FTRACE(); */

	bool ret;
	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	ret = ptr->update;
	pthread_mutex_unlock(&ptr->mutex);
	return ret;
}



static void modbus_slave_set_bit(struct modbus_slave_thread *ptr, int type, int addr, bool value)
{
	/* FTRACE(); */

	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	value = !!value;
	switch(type) {
	case modbus_coils:
		modbus_set_bits_from_bytes(ptr->mb_mapping->tab_bits, addr, 1, (uint8_t*)&value);
		break;
	case modbus_inpt_bits:
		modbus_set_bits_from_bytes(ptr->mb_mapping->tab_input_bits, addr, 1, (uint8_t*)&value);
		break;
	};
	pthread_mutex_unlock(&ptr->mutex);
}

static bool modbus_slave_get_bit(struct modbus_slave_thread *ptr, int type, int addr)
{
	/* FTRACE(); */

	bool ret = false;
	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	switch(type) {
	case modbus_coils:
		ret = modbus_get_byte_from_bits(ptr->mb_mapping->tab_bits, addr, 1);
		break;
	case modbus_inpt_bits:
		ret = modbus_get_byte_from_bits(ptr->mb_mapping->tab_input_bits, addr, 1);
		break;
	};
	pthread_mutex_unlock(&ptr->mutex);
	return ret;
}
static void modbus_slave_set_regs(struct modbus_slave_thread *ptr,
			int type,
			int addr,
			void* src,
			int len)
{
	/* FTRACE(); */

	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	switch(type) {
	case modbus_holding_regs:
		memcpy(&ptr->mb_mapping->tab_registers[addr], src, len);
		break;
	case modbus_inpt_regs:
		memcpy(&ptr->mb_mapping->tab_input_registers[addr], src, len);
		break;
	};
	pthread_mutex_unlock(&ptr->mutex);
}


static void modbus_slave_get_regs(struct modbus_slave_thread *ptr,
			int type,
			int addr,
			void* dest,
			int len)
{
	/* FTRACE(); */

	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	switch(type) {
	case modbus_holding_regs:
		memcpy(dest, &ptr->mb_mapping->tab_registers[addr], len);
		break;
	case modbus_inpt_regs:
		memcpy(dest, &ptr->mb_mapping->tab_input_registers[addr], len);
		break;
	};
	pthread_mutex_unlock(&ptr->mutex);
}


void modbus_slave_update_mapping(struct modbus_slave_thread *ptr,
			int coils_num,
			int inpt_bits_num,
			int hold_regs_num,
			int inpt_regs_num)
{
	FTRACE();

	pthread_mutex_lock_checked(&ptr->mutex, __func__);

	if (ptr->mb_mapping) {
		modbus_mapping_free(ptr->mb_mapping);
		ptr->mb_mapping = NULL;
	}
	ptr->mb_mapping = modbus_mapping_new(
		coils_num,
		inpt_bits_num,
		hold_regs_num,
		inpt_regs_num);

	if (ptr->mb_mapping == NULL) {
		ERROR("failed to allocate the mapping: %s\n", modbus_strerror(errno));
		ptr->status = STATUS_FAILED;
	}
	pthread_mutex_unlock(&ptr->mutex);
}


static void  modbus_slave_set_enable(struct modbus_slave_thread *ptr,bool enable)
{
	FTRACE();

	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	if (enable)
		pthread_cond_signal(&ptr->en_cond);
	else {
		ptr->update = true;
		ptr->ops->break_receive(ptr);
	}
	ptr->enable = enable;
	pthread_mutex_unlock(&ptr->mutex);
}

bool  modbus_slave_is_enable(struct modbus_slave_thread *ptr)
{
	FTRACE();

	bool ret;
	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	if (!ptr->enable)
		pthread_cond_wait(&ptr->en_cond, &ptr->mutex);
	ret = ptr->enable;
	pthread_mutex_unlock(&ptr->mutex);
	return ret;
}

void  modbus_slave_signal_started_work_thread(struct modbus_slave_thread *ptr)
{
	pthread_mutex_lock_checked(&ptr->mutex, __func__);
	ptr->status  = STATUS_DOWN;
	pthread_mutex_unlock(&ptr->mutex);
}


void modbus_slave_clear(struct modbus_slave_thread *thread)
{
	FTRACE();

	/* unregister_modbus_slave() zeroes *thread, so the original code passed
	 * NULL to modbus_mapping_free(); mem.fixes keeps the pointer first */
	modbus_mapping_t *mapping = svmConfig.memFixes ? thread->mb_mapping : NULL;

	unregister_modbus_slave(thread);

	pthread_mutex_lock_checked(&thread->mutex, __func__);
	modbus_mapping_free(thread->mb_mapping);
	pthread_mutex_unlock(&thread->mutex);
	if (mapping)
		modbus_mapping_free(mapping);
	pthread_mutex_destroy(&thread->mutex);
	pthread_cond_destroy(&thread->en_cond);
}



void register_modbus_slave(struct modbus_slave_thread* thread)
{
	FTRACE();

	pthread_mutex_lock_checked(&modbus_slave_threads_mutex, __func__);
	list_add(&thread->threads, &modbus_slave_threads);
	pthread_mutex_unlock(&modbus_slave_threads_mutex);
}




void unregister_modbus_slave(struct modbus_slave_thread* thread)
{
	FTRACE();

	pthread_mutex_lock_checked(&modbus_slave_threads_mutex, __func__);
	list_del(&thread->threads);
	pthread_mutex_unlock(&modbus_slave_threads_mutex);
	memset(thread, 0, sizeof(*thread));
}


static void modbus_slave_signal_handler(int signum)
{
	FTRACE();
}


bool modbus_slave_signal_setup(void)
{
	int signo = SIGUSR1;
	struct sigaction act;

	memset(&act, 0, sizeof(act));

	/* Install signal handler */
	act.sa_handler = modbus_slave_signal_handler;
	return (sigaction(signo, &act, NULL) == 0);
}


void modbus_slave_print_modbus_mappings(struct modbus_slave_thread* thread)
{
	int num, i;
	uint16_t* arr;
	uint8_t* arr_bit;

	if (!thread->mb_mapping)
		return;

	printf("modbus_mapping:\n");

	num = thread->mb_mapping->nb_input_registers;
	arr = thread->mb_mapping->tab_input_registers;
	printf("input registers: %d\n", num );
	for (i = 0; i < num; i++)
		printf("  %04d: 0x%04X\n", i, arr[i]);

	num = thread->mb_mapping->nb_registers;
	arr = thread->mb_mapping->tab_registers;
	printf("holding registers: %d\n", num );
	for (i = 0; i < num; i++)
		printf("  %04d: 0x%04X\n", i, arr[i]);




	num     = thread->mb_mapping->nb_input_bits;
	arr_bit = thread->mb_mapping->tab_input_bits;
	printf("input bits: %d\n", num );
        num = (num + 7)/8;
	for (i = 0; i < num; i++)
		printf("  %04d: 0x%02X\n", i, arr_bit[i]);


	num     = thread->mb_mapping->nb_bits;
	arr_bit = thread->mb_mapping->tab_bits;
	printf("coils: %d\n", num );
        num = (num + 7)/8;
	for (i = 0; i < num; i++)
		printf("  %04d: 0x%X\n", i, arr_bit[i]);
}



void modbus_slave_print_modbus_slave_thread(struct modbus_slave_thread* thread)
{
	pthread_mutex_lock_checked(&thread->mutex, __func__);
	printf("*** modbus_slave_thread at 0x%X ***\n", thread);
	printf("component:  0x%X\n", thread->component);
	printf("enable:     %d\n",   thread->enable);
	printf("modbus_ctx: 0x%X\n", thread->modbus_ctx);
	printf("status:     %d\n",   thread->status);
	printf("update:     %d\n",   thread->update);
	printf("quit:       %d\n",   thread->quit);
	printf("type:       %d\n",   thread->type);
	printf("slave_addr: %d\n",   thread->mb_slave_addr);
	printf("backend info\n");
	thread->ops->print(thread);

	modbus_slave_print_modbus_mappings(thread);
	pthread_mutex_unlock(&thread->mutex);
}


void modbus_slave_print_modbus_slave_threads(void)
{
	/* FTRACE(); */

	struct modbus_slave_thread *thread;
	pthread_mutex_lock_checked(&modbus_slave_threads_mutex, __func__);
	list_for_each_entry(thread, &modbus_slave_threads, threads)
		modbus_slave_print_modbus_slave_thread(thread);
	pthread_mutex_unlock(&modbus_slave_threads_mutex);
}


Cell sproModbusSlave_ModbusSlaveService_setSlaveAddr(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;
	int  slaveAddr = params[2].ival;

	modbus_slave_set_slave_addr(ptr, slaveAddr);
	return zeroCell;
}

Cell sproModbusSlave_ModbusSlaveService_setQuit(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;

	modbus_slave_set_quit(ptr);
	return zeroCell;
}

Cell sproModbusSlave_ModbusSlaveService_getStatus(SedonaVM* vm, Cell* params)
{
	/* FTRACE(); */

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;
	Cell ret;
	ret.ival = modbus_slave_get_status(ptr);
	return ret;
}

Cell sproModbusSlave_ModbusSlaveService_getRequests(SedonaVM* vm, Cell* params)
{
	/* FTRACE(); */

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;
	Cell ret;
	ret.ival = modbus_slave_get_requests(ptr);
	return ret;
}


Cell sproModbusSlave_ModbusSlaveService_updateRegNumber(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;

	modbus_slave_update_mapping(ptr,
				params[2].ival,
				params[3].ival,
				params[4].ival,
				params[5].ival);
	return zeroCell;
}

Cell sproModbusSlave_ModbusSlaveService_setEnable(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;
	modbus_slave_set_enable(ptr, params[2].ival);
	return zeroCell;
}

Cell sproModbusSlave_ModbusSlaveService_printInternalStateNative(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;
	modbus_slave_print_modbus_slave_thread(ptr);
	return zeroCell;
}

Cell sproModbusSlave_ModbusSlaveService_printInternalStateNativeAll(SedonaVM* vm, Cell* params)
{
	FTRACE();

	modbus_slave_print_modbus_slave_threads();
	return zeroCell;
}




Cell sproModbusSlave_ModbusSlavePointNumeric_setData(SedonaVM* vm, Cell* params)
{
	/* FTRACE(); */

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;

	modbus_slave_set_regs(ptr,
			params[2].ival, // type
 			params[3].ival, // address
			&params[4].ival,
			params[5].ival*2);  // size in reg1isters (*2
					    // in bytes
	return zeroCell;
}


Cell sproModbusSlave_ModbusSlavePointNumeric_getData(SedonaVM* vm, Cell* params)
{
	/* FTRACE(); */

	Cell ret;
	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;

	ret.ival = 0;
	modbus_slave_get_regs(ptr,
			params[2].ival, // type
			params[3].ival, // address
			&ret.ival,
			params[4].ival*2);  // size in registers (*2
					    // in bytes
	return ret;
}



Cell sproModbusSlave_ModbusSlavePointBoolean_setData(SedonaVM* vm, Cell* params)
{
	/* FTRACE(); */

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;

	modbus_slave_set_bit(ptr,
			params[2].ival, // type
 			params[3].ival, // address
			params[4].ival);  // size in reg1isters (*2
					    // in bytes
	return zeroCell;
}


Cell sproModbusSlave_ModbusSlavePointBoolean_getData(SedonaVM* vm, Cell* params)
{
	/* FTRACE(); */

	Cell ret;
	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* ptr = params[1].aval;

	ret.ival = modbus_slave_get_bit(ptr,
					params[2].ival, // type
					params[3].ival); // address

	return ret;
}
