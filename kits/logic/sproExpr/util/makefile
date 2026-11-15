# Makefile

FILES   =  expression.c main.c graph.c
FILES  += double_test.c double_lexer.c double_parser.c double_eval.c
FILES  += bool_test.c bool_lexer.c bool_parser.c bool_eval.c

CC	= gcc

VPATH   = ../native

CFLAGS	= -g -lm -I${VPATH} -I.
CFLAGS += -DYYDEBUG=1
# CFLAGS += -DFLEX_DEBUG

all: check

test: $(FILES)
	$(CC) $(CFLAGS) $^ -o $@

../native/double_lexer.c:	double_lexer.l
	flex $<

../native/double_parser.c: double_parser.y
	cd ../native && bison ../util/$<


../native/bool_lexer.c:	bool_lexer.l
	flex $<

../native/bool_parser.c: bool_parser.y
	cd ../native && bison ../util/$<


images:
	find . -iname '*.dot' -exec /bin/bash -c "dot -Tpng < {} > {}.png" \;

clean-gen-parse: double_parser.c double_parser.h bool_parser.c bool_parser.h
	rm -f $^

clean-gen-lex: double_lexer.c double_lexer.h bool_lexer.c bool_lexer.h
	rm -f $^

clean: clean-gen-lex clean-gen-parse
	rm -f *.o *.dot *.png *~   test

check: test
	./test
