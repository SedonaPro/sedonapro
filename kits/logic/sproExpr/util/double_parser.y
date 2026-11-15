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
#include "double_parser.h"
#include "double_lexer.h"
#include <math.h>

int double_error(SExpression **, yyscan_t scanner, const char *msg);	

%}

%code requires {
#include <stdbool.h>
	
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif
}


%output  "double_parser.c"
%defines "double_parser.h"

%define api.pure
%define api.prefix {double_}
%lex-param   { yyscan_t scanner }
%parse-param { SExpression **expression }
%parse-param { yyscan_t scanner }

%union {
	double* var;
	double  value;
	SExpression *expression;
}

%left '+' TOKEN_PLUS
%left '-' TOKEN_SUB 
%left '*' TOKEN_MULTIPLY
%left '/' TOKEN_DIV 
%left L_BRACKET R_BRACKET

%token TOKEN_LPAREN
%token TOKEN_RPAREN
%token TOKEN_PLUS
%token TOKEN_SUB
%token TOKEN_MULTIPLY
%token TOKEN_DIV
%token TOKEN_MOD
%token <value> TOKEN_NUMBER
%token TOKEN_PI TOKEN_E
%token <expression> TOKEN_FLOOR TOKEN_CEIL TOKEN_ABS
%token <value> TOKEN_COS TOKEN_SIN TOKEN_TAN TOKEN_COSH TOKEN_SINH TOKEN_TANH
%token <value> TOKEN_POW TOKEN_SQRT TOKEN_CBRT
%token <value> TOKEN_LOG2 TOKEN_LOG10 TOKEN_LN TOKEN_EXP
%token <var> TOKEN_VAR
%type  <expression> expr
%type  <value> constant
%type  <expression> function
%type  <expression> variable


%%
 
input
: expr                              { *expression = $1; }
;

variable
: TOKEN_VAR { $$ = createVariable($1, eVAR); }


constant
: TOKEN_NUMBER
| TOKEN_PI { $$ = M_PI; }
| TOKEN_E  { $$ = M_E; }
| TOKEN_LPAREN constant[V] TOKEN_RPAREN { $$ = $V; }

// ast optimizations
/* | TOKEN_SUB constant                         { $$ = -$2;} */
/* | constant[L] TOKEN_MULTIPLY constant[R]     { $$ = $L * $R;} */
/* | constant[L] TOKEN_DIV constant[R]          { $$ = $L / $R;} */
/* | constant[L] TOKEN_PLUS constant[R]         { $$ = $L + $R;} */
/* | constant[L] TOKEN_SUB constant[R]          { $$ = $L - $R;} */
/* | constant[L] TOKEN_POW TOKEN_NUMBER[R]         { $$ = pow($L, $R);} */
/* | constant[L] TOKEN_POW TOKEN_LPAREN constant[R] TOKEN_RPAREN { $$ = pow($L, $R);} */
/* | constant[L] TOKEN_MOD constant[R]          { $$ = (long)$L % (long)$R;} */

/* | TOKEN_FLOOR TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = floor($3); } */
/* | TOKEN_CEIL  TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = ceil($3); } */
/* | TOKEN_ABS   TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = fabs($3); } */
/* | TOKEN_COS   TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = cos($3); } */
/* | TOKEN_SIN   TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = sin($3); } */
/* | TOKEN_TAN   TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = tan($3); } */
/* | TOKEN_COSH  TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = cosh($3); } */
/* | TOKEN_SINH  TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = sinh($3); } */
/* | TOKEN_TANH  TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = tanh($3); } */
/* | TOKEN_SQRT  TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = sqrt($3); } */
/* | TOKEN_CBRT  TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = cbrt($3); } */
/* | TOKEN_LOG2  TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = log2($3); } */
/* | TOKEN_LOG10 TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = log10($3); } */
/* | TOKEN_LN    TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = log($3); } */
/* | TOKEN_EXP   TOKEN_LPAREN constant TOKEN_RPAREN  { $$ = exp($3);} */
;

function
: TOKEN_FLOOR TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eFLOOR, NULL, $3); }
| TOKEN_CEIL  TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eCEIL,  NULL, $3); }
| TOKEN_ABS   TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eABS,   NULL, $3); }
| TOKEN_COS   TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eCOS,   NULL, $3); }
| TOKEN_SIN   TOKEN_LPAREN expr TOKEN_RPAREN 	             { $$ = createOperation(eSIN,   NULL, $3); }
| TOKEN_TAN   TOKEN_LPAREN expr TOKEN_RPAREN 	             { $$ = createOperation(eTAN,   NULL, $3); }
| TOKEN_COSH  TOKEN_LPAREN expr TOKEN_RPAREN 	             { $$ = createOperation(eCOSH,  NULL, $3); }
| TOKEN_SINH  TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eSINH,  NULL, $3); }
| TOKEN_TANH  TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eTANH,  NULL, $3); }
| TOKEN_SQRT  TOKEN_LPAREN expr TOKEN_RPAREN 	             { $$ = createOperation(eSQRT,  NULL, $3); }
| TOKEN_CBRT  TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eCBRT,  NULL, $3); }
| TOKEN_LOG2  TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eLOG2,  NULL, $3); }
| TOKEN_LOG10 TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eLOG10, NULL, $3); }
| TOKEN_LN    TOKEN_LPAREN expr TOKEN_RPAREN	             { $$ = createOperation(eLN,    NULL, $3); }
| TOKEN_EXP   TOKEN_LPAREN expr TOKEN_RPAREN		     { $$ = createOperation(eEXP,   NULL, $3); }
;

expr
: TOKEN_SUB expr                                             { $$ = createOperation(eSUB,  createNumber(0), $2);}
| variable
| function	 
| constant                                                   { $$ = createNumber($1);}
| expr[L] TOKEN_MULTIPLY expr[R]                             { $$ = createOperation(eMULTIPLY, $L, $R);}
| expr[L] TOKEN_DIV expr[R]                                  { $$ = createOperation(eDIV,  $L, $R);}
| TOKEN_LPAREN expr[E] TOKEN_RPAREN                          { $$ = $E;}
| expr[L] TOKEN_PLUS expr[R]                                 { $$ = createOperation(ePLUS, $L, $R);}
| expr[L] TOKEN_SUB expr[R]                                  { $$ = createOperation(eSUB,  $L, $R);}
| expr[L] TOKEN_MOD expr[R]                                  { $$ = createOperation(eMOD,  $L, $R);}
| expr[L] TOKEN_POW constant[R]                              { $$ = createOperation(ePOW,  $L, createNumber($R));}
| expr[L] TOKEN_POW variable[R]                              { $$ = createOperation(ePOW,  $L, $R);}
| expr[L] TOKEN_POW TOKEN_LPAREN expr[E] TOKEN_RPAREN        { $$ = createOperation(ePOW,  $L, $E);}
;

%%
