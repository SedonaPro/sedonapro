#ifndef _SPRO_MODBUS_MASTER_COMMON_H
#define _SPRO_MODBUS_MASTER_COMMON_H

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

#include <stdbool.h>
#include <pthread.h>
#include <modbus.h>

#include "sproModbusMaster_list.h"

enum {
	TCP,
	RTU
};

enum {
	STATUS_DOWN,
	STATUS_FAILED,
	STATUS_OK
};

enum {
	MODBUS_READ_COILS = 1,
	MODBUS_READ_DISCRETE_INPUTS = 2,
	MODBUS_WRITE_SINGLE_COIL = 5,
	MODBUS_WRITE_MULTIPLE_COILS = 15,
	MODBUS_READ_HOLDING_REGS = 3,
	MODBUS_READ_INPUT_REGS = 4,
	MODBUS_WRITE_SINGLE_HOLDING_REG = 6,
	MODBUS_WRITE_MULTIPLE_HOLDING_REGS = 16
};

struct modbus_master_thread;

struct modbus_master_backend_ops {
	void (*init)(struct modbus_master_thread *thread);
	int  (*after_connect)(struct modbus_master_thread *thread);			
	void (*print)(struct modbus_master_thread *thread);		
	void (*free)(struct modbus_master_thread *thread);	
};

struct modbus_master_thread {
	pthread_mutex_t  master_mutex;

	bool enable;
	pthread_cond_t en_cond;

	uint8_t*  component;
	modbus_t* modbus_ctx;
	int  status;
	bool update;
	bool quit;
	int  type;
	const struct modbus_master_backend_ops *ops;
	pthread_t thread_id;
	struct list_head threads;
	struct list_head regs;

};

struct modbus_req_data {
	int  dev_addr;
	int  func;
	int  addr;
	uint32_t data;
	int  len;
	bool request;
	bool done;
	int  status;
};

struct modbus_reg {
	pthread_mutex_t reg_mutex;
	const uint8_t*  component;
	bool del_flag;

	struct modbus_req_data sedona;
	struct modbus_req_data backend_copy;

	struct list_head list;
};



void register_modbus_master(struct modbus_master_thread* thread);
void modbus_master_set_status(struct modbus_master_thread* thread, int status);
void unregister_modbus_master(struct modbus_master_thread* thread);
void*modbus_master_thread(void* arg);

#endif
