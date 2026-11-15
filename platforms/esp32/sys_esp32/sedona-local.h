// SPDX-License-Identifier: AFL-3.0
// Copyright 2026 SedonaPro contributors
//
// Sedona VM platform definitions for ESP-IDF (ESP32-S3, Xtensa LX7).
// Included by sedona.h when SEDONA_LOCAL_H is defined (see firmware CMake).
//
#ifndef __SEDONA_LOCAL_H
#define __SEDONA_LOCAL_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef O_DIRECTORY
#define O_DIRECTORY 0          // svm_stage.c: fsync of the parent directory
#endif
// fd_set must get its normal size before inet_util_std.h sets FD_SETSIZE=1:
// lwIP socket numbers start above 1 (LWIP_SOCKET_OFFSET).
#include <sys/select.h>
#include <sys/socket.h>

#define IS_LITTLE_ENDIAN
#define USE_STANDARD_MAIN
#define SCODE_BLOCK_SIZE 4
#define block2addr(cb, block) ((cb) + (block<<2))
#define SCODE_DEBUG
#ifndef TRUE
  #define TRUE 1
#endif
#ifndef FALSE
  #define FALSE 0
#endif
#define ISNANF(f) (isnan(f))
#define ISNAN(d)  (isnan(d))
#define _chdir chdir

// the shared inet/std sources pick their BSD socket headers on __UNIX__;
// lwIP provides the same API (defined here, after sedona.h chose its branch)
#ifndef __UNIX__
#define __UNIX__ 1
#endif

#endif
