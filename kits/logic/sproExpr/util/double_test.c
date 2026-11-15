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
#include "double_parser.h"
#include "double_eval.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

struct double_test_case {
	char *expr;
	double result;
	int    parse;
	double params[MAX_DOUBLE_PARAMS];

	double real_result;
	double real_error;
};

static struct double_test_case double_tests[] =
{
	{  "4.1294 + (2 + 10)", 16.1294, 0, },
	{  "4.1294 + 2*10 + 3*(5+1)", 42.1294, 0, },
	{  "-4.1294", -4.1294, 0, },
	{  "4.1294 + 2*10 + 3*(5+1)/6.0", 27.1294, 0, },
	{  "4.1294 + 2*10 + 3*(5+1)/6.0 - 2.1294", 25, 0, },
	{  "-1*(floor(4.1294) + 2*10 + 3*(5+1)/6.0)", -27, 0, },
	{  "-1*(ceil(4.1294) + 2*10 + 3*(5+1)/6.0)", -28, 0, },
	{  "-1*(ceil(-4.1294) + 2*10 + 3*(5+1)/6.0)", -19, 0, },
	{  "abs(-1.83)", 1.83, 0, },
	{  "abs(ceil(-4.1294))", 4, 0, },
	{  "abs(ceil(-4.1294)) + 1", 5, 0, },
	{  "-1*(abs(ceil(-4.1294)) + 2*10 + 3*(5+1)/6.0)", -27, 0, },
	{  "3*PI", 9.424779, 0, },
	{  "cos(PI/4)*2", 1.414213562, 0, },
	{  "sqrt(2)/2", 0.707107, 0, },
	{  "(2*sin(PI/4))^2", 2, 0, },
	{  "tan(PI/4)", 1, 0, },
	{  "4.1294 + cbrt(8)*10 + cbrt(27)*(sqrt(25)+1)/6.0 - 2.1294", 25, 0, },
	{  "log2(1)", 0, 0, },
	{  "log10(1000)", 3, 0, },
	{  "ln(e)", 1, 0, },
	{  "exp(5)*0.001", 0.148413159103, 0, },
	{  "(e^5)*0.001", 0.148413159103, 0, },
	{  "e^5*0.001", 0.148413159103, 0, },
	{  "e^(5*0.001)", 1.005012521, 0, },
	{  "5*in1 + in2*in3", 11.5, 0, {2, 3, 0.5}},

	{  "4.1294 + (in1 + in2)", 16.1294, 0, {2, 10}},
	{  "4.1294 + 2*10 + in1*(5+in2)", 42.1294, 0, {3, 1} },
	{  "-in1", -4.1294, 0, {4.1294} },
	{  "in4 + in1*in2 + 3*(5+1)/in3", 27.1294, 0, {2, 10, 6.0, 4.1294}},
	{  "in2 + 2*10 + 3*(5+1)/6.0 - in1", 25, 0, {2.1294, 4.1294}, },
	{  "in3*(floor(in1) + in2*10 + 3*(5+1)/6.0)", -27, 0, {4.1294, 2, -1}},
	{  "-1*(ceil(in1) + 2*10 + 3*(5+1)/6.0)", -28, 0, {4.1294}},
	{  "-1*(ceil(-in1) + 2*10 + 3*(5+1)/6.0)", -19, 0,{4.1294} },
	{  "abs(in1)", 1.83, 0, {-1.83}},
	{  "abs(ceil(-in1))", 4, 0, {4.1294}},
	{  "abs(ceil(in1)) + 1", 5, 0, {-4.1294}},
	{  "in3*(abs(ceil(in1)) + 2*10 + in2*(5+1)/6.0)", -27, 0, {-4.1294, 3, -1}},
	{  "in1*PI", 9.424779, 0, {3}},
	{  "cos(PI/in1)*2", 1.414213562, 0, {4}},
	{  "sqrt(in1)/2", 0.707107, 0, {2}},
	{  "(in2*sin(PI/in1))^2", 2, 0, {4, 2}},
	{  "tan(PI/in1)", 1, 0, {4}},
	{  "4.1294 + cbrt(in1)*10 + cbrt(in4)*(sqrt(in2)+1)/6.0 + in3", 25, 0, {8, 25, -2.1294, 27}},
	{  "log2(in1)", 0, 0, {1}},
	{  "log10(in1)", 3, 0, {1000}},
	{  "ln(e)", 1, 0, },
	{  "exp(in1)*0.001", 0.148413159103, 0, {5}},
	{  "(e^in1)*0.001", 0.148413159103, 0, {5}},
	{  "e^in1*0.001", 0.148413159103, 0, {5}},
	{  "e^(in1*0.001)", 1.005012521, 0, {5}},
};


static int double_check_test(struct double_test_case *test, char *filename)
{
	SExpression *e = NULL;
	double result = 0;
	double error = 0;
	e = double_getAST(test->expr);
	if (!e)
		return 1;

	if (filename)
		write_graphviz(e, filename);
	result = double_evaluate(e);
	deleteExpression(e);

	error = fabs(result - test->result);
	test->real_result = result;
	test->real_error = error;

	return error > 0.001 ? 1 : 0;
}

static void double_print_test(struct double_test_case *test)
{
	printf("error: %f, result: %f, expected: %f, expr: '%s'\n",
		test->real_error, test->real_result,
		test->result, test->expr);
}



int double_test(void)
{
	char filename[150];
	char*fileout;
#if YYDEBUG
	/* yydebug = 1; */
#endif
	int errors = 0;
	for(int i=0; i<sizeof(double_tests)/sizeof(double_tests[0]); i++) {
		fileout = NULL;
		memcpy(&double_variables, &double_tests[i].params, sizeof(double_variables));
		if (0) {
			snprintf(filename, sizeof(filename), "double-tree-%d.dot", i);
			fileout = filename;
		}
		if (double_check_test(&double_tests[i], fileout)) {
			errors++;
			double_print_test(&double_tests[i]);
		}
	}
	printf("Number of errors: %d\n", errors);
	return 0;
}
