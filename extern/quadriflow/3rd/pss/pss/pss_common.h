/*
  Copyright (C) 2014 Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
  * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/
#include <utility>
#include <iterator>

namespace pss {

namespace internal {

//! Destroy sequence [xs,xe)
template<class RandomAccessIterator>
void serial_destroy( RandomAccessIterator zs, RandomAccessIterator ze ) {
    typedef typename std::iterator_traits<RandomAccessIterator>::value_type T;
    while( zs!=ze ) {
        --ze;
        (*ze).~T();
    }
}

//! Merge sequences [xs,xe) and [ys,ye) to output sequence [zs,(xe-xs)+(ye-ys)), using std::move
template<class RandomAccessIterator1, class RandomAccessIterator2, class RandomAccessIterator3, class Compare>
void serial_move_merge( RandomAccessIterator1 xs, RandomAccessIterator1 xe, RandomAccessIterator2 ys, RandomAccessIterator2 ye, RandomAccessIterator3 zs, Compare comp ) {
    if( xs!=xe ) {
        if( ys!=ye ) {
            for(;;) {
                if( comp(*ys,*xs) ) {
                    *zs = std::move(*ys);
                    ++zs;
                    if( ++ys==ye ) { break; }
                } else {
                    *zs = std::move(*xs);
                    ++zs;
                    if( ++xs==xe ) { goto movey; }
                }
            }
        }
        ys = xs;
        ye = xe;
    }
movey:
    std::move( ys, ye, zs );
}

template<typename RandomAccessIterator1, typename RandomAccessIterator2, typename Compare>
void stable_sort_base_case(  RandomAccessIterator1 xs, RandomAccessIterator1 xe, RandomAccessIterator2 zs, int inplace, Compare comp) {
    std::stable_sort( xs, xe, comp );
    if( inplace!=2 ) {
        RandomAccessIterator2 ze = zs + (xe-xs);
        typedef typename std::iterator_traits<RandomAccessIterator2>::value_type T;
        if( inplace )
            // Initialize the temporary buffer
            for( ; zs<ze; ++zs )
                new(&*zs) T;
        else
            // Initialize the temporary buffer and move keys to it.
            for( ; zs<ze; ++xs, ++zs )
                new(&*zs) T(std::move(*xs));
    }
}

//! Raw memory buffer with automatic cleanup.
class raw_buffer {
    void* ptr;
public:
    //! Try to obtain buffer of given size.
    raw_buffer( size_t bytes ) : ptr( operator new(bytes,std::nothrow) ) {}
    //! True if buffer was successfully obtained, zero otherwise.
    operator bool() const {return ptr;}
    //! Return pointer to buffer, or  NULL if buffer could not be obtained.
    void* get() const {return ptr;}
    //! Destroy buffer
    ~raw_buffer() {operator delete(ptr);}
};

} // namespace internal

} // namespace pss
