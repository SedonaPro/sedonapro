// Hey, emacs! -*- c -*-
//
// Copyright 2018 Andrey Skvortsov <andrej.skvortzov@gmail.com>
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

#include <sqlite3.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#include "sedona.h"

struct sqlite_conv {
	const char   *format;
	NativeMethod  callback;
	void  	     *data;
};


Cell sys_Component_doSetBool(SedonaVM* vm, Cell* params);
Cell sys_Component_doSetInt(SedonaVM* vm, Cell* params);
Cell sys_Component_doSetLong(SedonaVM* vm, Cell* params);
Cell sys_Component_doSetFloat(SedonaVM* vm, Cell* params);
Cell sys_Component_doSetDouble(SedonaVM* vm, Cell* params);

static int read_data(void *pdata, int argc, char **argv, char **colname)
{
	struct sqlite_conv *p = pdata;
	int ret = 0;
	if ((argc > 0) && (argv[0]))
		ret = sscanf(argv[0], p->format, p->data) == 1 ? 0 : -1;
	return ret;
}

int doExecSql(sqlite3 *db, char *sql_stmt, struct sqlite_conv *dst)
{
	char *err_msg = NULL;
	int ret;
	void *callback = NULL;

	if (!db || !sql_stmt)
		return -1;

	if (dst)
		callback = read_data;
	ret = sqlite3_exec(db, sql_stmt, callback, dst, &err_msg);
	if ((ret != SQLITE_OK) && (ret !=SQLITE_BUSY)){
		fprintf(stderr, "sproSqlite: failed to execute SQL statement: '%s'\n", sql_stmt);
		fprintf(stderr, "sproSqlite: error: %d, %s\n", ret, err_msg);
		sqlite3_free(err_msg);
		return ret;
	}

	return ret;
}


// native method slots
Cell sproSqlite_SqliteDb_doOpen(SedonaVM* vm, Cell* params)
{
	char * filename = params[0].aval;
	Cell result = zeroCell;
	int ret;
	sqlite3 *db;

	ret = sqlite3_open_v2(filename, &db,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |  SQLITE_OPEN_FULLMUTEX,
			NULL);

	if (ret != SQLITE_OK) {
		fprintf(stderr, "sproSqlite: failed to open database %s: %d\n", filename, ret);
		return zeroCell;
	}

	result.aval = (void *)db;
	return result;
}

Cell sproSqlite_SqliteDb_doExecSql(SedonaVM* vm, Cell* params)
{
	sqlite3 *db = params[0].aval;
	char *sql_stmt = params[1].aval;
	Cell ret;

	ret.ival = doExecSql(db, sql_stmt, NULL);
	return ret;
}

Cell sproSqlite_SqliteDb_doExecInsertSql(SedonaVM* vm, Cell* params)
{
	uint8_t* self   = params[0].aval;
	sqlite3 *db = params[1].aval;
	char *sql_stmt = params[2].aval;
	uint64_t *id_out = params[3].aval;
	Cell ret;

	*id_out = -1L;
	ret.ival = doExecSql(db, sql_stmt, NULL);
	if (ret.ival == SQLITE_OK)
		*id_out = sqlite3_last_insert_rowid(db);

	return ret;
}


Cell sproSqlite_SqliteDb_doExecSelectSql(SedonaVM* vm, Cell* params)
{
	Cell paramArgs[4];
	uint8_t* self   = params[0].aval;
	sqlite3 *db = params[1].aval;
	char *sql_stmt = params[2].aval;
	uint8_t *slot = params[3].aval;
	Cell ret = negOneCell;
	uint16_t typeId = getTypeId(vm, getSlotType(vm, slot));

	struct sqlite_conv data;
	switch (typeId) {
	case ByteTypeId:
		data.format = "%hhd";
		data.callback = sys_Component_doSetInt;
		data.data = &paramArgs[2];
		*(uint8_t*)(data.data) = 0;
		break;
	case ShortTypeId:
		data.format = "%hd";
		data.callback = sys_Component_doSetInt;
		data.data = &paramArgs[2];
		*(uint16_t*)(data.data) = 0;
		break;
	case IntTypeId:
		data.format = "%d";
		data.callback = sys_Component_doSetInt;
		data.data = &paramArgs[2];
		*(uint32_t*)(data.data) = 0;
		break;
	case LongTypeId:
		data.format = "%" PRIu64 "";
		data.callback = sys_Component_doSetLong;
		data.data = &paramArgs[2];
		*(uint64_t*)(data.data) = 0;
		break;
	case FloatTypeId:
		data.format = "%f";
		data.callback = sys_Component_doSetFloat;
		data.data = &paramArgs[2];
		*(float*)(data.data) = 0;
		break;
	default:
		return negOneCell;
	}

	int result = doExecSql(db, sql_stmt, &data);
	if (result == SQLITE_OK) {
		paramArgs[0] = params[0];
		paramArgs[1] = params[3];
		(*data.callback)(vm, paramArgs);
	}
	ret.ival = result;
	return ret;
}


Cell sproSqlite_SqliteDb_doClose(SedonaVM* vm, Cell* params)
{
	sqlite3 *db = params[0].aval;
	int ret;

	if (!db)
		return negOneCell;

	ret = sqlite3_close_v2(db);
	if (ret != SQLITE_OK) {
		fprintf(stderr, "sproSqlite: failed to close database: %d\n", ret);
		return negOneCell;
	}

	return zeroCell;
}
