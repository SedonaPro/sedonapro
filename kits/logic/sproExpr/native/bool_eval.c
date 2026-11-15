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

#include "svm_config.h"
#include "bool_eval.h"
#include "bool_parser.h"
#include <math.h>

/* For compatability with bison 2.6 and later */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED && defined BOOL_STYPE_IS_DECLARED
#define YYSTYPE BOOL_STYPE
#endif

#include "bool_lexer.h"



SExpression *bool_getAST(const char *source_code)
{
	/* get Abstract Syntax Tree */
	SExpression *expression;
	yyscan_t scanner;
	YY_BUFFER_STATE state;
	int parse_status;
	int lex_status;

	lex_status = bool_lex_init(&scanner);              // initialize lexer
	if (lex_status) return NULL;

	state = bool__scan_string(source_code, scanner);   // do lexical analysis

	parse_status = bool_parse(&expression, scanner);   // create parse tree
	if (parse_status) {
		if (svmConfig.memFixes) {                  // mem.fixes: no leak on syntax errors
			bool__delete_buffer(state, scanner);
			bool_lex_destroy(scanner);
		}
		return NULL;
	}

	bool__delete_buffer(state, scanner);               // clean up
	bool_lex_destroy(scanner);

	return expression;
}



/*
  "execute" the parsed tree
 */
int bool_evaluate(SExpression *e)
{
	int r1, r2, ret;
	switch (e->type) {
	case eVARBool:
	case eVALUEBool:
		return *(int*)e->value;
	case eNOT: {
		r1 = bool_evaluate(e->right);
		if (r1 != boolNull)
			ret = !(!!r1);
		else
			ret = boolNull;
		return  ret;
	}
	case eAND: {
		r1 = bool_evaluate(e->left);
		r2 = bool_evaluate(e->right);
		if ((r1 != boolNull) && (r2 != boolNull))
			ret = r1 && r2;
		else
			if (r1 == boolNull)
				ret = r2;
			else
				ret = r1;
		return  ret;
	}
	case eOR: {
		r1 = bool_evaluate(e->left);
		r2 = bool_evaluate(e->right);
		if ((r1 != boolNull) && (r2 != boolNull))
			ret = r1 || r2;
		else
			if (r1 == boolNull)
				ret = r2;
			else
				ret = r1;
		return  ret;
	}
	case eXOR: {
		r1 = bool_evaluate(e->left);
		r2 = bool_evaluate(e->right);
		if ((r1 != boolNull) && (r2 != boolNull))
			ret = r1 != r2 ? boolTrue : boolFalse;
		else
			ret = boolNull;
		return  ret;
	}
	default:          // shouldn't get here
		return boolNull;
	}
}
