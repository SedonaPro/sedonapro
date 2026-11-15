//
// Copyright 2017 Andrey Skvortsov <andrej.skvortzov@gmail.com>
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

int checkIp(char *str)
{
	int tmp, ret;
	ret = sscanf(str, "%d.%d.%d.%d",
		&tmp, &tmp, &tmp, &tmp);
	return (ret == 4);
}

Cell sproUtil_NetworkConfig_checkNetConfig(SedonaVM* vm, Cell* params)
{
	Cell retCell = oneCell;
	char *settings[3];
	int i;
	settings[0] = params[0].aval + sizeof(int);
	settings[1] = params[1].aval + sizeof(int);
	settings[2] = params[2].aval + sizeof(int);

	for(i=0; i<sizeof(settings)/sizeof(settings[0]); i++) {
		if (!checkIp(settings[i])) {
			retCell = zeroCell;
			break;
		}
	}
	return retCell;
}
