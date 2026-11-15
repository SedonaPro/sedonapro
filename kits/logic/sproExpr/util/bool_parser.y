/*

 Copyright 2018 Andrey Skvortsov <andrej.skvortzov@gmail.com>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http:www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

*/

%{

#include "expression.h"
#include "bool_parser.h"
#include "bool_lexer.h"
#include <math.h>

int bool_error(SExpression **, yyscan_t scanner, const char *msg);

%}

%code requires {
#include <stdbool.h>

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif
}


%output  "bool_parser.c"
%defines "bool_parser.h"

%define api.pure
%define api.prefix {bool_}
%lex-param   { yyscan_t scanner }
%parse-param { SExpression **expression }
%parse-param { yyscan_t scanner }

%union {
	int* var;
	int  value;
	SExpression *expression;
}

%left '+' TOKEN_OR
%left '*' TOKEN_AND
%left '^' TOKEN_XOR
%left L_BRACKET R_BRACKET

%token TOKEN_LPAREN
%token TOKEN_RPAREN
%token TOKEN_NOT
%token TOKEN_OR
%token TOKEN_AND
%token TOKEN_XOR
%token <value> TOKEN_FALSE TOKEN_TRUE TOKEN_NULL
%token <var> TOKEN_VAR
%type  <expression> expr
%type  <value> constant
%type  <expression> variable


%%

input
: expr                              { *expression = $1; }
;

variable
: TOKEN_VAR { $$ = createVariable($1, eVARBool); }


constant
: TOKEN_TRUE   { $$ = boolTrue;  }
| TOKEN_FALSE  { $$ = boolFalse; }
| TOKEN_NULL   { $$ = boolNull;  }
| TOKEN_LPAREN constant[V] TOKEN_RPAREN { $$ = $V; }

// ast optimizations
;

expr
: TOKEN_NOT constant[R]                                      { $$ = createOperation(eNOT,  NULL, createIntNumber($R, eVALUEBool));}
| TOKEN_NOT variable[R]                                      { $$ = createOperation(eNOT,  NULL, $R);}
| TOKEN_NOT TOKEN_LPAREN expr[E] TOKEN_RPAREN                { $$ = createOperation(eNOT,  NULL, $E);}
| variable
| constant                                                   { $$ = createIntNumber($1, eVALUEBool);}
| expr[L] TOKEN_AND expr[R]                                  { $$ = createOperation(eAND, $L, $R);}
| TOKEN_LPAREN expr[E] TOKEN_RPAREN                          { $$ = $E;}
| expr[L] TOKEN_OR expr[R]                                   { $$ = createOperation(eOR,   $L, $R);}
| expr[L] TOKEN_XOR expr[R]                                  { $$ = createOperation(eXOR,  $L, $R);}
;

%%
