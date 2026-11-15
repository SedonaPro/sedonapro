
#ifndef _SPRO_MODBUS_SLAVE_COMMON_H_
#define _SPRO_MODBUS_SLAVE_COMMON_H_

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


#include <stdint.h>
#include <stdbool.h>
#include <modbus.h>
#include <pthread.h>

#include "sproModbusSlave_list.h"

enum {
	TCP,
	RTU
};

enum {
	STATUS_DOWN,
	STATUS_FAILED,
	STATUS_OK
};

struct modbus_slave_thread;

struct modbus_slave_backend_ops {
	void (*print)(struct modbus_slave_thread *thread);
	void (*break_receive)(struct modbus_slave_thread *thread);
};


struct modbus_slave_thread {
	pthread_mutex_t   mutex;

	bool enable;
	pthread_cond_t en_cond;

	uint8_t*  component;
	modbus_t* modbus_ctx;
	int  status;
	bool update;
	bool quit;
	int  type;
	const struct modbus_slave_backend_ops *ops;
	pthread_t thread_id;
	struct list_head threads;

	int mb_slave_addr;
	modbus_mapping_t *mb_mapping;
	int  requests;
};


enum {
	modbus_coils,
	modbus_inpt_bits,
	modbus_holding_regs,
	modbus_inpt_regs
};


static const int modbus_slave_sleep_after_error_s = 2;


void modbus_slave_set_status(		struct modbus_slave_thread *ptr, int status);
void modbus_slave_set_requests(		struct modbus_slave_thread *ptr, int requests);
bool modbus_slave_get_quit(		struct modbus_slave_thread *ptr);
bool modbus_slave_get_update(	struct modbus_slave_thread *ptr);

void modbus_slave_update_mapping(struct modbus_slave_thread *ptr,
				int coils_num,
				int inpt_bits_num,
				int hold_regs_num,
				int inpt_regs_num);

bool modbus_slave_signal_setup(void);
bool modbus_slave_is_enable(struct modbus_slave_thread *ptr);
void modbus_slave_signal_started_work_thread(struct modbus_slave_thread *ptr);
void modbus_slave_clear(struct modbus_slave_thread *ptr);

void register_modbus_slave(	struct modbus_slave_thread* thread);
void unregister_modbus_slave(	struct modbus_slave_thread* thread);


#endif
