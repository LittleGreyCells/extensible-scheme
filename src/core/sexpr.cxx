#include <cstdio>
#include <cstring>
#include <vector>

#include "sexpr.hxx"
#include "memory.hxx"
#include "error.hxx"

static void show( const char* s, const SEXPR n )
{
   if (symbolp(n))
      printf(" %s(%p) [%s]\n", s, n->id(), getname(n));
   else
      printf(" %s(%p)\n", s, n->id());
}

void show( const SEXPR n )
{
   if (nullp(n))
      printf("nil\n");
   else
   {
      switch (nodekind(n))
      {
	 case n_free:		show("free", n); break;
	 case n_null:           show("null", n); break;
	 case n_symbol:		show("symbol", n); break;
	 case n_fixnum:		show("fixnum", n); break;
	 case n_flonum:		show("flonum", n); break;
	 case n_string:		show("string", n); break;
	 case n_cons:		show("cons", n); break;
	 case n_vector:		show("vector", n); break;
	 case n_func:		show("func", n); break;
	 case n_apply:		show("apply", n); break;
	 case n_callcc:		show("call/cc", n); break;
	 case n_map:		show("map", n); break;
	 case n_foreach:	show("foreach", n); break;
	 case n_eval:		show("eval", n); break;
	 case n_closure:	show("closure", n); break;
	 case n_continuation:	show("continuation", n); break;
	 case n_environment:	show("environment", n); break;
	 case n_port:		show("port", n); break;
	 case n_string_port:    show("stringport", n); break;
	 case n_bvec:           show("byte-vector", n); break;
	 case n_gref:           show("gref", n); break;
	 case n_fref:           show("fref", n); break;
	 case n_promise:        show("promise", n); break;
	 case n_force:          show("force", n); break;
	 case n_code:           show("code", n); break;
	 default: 		show("<unknown>", n); break;	
      }
   }
}


/////////////////////////////////////////////////////////////////
//
// Accessors/Modifiers
//
/////////////////////////////////////////////////////////////////

char* name( const SEXPR n ) { return getname(guard(n, symbolp)); }

FIXNUM fixnum( const SEXPR n ) { return getfixnum(guard(n, fixnump)); }
FLONUM flonum( const SEXPR n ) { return getflonum(guard(n, flonump)); }

char* string( const SEXPR n ) { return getstringdata(guard(n, stringp)); }

SEXPR car( const SEXPR n ) { return anyp(n) ? getcar(guard(n, consp)) : null; }
SEXPR cdr( const SEXPR n ) { return anyp(n) ? getcdr(guard(n, consp)) : null; }

void rplaca( SEXPR n, SEXPR car ) { setcar(guard(n, consp), car); }
void rplacd( SEXPR n, SEXPR cdr ) { setcdr(guard(n, consp), cdr); }

SEXPR nthcar( const SEXPR list, UINT32 n )
{
   SEXPR s = list;
   for (int i = static_cast<int>(n); i > 0 && consp(s); s = getcdr(s))
      --i;
   return car(s);
}

SEXPR nthcdr( const SEXPR list, UINT32 n )
{
   SEXPR s = list;
   for (int i = static_cast<int>(n); i > 0 && consp(s); s = getcdr(s))
      --i;
   return cdr(s);
}

void  vset( SEXPR v, UINT32 index, SEXPR value ) { vectorset(guard(v,vectorp), index, value); }
SEXPR vref( SEXPR v, UINT32 index ) { return vectorref(guard(v,vectorp), index); }

SEXPR value( SEXPR n ) { return getvalue(guard(n, symbolp)); }

SEXPR set( SEXPR n, SEXPR value ) 
{ 
   setvalue(guard(n, symbolp), value);
   return n;
}

UINT32 list_length( const SEXPR x )
{
   SEXPR s = x;
   UINT32 length = 0;
  
   // while a pair
   for ( ; consp(s); s = getcdr(s) )
      ++length;

   return length;
}

void fset( FRAME frame, UINT32 index, SEXPR value )
{
   if ( frame == nullptr )
      ERROR::severe( "fset on null frame");

   if ( index >= getframenslots(frame) )
      ERROR::severe( "fset range error");

   frameset( frame, index, value );
}

SEXPR fref( FRAME frame, UINT32 index )
{
   if ( frame == nullptr )
      ERROR::severe( "fref on null frame");

   if ( index >= getframenslots(frame) )
      ERROR::severe( "fref range error");

   return frameref( frame, index );
}


/////////////////////////////////////////////////////////////////
//
// Predicates
//
/////////////////////////////////////////////////////////////////

bool symbolp( const SEXPR n ) { return n->kind == n_symbol; }

bool fixnump( const SEXPR n ) { return n->kind == n_fixnum; }
bool flonump( const SEXPR n ) { return n->kind == n_flonum; }
bool numberp( const SEXPR n ) { return n->kind == n_fixnum || n->kind == n_flonum; }

bool stringp( const SEXPR n ) { return n->kind == n_string; }
bool charp( const SEXPR n ) { return n->kind == n_char; }
bool vectorp( const SEXPR n ) { return n->kind == n_vector; }
bool consp( const SEXPR n ) { return n->kind == n_cons; }
bool funcp( const SEXPR n ) { return n->kind == n_func; }
bool portp( const SEXPR n ) { return n->kind == n_port; }
bool stringportp( const SEXPR n ) { return n->kind == n_string_port; }
bool anyportp(SEXPR n) { return portp(n) || stringportp(n); }

bool closurep( const SEXPR n ) { return n->kind == n_closure; }

bool specialp( const SEXPR n ) { return n->kind == n_eval || 
                                        n->kind == n_callcc || 
                                        n->kind == n_apply || 
                                        n->kind == n_map ||
                                        n->kind == n_foreach ||
                                        n->kind == n_force; }

bool primp( const SEXPR n ) { return funcp(n) || specialp(n); }

bool contp( const SEXPR n ) { return n->kind == n_continuation; }
bool envp( const SEXPR n ) { return n->kind == n_environment; }
bool bvecp( const SEXPR n ) { return n->kind == n_bvec; }
bool listp( const SEXPR n ) { return nullp(n) || n->kind == n_cons; }
bool atomp( const SEXPR n ) { return nullp(n) || n->kind != n_cons; }

bool inportp( const SEXPR n ) { return portp(n) && (getmode(n) & pm_input); }
bool outportp( const SEXPR n ) { return portp(n) && (getmode(n) & pm_output); }
bool instringportp( const SEXPR n ) { return stringportp(n) && (getmode(n) & pm_input); }
bool outstringportp( const SEXPR n ) { return stringportp(n) && (getmode(n) & pm_output); }
bool anyinportp( const SEXPR n ) { return inportp(n) || instringportp(n); }
bool anyoutportp( const SEXPR n ) { return outportp(n) || outstringportp(n); }

bool lastp( SEXPR n )  { return nullp(cdr(n)); }
bool promisep( SEXPR n )  { return n->kind == n_promise; }
bool codep( const SEXPR n ) { return n->kind == n_code; }

bool grefp( const SEXPR n ) { return n->kind == n_gref; }
bool frefp( const SEXPR n ) { return n->kind == n_fref; }

struct PredMap { PREDICATE pred; const char* name; };

std::vector<PredMap> predicate_map =
{
   { symbolp, "symbol" },
   { fixnump, "fixnum" },
   { flonump, "flonum" },
   { numberp, "number" },
   { stringp, "string" },
   { charp, "char" },
   { vectorp, "vector" },
   { consp, "pair" },
   { funcp, "func" },
   { portp, "port" },
   { stringportp, "string port" },
   { closurep, "closure" },
   { specialp, "special" },
   { contp, "continuation" },
   { envp, "environment" },
   { bvecp, "byte vector" },
   { listp, "list" },
   { atomp, "atom" },
   { inportp, "input port" },
   { outportp, "output port" },
   { instringportp, "input string port" },
   { outstringportp, "output string port" },
   { anyinportp, "any input port" },
   { anyoutportp, "any output port" },
   { lastp, "last argument" },
   { promisep, "promise" },
   { anyportp, "port or stringport" },
   { primp, "func or special" },
   { grefp, "global symbol reference" },
   { frefp, "frame symbol reference" },
};

SEXPR guard( SEXPR s, PREDICATE predicate )
{
   if ( !predicate(s) )
   {
      const char* expected = "<unknown>";

      for ( auto& x : predicate_map )
      {
	 if ( x.pred == predicate )
	 {
	    expected = x.name;
	    break;
	 }
      }

      char message[80];
      SPRINTF( message, "argument wrong type--expected %s", expected );

      ERROR::severe( message, s );
   }

   return s;
}

#ifdef CHECKED_ACCESS

static void typecheck( SEXPR s, PREDICATE predicate )
{
   if ( !predicate(s) )
   {
      const char* expected = "<unknown>";

      for ( auto& x : predicate_map )
      {
	 if ( x.pred == predicate )
	 {
	    expected = x.name;
	    break;
	 }
      }

      char message[80];
      SPRINTF( message, "type check failed--expected %s", expected );

      ERROR::fatal( message );
   }
}

SEXPR& getcar(SEXPR n) { typecheck(n,consp); return n->u.cons.car; }
SEXPR& getcdr(SEXPR n) { typecheck(n,consp); return n->u.cons.cdr; }
void setcar(SEXPR n, SEXPR x) { getcar(n) = x; }
void setcdr(SEXPR n, SEXPR x) { getcdr(n) = x; }
 
UINT32& getvectorlength(SEXPR n) { typecheck(n,vectorp); return n->u.vector.length; }
SEXPR*& getvectordata(SEXPR n) { typecheck(n,vectorp); return n->u.vector.data; }
SEXPR& vectorref(SEXPR n, UINT32 i) { typecheck(n,vectorp); return n->u.vector.data[i]; }
void setvectorlength(SEXPR n, UINT32 x) { getvectorlength(n) = x; }
void setvectordata(SEXPR n, SEXPR* x) { getvectordata(n) = x; }
void vectorset(SEXPR n, UINT32 i, SEXPR x) { vectorref(n,i) = x; }

UINT32& getstringlength(SEXPR n) { typecheck(n,stringp); return n->u.string.length; }
UINT32& getstringindex(SEXPR n) { typecheck(n,stringp); return n->u.string.index; }
char*& getstringdata(SEXPR n) { typecheck(n,stringp); return n->u.string.data; }
void setstringlength(SEXPR n, UINT32 x) { getstringlength(n) = x; }
void setstringindex(SEXPR n, UINT32 x) {  getstringindex(n) = x; }
void setstringdata(SEXPR n, char* x) { getstringdata(n) = x; }

char*& getname(SEXPR n) { typecheck(n,symbolp); return n->u.symbol.name; }
SEXPR& getvalue(SEXPR n) { typecheck(n,symbolp); return n->u.symbol.value; }
SEXPR& getplist(SEXPR n) { typecheck(n,symbolp); return n->u.symbol.plist; }
void setname(SEXPR n, char* x) { getname(n) = x; }
void setvalue(SEXPR n, SEXPR x) { getvalue(n) = x; }
void setplist(SEXPR n, SEXPR x) { getplist(n) = x; }

char& getcharacter(SEXPR n) { typecheck(n,charp); return n->u.ch; }
void setcharacter(SEXPR n, char ch) { getcharacter(n) = ch; }

FIXNUM& getfixnum(SEXPR n) { typecheck(n,fixnump); return n->u.fixnum; }
FLONUM& getflonum(SEXPR n) { typecheck(n,flonump); return n->u.flonum; }
void setfixnum(SEXPR n, FIXNUM x) { getfixnum(n) = x; }
void setflonum(SEXPR n, FLONUM x) { getflonum(n) = x; }

SEXPR& getclosurecode(SEXPR n) { typecheck(n,closurep); return n->u.closure.code; }
SEXPR& getclosurebenv(SEXPR n) { typecheck(n,closurep); return n->u.closure.benv; }
SEXPR& getclosurevars(SEXPR n) { typecheck(n,closurep); return n->u.closure.vars; }
BYTE& getclosurenumv(SEXPR n) { typecheck(n,closurep); return n->aux1; }
BYTE& getclosurerargs(SEXPR n) { typecheck(n,closurep); return n->aux2; }

void setclosurecode(SEXPR n, SEXPR x) { getclosurecode(n) = x; }
void setclosurebenv(SEXPR n, SEXPR x) { getclosurebenv(n) = x; }
void setclosurevars(SEXPR n, SEXPR x) { getclosurevars(n) = x; }
void setclosurenumv(SEXPR n, BYTE x) { getclosurenumv(n) = x; }
void setclosurerargs(SEXPR n, BYTE x) { getclosurerargs(n) = x; }

FRAME& getenvframe(SEXPR n) { typecheck(n,envp); return n->u.environ.frame; }
SEXPR& getenvbase(SEXPR n) { typecheck(n,envp); return n->u.environ.baseenv; }
void setenvframe(SEXPR n, FRAME x) { getenvframe(n) = x; }
void setenvbase(SEXPR n, SEXPR x) { getenvbase(n) = x; }

UINT32& getbveclength(SEXPR n) { typecheck(n,bvecp); return n->u.bvec.length; }
BYTE*& getbvecdata(SEXPR n) { typecheck(n,bvecp); return n->u.bvec.data; }
BYTE& bvecref(SEXPR n, UINT32 i) { typecheck(n,bvecp); return n->u.bvec.data[(i)]; }
void setbveclength(SEXPR n, UINT32 x) { getbveclength(n) = x; }
void setbvecdata(SEXPR n, BYTE* x) { getbvecdata(n) = x; }
void bvecset(SEXPR n, UINT32 i, BYTE x) { bvecref(n,i) = x; }

FILE*& getfile(SEXPR n) { typecheck(n,portp); return n->u.port.p.file; }
INT16& getmode(SEXPR n) { typecheck(n,anyportp); return n->u.port.mode; }
void setfile(SEXPR n, FILE* x) { getfile(n) = x; }
void setmode(SEXPR n, INT16 x) { getmode(n) = x; }

SEXPR& getstringportstring(SEXPR n) { typecheck(n,stringportp); return n->u.port.p.string; }
void setstringportstring(SEXPR n, SEXPR x) { getstringportstring(n) = x; }

SEXPR& code_getbcodes(SEXPR n) { typecheck(n,codep); return n->u.code.bcodes; }
SEXPR& code_getsexprs(SEXPR n) { typecheck(n,codep); return n->u.code.sexprs; }
void code_setbcodes(SEXPR n, SEXPR x) { code_getbcodes(n) = x; }
void code_setsexprs(SEXPR n, SEXPR x) { code_getsexprs(n) = x; }

SEXPR& promise_getexp(SEXPR n) { typecheck(n,promisep); return n->u.promise.exp; }
SEXPR& promise_getval(SEXPR n) { typecheck(n,promisep); return n->u.promise.val; }
void promise_setexp(SEXPR n, SEXPR x) { promise_getexp(n) = x; }
void promise_setval(SEXPR n, SEXPR x) { promise_getval(n) = x; }

PRIMITIVE& getfunc(SEXPR n) { typecheck(n, primp); return ((n)->u.func); }
void setfunc(SEXPR n, PRIMITIVE x) { getfunc(n) = (x); }

SEXPR& gref_getsymbol(SEXPR n) { typecheck(n,grefp); return n->u.gref.symbol; }
void gref_setsymbol(SEXPR n, SEXPR s) { gref_getsymbol(n) = s; }

UINT32& fref_getdepth(SEXPR n) { typecheck(n,frefp); return n->u.fref.depth; }
UINT32& fref_getindex(SEXPR n) { typecheck(n,frefp); return n->u.fref.index; }
void fref_setdepth(SEXPR n, UINT32 d) { fref_getdepth(n) = d; }
void fref_setindex(SEXPR n, UINT32 i) { fref_getindex(n) = i; }

#endif
