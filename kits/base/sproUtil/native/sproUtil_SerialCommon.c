// -*- mode: c++ -*-
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
#include "sproUtil_SerialCommon.h"


const int   baudrates[] = {
	2400,
	4800,
	9600,
	19200,
	38400,
	57600,
	115200,
	230400
};

const char parity[] = {
	'N',
	'O',
	'E'
};


char** serial_ports_filenames;
int serialMaxPortNum;



Cell sproUtil_SerialInfoPlatform_setMaxPortNumNative(SedonaVM* vm, Cell* params)
{
	serialMaxPortNum = params[0].ival;
	size_t len = sizeof(char*)*(serialMaxPortNum + 1);
	serial_ports_filenames = malloc(len);
	memset(serial_ports_filenames, 0, len);
	return zeroCell;
}


Cell sproUtil_SerialInfoPlatform_setPortNameNative(SedonaVM* vm, Cell* params)
{
	int  indx = params[0].ival;
	char *str = params[1].aval;
	if (serial_ports_filenames && indx >= 0 && indx <= serialMaxPortNum)
		serial_ports_filenames[indx] = str;

	return zeroCell;
}
