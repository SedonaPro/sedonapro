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
#include "double_eval.h"
#include "double_parser.h"
#include <math.h>

/* For compatability with bison 2.6 and later */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED && defined DOUBLE_STYPE_IS_DECLARED
#define YYSTYPE DOUBLE_STYPE
#endif

#include "double_lexer.h"

SExpression *double_getAST(const char *source_code)
{
	/* get Abstract Syntax Tree */
	SExpression *expression;
	yyscan_t scanner;
	YY_BUFFER_STATE state;
	int parse_status;
	int lex_status;

	lex_status = double_lex_init(&scanner);              // initialize lexer
	if (lex_status) return NULL;

	state = double__scan_string(source_code, scanner);   // do lexical analysis

	parse_status = double_parse(&expression, scanner);   // create parse tree
	if (parse_status) {
		if (svmConfig.memFixes) {                  // mem.fixes: no leak on syntax errors
			double__delete_buffer(state, scanner);
			double_lex_destroy(scanner);
		}
		return NULL;
	}

	double__delete_buffer(state, scanner);               // clean up
	double_lex_destroy(scanner);

	return expression;
}



/*
  "execute" the parsed tree
 */
double double_evaluate(SExpression *e)
{
	switch (e->type) {
	case eVAR:
	case eVALUE:
		return *(double*)e->value;
	case eMULTIPLY:
		return double_evaluate(e->left) * double_evaluate(e->right);
	case eDIV:
		return double_evaluate(e->left) / double_evaluate(e->right);
	case eMOD:
		return (long)double_evaluate(e->left) % (long)double_evaluate(e->right);
	case ePLUS:
		return double_evaluate(e->left) + double_evaluate(e->right);
	case eSUB:
		return double_evaluate(e->left) - double_evaluate(e->right);
	case eFLOOR:
		return floor(double_evaluate(e->right));
	case eCEIL:
		return ceil(double_evaluate(e->right));
	case eABS:
		return fabs(double_evaluate(e->right));
	case eCOS:
		return cos(double_evaluate(e->right));
	case eSIN:
		return sin(double_evaluate(e->right));
	case eTAN:
		return tan(double_evaluate(e->right));
	case eSINH:
		return sinh(double_evaluate(e->right));
	case eCOSH:
		return cosh(double_evaluate(e->right));
	case eTANH:
		return tanh(double_evaluate(e->right));
	case eSQRT:
		return sqrt(double_evaluate(e->right));
	case eCBRT:
		return cbrt(double_evaluate(e->right));
	case eLOG2:
		return log2(double_evaluate(e->right));
	case eLOG10:
		return log10(double_evaluate(e->right));
	case eLN:
		return log(double_evaluate(e->right));
	case eEXP:
		return exp(double_evaluate(e->right));
	case ePOW:
		return pow(double_evaluate(e->left), double_evaluate(e->right));
	default:          // shouldn't get here
		return 0;
	}
}
