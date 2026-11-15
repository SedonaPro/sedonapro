#ifndef _DOUBLE_EVAL_H_
#define _DOUBLE_EVAL_H_

#include "expression.h"
#include "double_params.h"

SExpression *double_getAST(const char *source_code);
double double_evaluate(SExpression *e);

#endif
