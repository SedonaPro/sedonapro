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

/*
 * lexer.l file
 * To generate the lexical analyzer run: "flex lexer.l"
 */

#include <stdbool.h>
#include "expression.h"
#include "double_params.h"
#include "double_parser.h"
#include <math.h>
#include <stdio.h>


double double_variables[MAX_DOUBLE_PARAMS];

/* For compatability with bison 2.6 and later */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED && defined DOUBLE_STYPE_IS_DECLARED
#define YYSTYPE DOUBLE_STYPE
#endif

%}


%option prefix="double_"
%option bison-bridge
%option outfile="../native/double_lexer.c"
%option header-file="../native/double_lexer.h"
%option warn nodefault
/* The '%option debug' makes the generated scanner run in debug mode. */
/* %option debug */
%option caseless
%option reentrant noyywrap never-interactive nounistd



LPAREN      "("
RPAREN      ")"
PLUS        "+"
MULTIPLY    "*"
SUB         "-"
DIV         "/"
MOD         ("%")|("MOD")
POW         "^"
PI          "PI"
E           "E"
FLOOR       "FLOOR"
CEIL        "CEIL"
ABS         "ABS"
COS         "COS"
SIN         "SIN"
TAN         "TAN"
SINH        "SINH"
COSH        "COSH"
TANH        "TANH"
SQRT        "SQRT"
CBRT        "CBRT"
LOG2        "LOG2"
LOG10       "LOG10"
LN          "LN"
EXP         "EXP"
VAR         in[1-8]

NUMBER      [0-9]+(\.[0-9]+)?
WS          [ \r\n\t]*

%%

{WS}            { /* Skip blanks. */ }
{NUMBER}        { double n; sscanf(yytext, "%lf", &n); yylval->value = n; return TOKEN_NUMBER; }
{POW}           { return TOKEN_POW; }
{MULTIPLY}      { return TOKEN_MULTIPLY; }
{DIV}           { return TOKEN_DIV;    }
{MOD}           { return TOKEN_MOD;    }
{PLUS}          { return TOKEN_PLUS; }
{SUB}           { return TOKEN_SUB;    }
{LPAREN}        { return TOKEN_LPAREN; }
{RPAREN}        { return TOKEN_RPAREN; }
{PI}            { return TOKEN_PI; }
{E}             { return TOKEN_E; }
{FLOOR}         { return TOKEN_FLOOR; }
{CEIL}          { return TOKEN_CEIL; }
{ABS}           { return TOKEN_ABS; }
{COS}           { return TOKEN_COS; }
{SIN}           { return TOKEN_SIN; }
{TAN}           { return TOKEN_TAN; }
{SINH}          { return TOKEN_SINH; }
{COSH}          { return TOKEN_COSH; }
{TANH}          { return TOKEN_TANH; }
{SQRT}          { return TOKEN_SQRT; }
{CBRT}          { return TOKEN_CBRT; }
{LOG2}          { return TOKEN_LOG2; }
{LOG10}         { return TOKEN_LOG10; }
{LN}            { return TOKEN_LN; }
{EXP}           { return TOKEN_EXP; }
{VAR}           { yylval->var = &double_variables[atoi(yytext + 2)-1]; return TOKEN_VAR; }
.               { fprintf(stderr, "Unknown symbol: %s\n", yytext); yyterminate(); }
%%

int double_error(SExpression **expr, yyscan_t scanner, const char *msg) {
    fprintf(stderr, "Error:%s\n", msg);
    return 0;
}
