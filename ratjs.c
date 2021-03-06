#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>

typedef struct Line Line;
typedef struct Node Node;
typedef struct NodeList NodeList;
typedef struct Symbol Symbol;
typedef struct SymTab SymTab;
typedef struct Trie Trie;
typedef struct TrieHead TrieHead;
typedef struct Type Type;

struct SymTab {
	Trie *root;
	SymTab *up;
	Symbol *list;
	Symbol **last;
};
SymTab globals;
SymTab *scope = &globals;

struct NodeList {
	int n;
	Node **a;
};
NodeList ZL;

struct Type {
	enum {
		TYPVAR,
		TYPINT,
		TYPSTRING,
		TYPBOOL,
		TYPFUNC,
		TYPSTRUCT,
		TYPARRAY,
		TYPMAP,
		TYPVOID,
		TYPENUM,
	} t;
	Type *ret, *idx;
	NodeList pl;
	SymTab st;
	char *label;
	int minarg;
	u8int incomplete;
};

Type vartype, inttype, stringtype, booltype;

struct Line {
	char *filen;
	int lineno;
};

struct TrieHead {
	uvlong hash;
	int l;
};

struct Symbol {
	TrieHead;
	enum {
		SYMNONE,
		SYMVAR,
		SYMLABEL,
		SYMTYPE,
		SYMFUNC,
		SYMDUMMY,
		SYMENUMV,
	} t;
	char *name;
	Node *def;
	Type *type;
	Line;
	Symbol *next;
	SymTab *st;
};

enum { TRIEB = 4 };
struct Trie {
	TrieHead;
	Trie *n[1<<TRIEB];
};

struct Node {
	enum {
		ASTINVAL,
		ASTSYM,
		ASTOP,
		ASTASS,
		ASTTERN,
		ASTFUNC,
		ASTRETURN,
		ASTIF,
		ASTWHILE,
		ASTDOWHILE,
		ASTFOR,
		ASTBLOCK,
		ASTLABEL,
		ASTBREAK,
		ASTCONTINUE,
		ASTDECL,
		ASTDECLS,
		ASTARRAY,
		ASTOBJECT,
		ASTOBJELEM,
		ASTUN,
		ASTINDE,
		ASTDOT,
		ASTCALL,
		ASTPARAM,
		ASTNUM,
		ASTSTR,
		ASTTHIS,
		ASTNEW,
		ASTSWITCH,
		ASTCASE,
		ASTDEFAULT,
		ASTIDX,
		ASTTHROW,
		ASTFORIN,
		ASTTRY,
		ASTCAST,
	} t;
	enum {
		OPINVAL,
		OPADD,
		OPAND,
		OPASR,
		OPCOMMA,
		OPDIV,
		OPEQ,
		OPEQS,
		OPEXP,
		OPGE,
		OPGT,
		OPLAND,
		OPLE,
		OPLOR,
		OPLSH,
		OPLT,
		OPMOD,
		OPMUL,
		OPNEQ,
		OPNEQS,
		OPOR,
		OPRSH,
		OPSUB,
		OPXOR,
		OPUPLUS,
		OPUMINUS,
		OPLNOT,
		OPCOM,
		OPIN,
		OPINSTANCEOF,
	} op;
	enum {
		ASTFUNDECL = 1,
	} flags;
	Node *n1, *n2, *n3, *n4;
	NodeList nl;
	char *str;
	Symbol *sym;
	Type *par;
	SymTab *st;
	int i;
	Type *type;
	Type *typearg;
	Line;
};

#pragma varargck type "α" int
int
astfmt(Fmt *f)
{
	int n;
	static char *asttab[] = {
		[ASTINVAL] "ASTINVAL",
		[ASTSYM] "ASTSYM",
		[ASTOP] "ASTOP",
		[ASTASS] "ASTASS",
		[ASTTERN] "ASTTERN",
		[ASTFUNC] "ASTFUNC",
		[ASTRETURN] "ASTRETURN",
		[ASTIF] "ASTIF",
		[ASTWHILE] "ASTWHILE",
		[ASTDOWHILE] "ASTDOWHILE",
		[ASTFOR] "ASTFOR",
		[ASTBLOCK] "ASTBLOCK",
		[ASTLABEL] "ASTLABEL",
		[ASTBREAK] "ASTBREAK",
		[ASTCONTINUE] "ASTCONTINUE",
		[ASTDECL] "ASTDECL",
		[ASTDECLS] "ASTDECLS",
		[ASTARRAY] "ASTARRAY",
		[ASTOBJECT] "ASTOBJECT",
		[ASTOBJELEM] "ASTOBJELEM",
		[ASTUN] "ASTUN",
		[ASTINDE] "ASTINDE",
		[ASTDOT] "ASTDOT",
		[ASTCALL] "ASTCALL",
		[ASTPARAM] "ASTPARAM",
		[ASTNUM] "ASTNUM",
		[ASTSTR] "ASTSTR",
		[ASTTHIS] "ASTTHIS",
		[ASTNEW] "ASTNEW",
		[ASTSWITCH] "ASTSWITCH",
		[ASTCASE] "ASTCASE",
		[ASTDEFAULT] "ASTDEFAULT",
		[ASTIDX] "ASTIDX",
		[ASTFORIN] "ASTFORIN",
		[ASTTRY] "ASTTRY",
		[ASTTHROW] "ASTTHROW",
		[ASTCAST] "ASTCAST",
	};
	
	n = va_arg(f->args, int);
	if((uint)n >= nelem(asttab) || asttab[n] == nil)
		return fmtprint(f, "??? (%d)", n);
	return fmtprint(f, "%s", asttab[n]);
}

int
symtfmt(Fmt *f)
{
	int n;
	static char *symttab[] = {
		[SYMNONE] "undeclared",
		[SYMVAR] "variable",
		[SYMLABEL] "label",
		[SYMTYPE] "type",
		[SYMFUNC] "function",
		[SYMDUMMY] "dummy variable",
		[SYMENUMV] "enum value",
	};
	
	n = va_arg(f->args, int);
	if((uint)n >= nelem(symttab) || symttab[n] == nil)
		return fmtprint(f, "??? (%d)", n);
	return fmtprint(f, "%s", symttab[n]);
}

Line curline;
int fail;
Biobuf *bin;

void *
emalloc(int n)
{
	void *v;
	
	v = malloc(n);
	if(v == nil)
		sysfatal("malloc: %r");
	memset(v, 0, n);
	setmalloctag(v, getcallerpc(&n));
	return v;
}

void *
error(Line *l, char *fmt, ...)
{
	va_list va;
	Fmt f;
	char buf[256];
	
	if(l == nil) l = &curline;
	fmtfdinit(&f, 2, buf, sizeof(buf));
	va_start(va, fmt);
	fmtprint(&f, "%s:%d ", l->filen, l->lineno);
	fmtvprint(&f, fmt, va);
	fmtrune(&f, '\n');
	fmtfdflush(&f);
	va_end(va);
	if(++fail >= 20)
		sysfatal("too many errors");
	return nil;
}

u64int
hash(char *s)
{
	u64int h;
	
	h = 0xcbf29ce484222325ULL;
	for(; *s != 0; s++){
		h ^= *s;
		h *= 0x100000001b3ULL;
	}
	return h;
}

int
clz(uvlong v)
{
	int n;
	
	n = 0;
	if(v >> 32 == 0) {n += 32; v <<= 32;}
	if(v >> 48 == 0) {n += 16; v <<= 16;}
	if(v >> 56 == 0) {n += 8; v <<= 8;}
	if(v >> 60 == 0) {n += 4; v <<= 4;}
	if(v >> 62 == 0) {n += 2; v <<= 2;}
	if(v >> 63 == 0) {n += 1; v <<= 1;}
	return n;
}

Symbol *
trieget(Trie **tp, uvlong hash, int new)
{
	Trie *t, *s;
	uvlong d;
	
	for(;;){
		t = *tp;
		if(t == nil){
			if(!new)
				return nil;
			t = emalloc(sizeof(Symbol));
			t->hash = hash;
			t->l = 64;
			*tp = t;
			return (Symbol *) t;
		}
		d = (hash ^ t->hash) & -(1ULL<<64 - t->l);
		if(d == 0 || t->l == 0){
			if(t->l == 64)
				return (Symbol *) t;
			tp = &t->n[hash << t->l >> 64 - TRIEB];
		}else{
			s = emalloc(sizeof(Trie));
			s->hash = hash;
			s->l = clz(d) & -TRIEB;
			s->n[t->hash << s->l >> 64 - TRIEB] = t;
			*tp = s;
			tp = &s->n[hash << s->l >> 64 - TRIEB];
		}
	}
}

Symbol *
getsym(SymTab *st, int hier, char *name)
{
	uvlong h, h0;
	Symbol *s;
	SymTab *sp;
	
	h0 = hash(name);
	if(hier)
		for(sp = st; sp != nil ; sp = sp->up){
			for(h = h0; s = trieget(&sp->root, h, 0), s != nil && strcmp(s->name, name) != 0; h++)
				;
			if(s != nil && s->t != SYMNONE)
				return s;
		}
	for(h = h0; s = trieget(&st->root, h, 1), s->name != nil && strcmp(s->name, name) != 0; h++)
		;
	if(s->name == nil){
		s->name = strdup(name);
		s->st = st;
		*st->last = s;
		st->last = &s->next;
	}
	return s;
}

void
checksym(Node *n)
{
	if(n->sym->type == SYMNONE)
		n->flags |= ASTFUNDECL;
}

Symbol *
fixsym(Symbol *s)
{
	if(s == nil) return nil;
	return getsym(s->st, 1, s->name);
}

Node *
node(Line *l, int t, ...)
{
	Node *n;
	va_list va;

	n = emalloc(sizeof(Node));
	if(l != nil)
		n->Line = *l;
	else
		n->Line = curline;
	n->t = t;
	va_start(va, t);
	switch(t){
	case ASTSYM:
	case ASTBREAK:
	case ASTCONTINUE:
	case ASTDECL:
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTLABEL:
		n->sym = va_arg(va, Symbol *);
		n->n1 = va_arg(va, Node *);
		break;
	case ASTOP:
	case ASTASS:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, Node *);
		n->n2 = va_arg(va, Node *);
		break;
	case ASTUN:
		n->op = va_arg(va, int);
		n->n1 = va_arg(va, Node *);
		break;
	case ASTTERN:
		n->n1 = va_arg(va, Node *);
		n->n2 = va_arg(va, Node *);
		n->n3 = va_arg(va, Node *);
		break;
	case ASTFUNC:
		break;
	case ASTRETURN:
		n->n1 = va_arg(va, Node *);
		break;
	case ASTIF:
	case ASTFORIN:
		n->n1 = va_arg(va, Node *);
		n->n2 = va_arg(va, Node *);
		n->n3 = va_arg(va, Node *);
		break;
	case ASTFOR:
		n->n1 = va_arg(va, Node *);
		n->n2 = va_arg(va, Node *);
		n->n3 = va_arg(va, Node *);
		n->n4 = va_arg(va, Node *);
		break;
	case ASTWHILE:
	case ASTDOWHILE:
	case ASTIDX:
		n->n1 = va_arg(va, Node *);
		n->n2 = va_arg(va, Node *);
		break;
	case ASTDECLS:
	case ASTARRAY:
		n->nl = va_arg(va, NodeList);
		break;
	case ASTOBJELEM:
		n->str = va_arg(va, char *);
		n->n1 = va_arg(va, Node *);
		break;
	case ASTBLOCK:
		break;
	case ASTINDE:
		n->i = va_arg(va, int);
		n->n1 = va_arg(va, Node *);
		break;
	case ASTDOT:
		n->n1 = va_arg(va, Node *);
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTCALL:
		n->n1 = va_arg(va, Node *);
		n->nl = va_arg(va, NodeList);
		break;
	case ASTPARAM:
		n->str = va_arg(va, char *);
		n->type = va_arg(va, Type *);
		break;
	case ASTNUM:
	case ASTSTR:
		n->str = va_arg(va, char *);
		break;
	case ASTNEW:
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTOBJECT:
		n->typearg = va_arg(va, Type *);
		break;
	case ASTTHIS:
	case ASTDEFAULT:
		break;
	case ASTSWITCH:
	case ASTCASE:
	case ASTTHROW:
		n->n1 = va_arg(va, Node *);
		break;
	case ASTTRY:
		n->n1 = va_arg(va, Node *);
		n->n2 = va_arg(va, Node *);
		n->n3 = va_arg(va, Node *);
		n->sym = va_arg(va, Symbol *);
		break;
	case ASTCAST:
		n->typearg = va_arg(va, Type *);
		n->n1 = va_arg(va, Node *);
		break;
	default:
		sysfatal("node: unimplemented %α", t);
	}
	va_end(va);
	return n;
}

Type *
type(int ty, ...)
{
	Type *t;
	va_list va;
	
	switch(ty){
	case TYPVAR: return &vartype;
	case TYPSTRING: return &stringtype;
	case TYPINT: return &inttype;
	case TYPBOOL: return &booltype;
	}
	t = emalloc(sizeof(Type));
	t->t = ty;
	t->st.last = &t->st.list;
	va_start(va, ty);
	switch(ty){
	case TYPFUNC:
		t->ret = va_arg(va, Type *);
		break;
	case TYPARRAY:
		t->ret = va_arg(va, Type *);
		break;
	case TYPMAP:
		t->idx = va_arg(va, Type *);
		t->ret = va_arg(va, Type *);
		break;
	}
	va_end(va);
	return t;
}

#pragma varargck type "τ" Type *
void
enumprint(Fmt *f, Type *t)
{
	Symbol *s;
	int room, first;
	
	room = 16;
	first = 1;
	fmtprint(f, "enum {");
	for(s = t->st.list; s != nil; s = s->next){
		if(strlen(s->name) + (first ? 0 : 2) > room)
			break;
		fmtprint(f, "%s%s", first ? "" : ", ", s->name);
		room -= strlen(s->name) + (first ? 0 : 2);
		first = 0;
	}
	if(s != nil)
		fmtprint(f, "%s...", first ? "" : ", ");
	fmtprint(f, "}");
}

int
typefmt(Fmt *f)
{
	Type *t;
	int i;
	int w;
	
	t = va_arg(f->args, Type *);
	w = f->width;
	if(t == nil){
		fmtprint(f, "<nil>");
		return 0;
	}
	if(t->label != nil){
		if((w & 1) == 0) fmtprint(f, "%s", t->label);
		return 0;
	}
	switch(t->t){
	case TYPVAR:
		if((w & 1) == 0) fmtprint(f, "var");
		return 0;
	case TYPINT:
		if((w & 1) == 0) fmtprint(f, "int");
		return 0;
	case TYPSTRING:
		if((w & 1) == 0) fmtprint(f, "string");
		return 0;
	case TYPBOOL:
		if((w & 1) == 0) fmtprint(f, "bool");
		return 0;
	case TYPVOID:
		if((w & 1) == 0) fmtprint(f, "void");
		return 0;
	case TYPSTRUCT:
		if((w & 1) == 0) fmtprint(f, "struct {...}");
		return 0;
	case TYPFUNC:
		if((w & 1) == 0) fmtprint(f, "%2τ", t->ret);
		if((w & 2) == 0){
			fmtrune(f, '(');
			for(i = 0; i < t->pl.n; i++){
				assert(t->pl.a[i]->t == ASTPARAM);
				fmtprint(f, "%s%τ", i>0?",":"", t->pl.a[i]->type);
			}
			fmtprint(f, ")%1τ", t->ret);
		}
		return 0;
	case TYPARRAY:
		if((w & 1) == 0) fmtprint(f, "%2τ", t->ret);
		if((w & 2) == 0) fmtprint(f, "[]%1τ", t->ret);
		return 0;
	case TYPMAP:
		if((w & 1) == 0) fmtprint(f, "%2τ", t->ret);
		if((w & 2) == 0) fmtprint(f, "[%τ]%1τ", t->idx, t->ret);
		return 0;
	case TYPENUM:
		if((w & 1) == 0) enumprint(f, t);
		return 0;
	default:
		if((w & 1) == 0) fmtprint(f, "??? (%d)", t->t);
		return 0;
	}
}

Symbol *
decl(SymTab *st, int t, Symbol *s, Type *type, Node *def)
{
	if(s == nil) return nil;
	s = getsym(st, 0, s->name);
	if(s->t != SYMNONE)
		error(nil, "'%s' redeclared as %σ", s->name, t);
	s->t = t;
	s->type = type;
	s->Line = curline;
	s->def = def;
	return s;
}

Node *
newscope(int t, Symbol *sym)
{
	Node *n;
	SymTab *st;
	
	n = node(nil, t);
	n->sym = sym;
	st = emalloc(sizeof(SymTab));
	st->last = &st->list;
	st->up = scope;
	scope = st;
	n->st = st;
	return n;
}

void
scopeup(void)
{
	assert(scope->up != nil);
	scope = scope->up;
}


NodeList
nladd(NodeList nl, int n, ...)
{
	NodeList r;
	int i;
	va_list va;
	
	r.a = realloc(nl.a, (nl.n + n) * sizeof(Node *));
	r.n = nl.n + n;
	va_start(va, n);
	for(i = 0; i < n; i++)
		r.a[nl.n + i] = va_arg(va, Node *);
	va_end(va);
	return r;
}

int
isident(int c)
{
	return isalnum(c) || c == '_';
}

enum {
	TINT = 1<<24,
	TNUM,
	TSTRING,
	TBOOL,
	TVAR,
	TRETURN,
	TIN,
	TINSTANCEOF,
	TIF,
	TELSE,
	TFOR,
	TWHILE,
	TDO,
	TBREAK,
	TCONTINUE,
	TSTRUCT,
	TANDEQ,
	TASR,
	TASREQ,
	TDIVEQ,
	TEQ,
	TEXP,
	TEXPEQ,
	TGE,
	TLAND,
	TLOR,
	TLE,
	TLSH,
	TLSHEQ,
	TMIEQ,
	TMODEQ,
	TMULEQ,
	TNEQ,
	TOREQ,
	TPLEQ,
	TRSH,
	TRSHEQ,
	TSEQ,
	TSNEQ,
	TXOREQ,
	TPP,
	TMM,
	TTYPE,
	TSYM,
	TSTRLIT,
	TTHIS,
	TVOID,
	TNEW,
	TEXTERN,
	TDOTS,
	TSWITCH,
	TCASE,
	TDEFAULT,
	TTHROW,
	TTRY,
	TCATCH,
	TFINALLY,
	TENUM,
	TEOF,
};
char idbuf[512];
Symbol *lexsym;
SymTab *lexsymscope;
int lexnextch;
int peektok;

#pragma varargck type "T" int
int
tokfmt(Fmt *f)
{
	int n;
	static char *toktab[] = {
		[TINT-TINT] "TINT",
		[TNUM-TINT] "TNUM",
		[TSTRING-TINT] "TSTRING",
		[TBOOL-TINT] "TBOOL",
		[TVAR-TINT] "TVAR",
		[TRETURN-TINT] "TRETURN",
		[TIN-TINT] "TIN",
		[TINSTANCEOF-TINT] "TINSTANCEOF",
		[TIF-TINT] "TIF",
		[TELSE-TINT] "TELSE",
		[TFOR-TINT] "TFOR",
		[TWHILE-TINT] "TWHILE",
		[TDO-TINT] "TDO",
		[TBREAK-TINT] "TBREAK",
		[TCONTINUE-TINT] "TCONTINUE",
		[TSTRUCT-TINT] "TSTRUCT",
		[TANDEQ-TINT] "TANDEQ",
		[TASR-TINT] "TASR",
		[TASREQ-TINT] "TASREQ",
		[TDIVEQ-TINT] "TDIVEQ",
		[TEQ-TINT] "TEQ",
		[TEXP-TINT] "TEXP",
		[TEXPEQ-TINT] "TEXPEQ",
		[TGE-TINT] "TGE",
		[TLAND-TINT] "TLAND",
		[TLOR-TINT] "TLOR",
		[TLE-TINT] "TLE",
		[TLSH-TINT] "TLSH",
		[TLSHEQ-TINT] "TLSHEQ",
		[TMIEQ-TINT] "TMIEQ",
		[TMODEQ-TINT] "TMODEQ",
		[TMULEQ-TINT] "TMULEQ",
		[TNEQ-TINT] "TNEQ",
		[TOREQ-TINT] "TOREQ",
		[TPLEQ-TINT] "TPLEQ",
		[TRSH-TINT] "TRSH",
		[TRSHEQ-TINT] "TRSHEQ",
		[TSEQ-TINT] "TSEQ",
		[TSNEQ-TINT] "TSNEQ",
		[TXOREQ-TINT] "TXOREQ",
		[TPP-TINT] "TPP",
		[TMM-TINT] "TMM",
		[TTYPE-TINT] "TTYPE",
		[TSYM-TINT] "TSYM",
		[TSTRLIT-TINT] "TSTRLIT",
		[TTHIS-TINT] "TTHIS",
		[TVOID-TINT] "TVOID",
		[TNEW-TINT] "TNEW",
		[TEOF-TINT] "TEOF",
		[TEXTERN-TINT] "TEXTERN",
		[TDOTS-TINT] "TDOTS",
		[TSWITCH-TINT] "TSWITCH",
		[TCASE-TINT] "TCASE",
		[TDEFAULT-TINT] "TDEFAULT",
		[TTHROW-TINT] "TTHROW",
		[TTRY-TINT] "TTRY",
		[TCATCH-TINT] "TCATCH",
		[TFINALLY-TINT] "TFINALLY",
		[TENUM-TINT] "TENUM",
	};
	
	n = va_arg(f->args, int);
	if(n < TINT)
		return fmtprint(f, "'%C'", n);
	if(n-TINT >= nelem(toktab) || toktab[n-TINT] == nil)
		return fmtprint(f, "??? (%d)", n);
	return fmtprint(f, "%s", toktab[n-TINT]);
}

typedef struct Keyword Keyword;

struct Keyword {
	char *name;
	int tok;
};
Keyword kwtab[] = {
	"int", TINT,
	"string", TSTRING,
	"bool", TBOOL,
	"var", TVAR,
	"return", TRETURN,
	"in", TIN,
	"instanceof", TINSTANCEOF,
	"if", TIF,
	"else", TELSE,
	"for", TFOR,
	"while", TWHILE,
	"do", TDO,
	"break", TBREAK,
	"continue", TCONTINUE,
	"struct", TSTRUCT,
	"this", TTHIS,
	"void", TVOID,
	"new", TNEW,
	"extern", TEXTERN,
	"function", TVAR,
	"switch", TSWITCH,
	"case", TCASE,
	"default", TDEFAULT,
	"throw", TTHROW,
	"try", TTRY,
	"catch", TCATCH,
	"finally", TFINALLY,
	"enum", TENUM,
};
Keyword optab[] = {
	"&=", TANDEQ,
	">>>", TASR,
	">>>=", TASREQ,
	"/=", TDIVEQ,
	"==", TEQ,
	"**", TEXP,
	"**=", TEXPEQ,
	">=", TGE,
	"&&", TLAND,
	"||", TLOR,
	"<=", TLE,
	"<<", TLSH,
	"<<=", TLSHEQ,
	"-=", TMIEQ,
	"%=", TMODEQ,
	"*=", TMULEQ,
	"!=", TNEQ,
	"|=", TOREQ,
	"+=", TPLEQ,
	">>", TRSH,
	">>=", TRSHEQ,
	"===", TSEQ,
	"!==", TSNEQ,
	"^=", TXOREQ,
	"++", TPP,
	"--", TMM,
	"..", TDOTS,
	"...", TDOTS,
};
typedef struct Oper Oper;
struct Oper {
	int tok;
	char *str;
	char prec;
	char rassoc;
};
Oper bintab[] = {
	[OPINVAL] {0, "", 0, 0},
	[OPADD] {'+', "+", 13, 0},
	[OPAND] {'&', "&", 9, 0},
	[OPASR] {TASR, ">>>", 12, 0},
	[OPCOMMA] {',', ",", 1, 0},
	[OPDIV] {'/', "/", 14, 0},
	[OPEQ] {TEQ, "==", 10, 0},
	[OPEQS] {TSEQ, "===", 10, 0},
	[OPEXP] {TEXP, "**", 15, 1},
	[OPGE] {TGE, ">=", 11, 0},
	[OPGT] {'>', ">", 11, 0},
	[OPLAND] {TLAND, "&&", 6, 0},
	[OPLE] {TLE, "<=", 11, 0},
	[OPLOR] {TLOR, "||", 5, 0},
	[OPLSH] {TLSH, "<<", 12, 0},
	[OPLT] {'<', "<", 11, 0},
	[OPMOD] {'%', "%", 14, 0},
	[OPMUL] {'*', "*", 14, 0},
	[OPNEQ] {TNEQ, "!=", 10, 0},
	[OPNEQS] {TSNEQ, "!==", 10, 0},
	[OPOR] {'|', "|", 7, 0},
	[OPRSH] {TRSH, ">>", 12, 0},
	[OPSUB] {'-', "-", 13, 0},
	[OPXOR] {'^', "^", 8, 0},
	[OPIN] {TIN, "in", 11, 0},
	[OPINSTANCEOF] {TINSTANCEOF, "instanceof", 11, 0},
};
Oper asstab[] = {
	[OPINVAL] {'=', "=", 3, 1},
	[OPADD] {TPLEQ, "+=", 3, 1},
	[OPAND] {TANDEQ, "&=", 3, 1},
	[OPASR] {TASREQ, ">>>=", 3, 1},
	[OPDIV] {TDIVEQ, "/=", 3, 1},
	[OPEXP] {TEXPEQ, "**", 3, 3},
	[OPLSH] {TLSHEQ, "<<=", 3, 1},
	[OPMOD] {TMODEQ, "%=", 3, 1},
	[OPMUL] {TMULEQ, "*=", 3, 1},
	[OPOR] {TOREQ, "|=", 3, 1},
	[OPRSH] {TRSHEQ, ">>=", 3, 1},
	[OPSUB] {TMIEQ, "-=", 3, 1},
	[OPXOR] {TXOREQ, "^=", 3, 1},
};
Oper untab[] = {
	[OPUPLUS] {'+', "+", 16, 0},
	[OPUMINUS] {'-', "-", 16, 0},
	[OPCOM] {'~', "~", 16, 0},
	[OPLNOT] {'!', "!", 16, 0},
};

int
opstart(int c)
{
	Keyword *kw;

	for(kw = optab; kw < optab + nelem(optab); kw++)
		if(kw->name[0] == c)
			return 1;
	return 0;
}

int
findop(char *s)
{
	Keyword *kw;

	if(strlen(s) == 1)
		return s[0];
	for(kw = optab; kw < optab + nelem(optab); kw++)
		if(strcmp(kw->name, s) == 0)
			return kw->tok;
	return -1;
}

int
lex(void)
{
	int c;
	Keyword *kw;
	char *p;

	if(peektok != 0){
		c = peektok;
		peektok = 0;
		if((c == TSYM || c == TTYPE) && lexsymscope != scope){
			lexsym = getsym(scope, 1, lexsym->name);
			lexsymscope = scope;
		}
		return c;
	
	}
again:
	do{
		c = Bgetc(bin);
		if(c < 0) return TEOF;
		if(c == '\n') curline.lineno++;
	}while(isspace(c));
	if(c == '/'){
		c = Bgetc(bin);
		if(c == '/'){
			while(c = Bgetc(bin), c >= 0 && c != '\n')
				;
			if(c == '\n') curline.lineno++;
			goto again;
		}else if(c == '*'){
		s0:	c = Bgetc(bin);
			if(c < 0) return TEOF;
			if(c == '\n') curline.lineno++;
			if(c != '*') goto s0;
		s1:	c = Bgetc(bin);
			if(c == '\n') curline.lineno++;
			if(c < 0) return TEOF;
			if(c == '*') goto s1;
			if(c == '/') goto again;
			goto s0;
		}
		Bungetc(bin);
		c = '/';
	}
	if(isdigit(c)){
		p = idbuf;
		*p++ = c;
		while(c = Bgetc(bin), isident(c) || c == '.')
			if(p < idbuf + sizeof(idbuf) - 1)
				*p++ = c;
		while(c >= 0 && isspace(c)){
			if(c == '\n') curline.lineno++;
			c = Bgetc(bin);
		}
		lexnextch = c;
		Bungetc(bin);
		*p = 0;
		return TNUM;
	}
	if(isident(c)){
		p = idbuf;
		*p++ = c;
		while(c = Bgetc(bin), isident(c))
			if(p < idbuf + sizeof(idbuf) - 1)
				*p++ = c;
		while(c >= 0 && isspace(c)){
			if(c == '\n') curline.lineno++;
			c = Bgetc(bin);
		}
		lexnextch = c;
		Bungetc(bin);
		*p = 0;
		if(isdigit(idbuf[0]))
			return TNUM;
		for(kw = kwtab; kw < kwtab + nelem(kwtab); kw++)
			if(strcmp(kw->name, idbuf) == 0)
				return kw->tok;
		lexsym = getsym(scope, 1, idbuf);
		lexsymscope = scope;
		if(lexsym->t == SYMTYPE)
			return TTYPE;
		else
			return TSYM;
	}
	if(opstart(c)){
		p = idbuf;
		*p++ = c;
		do{
			*p++ = Bgetc(bin);
			*p = 0;
		}while(findop(idbuf) >= 0);
		Bungetc(bin);
		*--p = 0;
		return findop(idbuf);
	}
	if(c == '"'){
		p = idbuf;
		*p++ = c;
		while(c = Bgetc(bin), c >= 0 && c != '"' && c != '\n'){
			if(p < idbuf + sizeof(idbuf) - 2)
				*p++ = c;
			if(c == '\\'){
				c = Bgetc(bin);
				if(c < 0 || c == '\n')
					break;
				if(p < idbuf + sizeof(idbuf) - 2)
					*p++ = c;
			}
		}
		*p++ = '"';
		*p = 0;
		if(c != '"')
			error(nil, "unterminated string");
		return TSTRLIT;
	}
	return c;
}

int
peek(void)
{
	return peektok = lex();
}

void
superman(int t)
{
	assert(peektok == 0);
	peektok = t;
}

int
got(int x)
{
	if(peek() == x)
		return lex(), 1;
	return 0;
}

int
goteof(int x)
{
	int t;
	
	t = peek();
	if(t == TEOF){
		error(nil, "unexpected eof");
		return 1;
	}
	if(t == x)
		return lex(), 1;
	return 0;
}

int
gotany(int x, ...)
{
	int t, i;
	va_list va;
	
	t = peek();
	if(t == x) return lex();
	va_start(va, x);
	while(i = va_arg(va, int), i != 0)
		if(t == i)
			return lex();
	va_end(va);
	return 0;
}


void
expect(int x)
{
	int t;
	
	t = lex();
	if(t != x)
		error(nil, "unexpected %T, expected %T", t, x);
}

void
expectany(int x, ...)
{
	int t, i;
	va_list va;
	
	t = lex();
	if(t == x) return;
	va_start(va, x);
	while(i = va_arg(va, int), i != 0)
		if(t == i){
			va_end(va);
			return;
	}
	va_end(va);
	error(nil, "unexpected %T", t);
}


Oper ternop1 = {.prec = 4}, ternop2 = {.prec = 3, .rassoc = -3};
Oper *
opfind(int t)
{
	int i;
	
	for(i = 0; i < nelem(bintab); i++)
		if(bintab[i].tok == t)
			return &bintab[i];
	for(i = 0; i < nelem(asstab); i++)
		if(asstab[i].tok == t)
			return &asstab[i];
	if(t == '?') return &ternop1;
	if(t == ':') return &ternop2;
	return nil;
}

typedef union {
	void *v;
	Node *n;
	Oper *o;
} StackEl;
typedef struct ExprStack ExprStack;
struct ExprStack {
	StackEl *s;
	int n, a;
};

void
exprpush(ExprStack *st, void *v)
{
	if(st->n == st->a)
		st->s = realloc(st->s, (st->a + 16) * sizeof(void *));
	st->s[st->n++].v = v;
}

Node *p_expr(int);

NodeList
p_exprl(int end)
{
	NodeList r;
	Node *n;
	
	r = (NodeList){0, nil};
	while(!goteof(end)){
		n = p_expr(0);
		if(n != nil)
			r = nladd(r, 1, n);
		if(goteof(end)) break;
		expect(',');
	}
	return r;
}

Node *
p_object(Type *typ)
{
	Node *n;
	char *s;
	
	n = node(nil, ASTOBJECT, typ);
	while(!goteof('}')){
		expectany(TSYM, TTYPE, TSTRLIT, 0);
		s = strdup(idbuf);
		expect(':');
		n->nl = nladd(n->nl, 1, node(nil, ASTOBJELEM, s, p_expr(0)));
		if(goteof('}')) break;
		expect(',');
	}
	return n;
}

Node *
p_new(void)
{
	Node *n;

	expect(TTYPE);
	n = node(nil, ASTNEW, lexsym);
	if(got('('))
		n->nl = p_exprl(')');
	return n;
}

int
gottype(void)
{
	return gotany(TVAR, TINT, TSTRING, TBOOL, TVOID, TTYPE, 0);
}

Type *
p_enum(void)
{
	Type *t;

	t = type(TYPENUM);
	if(!got('{')){
		expectany(TTYPE, TSYM, 0);
		decl(scope, SYMTYPE, lexsym, t, nil);
		t->label = strdup(idbuf);
		expect('{');
	}
	while(!goteof('}')){
		expectany(TTYPE, TSYM, 0);
		decl(&t->st, SYMENUMV, lexsym, t, nil);
		decl(scope, SYMENUMV, lexsym, t, nil);
		if(goteof('}')) break;
		expect(',');
	}
	return t;
}

Type *
p_type(int t)
{
	switch(t){
	case TVAR: return type(TYPVAR);
	case TINT: return type(TYPINT);
	case TSTRING: return type(TYPSTRING);
	case TBOOL: return type(TYPBOOL);
	case TVOID: return type(TYPVOID);
	case TTYPE: return lexsym->type;
	case TENUM: return p_enum();
	default:
		error(nil, "unexpected %T, expected type", t);
		return nil;
	}
}

Type *p_spec_tail(Type *);
Node *p_function(Symbol *, Type *, Type *);

Node *
p_lambda(Type *ty)
{
	Node *n, *m;
	int i;

	if(ty->t != TYPFUNC){
		error(nil, "%τ is not a function type", ty);
		ty = type(TYPVAR);
	}
	expect(TRETURN);
	n = newscope(ASTFUNC, nil);
	n->type = ty;
	for(i = 0; i < ty->pl.n; i++)
		if(ty->pl.a[i]->str != nil)
			decl(scope, SYMVAR, getsym(scope, 0, ty->pl.a[i]->str), ty->pl.a[i]->type, nil);
	m = p_expr(0);
	if(m != nil)
		n->nl = nladd(n->nl, 1, node(n, ASTRETURN, m));
	scopeup();
	return n;
}

Node *
p_primary(void)
{
	Node *n;
	int t;
	Type *ty;
	int parens;

	switch(t = lex()){
	case TTHIS: n = node(nil, ASTTHIS); break;
	case TSYM: n = node(nil, ASTSYM, lexsym); break;
	case TNUM: n = node(nil, ASTNUM, strdup(idbuf)); break;
	case TSTRLIT: n = node(nil, ASTSTR, strdup(idbuf)); break;
	case '+': n = node(nil, ASTUN, OPUPLUS, p_primary()); break;
	case '-': n = node(nil, ASTUN, OPUMINUS, p_primary()); break;
	case '~': n = node(nil, ASTUN, OPCOM, p_primary()); break;
	case '!': n = node(nil, ASTUN, OPLNOT, p_primary()); break;
	case TPP: n = node(nil, ASTINDE, 0, p_primary()); break;
	case TMM: n = node(nil, ASTINDE, 1, p_primary()); break;
	case '(':
		if(t = gottype()){
			ty = p_type(t);
			if(ty == nil) ty = type(TYPVAR);
			if(!got(')')){
				/* BUG: what if there's operators after the object? */
				parens = 1;
				goto notcast;
			}
			n = node(nil, ASTCAST, ty, p_primary());
		}else{
			n = p_expr(1);
			expect(')');
		}
		break;
	case '[': n = node(nil, ASTARRAY, p_exprl(']')); break;
	case '{': n = p_object(nil); break;
	case TVAR: case TINT: case TSTRING: case TBOOL: case TVOID: case TTYPE:
		ty = p_type(t);
		parens = 0;
	notcast:
		if(got('{')){
			if(ty->t != TYPSTRUCT)
				error(nil, "struct literal with non-struct type %τ", ty);
			n = p_object(ty);
			if(parens) expect(')');
			break;
		}
		if(peek() == '('){
			ty = p_spec_tail(ty);
			if(got('{'))
				n = p_function(nil, nil, ty);
			else
				n = p_lambda(ty);
			if(parens) expect(')');
			break;
		}
		if(t == TTYPE)
			return node(nil, ASTSYM, lexsym);
		error(nil, "unexpected type in expression");
		return nil;
	case TNEW: n = p_new(); break;
	default:
		error(nil, "unexpected %T in expression", t);
		return nil;
	}
	for(;;)
		switch(t = lex()){
		case TPP: n = node(nil, ASTINDE, 2, n); break;
		case TMM: n = node(nil, ASTINDE, 3, n); break;
		case '.': expectany(TSYM, TTYPE, 0); n = node(nil, ASTDOT, n, lexsym); break;
		case '(': n = node(nil, ASTCALL, n, p_exprl(')')); break;
		case '[': n = node(nil, ASTIDX, n, p_expr(1)); expect(']'); break;
		default:
			superman(t);
			return n;
		}
}

Node *
p_expr(int comma)
{
	ExprStack st;
	Node *n;
	Oper *o, *p;
	int t;
	int nok;
	int qu;
	
	memset(&st, 0, sizeof(st));
	n = p_primary();
	if(n == nil) goto giveup;
	exprpush(&st, n);
	qu = 0;
	for(;;){
		t = peek();
		o = opfind(t);
		nok = o == nil || !comma && t == ',' || qu == 0 && t == ':';
		while(st.n >= 3 && (nok || st.s[st.n - 2].o->prec >= o->prec + o->rassoc)){
			p = st.s[st.n - 2].o;
			if(p == &ternop1) break;
			if(p == &ternop2){
				if(st.n < 5 || st.s[st.n - 4].o != &ternop1) goto error;
				st.s[st.n - 5].n = node(nil, ASTTERN, st.s[st.n - 5].n, st.s[st.n - 3].n, st.s[st.n - 1].n);
				st.n -= 4;
				continue;
			}
			if(p >= bintab && p < bintab + nelem(bintab))
				st.s[st.n - 3].n = node(nil, ASTOP, p - bintab, st.s[st.n - 3].n, st.s[st.n - 1].n);
			else
				st.s[st.n - 3].n = node(nil, ASTASS, p - asstab, st.s[st.n - 3].n, st.s[st.n - 1].n);
			st.n -= 2;
		}
		if(nok)
			break;
		lex();
		if(t == '?') qu++;
		if(t == ':') qu--;
		exprpush(&st, o);
		n = p_primary();
		if(n == nil) goto giveup;
		exprpush(&st, n);
	}
	if(st.n != 1){
error:
		error(nil, "expression syntax error");
		goto giveup;
	}
	n = st.s[0].n;
	free(st.s);
	return n;
giveup:
	free(st.s);
	return nil;
}

void p_spec(Type *, Symbol **, Type **, Type **);

Type *
p_parlist(Type *ret)
{
	Type *ft;
	Type *ty;
	Symbol *sym;
	Type *par;
	int t;
	int dots;
	
	ft = type(TYPFUNC, ret);
	if(got(TVOID)){
		if(got(')'))
			return ft;
		t = TVOID;
		dots = 0;
		goto loop;
	}
	dots = 0;
	while(!goteof(')')){
		if(!dots && got(TDOTS))
			dots = 1;
		t = lex();
loop:
		if(t == TSYM){
			superman(t);
			ty = type(TYPVAR);
		}else{
			ty = p_type(t);
			if(got(',')){
				ft->pl = nladd(ft->pl, 1, node(nil, ASTPARAM, nil, ty));
				if(!dots) ft->minarg++;
				continue;
			}
			if(got(')')){
				ft->pl = nladd(ft->pl, 1, node(nil, ASTPARAM, nil, ty));
				if(!dots) ft->minarg++;
				break;
			}
		}
		p_spec(ty, &sym, &par, &ty);
		if(par != nil) error(nil, "unexpected .");
		ft->pl = nladd(ft->pl, 1, node(nil, ASTPARAM, sym->name, ty));
		if(!dots) ft->minarg++;
		if(goteof(')')) break;
		expect(',');
	}
	return ft;
}

Type *
p_spec_tail(Type *t)
{
	int to;
	Type **ip;
	
	ip = &t;
	for(;;)
		switch(peek()){
		case '(':
			lex();
			*ip = p_parlist(*ip);
			ip = &(*ip)->ret;
			break;
		case '[':
			lex();
			if(to = gottype())
				*ip = type(TYPMAP, p_type(to), *ip);
			else
				*ip = type(TYPARRAY, *ip);
			ip = &(*ip)->ret;
			expect(']');
			break;
		default:
			return t;
		}

}

void
p_spec(Type *t, Symbol **sp, Type **par, Type **tp)
{
	int to;

	switch(to = lex()){
	case TINT: case TSTRING: case TBOOL:
		*par = p_type(to);
		expect('.');
		goto dot;
	case TSYM:
	case TTYPE:
		break;
	}
	*sp = lexsym;
	if(*sp == nil) return;
	*par = nil;
	if(got('.')){
		if((*sp)->t != SYMTYPE)
			error(nil, "'%s' not a type", (*sp)->name);
		else
			*par = (*sp)->type;
dot:
		expectany(TSYM, TTYPE, 0);
		*sp = lexsym;
	}
	*tp = p_spec_tail(t);
}

void
p_struct(void)
{
	Type *t, *ty, *tz;
	Symbol *sym;
	Type *par;

	if(!got('{')){
		expectany(TSYM, TTYPE, 0);
		if(lexsym->t != SYMTYPE || !lexsym->type->incomplete){
			t = type(TYPSTRUCT);
			decl(scope, SYMTYPE, lexsym, t, nil);
			t->label = strdup(idbuf);
		}else
			t = lexsym->type;
		if(t->incomplete = peek() == ';')
			return;
		expect('{');
	}else
		t = type(TYPSTRUCT);
	while(!goteof('}')){
		ty = p_type(lex());
		if(ty == nil) continue;
		do{
			p_spec(ty, &sym, &par, &tz);
			if(par != nil) error(nil, "unexpected .");
			decl(&t->st, SYMVAR, sym, tz, nil);
		}while(got(','));
		expect(';');
	}
}

Node *p_stat(void);

Node *
p_function(Symbol *sym, Type *par, Type *ty)
{
	Node *n, *m;
	int i;

	if(ty->t != TYPFUNC){
		error(nil, "%τ is not a function type", ty);
		ty = type(TYPVAR);
	}
	if(sym != nil){
		if(par == nil && sym->st == scope && sym->t == SYMTYPE && sym->type->t == TYPSTRUCT)
			par = sym->type;
		else
			sym = decl(par != nil ? &par->st : scope, SYMFUNC, sym, ty, nil);
	}
	n = newscope(ASTFUNC, sym);
	n->par = par;
	if(sym != nil)
		sym->def = n;
	n->type = ty;
	for(i = 0; i < ty->pl.n; i++)
		if(ty->pl.a[i]->str != nil)
			decl(scope, SYMVAR, getsym(scope, 0, ty->pl.a[i]->str), ty->pl.a[i]->type, nil);
	while(!goteof('}')){
		m = p_stat();
		if(m == nil)
			continue;
		n->nl = nladd(n->nl, 1, m);
	}
	scopeup();
	return n;
}

Node *
p_stat(void)
{
	Node *a, *b, *c, *d;
	int t, gotextern;
	Type *ty, *tz;
	Symbol *sym;
	Type *par;
	NodeList nl;

	a = b = c = d = nil;
	gotextern = 0;
	USED(a); USED(b); USED(c); USED(d);
	switch(t = lex()){
	case '{':
		a = newscope(ASTBLOCK, nil);
		while(!goteof('}')){
			b = p_stat();
			if(b != nil)
				a->nl = nladd(a->nl, 1, b);
		}
		scopeup();
		return a;
	case TRETURN:
		if(!got(';')){
			a = p_expr(1);
			expect(';');
		}
		return node(nil, ASTRETURN, a);
	case TIF:
		expect('(');
		a = p_expr(1);
		expect(')');
		b = p_stat();
		if(got(TELSE))
			c = p_stat();
		return node(nil, ASTIF, a, b, c);
	case TWHILE:
		expect('(');
		a = p_expr(1);
		expect(')');
		b = p_stat();
		return node(nil, ASTWHILE, a, b);
	case TDO:
		a = p_stat();
		expect(TWHILE);
		expect('(');
		b = p_expr(1);
		expect(')');
		expect(';');
		return node(nil, ASTDOWHILE, b, a);
	case TFOR:
		expect('(');
		if(!got(';')){
			a = p_expr(1);
			if(a->t == ASTOP && a->op == OPIN){
				expect(')');
				b = p_stat();
				return node(nil, ASTFORIN, a->n1, a->n2, b);
			}
			expect(';');
		}
		if(!got(';')){
			b = p_expr(1);
			expect(';');
		}
		if(!got(')')){
			c = p_expr(1);
			expect(')');
		}
		d = p_stat();
		return node(nil, ASTFOR, a, b, c, d);
	case TSWITCH:
		expect('(');
		a = node(nil, ASTSWITCH, p_expr(1));
		expect(')');
		expect('{');
		while(!goteof('}')){
			b = p_stat();
			if(b != nil)
				a->nl = nladd(a->nl, 1, b);
		}
		return a;
	case TCASE:
		a = p_expr(1);
		expect(':');
		return node(nil, ASTCASE, a);
	case TDEFAULT:
		expect(':');
		return node(nil, ASTDEFAULT);
	case TBREAK:
		sym = nil;
		if(!got(';')){
			expect(TSYM);
			sym = lexsym;
			expect(';');
		}
		return node(nil, ASTBREAK, sym);
	case TCONTINUE:
		sym = nil;
		if(!got(';')){
			expect(TSYM);
			sym = lexsym;
			expect(';');
		}
		return node(nil, ASTCONTINUE, sym);
	case TTHROW:
		a = p_expr(1);
		if(a == nil) goto failed;
		expect(';');
		return node(nil, ASTTHROW, a);
	case ';':
		return nil;
	case TSYM:
		if(lexnextch == ':'){
			sym = lexsym;
			lex();
			return node(nil, ASTLABEL, decl(scope, SYMLABEL, sym, nil, nil), p_stat());
		}
	case '+': case '-': case '~': case '!': case TPP: case TMM: case '(': case TTHIS:
	case TNUM: case TSTRLIT:
		superman(t);
		a = p_expr(1);
		if(a == nil) goto failed;
		expect(';');
		return a;
	case TSTRUCT:
		p_struct();
		expect(';');
		return nil;
	case TEXTERN:
		gotextern = 1;
		t = lex();
	case TVAR: case TINT: case TSTRING: case TBOOL: case TVOID: case TTYPE: case TENUM:
		ty = p_type(t);
		if(got(';')) return nil;
		p_spec(ty, &sym, &par, &tz);
		if(got('{'))
			return p_function(sym, par, tz);
		else{
			nl = ZL;
			for(;;){
				if(got('=')){
					a = p_expr(0);
					if(a == nil)
						goto failed;
				}else
					a = nil;
				if(par != nil)
					decl(&par->st, SYMVAR, sym, tz, a);
				else{
					sym = decl(scope, SYMVAR, sym, tz, a);
					if(!gotextern)
						nl = nladd(nl, 1, node(nil, ASTDECL, sym));
				}
				if(!got(',')) break;
				p_spec(ty, &sym, &par, &tz);
			}
			expect(';');
			return node(nil, ASTDECLS, nl);
		}
		break;
	case TTRY:
		a = p_stat();
		sym = nil;
		if(got(TCATCH)){
			if(got('(')){
				expectany(TSYM, TTYPE, 0);
				sym = lexsym;
				expect(')');
			}
			b = p_stat();
		}
		if(got(TFINALLY))
			c = p_stat();
		return node(nil, ASTTRY, a, b, c, sym);
	default:
		error(nil, "unexpected %T", t);
	failed:
		while(t != TEOF && t != ';' && t != '}')
			t = lex();
		lex();
		return nil;
	}
}

void
p_program(NodeList *nl)
{
	Node *n;

	while(!got(TEOF)){
		n = p_stat();
		if(n != nil)
			*nl = nladd(*nl, 1, n);
	}
}

int
indfmt(Fmt *f)
{
	int n;
	
	n = va_arg(f->args, int);
	while(n--)
		fmtrune(f, '\t');
	return 0;
}

#pragma varargck type "ε" Node *
#pragma varargck type "I" int

int
opfmt(Fmt *f)
{
	int n;
	
	n = va_arg(f->args, int);
	if(n >= nelem(bintab) || bintab[n].str == nil){
		if(n >= nelem(untab) || untab[n].str == nil)
			return fmtprint(f, "??? (%d)", n);
		return fmtprint(f, "%s", untab[n].str);
	}
	return fmtprint(f, "%s", bintab[n].str);
}

int
octalparse(char *s, uvlong *v)
{
	char *p;

	if(*s != '0') return 0;
	*v = strtoull(s, &p, 8);
	return *p == 0;
}

void output(Fmt *, Node *, int, int);

int
exprfmt(Fmt *f)
{
	Node *n;
	Oper *op;
	int env, i, terse;
	uvlong val;
	
	env = f->prec;
	terse = (f->flags & FmtSign) != 0;
	n = va_arg(f->args, Node *);
	assert(n != nil);
	switch(n->t){
	case ASTSYM:
		if(n->sym->t == SYMENUMV)
			fmtprint(f, "\"%s\"", n->sym->name);
		else
			fmtprint(f, "%s", n->sym->name);
		break;
	case ASTNUM:
		if(octalparse(n->str, &val))
			fmtprint(f, "%#ullx", val);
		else
			fmtprint(f, "%s", n->str);
		break;
	case ASTTHIS:
		fmtprint(f, "this");
		break;
	case ASTOP:
		assert(n->op < nelem(bintab) && bintab[n->op].str != nil);
		op = &bintab[n->op];
		if(op->prec < env) fmtrune(f, '(');
		fmtprint(f, "%.*ε %s %.*ε", op->prec + op->rassoc, n->n1, op->str, op->prec + !op->rassoc, n->n2);
		if(op->prec < env) fmtrune(f, ')');
		break;
	case ASTUN:
		assert(n->op < nelem(untab) && untab[n->op].str != nil);
		op = &untab[n->op];
		if(op->prec < env) fmtrune(f, '(');
		fmtprint(f, "%s%.*ε", op->str, op->prec, n->n1);
		if(op->prec < env) fmtrune(f, ')');
		break;
	case ASTASS:
		assert(n->op < nelem(bintab) && bintab[n->op].str != nil);
		op = &bintab[n->op];
		if(env > 3) fmtrune(f, '(');
		fmtprint(f, "%.4ε %s= %.3ε", n->n1, op->str, n->n2);
		if(env > 3) fmtrune(f, ')');
		break;
	case ASTFUNC:
		fmtprint(f, "function(");
		for(i = 0; i < n->type->pl.n; i++)
			fmtprint(f, "%s%s", i>0?", ":"", n->type->pl.a[i]->str);
		fmtprint(f, ")");
		if(!terse){
			fmtprint(f, " {\n");
			for(i = 0; i < n->nl.n; i++)
				output(f, n->nl.a[i], 0, 0);
			fmtprint(f, "}");
		}
		break;
	case ASTTERN:
		if(env > 4) fmtrune(f, '(');
		fmtprint(f, "%.5ε ? %.4ε : %.4ε", n->n1, n->n2, n->n3);
		if(env > 4) fmtrune(f, ')');
		break;
	case ASTARRAY:
		fmtprint(f, "[");
		for(i = 0; i < n->nl.n; i++)
			fmtprint(f, "%s%.2ε", i>0?", ":"", n->nl.a[i]);
		fmtprint(f, "]");
		break;
	case ASTOBJECT:
		fmtprint(f, "{");
		for(i = 0; i < n->nl.n; i++){
			assert(n->nl.a[i]->t == ASTOBJELEM);
			fmtprint(f, "%s%s: %.2ε", i>0?", ":"", n->nl.a[i]->str, n->nl.a[i]->n1);
		}
		fmtprint(f, "}");
		break;
	case ASTINDE:
		if(env > 17) fmtrune(f, '(');
		switch(n->i){
		case 0: fmtprint(f, "++%.16ε", n->n1); break;
		case 1: fmtprint(f, "--%.16ε", n->n1); break;
		case 2: fmtprint(f, "%.17ε++", n->n1); break;
		case 3: fmtprint(f, "%.17ε--", n->n1); break;
		default: assert(0);
		}
		if(env > 17) fmtrune(f, ')');
		break;
	case ASTDOT:
		fmtprint(f, "%.19ε.%s", n->n1, n->sym->name);
		break;
	case ASTCALL:
		fmtprint(f, "%.19ε(", n->n1);
		for(i = 0; i < n->nl.n; i++)
			fmtprint(f, "%s%.2ε", i>0?", ":"", n->nl.a[i]);
		fmtprint(f, ")");
		break;
	case ASTSTR:
		fmtprint(f, "%s", n->str);
		break;
	case ASTNEW:
		fmtprint(f, "new %s(", n->sym->name);
		for(i = 0; i < n->nl.n; i++)
			fmtprint(f, "%s%.2ε", i>0?", ":"", n->nl.a[i]);
		fmtprint(f, ")");
		break;
	case ASTIDX:
		fmtprint(f, "%.19ε[%ε]", n->n1, n->n2);
		break;
	case ASTCAST:
		if(n->typearg->t == TYPSTRING || n->typearg->t == TYPENUM)
			fmtprint(f, "String(%ε)", n->n1);
		else if(n->typearg->t == TYPINT)
			fmtprint(f, "Number(%ε)", n->n1);
		else if(n->typearg->t == TYPVAR)
			fmtprint(f, "%.*ε", env, n->n1);
		else
			fmtprint(f, "(/* %τ */ %ε)", n->typearg, n->n1);
		break;
	default:
		sysfatal("exprfmt: unimplemented %α", n->t);
	}
	return 0;
}

int
typeeq(Type *l, Type *r)
{
	int i;

	if(l == nil) return r == nil;
	if(r == nil) return 0;
	if(l->t != r->t) return 0;
	switch(l->t){
	case TYPSTRUCT:
		return l == r;
	case TYPFUNC:
		if(!typeeq(l->ret, r->ret) || l->pl.n != r->pl.n)
			return 0;
		for(i = 0; i < l->pl.n; i++)
			if(!typeeq(l->pl.a[i]->type, r->pl.a[i]->type))
				return 0;
		return 1;
	case TYPARRAY:
		return typeeq(l->ret, r->ret);
	case TYPMAP:
		return typeeq(l->idx, r->idx) && typeeq(l->ret, r->ret);
	default:
		return 1;
	}
}

int
cmpok(Type *l, Type *r)
{
	return l->t == TYPVAR || r->t == TYPVAR || typeeq(l, r);
}

Type *
optype(Line *lno, int op, Type *l, Type *r)
{
	switch(op){
	case OPADD:
		if(l->t == TYPVAR || r->t == TYPVAR)
			return l;
		if(l->t == TYPINT && r->t == TYPINT)
			return l;
		if(l->t == TYPSTRING && r->t == TYPSTRING)
			return l;
		break;
	case OPSUB: case OPMUL: case OPDIV: case OPAND: case OPASR: case OPEXP:
	case OPLSH: case OPMOD: case OPOR: case OPRSH: case OPXOR:
		if(l->t == TYPVAR || r->t == TYPVAR || l->t == TYPINT && r->t == TYPINT)
			return type(TYPINT);
		break;
	case OPGE: case OPGT: case OPLE: case OPLT: case OPEQ: case OPNEQ: case OPEQS: case OPNEQS:
		if(cmpok(l, r))
			return type(TYPBOOL);
		break;
	case OPLOR: case OPLAND:
		if(typeeq(l, r))
			return l;
		return type(TYPVAR);
	case OPCOMMA:
		return r;
	case OPIN:
	case OPINSTANCEOF:
		return type(TYPBOOL);
	default:
		sysfatal("optype: unimplemented %d", op);
	}
	error(lno, "operation %ω illegal with operands %τ and %τ", op, l, r);
	return type(TYPVAR);
}

Type *
untype(Line *lno, int op, Type *l)
{
	switch(op){
	case OPUPLUS:
	case OPUMINUS:
	case OPCOM:
		if(l->t == TYPVAR || l->t == TYPINT)
			return type(TYPINT);
		break;
	case OPLNOT:
		return type(TYPBOOL);
	default:
		sysfatal("untype: unimplemented %d", op);
	}
	error(lno, "operation %ω illegal with operand %τ", op, l);
	return type(TYPVAR);
}

Type *
dottype(Line *lno, Type *l, char *memb)
{
	Symbol *s;

	if(l->t == TYPVAR || l->t == TYPARRAY)
		return type(TYPVAR);
	s = getsym(&l->st, 0, memb);
	if(s->t == SYMNONE){
		error(lno, "no such field '%s' in type %τ", memb, l);
		return type(TYPVAR);
	}else
		return s->type;
}

int
assok(Type *l, Type *r)
{
	assert(l != nil);
	assert(r != nil);
	if(l->t == TYPVAR || r->t == TYPVAR)
		return 1;
	if(l->t == TYPARRAY && r->t == TYPARRAY)
		return assok(l->ret, r->ret);
	if(l->t == TYPMAP && r->t == TYPMAP){
		if(!assok(l->ret, r->ret)) return 0;
		if(typeeq(l->idx, r->idx)) return 1;
		if(l->idx->t == TYPSTRING && r->idx->t == TYPENUM) return 1;
		if(l->idx->t == TYPENUM && r->idx->t == TYPSTRING) return 1;
		return 0;
	}
	return typeeq(l, r);
}

int
assfix(Type *l, Node **r)
{
	if(typeeq(l, (*r)->type))
		return 1;
	if(!assok(l, (*r)->type))
		return 0;
	*r = node(*r, ASTCAST, l, *r);
	(*r)->type = l;
	return 1;
}

Type *
funtype(Line *lno, Node *fun, NodeList l)
{
	int i;
	Type *t;
	
	t = fun->type;
	if(t->t == TYPVAR)
		return type(TYPVAR);
	if(t->t != TYPFUNC){
		error(lno, "illegal call of %τ", t);
		return type(TYPVAR);
	}
	if(l.n < t->minarg)
		error(lno, "too few arguments in call of %+ε", fun);
	if(l.n > t->pl.n)
		error(lno, "too many arguments in call of %+ε", fun);
	for(i = 0; i < l.n && i < t->pl.n; i++)
		if(!assok(t->pl.a[i]->type, l.a[i]->type))
			error(l.a[i], "%τ instead of %τ as arg %d to %+ε", l.a[i]->type, t->pl.a[i]->type, i+1, fun);
	return t->ret;
}

void
condcheck(Node *n, Type *t)
{
	if(t->t != TYPVAR && t->t != TYPBOOL)
		error(n, "%τ as condition", t);
}

Type *
idxtype(Line *lno, Type *a, Type *i)
{
	int numok, strok;
	
	if(a->t == TYPMAP){
		if(!assok(a->idx, i) && i->t != TYPSTRING && (a->idx->t != TYPSTRING || i->t != TYPENUM))
			error(lno, "%τ as index into %τ", i, a);
		return a->ret;
	}
	numok = 0;
	strok = 0;
	switch(a->t){
	case TYPARRAY:
		numok = 1;
		break;
	case TYPSTRING:
		numok = 1;
		break;
	case TYPVAR:
		numok = 1;
		strok = 1;
		break;
	default:
		strok = 1;
	}
	if(i->t != TYPVAR && (i->t != TYPINT || !numok) && (i->t != TYPSTRING || !strok)){
		error(lno, "%τ as index into %τ", i, a);
		return type(TYPVAR);
	}
	switch(a->t){
	case TYPSTRUCT:
	case TYPVAR:
		return type(TYPVAR);
	case TYPARRAY:
		return a->ret;
	case TYPSTRING:
		return type(TYPSTRING);
	default:
		error(lno, "index into %τ", a);
		return type(TYPVAR);
	}
}

int
hasbreak(Node *n, int nullok, Symbol *label)
{
	int i;

	if(n == nil) return 0;
	switch(n->t){
	case ASTSYM: case ASTOP: case ASTASS: case ASTTERN:
	case ASTFUNC: case ASTRETURN: case ASTCONTINUE: case ASTDECL:
	case ASTDECLS: case ASTARRAY: case ASTOBJECT: case ASTOBJELEM:
	case ASTUN: case ASTINDE: case ASTDOT: case ASTCALL:
	case ASTPARAM: case ASTNUM: case ASTSTR: case ASTTHIS:
	case ASTNEW: case ASTCASE: case ASTDEFAULT: case ASTIDX:
	case ASTTHROW: case ASTCAST:
		return 0;
	case ASTIF:
		return hasbreak(n->n2, nullok, label) || hasbreak(n->n3, nullok, label);
	case ASTLABEL:
		return hasbreak(n->n1, nullok, label);
	case ASTWHILE:
	case ASTDOWHILE:
		return label != nil && hasbreak(n->n2, 0, label);
	case ASTFORIN:
		return label != nil && hasbreak(n->n3, 0, label);
	case ASTFOR:
		return label != nil && hasbreak(n->n4, 0, label);
	case ASTBLOCK:
		for(i = 0; i < n->nl.n; i++)
			if(hasbreak(n->nl.a[i], nullok, label))
				return 1;
		return 0;
	case ASTBREAK:
		if(n->sym == nil)
			return nullok;
		return n->sym == label;
	case ASTSWITCH:
		if(label == nil) return 0;
		for(i = 0; i < n->nl.n; i++)
			if(hasbreak(n->nl.a[i], 0, label))
				return 1;
		return 0;
	case ASTTRY:
		return hasbreak(n->n1, nullok, label) || hasbreak(n->n2, nullok, label) || hasbreak(n->n3, nullok, label);
	default:
		sysfatal("hasbreak: unimplemented %α", n->t);
		return 0;
	}

}

int
returns(Node *n)
{
	int i, alive;

	if(n == nil) return 0;
	switch(n->t){
	case ASTSYM: case ASTOP: case ASTASS: case ASTTERN:
	case ASTFUNC: case ASTBREAK: case ASTCONTINUE:
	case ASTDECL: case ASTDECLS: case ASTARRAY: case ASTOBJECT:
	case ASTOBJELEM: case ASTUN: case ASTINDE: case ASTDOT:
	case ASTCALL: case ASTPARAM: case ASTNUM: case ASTSTR:
	case ASTTHIS: case ASTNEW: case ASTFORIN: case ASTCASE:
	case ASTDEFAULT: case ASTIDX: case ASTCAST:
		return 0;
	case ASTRETURN: case ASTTHROW:
		return 1;
	case ASTIF:
		return returns(n->n2) && returns(n->n3);
	case ASTDOWHILE:
		return returns(n->n2) && !hasbreak(n->n2, 1, nil);
	case ASTWHILE:
		return 0;
	case ASTFOR:
		return n->n2 == nil && !hasbreak(n->n4, 1, nil);
	case ASTBLOCK:
		for(i = 0; i < n->nl.n; i++)
			if(returns(n->nl.a[i]))
				return 1;
		return 0;
	case ASTLABEL:
		return returns(n->n1) && !hasbreak(n->n1, 0, n->sym);
	case ASTSWITCH:
		alive = 1;
		for(i = 0; i < n->nl.n; i++){
			if(n->nl.a[i]->t == ASTCASE || n->nl.a[i]->t == ASTDEFAULT)
				alive = 1;
			else if(hasbreak(n->nl.a[i], 1, nil))
				break;
			else if(returns(n->nl.a[i]))
				alive = 0;
		}
		return !alive;
	case ASTTRY:
		return returns(n->n1) || returns(n->n3);
	default:
		sysfatal("returns: unimplemented %α", n->t);
		return 0;
	}
}

void
typecheck(Node *n, Node *func)
{
	int i;
	Type *t;
	Symbol *s;
	char *inde[] = {"preincrement", "predecrement", "postincrement", "postdecrement"};

	if(n == nil) return;
	switch(n->t){
	case ASTNUM:
		n->type = type(TYPINT);
		break;
	case ASTSTR:
		n->type = type(TYPSTRING);
		break;
	case ASTFUNC:
		for(i = 0; i < n->nl.n; i++)
			typecheck(n->nl.a[i], n);
		if(n->type->ret->t != TYPVOID && n->type->ret->t != TYPVAR){
			for(i = 0; i < n->nl.n; i++)
				if(returns(n->nl.a[i]))
					break;
			if(i == n->nl.n)
				if(n->sym == nil)
					error(n, "no return at end of anonymous function");
				else
					error(n, "no return at end of function '%s'", n->sym->name);
		}
		break;
	case ASTOBJECT:
		for(i = 0; i < n->nl.n; i++)
			typecheck(n->nl.a[i]->n1, func);
		if(n->typearg != nil){
			t = n->typearg;
			for(i = 0; i < n->nl.n; i++){
				s = getsym(&t->st, 0, n->nl.a[i]->str);
				if(s->t == SYMNONE)
					error(n->nl.a[i], "no such field '%s' in type %τ", s->name, t);
				else if(!assfix(s->type, &n->nl.a[i]->n1))
					error(n->nl.a[i]->n1, "%τ instead of %τ for field %s of %τ", n->nl.a[i]->n1->type, s->type, s->name, t);
			}
			n->type = t;
		}else{
			t = nil;
			for(i = 0; i < n->nl.n; i++){
				if(t == nil)
					t = n->nl.a[i]->n1->type;
				else if(!typeeq(t, n->nl.a[i]->n1->type))
					t = type(TYPVAR);
			}
			if(t == nil) t = type(TYPVAR);
			n->type = type(TYPMAP, type(TYPSTRING), t);
		}
		break;
	case ASTSWITCH:
		typecheck(n->n1, func);
		for(i = 0; i < n->nl.n; i++){
			if(n->nl.a[i]->t == ASTDEFAULT)
				continue;
			if(n->nl.a[i]->t != ASTCASE){
				typecheck(n->nl.a[i], func);
				continue;
			}
			typecheck(n->nl.a[i]->n1, func);
			if(!cmpok(n->n1->type, n->nl.a[i]->n1->type))
				error(n->nl.a[i], "type %τ in case does not match %τ of switch", n->nl.a[i]->n1->type, n->n1->type);
		}
		break;
	case ASTBLOCK:
	case ASTDECLS:
		for(i = 0; i < n->nl.n; i++)
			typecheck(n->nl.a[i], func);
		break;
	case ASTCASE: error(n, "lone case statement"); break;
	case ASTDEFAULT: error(n, "lone default statement"); break;
	case ASTDECL:
		typecheck(n->sym->def, func);
		if(n->sym->def != nil && !assfix(n->sym->type, &n->sym->def))
			error(n, "illegal assignment of %τ to %τ", n->sym->def->type, n->sym->type);
		break;
	case ASTLABEL:
		typecheck(n->n1, func);
		break;
	case ASTBREAK:
		n->sym = fixsym(n->sym);
		if(n->sym != nil && n->sym->t != SYMLABEL)
			error(n, "break with %σ", n->sym->t);
		break;
	case ASTCONTINUE:
		n->sym = fixsym(n->sym);
		if(n->sym != nil && n->sym->t != SYMLABEL)
			error(n, "continue with %σ", n->sym->t);
		break;
	case ASTASS:
		typecheck(n->n1, func);
		typecheck(n->n2, func);
		if(n->op != OPINVAL)
			optype(n, n->op, n->n1->type, n->n2->type);
		if(!assfix(n->n1->type, &n->n2))
			error(n, "illegal assignment of %τ to %τ", n->n2->type, n->n1->type);
		n->type = n->n1->type;
		break;
	case ASTOP:
		typecheck(n->n1, func);
		typecheck(n->n2, func);
		n->type = optype(n, n->op, n->n1->type, n->n2->type);
		break;
	case ASTUN:
		typecheck(n->n1, func);
		n->type = untype(n, n->op, n->n1->type);
		break;
	case ASTSYM:
		n->sym = fixsym(n->sym);
		switch(n->sym->t){
		case SYMNONE:
		undecl:
			error(n, "'%s' undeclared", n->sym->name);
			break;
		case SYMVAR:
		case SYMENUMV:
			if((n->flags & ASTFUNDECL) != 0)
				goto undecl;
			break;
		case SYMFUNC:
			break;
		case SYMTYPE:
			if(n->sym->def != nil){
				n->type = n->sym->def->type;
				return;
			}
			/* wet floor */
		default:
			error(n, "illegal %σ in expression", n->sym->t);
		}
		if(n->sym->type == nil)
			n->type = type(TYPVAR);
		else
			n->type = n->sym->type;
		break;
	case ASTINDE:
		typecheck(n->n1, func);
		if(n->n1->type->t != TYPVAR && n->n1->type->t != TYPINT)
			error(n, "illegal %s of %τ", inde[n->i], n->n1->type);
		n->type = type(TYPINT);
		break;
	case ASTRETURN:
		typecheck(n->n1, func);
		if(func == nil)
			error(n, "return outside of function");
		else if(n->n1 == nil){
			if(func->type->ret->t != TYPVOID && func->type->ret->t != TYPVAR)
				error(n, "null return in function returning %τ", func->type->ret);
		}else if(!assfix(func->type->ret, &n->n1))
			error(n, "return of %τ does not match return type %τ", n->n1->type, func->type->ret);
		break;
	case ASTDOT:
		typecheck(n->n1, func);
		n->type = dottype(n, n->n1->type, n->sym->name);
		break;
	case ASTIDX:
		typecheck(n->n1, func);
		typecheck(n->n2, func);
		n->type = idxtype(n, n->n1->type, n->n2->type);
		break;
	case ASTCALL:
		typecheck(n->n1, func);
		for(i = 0; i < n->nl.n; i++)
			typecheck(n->nl.a[i], func);
		n->type = funtype(n, n->n1, n->nl);
		break;
	case ASTNEW:
		for(i = 0; i < n->nl.n; i++)
			typecheck(n->nl.a[i], func);
		if(n->sym->def != nil){
			funtype(n, n->sym->def, n->nl);
			n->type = n->sym->type;
		}else{
			n->type = type(TYPVAR);
		}
		break;
	case ASTARRAY:
		t = nil;
		for(i = 0; i < n->nl.n; i++){
			typecheck(n->nl.a[i], func);
			if(t == nil)
				t = n->nl.a[i]->type;
			else if(!typeeq(t, n->nl.a[i]->type))
				t = type(TYPVAR);
		}
		if(t == nil) t = type(TYPVAR);
		n->type = type(TYPARRAY, t);
		break;
	case ASTTHIS:
		if(func == nil || func->par == nil){
			error(n, "'this' outside of method");
			n->type = type(TYPVAR);
		}else
			n->type = func->par;
		break;
	case ASTTERN:
		typecheck(n->n1, func);
		typecheck(n->n2, func);
		typecheck(n->n3, func);
		condcheck(n->n1, n->n1->type);
		if(n->n2->type == TYPVAR || n->n3->type == TYPVAR)
			n->type = type(TYPVAR);
		else if(!typeeq(n->n2->type, n->n3->type)){
			error(n, "?: with types %τ and %τ", n->n2->type, n->n3->type);
			n->type = type(TYPVAR);
		}else
			n->type = n->n2->type;
		break;
	case ASTIF:
	case ASTWHILE:
	case ASTDOWHILE:
		typecheck(n->n1, func);
		typecheck(n->n2, func);
		typecheck(n->n3, func);
		condcheck(n->n1, n->n1->type);
		break;
	case ASTFOR:
		typecheck(n->n1, func);
		typecheck(n->n2, func);
		typecheck(n->n3, func);
		typecheck(n->n4, func);
		if(n->n2 != nil)
			condcheck(n->n2, n->n2->type);
		break;
	case ASTFORIN:
		typecheck(n->n1, func);
		typecheck(n->n2, func);
		typecheck(n->n3, func);
		break;
	case ASTTHROW:
		typecheck(n->n1, func);
		break;
	case ASTTRY:
		typecheck(n->n1, func);
		typecheck(n->n2, func);
		typecheck(n->n3, func);
		break;
	case ASTCAST:
		typecheck(n->n1, func);
		n->type = n->typearg;
		break;
	default:
		sysfatal("typecheck: unimplemented %α", n->t);
	}
}

void
outblock(Fmt *f, Node *n, int ind, int dowhile, int force)
{
	int i;

	if(n == nil)
		if(force){
			fmtprint(f, "{\n%I}%s", ind+1, dowhile?"":"\n");
			return;
		}else
			fmtprint(f, "\n%I;\n", ind+1);
	else if(n->t == ASTBLOCK){
		fmtprint(f, "{\n");
		for(i = 0; i < n->nl.n; i++)
			output(f, n->nl.a[i], ind + 1, ind + 1);
		fmtprint(f, "%I}%s", ind, dowhile?"":"\n");
		return;
	}else{
		if(force){
			fmtprint(f, "{\n");
			output(f, n, ind + 1, ind + 1);
			fmtprint(f, "%I}%s", ind, dowhile?"":"\n");
			return;
		}else{
			fmtprint(f, "\n");
			output(f, n, ind + 1, ind + 1);
		}
	}
	if(dowhile) fmtprint(f, "%I", ind);
}

char *
fixnilname(SymTab *st, char *name)
{
	int n;
	static char buf[32];
	Symbol *sym;

	if(name != nil)
		return name;
	for(n = 0; ; n++){
		sprint(buf, "__dummy__%d", n);
		sym = getsym(st, 1, buf);
		if(sym->t != SYMNONE)
			continue;
		sym->t = SYMDUMMY;
		return buf;
	}
}

void
output(Fmt *f, Node *n, int ind, int ind0)
{
	int i;

	if(n == nil) return;
	switch(n->t){
	case ASTFUNC:
		if(n->sym == nil)
			fmtprint(f, "%I%ε;\n", ind0, n);
		else{
			if(n->par != nil && n->par != n->sym->type)
				fmtprint(f, "%I%s.prototype.%s = function(", ind0, n->par->label, n->sym->name);
			else
				fmtprint(f, "%Ifunction\n%I%s(", ind0, ind, n->sym->name);
			for(i = 0; i < n->type->pl.n; i++)
				fmtprint(f, "%s%s", i>0?", ":"", fixnilname(n->st, n->type->pl.a[i]->str));
			fmtprint(f, ") //%τ\n%I{\n", n->sym->type, ind);
			for(i = 0; i < n->nl.n; i++)
				output(f, n->nl.a[i], ind + 1, ind + 1);
			fmtprint(f, "%I}\n", ind);
		}
		break;
	case ASTRETURN:
		if(n->n1 == nil)
			fmtprint(f, "%Ireturn;\n", ind0);
		else
			fmtprint(f, "%Ireturn %ε;\n", ind0, n->n1);
		break;
	case ASTIF:
		fmtprint(f, "%Iif(%ε)", ind0, n->n1);
		outblock(f, n->n2, ind, n->n3 != nil, n->n2 != nil && n->n2->t == ASTIF);
		if(n->n3 != nil){
			fmtprint(f, "else");
			if(n->n3->t == ASTIF){
				fmtrune(f, ' ');
				output(f, n->n3, ind, 0);
			}else
				outblock(f, n->n3, ind, 0, 0);
		}
		break;
	case ASTWHILE:
		fmtprint(f, "%Iwhile(%ε)", ind0, n->n1);
		outblock(f, n->n2, ind, 0, 0);
		break;
	case ASTDOWHILE:
		fmtprint(f, "%Ido", ind0);
		outblock(f, n->n2, ind, 1, 0);
		fmtprint(f, "while(%ε);\n", n->n1);
		break;
	case ASTFOR:
		fmtprint(f, "%Ifor(", ind0);
		if(n->n1 != nil) fmtprint(f, "%ε", n->n1);
		fmtprint(f, "; ");
		if(n->n2 != nil) fmtprint(f, "%ε", n->n2);
		fmtprint(f, "; ");
		if(n->n3 != nil) fmtprint(f, "%ε", n->n3);
		fmtprint(f, ")");
		outblock(f, n->n4, ind, 0, 0);
		break;
	case ASTFORIN:
		fmtprint(f, "%Ifor(%ε in %ε)", ind0, n->n1, n->n2);
		outblock(f, n->n3, ind, 0, 0);
		break;
	case ASTLABEL:
		fmtprint(f, "%I%s: ", ind0, n->sym->name);
		output(f, n->n1, ind, 0);
		break;
	case ASTBREAK:
		if(n->sym != nil)
			fmtprint(f, "%Ibreak %s;\n", ind0, n->sym->name);
		else
			fmtprint(f, "%Ibreak;\n", ind0);
		break;
	case ASTCONTINUE:
		if(n->sym != nil)
			fmtprint(f, "%Icontinue %s;\n", ind0, n->sym->name);
		else
			fmtprint(f, "%Icontinue;\n", ind0);
		break;
	case ASTDECL:
		fmtprint(f, "%Ivar %s", ind0, n->sym->name);
		if(n->sym->def != nil)
			fmtprint(f, " = %.2ε", n->sym->def);
		fmtprint(f, "; // %τ\n", n->sym->type);
		break;
	case ASTDECLS:
		for(i = 0; i < n->nl.n; i++)
			output(f, n->nl.a[i], ind, ind);
		break;
	case ASTBLOCK:
		fmtprint(f, "%I{\n", ind0);
		for(i = 0; i < n->nl.n; i++)
			output(f, n->nl.a[i], ind + 1, ind + 1);
		fmtprint(f, "%I}\n", ind);
		break;
	case ASTSWITCH:
		fmtprint(f, "%Iswitch(%ε){\n", ind0, n->n1);
		for(i = 0; i < n->nl.n; i++)
			output(f, n->nl.a[i], ind + 1, ind + 1);
		fmtprint(f, "%I}\n", ind);
		break;
	case ASTCASE:
		fmtprint(f, "%Icase %ε:\n", ind0-1, n->n1);
		break;
	case ASTTHROW:
		fmtprint(f, "%Ithrow %ε;\n", ind0, n->n1);
		break;
	case ASTDEFAULT:
		fmtprint(f, "%Idefault:\n", ind0-1);
		break;
	case ASTTRY:
		fmtprint(f, "%Itry", ind0);
		outblock(f, n->n1, ind, n->n2 != nil || n->n3 != nil, 1);
		if(n->n2 != nil){
			if(n->sym != nil)
				fmtprint(f, "catch(%s)", n->sym->name);
			else
				fmtprint(f, "catch");
			outblock(f, n->n2, ind, n->n3 != nil, 1);
		}
		if(n->n3 != nil){
			fmtprint(f, "finally");
			outblock(f, n->n3, ind, 0, 1);
		}
		break;
	default:
		fmtprint(f, "%I%ε;\n", ind0, n);
		break;
	}
}

extern int yyparse(void);

void
main(int argc, char **argv)
{
	int i;
	Fmt f;
	static char buf[8192];
	static NodeList prog;
	
	booltype.t = TYPBOOL; booltype.st.last = &booltype.st.list;
	stringtype.t = TYPSTRING; stringtype.st.last = &stringtype.st.list;
	inttype.t = TYPINT; inttype.st.last = &inttype.st.list;
	vartype.t = TYPVAR; vartype.st.last = &vartype.st.list;
	globals.last = &globals.list;
	fmtinstall('I', indfmt);
	fmtinstall(L'α', astfmt);
	fmtinstall(L'ε', exprfmt);
	fmtinstall(L'ω', opfmt);
	fmtinstall(L'σ', symtfmt);
	fmtinstall(L'τ', typefmt);
	fmtinstall('T', tokfmt);
	for(i = 1; i < argc; i++){
		curline.filen = argv[i];
		curline.lineno = 1;
		bin = Bopen(argv[i], OREAD);
		if(bin == nil)
			sysfatal("Bopen: %r");
		p_program(&prog);
		Bterm(bin);
	}
	for(i = 0; i < prog.n; i++)
		typecheck(prog.a[i], nil);
	if(!fail){
		fmtfdinit(&f, 1, buf, sizeof(buf));
		fmtprint(&f, "\"use strict\";\n\n");
		for(i = 0; i < prog.n; i++)
			output(&f, prog.a[i], 0, 0);
		fmtfdflush(&f);
		exits(nil);
	}else
		exits("no");
}
