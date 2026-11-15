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
#include <signal.h>

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

#include "sproUtil.h"
#include "sproModbusSlave_common.h"

struct modbus_slave_tcp_thread {
	struct modbus_slave_thread thread;

	char  ip[INET6_ADDRSTRLEN];
	int   port;

	int   server_socket;
	fd_set refset;
	fd_set rdset;

	/* Maximum file descriptor number */
	int fdmax;
};

static void   modbus_slave_tcp_print(struct modbus_slave_thread *thread)
{
	struct modbus_slave_tcp_thread *ptr;
	ptr = container_of(thread, struct modbus_slave_tcp_thread, thread);
	printf("ip:             %s\n", ptr->ip);
	printf("port:           %d\n", ptr->port);

	printf("server_socket:  %d\n", ptr->server_socket);
	printf("fdmax:          %d\n", ptr->fdmax);
}

static void   modbus_slave_tcp_break_receive(struct modbus_slave_thread *thread)
{
	FTRACE();

	struct modbus_slave_tcp_thread *ptr;
	ptr = container_of(thread, struct modbus_slave_tcp_thread, thread);
	if (ptr->server_socket != -1)
	{
		ERROR("close server socket\n");
		close(ptr->server_socket);
		ptr->server_socket = -1;
	}
	modbus_close(thread->modbus_ctx);
	pthread_kill(thread->thread_id, SIGUSR1);
}



static const struct modbus_slave_backend_ops modbus_slave_backend_tcp_ops = {
	.print = modbus_slave_tcp_print,
	.break_receive = modbus_slave_tcp_break_receive
};



bool modbus_slave_tcp_init(struct modbus_slave_tcp_thread* ptr)
{
	FTRACE();
	const bool mb_debug = 
#ifdef NATIVE_DEBUG
		true;
#else
		false;
#endif

	const int nb_connection = 5;


	pthread_mutex_lock_checked(&ptr->thread.mutex, __func__);

	static char port[7];
	snprintf(port, sizeof(port), "%d", ptr->port);
	ptr->thread.modbus_ctx = modbus_new_tcp_pi(ptr->ip, port);

	if (!ptr->thread.modbus_ctx) {
		ERROR("failed to create libmodbus context\n");
		ptr->thread.status = STATUS_FAILED;
		pthread_mutex_unlock(&ptr->thread.mutex);
		sleep(modbus_slave_sleep_after_error_s);
		return false;
	}

	modbus_set_debug(ptr->thread.modbus_ctx, mb_debug);	
	modbus_set_slave(ptr->thread.modbus_ctx, ptr->thread.mb_slave_addr);
	
	ptr->server_socket = modbus_tcp_pi_listen(ptr->thread.modbus_ctx, nb_connection);
	if (ptr->server_socket == -1) {
		ERROR("failed to create server socket (%s)\n", modbus_strerror(errno));
		ptr->thread.status = STATUS_FAILED;
		modbus_close(ptr->thread.modbus_ctx);
		modbus_free(ptr->thread.modbus_ctx);
		ptr->thread.modbus_ctx = NULL;

		pthread_mutex_unlock(&ptr->thread.mutex);
		sleep(modbus_slave_sleep_after_error_s);
		return false;
	}

	ptr->thread.status = STATUS_OK;
	ptr->thread.update = false;
	ptr->thread.requests = 0;

	/* Clear the reference set of socket */
	FD_ZERO(&ptr->refset);
	/* Add the server socket */
	FD_SET(ptr->server_socket, &ptr->refset);

	/* Keep track of the max file descriptor */
	ptr->fdmax = ptr->server_socket;

	pthread_mutex_unlock(&ptr->thread.mutex);

	return true;
}


void modbus_slave_tcp_new_connection(struct modbus_slave_tcp_thread* ptr)
{
	FTRACE();

	/* A client is asking a new connection */
	socklen_t addrlen;
	struct sockaddr_in clientaddr;
	int newfd;

	/* Handle new connections */
	addrlen = sizeof(clientaddr);
	memset(&clientaddr, 0, sizeof(clientaddr));
	newfd = accept(ptr->server_socket, (struct sockaddr *)&clientaddr, &addrlen);
	if (newfd == -1) {
		ERROR("server accept() error");
	} else {
		FD_SET(newfd, &ptr->refset);

		if (newfd > ptr->fdmax) {
			/* Keep track of the maximum */
			ptr->fdmax = newfd;
		}
		INFO("new connection from %s:%d on socket %d, %d\n",
			inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd, ptr->fdmax);
	}
}

void modbus_slave_tcp_process(struct modbus_slave_tcp_thread* ptr, int master_socket)
{
	FTRACE();

	uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
	int rc;

	modbus_set_socket(ptr->thread.modbus_ctx, master_socket);
	rc = modbus_receive(ptr->thread.modbus_ctx, query);
	if (rc > 0) {
		pthread_mutex_lock_checked(&ptr->thread.mutex, __func__);
		if (ptr->thread.mb_mapping) {
			ptr->thread.requests++;
			rc = modbus_reply(ptr->thread.modbus_ctx, query, rc, ptr->thread.mb_mapping);				}
		pthread_mutex_unlock(&ptr->thread.mutex);
		if (rc == -1)
			ERROR("connection error (%s) on socket %d\n",
				modbus_strerror(errno), master_socket);

	} else	{
		if (rc == -1) {
			/* This example server in ended on connection closing or
			 * any errors. */
			INFO("connection closed on socket %d\n", master_socket);
			close(master_socket);

			/* Remove from reference set */
			FD_CLR(master_socket, &ptr->refset);

			if (master_socket == ptr->fdmax) {
				ptr->fdmax--;
			}
		}
	}
}

static void modbus_slave_process_ready_sockets(struct modbus_slave_tcp_thread* ptr)
{
	FTRACE();

	int master_socket;

	/* Run through the existing connections looking for data to be
	 * read */
	for (master_socket = 0; master_socket <= ptr->fdmax; master_socket++) {
		if (!FD_ISSET(master_socket, &ptr->rdset))
			continue;

		if (master_socket == ptr->server_socket)
			modbus_slave_tcp_new_connection(ptr);
		else
			modbus_slave_tcp_process(ptr, master_socket);
	}
}

void* modbus_slave_tcp_thread(void* arg)
{
	FTRACE();

	int requests = 0;
	struct modbus_slave_tcp_thread* ptr = arg;
	struct modbus_slave_thread* thread = &ptr->thread;

	int rc;

	modbus_slave_signal_started_work_thread(&ptr->thread);
	while(!modbus_slave_get_quit(thread)) {
		if (!modbus_slave_is_enable(thread))
			continue;

		if (!modbus_slave_tcp_init(ptr))
			continue;

		for (;;) {
			struct timeval timeout = {
				.tv_sec = 0,
				.tv_usec = 20000
			};
			int ready_fd;

			ptr->rdset = ptr->refset;
			ready_fd = select(ptr->fdmax+1, &ptr->rdset, NULL, NULL, &timeout);
			if (ready_fd == -1) {
				ERROR("server select() failure.");
				modbus_slave_set_status(thread, STATUS_FAILED);
				sleep(2);
				break;
			}
			if (ready_fd > 0)
				modbus_slave_process_ready_sockets(ptr);

			if (modbus_slave_get_quit(thread)) {
				break;
			}

			if (modbus_slave_get_update(thread)) {
				break;
			}

		}

		if (ptr->server_socket != -1) {
			ERROR("close server socket\n");
			close(ptr->server_socket);
		}

		modbus_close(ptr->thread.modbus_ctx);
		modbus_free(ptr->thread.modbus_ctx);
		modbus_slave_set_status(thread, STATUS_DOWN);
    }
    modbus_slave_clear(thread);
    return 0;
}


Cell sproModbusSlave_ModbusSlaveTcpService_addSlaveService(SedonaVM* vm, Cell* params)
{
	FTRACE();

	int ret;
	Cell ret_cell;
	uint8_t* self = params[0].aval;

	if (!modbus_slave_signal_setup()) {
		ERROR("failed to set signal handler\n");
		return zeroCell;
	}

	struct modbus_slave_tcp_thread *ptr = malloc(sizeof(*ptr));
	if (!ptr) {
		ERROR("failed allocate memory\n");
		return zeroCell;
	}
	memset(ptr, 0, sizeof(*ptr));

	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&ptr->thread.mutex, &mutexattr);
	pthread_mutexattr_destroy(&mutexattr);

	pthread_cond_init(&ptr->thread.en_cond, NULL);

	ptr->thread.type = TCP;
	ptr->thread.ops  = &modbus_slave_backend_tcp_ops;
	ptr->thread.component = self;

	ptr->thread.mb_slave_addr  = params[3].ival;

	Cell slot = params[1];
	const char *str = getSlotBufPtr(vm, self, slot);
	int size = sizeof(ptr->ip);
	strncpy(ptr->ip, str, size -1);
	ptr->ip[size - 1] = '\0';
	ptr->port     = params[2].ival;

	ptr->server_socket = -1;

	modbus_slave_update_mapping(&ptr->thread,
				params[4].ival,
				params[5].ival,
				params[6].ival,
				params[7].ival);
	ret_cell.aval = &ptr->thread;

	ret = pthread_create(&ptr->thread.thread_id, NULL, modbus_slave_tcp_thread, ptr);
	if (ret) {
		ptr->thread.status = STATUS_FAILED;
		ERROR("failed to create thread (%s)\n", strerror(ret));
	}
	register_modbus_slave(&ptr->thread);
	return ret_cell;
}



Cell sproModbusSlave_ModbusSlaveTcpService_updateTcpSettings(SedonaVM* vm, Cell* params)
{
	FTRACE();

	uint8_t* self = params[0].aval;
	struct modbus_slave_thread* thread = params[1].aval;
	struct modbus_slave_tcp_thread* ptr;

	ptr = container_of(thread, struct modbus_slave_tcp_thread, thread);

	pthread_mutex_lock_checked(&thread->mutex, __func__);

	Cell slot = params[2];
	const char *str = getSlotBufPtr(vm, self, slot);
	int size = sizeof(ptr->ip);
	strncpy(ptr->ip, str, size -1);
	ptr->ip[size - 1] = '\0';

	ptr->port     = params[3].ival;

	thread->update = true;
	pthread_mutex_unlock(&thread->mutex);
	return zeroCell;
}
