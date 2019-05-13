#include "memory.hxx"

#include <cstring>
#include <array>
#include <list>

#include "error.hxx"
#include "regstack.hxx"
#include "framestore.hxx"

#ifdef OBJECT_CACHE
#include "varpool.hxx"
#define CACHE_FRAME
#endif

SEXPR MEMORY::string_null;
SEXPR MEMORY::vector_null;
SEXPR MEMORY::listtail;
SEXPR MEMORY::listhead;

#ifdef GC_STATISTICS_DETAILED
std::array<UINT32, NUMKINDS> MEMORY::ReclamationCounts;
#endif

MEMORY::FrameStore MEMORY::frameStore;

std::list<MEMORY::Marker> markers;

void MEMORY::register_marker( Marker marker )
{
   markers.push_back( marker );
}

//
// Node Block Pool
//

long  MEMORY::TotalNodeCount  = 0;
long  MEMORY::FreeNodeCount   = 0;
int   MEMORY::CollectionCount = 0;

static SEXPR FreeNodeList;

struct NodeBlock
{
   std::array<Node, NODE_BLOCK_SIZE> nodes;
};

static std::list<NodeBlock*> blocks;

static void NewNodeBlock()
{
   auto block = new NodeBlock;

   blocks.push_back( block );

   MEMORY::TotalNodeCount += block->nodes.size();
   MEMORY::FreeNodeCount += block->nodes.size();

   for ( auto& node : block->nodes )
   {
      SEXPR p = &node;
      new (p) Node( n_free, FreeNodeList );
      FreeNodeList = p;
   }
}

//
// Private Allocation/Deallocation Functions
//

static SEXPR newnode( NodeKind kind )
{
   if ( nullp(FreeNodeList) )
   {
      MEMORY::gc();

      // don't wait till 0, before allocating a new block.
      //   make the threshold 1/5 of the NODE_BLOCK_SIZE.
      if ( MEMORY::FreeNodeCount < NODE_BLOCK_SIZE / 5 )
	 NewNodeBlock();
   }

   MEMORY::FreeNodeCount -= 1;

   SEXPR n = FreeNodeList;
   FreeNodeList = FreeNodeList->getnext();
   n->kind = kind;

   return n;
}


#ifdef OBJECT_CACHE

namespace MEMORY
{
   bool cache_copy = false;
}

MEMORY::VarPool cache( "cache", CACHE_BLOCK_SIZE );

unsigned MEMORY::CacheSwapCount = 0;
unsigned MEMORY::CacheSize      = cache.getsize();

unsigned MEMORY::get_cache_highwater() { return cache.getindex(); }

//
// Copy To/From Cache
//

#ifdef CACHE_FRAME
inline FRAME copy_frame( SEXPR n )
{
   FRAME fr = getenvframe(n);
   return reinterpret_cast<FRAME>( cache.copy_to_inactive( fr, getframesize(fr) ) );
}

inline FRAME tenure_frame( SEXPR n )
{
   // clone the old frame
   return MEMORY::frameStore.clone( getenvframe(n) );
}
#endif

//
// Aging
//

inline void increment_age( SEXPR n ) 
{ 
   if ( n->nage < CACHE_MAXAGE ) 
      ++n->nage; 
}

#endif


//
// Garbage Collection
//
//   GC consists of mark and sweep phases. The mark phase marks all nodes
// reachable from the execution environment. The sweep phase collects all
// unmarked objects into freelists.
//
//   Marking is a cooperative process. Marking clients have registered
// callbacks to be invoked during the marking phase of garbage collection.
// The success of this operation depends entirely upon the dutiful marking
// of client structures by the client. Failure to mark an essential object
// will lead to disaster.
//
//   Node space is managed in a NodeBlock "pool". Presently all objects
// are allocated from this uniform pool. This approached which uses a
// discriminated union is not ideal and definitely not object-oriented.
// But for now this is the implementation which stands. Adopting a
// purely object-orient approach will require considerable redesign.
//

#define markedp(n) ((n)->mark)
#define setmark(n) ((n)->mark = 1)
#define resetmark(n) ((n)->mark = 0)

int MEMORY::suspensions = 0;

static void badnode( SEXPR n )
{
   char buffer[80];
   SPRINTF(buffer, "bad node (%p,%d) during gc", n->id(), nodekind(n));
   ERROR::fatal(buffer);
}

void MEMORY::mark( SEXPR n )
{
   if ( nullp(n) || markedp(n) )
      return;

   switch ( nodekind(n) )
   {
      case n_cons:
	 setmark(n);
	 mark( getcar(n) );
	 mark( getcdr(n) );
	 break;
    
      case n_promise:
	 setmark(n);
	 mark( promise_getexp(n) );
	 mark( promise_getval(n) );
	 break;
    
      case n_code:
	 setmark(n);
	 mark( code_getbcodes(n) );
	 mark( code_getsexprs(n) );
	 break;
    
      case n_continuation:
	 setmark(n);
	 mark( cont_getstate(n) );
	 break;

      case n_environment:
      {
	 setmark(n);
         // frame
#ifdef CACHE_FRAME
         if ( cache_copy )
         {
            increment_age( n );
            if ( n->nage < CACHE_TENURE )
               setenvframe( n, copy_frame(n) );
            else if ( n->nage == CACHE_TENURE )
               setenvframe( n, tenure_frame(n) );
         }
#endif
         auto frame = getenvframe(n);
         mark( getframevars(frame) );
         mark( getframeclosure(frame) );
         const auto nslots = getframenslots(frame);
         for ( int i = 0; i < nslots; ++i )
            mark( frameref(frame, i) );
         // benv
	 mark( getenvbase(n) );
	 break;
      }
  
      case n_vector:
      {
	 setmark(n);
	 const auto length = getvectorlength(n);
	 for ( int i = 0; i < length; ++i )
	    mark( vectorref(n, i) );
	 break;
      }
  
      case n_closure:
      {
	 setmark(n);
	 mark( getclosurecode(n) );
	 mark( getclosurebenv(n) );
	 mark( getclosurevars(n) );
	 break;
      }

      case n_symbol:
	 setmark(n);
	 mark( getpair(n) );
	 break;
         
      case n_bvec:
	 setmark(n);
         break;
         
      case n_string:
	 setmark(n);
	 break;

      case n_string_port:
      case n_fixnum:
      case n_flonum:
      case n_port:
      case n_char:
      case n_func:
      case n_eval:
      case n_apply:
      case n_callcc:
      case n_map:
      case n_foreach:
      case n_force:
	 setmark(n);
	 break;

      case n_null:
	 // null is not allocated from node space
	 break;
   
      case n_free:
      default:
	 badnode(n);
	 break;
   }
}

void MEMORY::mark( TSTACK<SEXPR>& stack )
{
   const auto depth = stack.getdepth();
   for ( int i = 0; i < depth; ++i )
      mark( stack[i] );
}

static void sweep()
{
   FreeNodeList = null;
   MEMORY::FreeNodeCount = 0;

#ifdef GC_STATISTICS_DETAILED
   for ( auto& count : MEMORY::ReclamationCounts ) 
      count = 0;
#endif

   for ( auto block : blocks )
   {
      for ( auto& node : block->nodes )
      {
	 auto p = &node;

	 if ( markedp(p) )
	 {
	    resetmark(p);
	 }
	 else
	 {
	    // reclaim the node
	    switch ( nodekind(p) )
	    {
	       case n_symbol:
		  delete[] getname( p );
		  break;

	       case n_string:
		  delete[] getstringdata( p );
		  break;

	       case n_bvec:
		  delete[] getbvecdata( p );
		  break;

               case n_environment:
#ifdef CACHE_FRAME
                  if ( p->nage >= CACHE_TENURE )
#endif
                  MEMORY::frameStore.free( getenvframe(p) );
		  break;                  
                  
	       case n_vector:
		  delete[] getvectordata( p );
		  break;

	       case n_closure:
                  delete[] getclosuredata( p );
		  break;

               case n_port:
                  if ( getfile(p) != NULL )
                     fclose( getfile(p) );
                  break;

               case n_string_port:
                  delete getstringportstring( p );
                  break;

	       default:
		  break;
	    }

	    MEMORY::FreeNodeCount += 1;
#ifdef GC_STATISTICS_DETAILED
	    MEMORY::ReclamationCounts[nodekind(p)] += 1;
#endif
	    // minimal reinitialization and link
	    new (p) Node( n_free, FreeNodeList );

	    FreeNodeList = p;
	 }
      }
   }
}

#ifdef OBJECT_CACHE
void MEMORY::gc( bool copy )
#else
void MEMORY::gc()
#endif
{
   if (suspensions > 0)
      return;

   CollectionCount += 1;

#ifdef OBJECT_CACHE
   cache_copy = copy;

   if ( cache_copy )
   {
      cache.prep();
   }
#endif

   // mark memory managed roots
   mark( string_null );
   mark( vector_null );
   mark( listtail );
   mark( listhead );

   // notify all clients to mark their active roots
   for ( auto marker : markers )
      marker();

   // collect the unused nodes
   sweep();

#ifdef OBJECT_CACHE
   if ( cache_copy )
   {
      CacheSwapCount += 1;
      cache.swap();
      cache_copy = false;
   }
#endif

}

//
// Public Allocation Functions
//

SEXPR MEMORY::fixnum( FIXNUM fixnum )     // (<long int>)
{
   SEXPR n = newnode(n_fixnum);
   setfixnum(n, fixnum);
   return n;
}

SEXPR MEMORY::flonum( FLONUM flonum )     // (<double>)
{
   SEXPR n = newnode(n_flonum);
   setflonum(n, flonum);
   return n;
}

SEXPR MEMORY::character( CHAR ch )   // (<char>)
{
   SEXPR n = newnode(n_char);
   setcharacter(n, ch);
   return n;
}

namespace
{
   inline
   SEXPR new_symbol( const char* s, int length )
   {
      const auto size = length+1;
      auto name = new char[size];
      strcpy(name, s);
      // node space
      regstack.push( MEMORY::cons(null, null) );
      SEXPR n = newnode(n_symbol);
      setname(n, name);
      setpair( n, regstack.pop() );
      return n;
   }
}

SEXPR MEMORY::symbol( const char* s )      // (<name> <value>  <plist>)
{
   return new_symbol( s, strlen(s) );
}

SEXPR MEMORY::symbol( const std::string& s )      // (<name> <value>  <plist>)
{
   return new_symbol( s.c_str(), s.length() );
}

SEXPR MEMORY::string( UINT32 length )        // (<length> . "")
{
      // cache or heap
   auto data = new char[length+1];
   data[0] = '\0';
   SEXPR n = newnode(n_string);
   setstringlength(n, length);
   setstringdata(n, data);
   return n;
}

namespace
{
   inline
   SEXPR new_string( const char* s, int length )
   {
      if ( length == 0 )
      {
         return MEMORY::string_null;
      }
      else
      {
         SEXPR n = MEMORY::string( length );
         strcpy( getstringdata(n), s );
         return n;
      }
   }
}

SEXPR MEMORY::string( const char* s )
{
   return new_string( s, strlen(s) );
}

SEXPR MEMORY::string( const std::string& s )
{
   return new_string( s.c_str(), s.length() );
}

SEXPR MEMORY::string_port( SEXPR str )
{
   SEXPR n = newnode(n_string_port);
   setmode( n, 0 );
   setstringportstring( n, new std::string( getstringdata(str) ) );
   setstringportindex( n, 0 );
   return n;
}

SEXPR MEMORY::cons( SEXPR car, SEXPR cdr )  // (<car> . <cdr> )
{
   SEXPR n = newnode(n_cons);
   setcar(n, car);
   setcdr(n, cdr);
   return n;
}

SEXPR MEMORY::vector( UINT32 length )         // (<length> . data[])
{
   auto data = new SEXPR[length];
   for ( int i = 0; i < length; ++i )
      data[i] = null;
   SEXPR n = newnode(n_vector);
   setvectorlength(n, length);
   setvectordata(n, data);
   return n;
}

SEXPR MEMORY::continuation()
{
   SEXPR n = newnode(n_continuation);
   cont_setstate(n, null);
   return n;
}

SEXPR MEMORY::byte_vector( UINT32 length )                // (<byte-vector>)
{
   auto data = new BYTE[length];
   for ( int i = 0; i < length; ++i )
      data[i] = 0;
   // node space
   SEXPR n = newnode(n_bvec);
   setbveclength(n, length);
   setbvecdata(n, data);
   return n;
}

SEXPR MEMORY::prim( FUNCTION func, NodeKind kind )     // (<prim>)
{
   SEXPR n = newnode(kind);
   setfunc(n, func);
   return n;
}

SEXPR MEMORY::port( FILE* file, short mode )          // (<file>)
{
   SEXPR n = newnode(n_port);
   setfile(n, file);
   setmode(n, mode);
   return n;
}

SEXPR MEMORY::closure( SEXPR code, SEXPR env )       // ( <numv> [<code> <benv> <vars>] )
{
   auto data = new SEXPR[3];
   SEXPR n = newnode(n_closure);
   setclosuredata(n, data);
   setclosurecode(n, code);
   setclosurebenv(n, env);
   setclosurevars(n, null);
   return n;
}

SEXPR MEMORY::environment( UINT32 nvars, SEXPR vars, SEXPR env )   // (<frame> . <env>)
{
   // cache or heap
#ifdef CACHE_FRAME
   const auto ndwords = FRAMESIZE( nvars );
   auto frame = reinterpret_cast<FRAME>( cache.alloc( ndwords ) );
   setframesize( frame, ndwords );
   setframenslots( frame, nvars );
   for ( int i = 0; i < nvars; ++i )
      frameset( frame, i, null );
#else
   auto frame = frameStore.alloc( nvars );
#endif
   setframevars(frame, vars);
   setframeclosure(frame, null);
   SEXPR n = newnode(n_environment);
   setenvbase(n, env);
   setenvframe(n, frame);
   return n;
}

SEXPR MEMORY::promise( SEXPR exp )
{
   SEXPR n = newnode(n_promise);
   promise_setexp(n, exp);
   promise_setval(n, null);
   return n;
}

SEXPR MEMORY::code( SEXPR bcodes, SEXPR sexprs )
{
   SEXPR n = newnode(n_code);
   code_setbcodes(n, bcodes);
   code_setsexprs(n, sexprs);
   return n;
}

//
// Nullities
//
//   ()  -- null
//   ""  -- null string
//   #() -- null vector
//
//   note: the null object is not allocated from node space.
//

// the null object
SEXPR null = new Node( n_null );

void MEMORY::initialize()
{
   FreeNodeList = null;
   NewNodeBlock();
   string_null = string( 0u );
   vector_null = vector( 0u );
   listtail = null;
   listhead = cons(null, null);
}
