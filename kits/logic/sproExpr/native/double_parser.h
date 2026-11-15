/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_DOUBLE_DOUBLE_PARSER_H_INCLUDED
# define YY_DOUBLE_DOUBLE_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef DOUBLE_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define DOUBLE_DEBUG 1
#  else
#   define DOUBLE_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define DOUBLE_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined DOUBLE_DEBUG */
#if DOUBLE_DEBUG
extern int double_debug;
#endif
/* "%code requires" blocks.  */
#line 30 "../util/double_parser.y" /* yacc.c:1909  */

#include <stdbool.h>
	
#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#line 61 "double_parser.h" /* yacc.c:1909  */

/* Token type.  */
#ifndef DOUBLE_TOKENTYPE
# define DOUBLE_TOKENTYPE
  enum double_tokentype
  {
    TOKEN_PLUS = 258,
    TOKEN_SUB = 259,
    TOKEN_MULTIPLY = 260,
    TOKEN_DIV = 261,
    L_BRACKET = 262,
    R_BRACKET = 263,
    TOKEN_LPAREN = 264,
    TOKEN_RPAREN = 265,
    TOKEN_MOD = 266,
    TOKEN_NUMBER = 267,
    TOKEN_PI = 268,
    TOKEN_E = 269,
    TOKEN_FLOOR = 270,
    TOKEN_CEIL = 271,
    TOKEN_ABS = 272,
    TOKEN_COS = 273,
    TOKEN_SIN = 274,
    TOKEN_TAN = 275,
    TOKEN_COSH = 276,
    TOKEN_SINH = 277,
    TOKEN_TANH = 278,
    TOKEN_POW = 279,
    TOKEN_SQRT = 280,
    TOKEN_CBRT = 281,
    TOKEN_LOG2 = 282,
    TOKEN_LOG10 = 283,
    TOKEN_LN = 284,
    TOKEN_EXP = 285,
    TOKEN_VAR = 286
  };
#endif

/* Value type.  */
#if ! defined DOUBLE_STYPE && ! defined DOUBLE_STYPE_IS_DECLARED

union DOUBLE_STYPE
{
#line 49 "../util/double_parser.y" /* yacc.c:1909  */

	double* var;
	double  value;
	SExpression *expression;

#line 111 "double_parser.h" /* yacc.c:1909  */
};

typedef union DOUBLE_STYPE DOUBLE_STYPE;
# define DOUBLE_STYPE_IS_TRIVIAL 1
# define DOUBLE_STYPE_IS_DECLARED 1
#endif



int double_parse (SExpression **expression, yyscan_t scanner);

#endif /* !YY_DOUBLE_DOUBLE_PARSER_H_INCLUDED  */
