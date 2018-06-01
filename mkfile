</$objtype/mkfile

BIN=/$objtype/bin
TARG=ratjs
OFILES=ratjs.$O

RJ=\
	u.rj \
	dat.rj \
	arith.rj\
	parse.rj\
	ast.rj \
	cfold.rj \
	comp.rj \
	wave.rj \

all: out.js

</sys/src/cmd/mkone

out.js:D: $RJ $O.out
	$O.out $RJ > out.js
