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


#include "graph.h"
#include "bool_parser.h"
#include "bool_eval.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

struct bool_test_case {
	char *expr;
	int result;
	int    parse;
	int params[MAX_BOOL_PARAMS];

	int real_result;
};

static struct bool_test_case bool_tests[] =
{
	{  "TRUE AND true", boolTrue, 0, },
	{  "false AND true", boolFalse, 0, },
	{  "null AND true", boolTrue, 0, },
	{  "1 AND null", boolTrue, 0, },
	{  "0 AND null", boolFalse, 0, },
	{  "null && false", boolFalse, 0, },
	{  "null AND null", boolNull, 0, },

	{  "TRUE OR true", boolTrue, 0, },
	{  "false OR true", boolTrue, 0, },
	{  "null || true", boolTrue, 0, },
	{  "null OR 0", boolFalse, 0, },
	{  "null OR null", boolNull, 0, },

	{  "TRUE XOR true", boolFalse, 0, },
	{  "false ^ true", boolTrue, 0, },
	{  "null XOR true", boolNull, 0, },
	{  "null XOR false", boolNull, 0, },
	{  "null XOR null", boolNull, 0, },

	{  "NOT true", boolFalse, 0, },
	{  "NOT false", boolTrue, 0, },
	{  "! false", boolTrue, 0, },

	{  "1 AND 1 AND 1", boolTrue, 0, },
	{  "1 AND 0 AND 1 AND 1 AND 1 AND 2", boolFalse, 0, },
	{  "(1 OR 0) AND 1 AND 1 AND 1 AND 2", boolTrue, 0, },
	{  "(1 OR 0) AND (1 AND 0 AND 1 AND 2)", boolFalse, 0, },
	{  "1 && (2 OR 2) AND (1 XOR null) AND 2", boolTrue, 0, },
	{  "2 && (2 OR 2) AND (1 XOR null) AND 2", boolNull, 0, },


	{  "in1 AND in2", boolTrue, 0, {boolTrue, boolTrue}},
	{  "false AND in1", boolFalse, 0, {boolTrue}},
	{  "in1 AND in2", boolTrue, 0, {boolNull, boolTrue}},
	{  "in1 && false", boolFalse, 0, {boolNull} },
	{  "null AND in1", boolNull, 0, {boolNull}},

	{  "TRUE OR in1", boolTrue, 0, {boolTrue}},
	{  "in2 OR in1", boolTrue, 0, {boolTrue, boolFalse}},
	{  "in1 || true", boolTrue, 0,{boolNull} },
	{  "in1 OR 0", boolFalse, 0, {boolNull}},
	{  "in1 OR null", boolNull, 0, {boolNull}},

	{  "in1 XOR true", boolFalse, 0, {boolTrue}},
	{  "false ^ in1", boolTrue, 0, {boolTrue}},
	{  "null XOR in1", boolNull, 0, {boolTrue}},
	{  "in1 XOR false", boolNull, 0, {boolNull}},
	{  "null XOR in1", boolNull, 0, {boolNull}},

	{  "NOT true", boolFalse, 0, {boolTrue}},
	{  "NOT false", boolTrue, 0, {boolFalse}},
	{  "! null", boolNull, 0, {boolNull}},
	{  "! false", boolTrue, 0, {boolTrue}},

	{  "false", boolFalse, 0, {boolFalse}},
	{  "in1", boolFalse, 0, {boolFalse}},

	{  "1 AND 1 AND 1", boolTrue, 0, },
	{  "1 AND 0 AND 1 AND 1 AND 1 AND 2", boolFalse, 0, },
	{  "(1 OR 0) AND 1 AND 1 AND 1 AND 2", boolTrue, 0, },
	{  "(1 OR 0) AND (1 AND 0 AND 1 AND 2)", boolFalse, 0, },
	{  "1 && (2 OR 2) AND (1 XOR null) AND 2", boolTrue, 0, },
	{  "2 && (2 OR 2) AND (1 XOR null) AND 2", boolNull, 0, },

};


static int bool_check_test(struct bool_test_case *test, char *filename)
{
	SExpression *e = NULL;
	int result = 0;
	e = bool_getAST(test->expr);
	if (!e)
		return 1;

	if (filename)
		write_graphviz(e, filename);
	result = bool_evaluate(e);
	deleteExpression(e);

	test->real_result = result;
	return test->result != result;
}

static void bool_print_test(struct bool_test_case *test)
{
	printf("result: %d, expected: %d, expr: '%s'\n",
		test->real_result, test->result, test->expr);
}



int bool_test(void)
{
	char filename[150];
	char*fileout;
#if YYDEBUG
	/* yydebug = 1; */
#endif
	int errors = 0;
	for(int i=0; i<sizeof(bool_tests)/sizeof(bool_tests[0]); i++) {
		fileout = NULL;
		memcpy(&bool_variables, &bool_tests[i].params, sizeof(bool_variables));
		if (0) {
			snprintf(filename, sizeof(filename), "bool-tree-%d.dot", i);
			fileout = filename;
		}

		if (bool_check_test(&bool_tests[i], fileout)) {
			errors++;
			bool_print_test(&bool_tests[i]);
		}
	}
	printf("Number of errors: %d\n", errors);
	return 0;
}
