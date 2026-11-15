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


#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

/* #define NATIVE_DEBUG */

#include "sproModbusMaster_list.h"
#include "sproModbusMaster_common.h"
#include "sproUtil.h"
#include "sproModbusMaster_ModbusSlaveTcpDevice.h"

struct modbus_master_tcp_cfg {
	char  ip[INET6_ADDRSTRLEN];
	int   port;
};

struct modbus_master_tcp_thread {
	struct modbus_master_thread thread;

	struct modbus_master_tcp_cfg sedona;
	struct modbus_master_tcp_cfg backend_copy;
};




static void   modbus_master_tcp_init(struct modbus_master_thread *thread)
{
	FTRACE();

	struct modbus_master_tcp_thread *ptr;
	static char port[7];

	ptr = container_of(thread, struct modbus_master_tcp_thread, thread);

	pthread_mutex_lock_checked(&ptr->thread.master_mutex, __func__);
	ptr->backend_copy = ptr->sedona;
	ptr->thread.update = false;
	pthread_mutex_unlock(&ptr->thread.master_mutex);

	snprintf(port, sizeof(port), "%d", ptr->backend_copy.port);
	ptr->thread.modbus_ctx = modbus_new_tcp_pi(ptr->backend_copy.ip, port);
}

static int   modbus_master_tcp_after_connect(struct modbus_master_thread *thread)
{
	return 0;
}

static void   modbus_master_tcp_free(struct modbus_master_thread *thread)
{
	FTRACE();

	struct modbus_master_tcp_thread *ptr;
	ptr = container_of(thread, struct modbus_master_tcp_thread, thread);

	memset(ptr, 0, sizeof(*ptr));
	free(ptr);
}

static void   modbus_master_tcp_print_cfg(struct modbus_master_tcp_cfg* cfg)
{
	printf("ip:       %s\n", cfg->ip);
	printf("port:     %d\n", cfg->port);
}

static void   modbus_master_tcp_print(struct modbus_master_thread *thread)
{
	struct modbus_master_tcp_thread *ptr;
	ptr = container_of(thread, struct modbus_master_tcp_thread, thread);
	printf("sedona config\n");
	modbus_master_tcp_print_cfg(&ptr->sedona);
	printf("backend config\n");
	modbus_master_tcp_print_cfg(&ptr->backend_copy);
}


static const struct  modbus_master_backend_ops modbus_master_backend_tcp_ops = {
	.init          = modbus_master_tcp_init,
	.after_connect = modbus_master_tcp_after_connect,	
	.print         = modbus_master_tcp_print,
	.free          = modbus_master_tcp_free
};

Cell sproModbusMaster_ModbusSlaveTcpAsyncNetwork_addNetwork(SedonaVM* vm, Cell* params)
{
	FTRACE();

	int ret;
	uint8_t* self = params[0].aval;
	Cell ret_cell;


	struct modbus_master_tcp_thread *ptr = malloc(sizeof(*ptr));



	if (!ptr) {
		ERROR("failed allocate memory for modbus_master_tcp_thread\n");
		return zeroCell;
	}

	memset(ptr, 0, sizeof(*ptr));
	ret_cell.aval = &ptr->thread;


	ptr->thread.type = TCP;
	ptr->thread.ops = &modbus_master_backend_tcp_ops;
	ptr->thread.component = self;

	INIT_LIST_HEAD(&ptr->thread.threads);
	INIT_LIST_HEAD(&ptr->thread.regs);

	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&ptr->thread.master_mutex, &mutexattr);
	pthread_mutexattr_destroy(&mutexattr);

	/* make copy of ip address */
	Cell slot = params[1];
	const char *str = getSlotBufPtr(vm, self, slot);
	int size = sizeof(ptr->sedona.ip);
	strncpy(ptr->sedona.ip, str, size - 1);
	ptr->sedona.ip[size - 1] = '\0';

	ptr->sedona.port = params[2].ival;

	ret = pthread_create(&ptr->thread.thread_id, NULL, modbus_master_thread, &ptr->thread);
	INFO("created thread 0x%X\n", ptr->thread.thread_id);
	if (ret) {
		modbus_master_set_status(&ptr->thread, STATUS_FAILED);
		ERROR("failed to create thread (%s)\n", strerror(ret));
	}
	register_modbus_master(&ptr->thread);
	return ret_cell;
}



Cell sproModbusMaster_ModbusSlaveTcpAsyncNetwork_updateTcpSettings(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_master_thread *thread = params[1].aval;
	struct modbus_master_tcp_thread* ptr;

	ptr = container_of(thread, struct modbus_master_tcp_thread, thread);

	pthread_mutex_lock_checked(&ptr->thread.master_mutex, __func__);

	/* make copy of ip address */
	Cell slot = params[2];
	const char *str = getSlotBufPtr(vm, self, slot);
	int size = sizeof(ptr->sedona.ip);
	strncpy(ptr->sedona.ip, str, size - 1);
	ptr->sedona.ip[size - 1] = '\0';
	ptr->sedona.port = params[3].ival;

	ptr->thread.update = true;
	pthread_mutex_unlock(&ptr->thread.master_mutex);

	return zeroCell;
}
