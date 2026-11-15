// Hey, emacs! -*- c++ -*-
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

#include <sys/param.h>
#include "sedona.h"
#include "bool_eval.h"

Cell sproExpr_ExpressionBool_doParse(SedonaVM* vm, Cell* params)
{
	char* expression = params[0].aval;
	Cell  result;
	result.aval = (void *)bool_getAST(expression);
	return result;
}


Cell sproExpr_ExpressionBool_doFree(SedonaVM* vm, Cell* params)
{
	SExpression *expr = params[0].aval;
	deleteExpression(expr);
	return zeroCell;
}


Cell sproExpr_ExpressionBool_doCalc(SedonaVM* vm, Cell* params)
{
	Cell result;
	SExpression *expr = params[0].aval;
	uint8_t *exprParams = params[1].aval;
	int paramsNum = params[2].ival;
	int maxI = MIN(paramsNum, MAX_BOOL_PARAMS);
	int i;

	for(i = 0; i<maxI; i++)
		bool_variables[i] = exprParams[i];
	result.ival = bool_evaluate(expr);
	return result;
}
