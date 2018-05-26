</$objtype/mkfile

BIN=/$objtype/bin
TARG=ratjs
OFILES=ratjs.$O

RJ=\
	arith.rj\
	parse.rj\
	ast.rj\

all: out.js

</sys/src/cmd/mkone

out.js:D: $RJ $O.out
	$O.out $RJ > out.js
