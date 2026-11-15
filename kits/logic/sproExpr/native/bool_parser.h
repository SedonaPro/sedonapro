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

#ifndef YY_BOOL_BOOL_PARSER_H_INCLUDED
# define YY_BOOL_BOOL_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef BOOL_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define BOOL_DEBUG 1
#  else
#   define BOOL_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define BOOL_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined BOOL_DEBUG */
#if BOOL_DEBUG
extern int bool_debug;
#endif
/* "%code requires" blocks.  */
#line 30 "../util/bool_parser.y" /* yacc.c:1909  */

#include <stdbool.h>

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#line 61 "bool_parser.h" /* yacc.c:1909  */

/* Token type.  */
#ifndef BOOL_TOKENTYPE
# define BOOL_TOKENTYPE
  enum bool_tokentype
  {
    TOKEN_OR = 258,
    TOKEN_AND = 259,
    TOKEN_XOR = 260,
    L_BRACKET = 261,
    R_BRACKET = 262,
    TOKEN_LPAREN = 263,
    TOKEN_RPAREN = 264,
    TOKEN_NOT = 265,
    TOKEN_FALSE = 266,
    TOKEN_TRUE = 267,
    TOKEN_NULL = 268,
    TOKEN_VAR = 269
  };
#endif

/* Value type.  */
#if ! defined BOOL_STYPE && ! defined BOOL_STYPE_IS_DECLARED

union BOOL_STYPE
{
#line 49 "../util/bool_parser.y" /* yacc.c:1909  */

	int* var;
	int  value;
	SExpression *expression;

#line 94 "bool_parser.h" /* yacc.c:1909  */
};

typedef union BOOL_STYPE BOOL_STYPE;
# define BOOL_STYPE_IS_TRIVIAL 1
# define BOOL_STYPE_IS_DECLARED 1
#endif



int bool_parse (SExpression **expression, yyscan_t scanner);

#endif /* !YY_BOOL_BOOL_PARSER_H_INCLUDED  */
