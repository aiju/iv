"use strict";

function
maxi(a, b)
{
	return a > b ? a : b;
}

function
lvalcheck(n, cont)
{
	function
	lvalcheck0(n, cont)
	{
		switch(n.t){
		case "sym":
			switch(n.sym.t){
			case "port":
				if(n.sym.dir == "input")
					error(n.lineno, "assignment to input port '" + n.sym + "'");
				if(n.sym.portreg && cont)
					error(n.lineno, "continuous assignment to reg '" + n.sym + "'");
				if(!n.sym.portreg && !cont)
					error(n.lineno, "procedural assignment to wire '" + n.sym + "'");
				break;
			default:
				error(n.lineno, "invalid lval " + n.sym.t);
			}
			break;
		default:
			error(n.lineno, "lvalcheck: unknown " + n.t);
		}
	}
	
	lvalcheck0(n, cont);
}

function
typecheck(n, ctxt)
{
	var i, t1, t2, s;

	switch(n.t){
	case "assign":
		typecheck(n.n2, ctxt);
		typecheck(n.n1, ctxt);
		lvalcheck(n.n1, true);
		break;
	case "bin":
		switch(n.op){
		case "==": case "!=": case "<": case "<=":
		case ">": case ">=": case "===": case "!==":
			typecheck(n.n1, null);
			typecheck(n.n2, null);
			break;
		default:
			typecheck(n.n1, ctxt);
			typecheck(n.n2, ctxt);
		}
		n.isconst = n.n1.isconst && n.n2.isconst;
		s = n.n1.sign && n.n2.sign;
		t1 = n.n1.type.t;
		t2 = n.n2.type.t;
		if(t1 == "mem" || t2 == "mem"){
			error(n.lineno, "memory in expression");
			n.type = bittype;
			return;
		}
		if(n.op == "or"){
			n.type = eventtype;
			return;
		}
		if(t1 == "event" || t2 == "event"){
			error(n.lineno, "event in expression");
			n.type = bittype;
			return;
		}
		if(t1 == "real" || t2 == "real"){
			switch(n.op){
			case "+": case "/": case "%": case "*": case "-": case "**":
				n.type = realtype;
				return;
			case "==": case "!=": case "<": case "<=": case ">": case ">=": case "&&": case "||":
				n.type = bittype;
				return;
			default:
				error(n.lineno, "real as operand to '" + n.op + "'");
				n.type = bittype;
				return;
			}
		}
		switch(n.op){
		case "+": case "/": case "%": case "*": case "-":
		case "&": case "|": case "~^": case "^":
			if(t1 == "unsz" || t2 == "unsz" || ctxt !== null && ctxt.t == "unsz")
				n.type = s ? sunsztype : unsztype;
			else if(ctxt !== null && (ctxt.t == "bits" || ctxt.t == "bitv" || ctxt.t == "bit"))
				n.type = new Type("bits", s, maxi(maxi(n.n1.type.sz, n.n2.type.sz), ctxt.sz));
			else
				n.type = new Type("bits", s, maxi(n.n1.type.sz, n.n2.type.sz));
			return;
		case "<<<": case ">>>": case "<<": case ">>": case "**":
			n.type = n.n1.type;
			return;
		case "==": case "!=": case "===": case "!==": case "<": case "<=": case ">": case ">=": case "&&": case "||":
			n.type = bittype;
			return;
		default:
			error(n.lineno, "typecheck: unknown op '" + n.op + "'");
		}
		break;
	case "un":
		typecheck(n.n1, ctxt);
		break;
	case "sym":
		n.type = n.sym.type;
		break;
	case "num":
		if(n.num.sz === null)
			n.type = new Type("unsz", n.num.sign, null, null, null);
		else
			n.type = new Type("bits", n.num.sign, n.num.sz, null, null);
		n.isconst = true;
		break;
	case "module":
		for(i = 0; i < n.stat.length; i++)
			typecheck(n.stat[i], null);
		break;
	default:
		error(n.lineno, "typecheck: unknown " + n.t);
	}
}
