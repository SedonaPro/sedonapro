#ifndef _BOOL_EVAL_H_
#define _BOOL_EVAL_H_

#include "expression.h"
#include "bool_params.h"

SExpression *bool_getAST(const char *source_code);
int bool_evaluate(SExpression *e);

#endif
