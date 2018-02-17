#include <cstdio>

#include "argstack.hxx"
#include "error.hxx"

//
// Argument Stack
//

ARGSTACK argstack( ARGSTACK_SIZE );

//
// Argument Stack Iterator
//

void ArgstackIterator::reset()
{
   argc = argstack.argc;
   argp = argstack.getfirstargindex();
}  
  
void ArgstackIterator::show()
{
   printf("argstackiterator: argc=%d, argp=%d\n", argc, argp);
}