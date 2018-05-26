"use strict";

function
max(a, b)
{
	return a > b ? a : b;
}

function
mpdig(s, i)
{
	var t;
	
	t = s.charAt(i);
	if(t == "") return s.charAt(s.length - 1);
	return t;
}

function
mpnorm(r)
{
	var i;

	i = r.length - 2;
	while(i >= 0 && r.charAt(i) == r.charAt(r.length - 1))
		i--;
	return r.substr(0, i + 2);
}

function
mpadd(a, b)
{
	var m, r, c, i;
	
	m = max(a.length, b.length) + 1;
	c = 0;
	r = "";
	for(i = 0; i < m; i++){
		c += mpdig(a, i) == "1" ? 1 : 0;
		c += mpdig(b, i) == "1" ? 1 : 0;
		r += String.fromCharCode(48 + (c & 1));
		c >>= 1;
	}
	return mpnorm(r);
}

function
mpsub(a, b)
{
	var m, r, c, i;
	
	m = max(a.length, b.length) + 1;
	c = 1;
	r = "";
	for(i = 0; i < m; i++){
		c += mpdig(a, i) == "1" ? 1 : 0;
		c += mpdig(b, i) == "1" ? 0 : 1;
		r += String.fromCharCode(48 + (c & 1));
		c >>= 1;
	}
	return mpnorm(r);
}


function
mpmul(a, b)
{
	var i, r;
	
	r = "0";
	for(i = 0; i < b.length - 1; i++)
		if(mpdig(b, i) == "1")
			r = mpadd(r, "0".repeat(i) + a);
	if(mpdig(b, i) == "1")
		r = mpsub(r, "0".repeat(i) + a);
	return r;
}

function
mptrunc(a, n)
{
	return mpnorm(a.substr(0, n) + "0");
}

function
mpxtend(a, n)
{
	return mpnorm(a.substr(0, n));
}

function
mptoi(n)
{
	var i, r;
	
	r = -parseInt(n.charAt(n.length - 1));
	for(i = n.length - 2; i >= 0; i--)
		r = r * 2 + parseInt(n.charAt(i));
	return r;
}
