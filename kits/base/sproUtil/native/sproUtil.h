
#ifndef _SPRO_UTIL_H_
#define _SPRO_UTIL_H_

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
#include <pthread.h>

static inline void* getSlotPtr(SedonaVM *vm, uint8_t* self, Cell slot)
{
	uint16_t offset = getSlotHandle(vm, slot.aval);
	return getInline(self, offset);
}


static inline void* getSlotBufPtr(SedonaVM *vm, uint8_t* self, Cell slot)
{
	return (void*)getSlotPtr(vm, self, slot) + sizeof(int);
}

static inline int getSlotBufSize(SedonaVM *vm, uint8_t* self, Cell slot)
{
	return *(int*)getSlotPtr(vm, self, slot);
}


#define MESSAGE(out, format, ...) fprintf(out, "THREAD 0x%X: %s: " format, pthread_self(), __func__, ##__VA_ARGS__)

/* uncomment next line to enable debug messages in all kits */
/* #define NATIVE_DEBUG */

#ifdef NATIVE_DEBUG
#define INFO(format, ...) MESSAGE(stdout, format, ##__VA_ARGS__)
#else
#define INFO(format, ...)
#endif

#define ERROR(format, ...) MESSAGE(stderr, format, ##__VA_ARGS__)

#define FTRACE()  	INFO("\n")


#define pthread_mutex_lock_checked(mutex, func)			\
	do {							\
		if (pthread_mutex_lock(mutex))			\
			ERROR("failed to lock the mutex\n");	\
	} while(0)

#endif
