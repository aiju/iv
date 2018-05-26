"use strict";

var kwtab = {
	"always": true, "and": true, "assign": true, 
	"automatic": true, "begin": true, "buf": true, "bufif0": true, 
	"bufif1": true, "case": true, "casex": true, "casez": true, 
	"cell": true, "cmos": true, "config": true, "deassign": true, 
	"default": true, "defparam": true, "design": true, "disable": true, 
	"edge": true, "else": true, "end": true, "endcase": true, 
	"endconfig": true, "endfunction": true, "endgenerate": true, "endmodule": true, 
	"endprimitive": true, "endspecify": true, "endtable": true, "endtask": true, 
	"event": true, "for": true, "force": true, "forever": true, 
	"fork": true, "function": true, "generate": true, "genvar": true, 
	"highz0": true, "highz1": true, "if": true, "ifnone": true, 
	"incdir": true, "include": true, "initial": true, "inout": true, 
	"input": true, "instance": true, "integer": true, "join": true, 
	"large": true, "liblist": true, "library": true, "localparam": true, 
	"macromodule": true, "medium": true, "module": true, "nand": true, 
	"negedge": true, "nmos": true, "nor": true, "noshowcancelled": true, 
	"not": true, "notif0": true, "notif1": true, "or": true, 
	"output": true, "parameter": true, "pmos": true, "posedge": true, 
	"primitive": true, "pull0": true, "pull1": true, "pulldown": true, 
	"pullup": true, "pulsestyle_ondetect": true, "pulsestyle_onevent": true, "rcmos": true, 
	"real": true, "realtime": true, "reg": true, "release": true, 
	"repeat": true, "rnmos": true, "rpmos": true, "rtran": true, 
	"rtranif0": true, "rtranif1": true, "scalared": true, "showcancelled": true, 
	"signed": true, "small": true, "specify": true, "specparam": true, 
	"strong0": true, "strong1": true, "supply0": true, "supply1": true, 
	"table": true, "task": true, "time": true, "tran": true, 
	"tranif0": true, "tranif1": true, "tri": true, "tri0": true, 
	"tri1": true, "triand": true, "trior": true, "trireg": true, 
	"unsigned1": true, "use": true, "uwire": true, "vectored": true, 
	"wait": true, "wand": true, "weak0": true, "weak1": true, 
	"while": true, "wire": true, "wor": true, "xnor": true, 
	"xor": true
};

function
Type(t, sign, lo, hi, elem)
{
	this.t = t;
	this.sign = sign;
	if(t == "bitv"){
		this.lo = lo;
		this.hi = hi;
	}else{
		this.lo = null;
		this.hi = null;
	}
	if(t == "bitv")
		this.sz = new Node({t: "bin", op: "+", n1: new Node({t: "bin", op: "-", n1: this.hi, n2: this.lo}), n2: new Node({t: "cint", num: 1})});
	else if(t == "bits")
		this.sz = lo;
	else if(t == "bit")
		this.sz = 1;
	else
		this.sz = null;
	this.elem = elem;
}

var bittype = new Type("bit", false);
var sbittype = new Type("bit", true);
var eventtype = new Type("event", false);
var unsztype = new Type("unsz", false);
var sunsztype = new Type("unsz", true);

Type.prototype.toDebug = function()
{
	switch(this.t){
	case "bit":
	case "unsz":
		return this.t + (this.sign ? " signed" : "");
	case "bitv":
		return this.t + (this.sign ? " signed [" : " [") + this.hi + ":" + this.lo + "]";
	case "bits":
		return this.t + (this.sign ? " signed [" : " [") + this.sz + "]";
	}
}

function
Const(v, sz, sign)
{
	this.v = v;
	this.sz = sz;
	this.sign = sign;
}

Const.prototype.toString = function()
{
	var s, i;

	s = "";
	if(this.sz !== null)
		s += this.sz;
	if(this.sign)
		s += "'sb";
	else
		s += "'b";
	for(i = 0; i < this.v.length; i++)
		s += this.v.charAt(this.v.length - 1 - i);
	return s;
}

function
Node(attr)
{
	var i;

	for(i in attr)
		this[i] = attr[i];
	this.isconst = false;
}

Node.prototype.toString = function(ind, lvl)
{
	var s, i, t;

	if(ind === undefined) ind = "";
	if(lvl === undefined) lvl = 0;
	switch(this.t){
	case "module":
		s = ind + "module;\n";
		for(i = 0; i < this.stat.length; i++)
			s += this.stat[i].toString(ind + "\t") + "\n";
		s += ind + "endmodule";
		return s;
	case "sym":
		return this.sym.toString();
	case "bin":
		t = optab[this.op];
		s = this.n1.toString(ind, t.prec) + " " + this.op + " " + this.n2.toString(ind, t.prec + 1);
		if(optab[this.op].prec < lvl)
			s = "(" + s + ")";
		return s;
	case "concat":
		s = "{";
		for(i = 0; i < this.l.length; i++){
			if(i > 0) s += ", ";
			s += this.l[i].toString(ind, 0);
		}
		s += "}";
		return s;
	case "assign":
		return this.n1 + " = " + this.n2 + ";";
	case "num":
		return this.num.toString();
	case "cint":
		return this.num.toString();
	case "un":
		return this.op + this.n1;
	default:
		return this.t + "(...)";
	}
}

Node.prototype.toDebug = function(ind)
{
	var i, s, tab;

	tab = "  ";
	if(ind === undefined) ind = "";
	s = ind + this.t;
	if(this.type !== undefined)
		s += " [" + this.type.toDebug() + "]";
	if(this.isconst)
		s += " const";
	s += "\n";
	switch(this.t){
	case "module":
		for(i = 0; i < this.stat.length; i++)
			s += this.stat[i].toDebug(ind + tab);
		break;
	case "un": s += this.n1.toDebug(ind + tab); break;
	case "bin":
	case "assign":
		s += this.n1.toDebug(ind + tab);
		s += this.n2.toDebug(ind + tab);
		break;
	case "num":
	case "cint":
		s += ind + tab + this.num.toString() + "\n";
	}
	return s;
}

function
Symbol(name)
{
	this.name = name;
}

Symbol.prototype.toString = function()
{
	return this.name;
}

function
Scope(up, sym)
{
	this.up = up;
	this.sym = sym;
	this.tab = {};
}

function
Parser(s)
{
	this.str = s;
	this.idx = 0;
	this.lineno = 1;
	this.peektok = null;
	this.global = new Scope(null, null);
	this.scope = this.global;
	this.nextnum = null;
}

Parser.prototype.getsym = function(name)
{
	var sc;
	
	for(sc = this.scope; sc !== null; sc = sc.up)
		if(sc.tab[name] !== undefined && sc.tab[name].t !== undefined)
			return sc.tab[name];
	return this.scope.tab[name] = new Symbol(name);
}

Parser.prototype.newscope = function(sym)
{
	this.scope = new Scope(this.scope, sym);
}

Parser.prototype.scopeup = function()
{
	this.scope = this.scope.up;
}

function
error(line, str)
{
	if(line === null || line === undefined)
		line = this.lineno;
	throw "line " + line + ": " + str;
}

Parser.prototype.getc = function()
{
	var c;
	
	c = this.str.charAt(this.idx++);
	if(c == "\n") this.lineno++;
	return c;
}

Parser.prototype.ungetc = function()
{
	if(this.str.charAt(--this.idx) == "\n")
		this.lineno--;
}

function
isspace(s)
{
	return s == " " || s == "\t" || s == "\n";
}

function
isdigit(s)
{
	return s >= "0" && s <= "9";
}

function
isident(s)
{
	return s >= "A" && s <= "Z" || s >= "a" && s <= "z" || s >= "0" && s <= "9" || s == "_" || s == "$";
}

function
decconv(lno, s)
{
	var r, digs, p, i;

	r = "0";
	digs = "00000 10000 01000 11000 00100 10100 01100 11100 00010 10010";
	for(i = 0; i < s.length; i++){
		p = s.charAt(i);
		if(p < "0" || p > "9")
			error(lno, "invalid character '" + p + "' in decimal number");
		r = mpmul(r, "01010");
		r = mpadd(r, digs.substr((s.charCodeAt(i) - 48) * 6, 5));
	}
	return r;
}

function
numparse(lno, a, b, c)
{
	var r, p, i, digs;

	if(b == null)
		b = {base: 10, sign: true};
	switch(b.base){
	case 2:
		r = "";
		for(i = 0; i < c.s.length; i++){
			p = "01xXzZ?".indexOf(c.s[i]);
			if(p < 0)
				error(lno, "invalid character '" + c.s[i] + "' in binary number");
			r = "01xXzZ?"[p] + r;
		}
		if("01".indexOf(c.s[0]) >= 0)
			r = r + "0";
		break;
	case 8:
		digs = "000 100 010 110 001 101 110 111 xxx xxx zzz zzz zzz";
		r = "";
		for(i = 0; i < c.s.length; i++){
			p = "01234567xXzZ?".indexOf(c.s[i]);
			if(p < 0)
				error(lno, "invalid character '" + c.s[i] + "' in octal number");
			r = digs.substr(p * 4, 3) + r;
		}
		if("01234567".indexOf(c.s[0]) >= 0)
			r = r + "0";
		break;
	case 10:
		if(c.s.length == 1 && (i = "xXzZ?".indexOf(c.s)) >= 0)
			r = "xxzzz"[i];
		else
			r = decconv(lno, c.s);
		break;
	case 16:
		digs = "0000 1000 0100 1100 0010 1010 1100 1110 0001 1001 0101 1101 0011 1011 0111 1111 xxxx zzzz zzzz";
		r = "";
		for(i = 0; i < c.s.length; i++){
			p = "0123456789abcdefxz?".indexOf(c.s[i].toLowerCase());
			if(p < 0)
				error(lno, "invalid character '" + c.s[i] + "' in hexadecimal number");
			r = digs.substr(p * 5, 4) + r;
		}
		if("xz".indexOf(r[r.length - 1]) < 0)
			r = r + "0";
		break;
	
	default:
		error(lno, "unimplemented base " + b.base);
	}
	if(a !== null){
		p = parseInt(a.s, 10);
		return new Const(b.sign ? mpxtend(r, p) : mptrunc(r, p), p, b.sign);
	}
	return new Const(mpnorm(r), null, b.sign);
}

var longop = {
	"!=": "!=",
	"!==": "!==",
	"&&": "&&",
	"**": "**",
	"<<": "<<",
	"<<<": "<<<",
	"<=": "<=",
	"==": "==",
	"===": "===",
	">=": ">=",
	">>": ">>",
	">>>": ">>>",
	"^~": "~^",
	"||": "||",
	"~&": "~&",
	"~^": "~^",
	"~|": "~|"
};
var opchar = {
	"!": true,
	"&": true,
	"*": true,
	"<": true,
	"=": true,
	">": true,
	"^": true,
	"|": true,
	"~": true
};

Parser.prototype.lex = function()
{
	var c, s, t, i;
	
	if(this.peektok !== null){
		t = this.peektok;
		this.peektok = null;
		return t;
	}
	again: for(;;){
		do{
			c = this.getc();
		}while(isspace(c));
		if(c == "") return {t: "EOF"};
		if(c == "/"){
			c = this.getc();
			if(c == "/"){
				while(c = this.getc(), c != "" && c != "\n");
					;
				continue again;
			}else if(c == "*"){
				do{
					while(c = this.getc(), c != "" && c != "*")
						;
					while(c == "*")
						c = this.getc();
				}while(c != "" && c != "/");
				continue again;
			}else{
				this.ungetc();
				return {t: "/"};
			}
		}
		if(c == "\""){
			s = "";
			while(c = this.getc(), c != "\""){
				if(c == "\n")
					error(this.lineno, "nl in string");
				if(c == "")
					error(this.lineno, "eof in string");
				if(c == "\\")
					switch(c = this.getc()){
					case "n": s += "\n"; break;
					case "t": s += "\t"; break;
					default:
						if(isdigit(c)){
							t = "";
							i = 0;
							do
								t += c;
							while(++i < 3 && (c = this.getc(), isdigit(c)));
							if(i < 3){
								this.ungetc();
								s += "\\" + t;
							}else
								s += String.fromCharCode(parseInt(t, 8));
						}else
							s += c;
					}
				else
					s += c;
			}
			return {t: "str", s: s};
		}
		if(c == "\\"){
			s = "";
			while(c = this.getc(), c != "" && !isspace(c))
				s += c;
			return {t: "id", s: this.getsym(s)};
		}
		if(isdigit(c) || this.nextnum && (isident(c) || c == "?")){
			s = c;
			if(s == "_") s = "";
			while(c = this.getc(), isident(c) || c == "?")
				if(c != "_")
					s += c;
			this.ungetc();
			this.nextnum = null;
			return {t: "num", s: s};
		}
		if(isident(c)){
			s = c;
			while(c = this.getc(), isident(c))
				s += c;
			this.ungetc();
			if(kwtab[s])
				return {t: "kw:" + s};
			return {t: "id", s: this.getsym(s)};
		}
		if(c == "'"){
			s = c;
			c = this.getc();
			if(c == "s" || c == "S"){
				s += c;
				c = this.getc();
			}
			if("bodhBODH".indexOf(c) >= 0)
				s += c;
			else
				this.ungetc();
			return this.nextnum = {t: "base", base: {"b": 2, "B": 2, "o": 8, "O": 8, "d": 10, "D": 10, "h": 16, "H": 16}[c], sign: s[1] == "s" || s[1] == "S"};
		}
		if(opchar[c]){
			s = c;
			while(c = this.getc(), c != "" && longop[s + c] !== undefined)
				s += c;
			this.ungetc();
			return {t: longop[s] || s};
		}
		return {t: c};
	}
}

Parser.prototype.superman = function(t)
{
	if(this.peektok !== null)
		throw "This is not Superman IV";
	this.peektok = t;
}

Parser.prototype.peek = function()
{
	if(this.peektok !== null)
		return this.peektok;
	return this.peektok = this.lex();
}

Parser.prototype.expect = function(tt)
{
	var t;
	
	t = this.lex();
	if(t.t !== tt)
		error(this.lineno, "expected " + tt + ", got " + t.t);
	return t;
}

Parser.prototype.expectany = function(tt)
{
	var t, s, first, i;
	
	t = this.lex();
	if(tt[t.t] !== true){
		s = "expected ";
		first = true;
		for(i in tt){
			if(!first) s += " or ";
			else first = false;
			s += i;
		}
		error(this.lineno, "expected " + s + ", got " + t.t);
	}
	return t;
}

Parser.prototype.got = function(tt)
{
	if(this.peek().t === tt)
		return this.lex();
	return null;
}

Parser.prototype.gotany = function(tt)
{
	if(tt[this.peek().t] !== null)
		return this.lex();
	return null;
}

Parser.prototype.p_attributes = function()
{
}

Parser.prototype.p_range = function()
{
	var a, b;

	this.expect("[");
	a = this.p_expr();
	this.expect(":");
	b = this.p_expr();
	this.expect("]");
	return [a, b];
}

Parser.prototype.p_port_declaration = function()
{
	var dir, wire, signed, type, range, sym;

	this.p_attributes();
	dir = this.expectany({"kw:input": true, "kw:output": true, "kw:inout": true}).t.substr(3);
	wire = this.expectany({"kw:wire": true, "kw:reg": true}).t == "kw:wire";
	signed = this.got("kw:signed") !== null;
	if(this.peek().t === "[")
		range = this.p_range();
	else
		range = null;
	sym = this.expect("id").s;
	
	sym.t = "port";
	sym.dir = dir;
	sym.portreg = !wire;
	if(range === null)
		sym.type = signed ? sbittype : bittype;
	else
		sym.type = new Type("bitv", signed, range[1], range[0], null);
}

Parser.prototype.p_delay3 = function()
{
	if(this.got("#") !== null){
		this.expect("num");
	}
}

Parser.prototype.p_hier_identifier = function()
{
	var s, n;
	
	s = this.expect("id").s;
	if(s.t === undefined)
		error(this.lineno, "undeclared '" + s + "'");
	n = new Node({t: "sym", sym: s, lineno: this.lineno});
/*	while(this.got(".") !== null)
		n = new Node({t: "hier", n1: n, n2: this.expect("id").s});*/
	
	return n;
}

Parser.prototype.p_lvalue = function()
{
	var n, l, a;
	
	if(this.got("{") !== null){
		l = [this.p_lvalue()];
		while(this.got(",") !== null)
			l.push(this.p_lvalue());
		this.expect("}");
		return new Node({t: "concat", l: l, lineno: this.lineno});
	}else{
		n = this.p_hier_identifier();
		while(this.got("[") !== null){
			a = this.p_expr();
			if(this.got(":") !== null)
				n = new Node({t: "idx", n1: n, n2: a, n3: this.p_expr(), lineno: this.lineno});
			else
				n = new Node({t: "idx", n1: n, n2: a, n3: null, lineno: this.lineno});
			this.expect("]");
		}
		return n;
	}
}

var untab = {
	"+": true,
	"-": true,
	"!": true,
	"~": true,
	"&": true,
	"~&": true,
	"|": true,
	"~|": true,
	"^": true,
	"~^": true
};

Parser.prototype.p_primary = function()
{
	var t, n, a, b, c;

	switch(t = this.lex(), t.t){
	case "id":
		this.superman(t);
		return this.p_hier_identifier();
	case "num":
		b = this.got("base");
		if(b !== null){
			a = t;
			c = this.expect("num");
		}else{
			a = null;
			c = t;
		}
		n = numparse(this.lineno, null, b, c);
		if(n.sign && n.v.length < 32 && n.v.indexOf("x") < 0 && n.v.indexOf("z") < 0)
			return new Node({t: "cint", num: mptoi(n.v), lineno: this.lineno});
		return new Node({t: "num", num: n, lineno: this.lineno});
	case "base":
		b = t;
		c = this.expect("num");
		n = numparse(this.lineno, null, b, c);
		if(n.sign && n.v.length < 32 && n.v.indexOf("x") < 0 && n.v.indexOf("z") < 0)
			return new Node({t: "cint", num: mptoi(n.v), lineno: this.lineno});
		return new Node({t: "num", num: n, lineno: this.lineno});
	case "(":
		n = this.p_expr();
		this.expect(")");
		return n;
	default:
		if(untab[t.t] !== undefined){
			this.p_attributes();
			return new Node({t: "un", op: t.t, n1: this.p_primary(), lineno: this.lineno});
		}
		error(this.lineno, "unexpected " + t.t);
	}
}

var optab = {
	"**": {prec: 11},
	"*": {prec: 10},
	"/": {prec: 10},
	"%": {prec: 10},
	"+": {prec: 9},
	"-": {prec: 9},
	"<<": {prec: 8},
	">>": {prec: 8},
	"<<<": {prec: 8},
	">>>": {prec: 8},
	"<": {prec: 7},
	"<=": {prec: 7},
	">": {prec: 7},
	">=": {prec: 7},
	"==": {prec: 6},
	"!=": {prec: 6},
	"===": {prec: 6},
	"!==": {prec: 6},
	"&": {prec: 5},
	"^": {prec: 4},
	"~^": {prec: 4},
	"|": {prec: 3},
	"&&": {prec: 2},
	"||": {prec: 1},
	"max": {prec: 0},
};

Parser.prototype.p_expr_core = function()
{
	var ex, op, t, a;
	
	ex = [this.p_primary()];
	op = [];
	while(optab[t = this.peek().t] !== undefined){
		while(op.length > 0 && optab[op[0]].prec >= optab[t].prec){
			a = ex.shift();
			ex[0] = new Node({t: "bin", op: op.shift(), n1: ex[0], n2: a, lineno: this.lineno});
		}
		this.lex();
		op.unshift(t);
		this.p_attributes();
		ex.unshift(this.p_primary());
	}
	while(op.length > 0){
		a = ex.shift();
		ex[0] = new Node({t: "bin", op: op.shift(), n1: ex[0], n2: a, lineno: this.lineno});
	}
	return ex[0];
}

Parser.prototype.p_expr = function()
{
	var p, q, r, l;
	
	if(this.got("{") !== null){
		p = this.p_expr();
		if(this.got("{") !== null){
			q = this.p_expr();
			this.expect("}");
			this.expect("}");
			return new Node({t: "repl", n1: p, n2: q, lineno: this.lineno});
		}else{
			l = [p];
			while(this.got(",") !== null)
				l.push(this.p_expr());
			while(this.got(",") !== null);
			this.expect("}");
			return new Node({t: "concat", l: l, lineno: this.lineno});
		}
	}else{
		p = this.p_expr_core();
		if(this.got("?")){
			q = this.p_expr();
			this.expect(":");
			r = this.p_expr();
			return new Node({t: "tern", n1: p, n2: q, n3: r, lineno: this.lineno});
		}else
			return p;
	}
	
}

Parser.prototype.p_assignment = function()
{
	var l, r;
	
	l = this.p_lvalue();
	this.expect("=");
	r = this.p_expr();
	return new Node({t: "assign", n1: l, n2: r, lineno: this.lineno});
}

Parser.prototype.p_module_item = function()
{
	var t;
	var r;
	
	r = [];
	switch(t = this.lex(), t.t){
	case "kw:assign":
		this.p_delay3();
		r.push(this.p_assignment());
		while(this.got(",") !== null)
			r.push(this.p_assignment());
		this.expect(";");
		break;
	default:
		error(this.lineno, "unexpected " + t.t);
	}
	return r;
}

Parser.prototype.p_module = function()
{
	var r, n, lno;

	this.p_attributes();
	this.expect("kw:module");
	lno = this.lineno;
	n = this.expect("id");
	this.newscope(n);
	try{
		if(this.got("(") !== null && this.got(")") === null){
			this.p_port_declaration();
			while(this.got(",") !== null)
				this.p_port_declaration();
			this.expect(")");
		}
		this.expect(";");
		r = [];
		while(this.got("kw:endmodule") === null)
			r.push.apply(r, this.p_module_item());
		n = new Node({t: "module", sym: n, stat: r, lineno: lno});
	}finally{
		this.scopeup(n);
	}
	return n;
}

function parse()
{
	var p, s, m;
	
	s = document.getElementById("code").value;
	localStorage.setItem("text", s);
	p = new Parser(s);
	try{
		m = p.p_module();
		typecheck(m);
		document.getElementById("out").innerHTML = m.toDebug();
	}catch(e){
		document.getElementById("out").innerHTML = e;
	}
}
