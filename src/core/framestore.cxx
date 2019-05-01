#include "framestore.hxx"

#include <cstring>

#include "sexpr.hxx"
#include "error.hxx"

//
// Frame Store
//

FRAME FrameStore::alloc( UINT32 nslots )
{
   // allocate a frame with all slots defined
   //    framenslots = nslots
   //    framevars = null
   //    frameclosure = null
   //    frameslots = {null}
   //    framesize = sizeof(header) + sizeof(slots)
   FRAME frame;
   
   if ( (nslots < store.size()) && store[nslots] )
   {
      // reuse an existing frame
      frame = store[nslots];
      store[nslots] = frame->next;
      count[nslots] -= 1;
      
      if ( getframenslots(frame) != nslots )
         ERROR::fatal( "recycled frame size inconsistent with request" );
   }
   else
   {
      // allocate a new frame from heap
      const size_t frameSize = FRAMESIZE_NDW( nslots );   
      frame = (FRAME) new DWORD[frameSize];
      
      setframesize( frame, (UINT32)frameSize );
      setframenslots( frame, nslots );
   }
   
   setframevars( frame, null );
   setframeclosure( frame, null );
   
   for ( int i = 0; i < nslots; ++i )
      frameset( frame, i, null );
   
   return frame;
}

FRAME FrameStore::clone( FRAME fr )
{
   FRAME frame;
   const auto nslots = getframenslots(fr);
   
   if ( (nslots < store.size()) && store[nslots] )
   {
      // reuse an existing frame
      frame = store[nslots];
      store[nslots] = frame->next;
      count[nslots] -= 1;
      
      if ( getframenslots(frame) != nslots )
         ERROR::fatal( "recycled frame size inconsistent with request" );
   }
   else
   {
      // allocate a new frame from heap
      frame = (FRAME) new SEXPR[getframesize(fr)];
   }
      
   return (FRAME)std::memcpy( frame, fr, NBYTES(getframesize(fr)) );
}

void FrameStore::free( FRAME frame )
{
   // some frames might be nullptrs
   
   if ( frame )
   {
      const UINT32 nslots = frame->nslots;
      
      if ( nslots < store.size() )
      {
         frame->next = store[nslots];
         store[nslots] = frame;
         count[nslots] += 1;
      }
      else
      {
         delete frame;
      }
   }
}
