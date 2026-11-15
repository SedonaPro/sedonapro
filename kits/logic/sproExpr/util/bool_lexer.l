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
#include "bool_params.h"
#include "bool_parser.h"
#include <math.h>
#include <stdio.h>


int bool_variables[MAX_BOOL_PARAMS];

/* For compatability with bison 2.6 and later */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED && defined BOOL_STYPE_IS_DECLARED
#define YYSTYPE BOOL_STYPE
#endif

%}


%option prefix="bool_"
%option bison-bridge
%option outfile="../native/bool_lexer.c"
%option header-file="../native/bool_lexer.h"
%option warn nodefault
/* The '%option debug' makes the generated scanner run in debug mode. */
/* %option debug */
%option caseless
%option reentrant noyywrap never-interactive nounistd



LPAREN      "("
RPAREN      ")"
NOT         ("!")|("NOT")
OR          ("+")|("OR")|("||")
AND         ("*")|("AND")|("&&")
XOR         ("^")|("XOR")
NULL        ("NULL")|("2")
TRUE        ("TRUE")|("1")
FALSE       ("FALSE")|("0")
VAR         in[1-8]

WS          [ \r\n\t]*

%%

{WS}            { /* Skip blanks. */ }
{NOT}           { return TOKEN_NOT; }
{OR}            { return TOKEN_OR; }
{AND}           { return TOKEN_AND;    }
{XOR}           { return TOKEN_XOR;    }
{NULL}          { return TOKEN_NULL; }
{FALSE}         { return TOKEN_FALSE;    }
{TRUE}          { return TOKEN_TRUE;    }
{LPAREN}        { return TOKEN_LPAREN; }
{RPAREN}        { return TOKEN_RPAREN; }
{VAR}           { yylval->var = &bool_variables[atoi(yytext + 2)-1]; return TOKEN_VAR; }
.               { fprintf(stderr, "Unknown symbol: %s\n", yytext); yyterminate(); }
%%

int bool_error(SExpression **expr, yyscan_t scanner, const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    return 0;
}
