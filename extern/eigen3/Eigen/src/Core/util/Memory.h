// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2008-2009 Benoit Jacob <jacob.benoit.1@gmail.com>
// Copyright (C) 2009 Kenneth Riddile <kfriddile@yahoo.com>
// Copyright (C) 2010 Hauke Heibel <hauke.heibel@gmail.com>
// Copyright (C) 2010 Thomas Capricelli <orzel@freehackers.org>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.


/*****************************************************************************
*** Platform checks for aligned malloc functions                           ***
*****************************************************************************/

#ifndef EIGEN_MEMORY_H
#define EIGEN_MEMORY_H

// On 64-bit systems, glibc's malloc returns 16-byte-aligned pointers, see:
//   http://www.gnu.org/s/libc/manual/html_node/Aligned-Memory-Blocks.html
// This is true at least since glibc 2.8.
// This leaves the question how to detect 64-bit. According to this document,
//   http://gcc.fyxm.net/summit/2003/Porting%20to%2064%20bit.pdf
// page 114, "[The] LP64 model [...] is used by all 64-bit UNIX ports" so it's indeed
// quite safe, at least within the context of glibc, to equate 64-bit with LP64.
#if defined(__GLIBC__) && ((__GLIBC__>=2 && __GLIBC_MINOR__ >= 8) || __GLIBC__>2) \
 && defined(__LP64__)
  #define EIGEN_GLIBC_MALLOC_ALREADY_ALIGNED 1
#else
  #define EIGEN_GLIBC_MALLOC_ALREADY_ALIGNED 0
#endif

// FreeBSD 6 seems to have 16-byte aligned malloc
//   See http://svn.freebsd.org/viewvc/base/stable/6/lib/libc/stdlib/malloc.c?view=markup
// FreeBSD 7 seems to have 16-byte aligned malloc except on ARM and MIPS architectures
//   See http://svn.freebsd.org/viewvc/base/stable/7/lib/libc/stdlib/malloc.c?view=markup
#if defined(__FreeBSD__) && !defined(__arm__) && !defined(__mips__)
  #define EIGEN_FREEBSD_MALLOC_ALREADY_ALIGNED 1
#else
  #define EIGEN_FREEBSD_MALLOC_ALREADY_ALIGNED 0
#endif

#if defined(__APPLE__) \
 || defined(_WIN64) \
 || EIGEN_GLIBC_MALLOC_ALREADY_ALIGNED \
 || EIGEN_FREEBSD_MALLOC_ALREADY_ALIGNED
  #define EIGEN_MALLOC_ALREADY_ALIGNED 1
#else
  #define EIGEN_MALLOC_ALREADY_ALIGNED 0
#endif

#if ((defined __QNXNTO__) || (defined _GNU_SOURCE) || ((defined _XOPEN_SOURCE) && (_XOPEN_SOURCE >= 600))) \
 && (defined _POSIX_ADVISORY_INFO) && (_POSIX_ADVISORY_INFO > 0)
  #define EIGEN_HAS_POSIX_MEMALIGN 1
#else
  #define EIGEN_HAS_POSIX_MEMALIGN 0
#endif

#ifdef EIGEN_VECTORIZE_SSE
  #define EIGEN_HAS_MM_MALLOC 1
#else
  #define EIGEN_HAS_MM_MALLOC 0
#endif

namespace internal {

/*****************************************************************************
*** Implementation of handmade aligned functions                           ***
*****************************************************************************/

/* ----- Hand made implementations of aligned malloc/free and realloc ----- */

/** \internal Like malloc, but the returned pointer is guaranteed to be 16-byte aligned.
  * Fast, but wastes 16 additional bytes of memory. Does not throw any exception.
  */
inline void* handmade_aligned_malloc(size_t size)
{
  void *original = std::malloc(size+16);
  if (original == 0) return 0;
  void *aligned = reinterpret_cast<void*>((reinterpret_cast<size_t>(original) & ~(size_t(15))) + 16);
  *(reinterpret_cast<void**>(aligned) - 1) = original;
  return aligned;
}

/** \internal Frees memory allocated with handmade_aligned_malloc */
inline void handmade_aligned_free(void *ptr)
{
  if (ptr) std::free(*(reinterpret_cast<void**>(ptr) - 1));
}

/** \internal
  * \brief Reallocates aligned memory.
  * Since we know that our handmade version is based on std::realloc
  * we can use std::realloc to implement efficient reallocation.
  */
inline void* handmade_aligned_realloc(void* ptr, size_t size, size_t = 0)
{
  if (ptr == 0) return handmade_aligned_malloc(size);
  void *original = *(reinterpret_cast<void**>(ptr) - 1);
  original = std::realloc(original,size+16);
  if (original == 0) return 0;
  void *aligned = reinterpret_cast<void*>((reinterpret_cast<size_t>(original) & ~(size_t(15))) + 16);
  *(reinterpret_cast<void**>(aligned) - 1) = original;
  return aligned;
}

/*****************************************************************************
*** Implementation of generic aligned realloc (when no realloc can be used)***
*****************************************************************************/

void* aligned_malloc(size_t size);
void  aligned_free(void *ptr);

/** \internal
  * \brief Reallocates aligned memory.
  * Allows reallocation with aligned ptr types. This implementation will
  * always create a new memory chunk and copy the old data.
  */
inline void* generic_aligned_realloc(void* ptr, size_t size, size_t old_size)
{
  if (ptr==0)
    return aligned_malloc(size);

  if (size==0)
  {
    aligned_free(ptr);
    return 0;
  }

  void* newptr = aligned_malloc(size);
  if (newptr == 0)
  {
    #ifdef EIGEN_HAS_ERRNO
    errno = ENOMEM; // according to the standard
    #endif
    return 0;
  }

  if (ptr != 0)
  {
    std::memcpy(newptr, ptr, std::min(size,old_size));
    aligned_free(ptr);
  }

  return newptr;
}

/*****************************************************************************
*** Implementation of portable aligned versions of malloc/free/realloc     ***
*****************************************************************************/

#ifdef EIGEN_NO_MALLOC
inline void check_that_malloc_is_allowed()
{
  eigen_assert(false && "heap allocation is forbidden (EIGEN_NO_MALLOC is defined)");
}
#elif defined EIGEN_RUNTIME_NO_MALLOC
inline bool is_malloc_allowed_impl(bool update, bool new_value = false)
{
  static bool value = true;
  if (update == 1)
    value = new_value;
  return value;
}
inline bool is_malloc_allowed() { return is_malloc_allowed_impl(false); }
inline bool set_is_malloc_allowed(bool new_value) { return is_malloc_allowed_impl(true, new_value); }
inline void check_that_malloc_is_allowed()
{
  eigen_assert(is_malloc_allowed() && "heap allocation is forbidden (EIGEN_RUNTIME_NO_MALLOC is defined and g_is_malloc_allowed is false)");
}
#else 
inline void check_that_malloc_is_allowed()
{}
#endif

/** \internal Allocates \a size bytes. The returned pointer is guaranteed to have 16 bytes alignment.
  * On allocation error, the returned pointer is null, and if exceptions are enabled then a std::bad_alloc is thrown.
  */
inline void* aligned_malloc(size_t size)
{
  check_that_malloc_is_allowed();

  void *result;
  #if !EIGEN_ALIGN
    result = std::malloc(size);
  #elif EIGEN_MALLOC_ALREADY_ALIGNED
    result = std::malloc(size);
  #elif EIGEN_HAS_POSIX_MEMALIGN
    if(posix_memalign(&result, 16, size)) result = 0;
  #elif EIGEN_HAS_MM_MALLOC
    result = _mm_malloc(size, 16);
  #elif (defined _MSC_VER)
    result = _aligned_malloc(size, 16);
  #else
    result = handmade_aligned_malloc(size);
  #endif

  #ifdef EIGEN_EXCEPTIONS
    if(result == 0)
      throw std::bad_alloc();
  #endif
  return result;
}

/** \internal Frees memory allocated with aligned_malloc. */
inline void aligned_free(void *ptr)
{
  #if !EIGEN_ALIGN
    std::free(ptr);
  #elif EIGEN_MALLOC_ALREADY_ALIGNED
    std::free(ptr);
  #elif EIGEN_HAS_POSIX_MEMALIGN
    std::free(ptr);
  #elif EIGEN_HAS_MM_MALLOC
    _mm_free(ptr);
  #elif defined(_MSC_VER)
    _aligned_free(ptr);
  #else
    handmade_aligned_free(ptr);
  #endif
}

/**
* \internal
* \brief Reallocates an aligned block of memory.
* \throws std::bad_alloc if EIGEN_EXCEPTIONS are defined.
**/
inline void* aligned_realloc(void *ptr, size_t new_size, size_t old_size)
{
  EIGEN_UNUSED_VARIABLE(old_size);

  void *result;
#if !EIGEN_ALIGN
  result = std::realloc(ptr,new_size);
#elif EIGEN_MALLOC_ALREADY_ALIGNED
  result = std::realloc(ptr,new_size);
#elif EIGEN_HAS_POSIX_MEMALIGN
  result = generic_aligned_realloc(ptr,new_size,old_size);
#elif EIGEN_HAS_MM_MALLOC
  // The defined(_mm_free) is just here to verify that this MSVC version
  // implements _mm_malloc/_mm_free based on the corresponding _aligned_
  // functions. This may not always be the case and we just try to be safe.
  #if defined(_MSC_VER) && defined(_mm_free)
    result = _aligned_realloc(ptr,new_size,16);
  #else
    result = generic_aligned_realloc(ptr,new_size,old_size);
  #endif
#elif defined(_MSC_VER)
  result = _aligned_realloc(ptr,new_size,16);
#else
  result = handmade_aligned_realloc(ptr,new_size,old_size);
#endif

#ifdef EIGEN_EXCEPTIONS
  if (result==0 && new_size!=0)
    throw std::bad_alloc();
#endif
  return result;
}

/*****************************************************************************
*** Implementation of conditionally aligned functions                      ***
*****************************************************************************/

/** \internal Allocates \a size bytes. If Align is true, then the returned ptr is 16-byte-aligned.
  * On allocation error, the returned pointer is null, and if exceptions are enabled then a std::bad_alloc is thrown.
  */
template<bool Align> inline void* conditional_aligned_malloc(size_t size)
{
  return aligned_malloc(size);
}

template<> inline void* conditional_aligned_malloc<false>(size_t size)
{
  check_that_malloc_is_allowed();

  void *result = std::malloc(size);
  #ifdef EIGEN_EXCEPTIONS
    if(!result) throw std::bad_alloc();
  #endif
  return result;
}

/** \internal Frees memory allocated with conditional_aligned_malloc */
template<bool Align> inline void conditional_aligned_free(void *ptr)
{
  aligned_free(ptr);
}

template<> inline void conditional_aligned_free<false>(void *ptr)
{
  std::free(ptr);
}

template<bool Align> inline void* conditional_aligned_realloc(void* ptr, size_t new_size, size_t old_size)
{
  return aligned_realloc(ptr, new_size, old_size);
}

template<> inline void* conditional_aligned_realloc<false>(void* ptr, size_t new_size, size_t)
{
  return std::realloc(ptr, new_size);
}

/*****************************************************************************
*** Construction/destruction of array elements                             ***
*****************************************************************************/

/** \internal Constructs the elements of an array.
  * The \a size parameter tells on how many objects to call the constructor of T.
  */
template<typename T> inline T* construct_elements_of_array(T *ptr, size_t size)
{
  for (size_t i=0; i < size; ++i) ::new (ptr + i) T;
  return ptr;
}

/** \internal Destructs the elements of an array.
  * The \a size parameters tells on how many objects to call the destructor of T.
  */
template<typename T> inline void destruct_elements_of_array(T *ptr, size_t size)
{
  // always destruct an array starting from the end.
  if(ptr)
    while(size) ptr[--size].~T();
}

/*****************************************************************************
*** Implementation of aligned new/delete-like functions                    ***
*****************************************************************************/

/** \internal Allocates \a size objects of type T. The returned pointer is guaranteed to have 16 bytes alignment.
  * On allocation error, the returned pointer is undefined, but if exceptions are enabled then a std::bad_alloc is thrown.
  * The default constructor of T is called.
  */
template<typename T> inline T* aligned_new(size_t size)
{
  T *result = reinterpret_cast<T*>(aligned_malloc(sizeof(T)*size));
  return construct_elements_of_array(result, size);
}

template<typename T, bool Align> inline T* conditional_aligned_new(size_t size)
{
  T *result = reinterpret_cast<T*>(conditional_aligned_malloc<Align>(sizeof(T)*size));
  return construct_elements_of_array(result, size);
}

/** \internal Deletes objects constructed with aligned_new
  * The \a size parameters tells on how many objects to call the destructor of T.
  */
template<typename T> inline void aligned_delete(T *ptr, size_t size)
{
  destruct_elements_of_array<T>(ptr, size);
  aligned_free(ptr);
}

/** \internal Deletes objects constructed with conditional_aligned_new
  * The \a size parameters tells on how many objects to call the destructor of T.
  */
template<typename T, bool Align> inline void conditional_aligned_delete(T *ptr, size_t size)
{
  destruct_elements_of_array<T>(ptr, size);
  conditional_aligned_free<Align>(ptr);
}

template<typename T, bool Align> inline T* conditional_aligned_realloc_new(T* pts, size_t new_size, size_t old_size)
{
  if(new_size < old_size)
    destruct_elements_of_array(pts+new_size, old_size-new_size);
  T *result = reinterpret_cast<T*>(conditional_aligned_realloc<Align>(reinterpret_cast<void*>(pts), sizeof(T)*new_size, sizeof(T)*old_size));
  if(new_size > old_size)
    construct_elements_of_array(result+old_size, new_size-old_size);
  return result;
}


template<typename T, bool Align> inline T* conditional_aligned_new_auto(size_t size)
{
  T *result = reinterpret_cast<T*>(conditional_aligned_malloc<Align>(sizeof(T)*size));
  if(NumTraits<T>::RequireInitialization)
    construct_elements_of_array(result, size);
  return result;
}

template<typename T, bool Align> inline T* conditional_aligned_realloc_new_auto(T* pts, size_t new_size, size_t old_size)
{
  if(NumTraits<T>::RequireInitialization && (new_size < old_size))
    destruct_elements_of_array(pts+new_size, old_size-new_size);
  T *result = reinterpret_cast<T*>(conditional_aligned_realloc<Align>(reinterpret_cast<void*>(pts), sizeof(T)*new_size, sizeof(T)*old_size));
  if(NumTraits<T>::RequireInitialization && (new_size > old_size))
    construct_elements_of_array(result+old_size, new_size-old_size);
  return result;
}

template<typename T, bool Align> inline void conditional_aligned_delete_auto(T *ptr, size_t size)
{
  if(NumTraits<T>::RequireInitialization)
    destruct_elements_of_array<T>(ptr, size);
  conditional_aligned_free<Align>(ptr);
}

/****************************************************************************/

/** \internal Returns the index of the first element of the array that is well aligned for vectorization.
  *
  * \param array the address of the start of the array
  * \param size the size of the array
  *
  * \note If no element of the array is well aligned, the size of the array is returned. Typically,
  * for example with SSE, "well aligned" means 16-byte-aligned. If vectorization is disabled or if the
  * packet size for the given scalar type is 1, then everything is considered well-aligned.
  *
  * \note If the scalar type is vectorizable, we rely on the following assumptions: sizeof(Scalar) is a
  * power of 2, the packet size in bytes is also a power of 2, and is a multiple of sizeof(Scalar). On the
  * other hand, we do not assume that the array address is a multiple of sizeof(Scalar), as that fails for
  * example with Scalar=double on certain 32-bit platforms, see bug #79.
  *
  * There is also the variant first_aligned(const MatrixBase&) defined in DenseCoeffsBase.h.
  */
template<typename Scalar, typename Index>
inline static Index first_aligned(const Scalar* array, Index size)
{
  typedef typename packet_traits<Scalar>::type Packet;
  enum { PacketSize = packet_traits<Scalar>::size,
         PacketAlignedMask = PacketSize-1
  };

  if(PacketSize==1)
  {
    // Either there is no vectorization, or a packet consists of exactly 1 scalar so that all elements
    // of the array have the same alignment.
    return 0;
  }
  else if(size_t(array) & (sizeof(Scalar)-1))
  {
    // There is vectorization for this scalar type, but the array is not aligned to the size of a single scalar.
    // Consequently, no element of the array is well aligned.
    return size;
  }
  else
  {
    return std::min<Index>( (PacketSize - (Index((size_t(array)/sizeof(Scalar))) & PacketAlignedMask))
                           & PacketAlignedMask, size);
  }
}

} // end namespace internal

/*****************************************************************************
*** Implementation of runtime stack allocation (falling back to malloc)    ***
*****************************************************************************/

/** \internal
  * Allocates an aligned buffer of SIZE bytes on the stack if SIZE is smaller than
  * EIGEN_STACK_ALLOCATION_LIMIT, and if stack allocation is supported by the platform
  * (currently, this is Linux only). Otherwise the memory is allocated on the heap.
  * Data allocated with ei_aligned_stack_alloc \b must be freed by calling
  * ei_aligned_stack_free(PTR,SIZE).
  * \code
  * float * data = ei_aligned_stack_alloc(float,array.size());
  * // ...
  * ei_aligned_stack_free(data,float,array.size());
  * \endcode
  */
#if (defined __linux__)
  #define ei_aligned_stack_alloc(SIZE) (SIZE<=EIGEN_STACK_ALLOCATION_LIMIT) \
                                    ? alloca(SIZE) \
                                    : Eigen::internal::aligned_malloc(SIZE)
  #define ei_aligned_stack_free(PTR,SIZE) if(SIZE>EIGEN_STACK_ALLOCATION_LIMIT) Eigen::internal::aligned_free(PTR)
#elif defined(_MSC_VER)
  #define ei_aligned_stack_alloc(SIZE) (SIZE<=EIGEN_STACK_ALLOCATION_LIMIT) \
                                    ? _alloca(SIZE) \
                                    : Eigen::internal::aligned_malloc(SIZE)
  #define ei_aligned_stack_free(PTR,SIZE) if(SIZE>EIGEN_STACK_ALLOCATION_LIMIT) Eigen::internal::aligned_free(PTR)
#else
  #define ei_aligned_stack_alloc(SIZE) Eigen::internal::aligned_malloc(SIZE)
  #define ei_aligned_stack_free(PTR,SIZE) Eigen::internal::aligned_free(PTR)
#endif

#define ei_aligned_stack_new(TYPE,SIZE) Eigen::internal::construct_elements_of_array(reinterpret_cast<TYPE*>(ei_aligned_stack_alloc(sizeof(TYPE)*SIZE)), SIZE)
#define ei_aligned_stack_delete(TYPE,PTR,SIZE) do {Eigen::internal::destruct_elements_of_array<TYPE>(PTR, SIZE); \
                                                   ei_aligned_stack_free(PTR,sizeof(TYPE)*SIZE);} while(0)


/*****************************************************************************
*** Implementation of EIGEN_MAKE_ALIGNED_OPERATOR_NEW [_IF]                ***
*****************************************************************************/

#if EIGEN_ALIGN
  #ifdef EIGEN_EXCEPTIONS
    #define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_NOTHROW(NeedsToAlign) \
      void* operator new(size_t size, const std::nothrow_t&) throw() { \
        try { return Eigen::internal::conditional_aligned_malloc<NeedsToAlign>(size); } \
        catch (...) { return 0; } \
        return 0; \
      }
  #else
    #define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_NOTHROW(NeedsToAlign) \
      void* operator new(size_t size, const std::nothrow_t&) throw() { \
        return Eigen::internal::conditional_aligned_malloc<NeedsToAlign>(size); \
      }
  #endif

  #define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(NeedsToAlign) \
      void *operator new(size_t size) { \
        return Eigen::internal::conditional_aligned_malloc<NeedsToAlign>(size); \
      } \
      void *operator new[](size_t size) { \
        return Eigen::internal::conditional_aligned_malloc<NeedsToAlign>(size); \
      } \
      void operator delete(void * ptr) throw() { Eigen::internal::conditional_aligned_free<NeedsToAlign>(ptr); } \
      void operator delete[](void * ptr) throw() { Eigen::internal::conditional_aligned_free<NeedsToAlign>(ptr); } \
      /* in-place new and delete. since (at least afaik) there is no actual   */ \
      /* memory allocated we can safely let the default implementation handle */ \
      /* this particular case. */ \
      static void *operator new(size_t size, void *ptr) { return ::operator new(size,ptr); } \
      void operator delete(void * memory, void *ptr) throw() { return ::operator delete(memory,ptr); } \
      /* nothrow-new (returns zero instead of std::bad_alloc) */ \
      EIGEN_MAKE_ALIGNED_OPERATOR_NEW_NOTHROW(NeedsToAlign) \
      void operator delete(void *ptr, const std::nothrow_t&) throw() { \
        Eigen::internal::conditional_aligned_free<NeedsToAlign>(ptr); \
      } \
      typedef void eigen_aligned_operator_new_marker_type;
#else
  #define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(NeedsToAlign)
#endif

#define EIGEN_MAKE_ALIGNED_OPERATOR_NEW EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(true)
#define EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF_VECTORIZABLE_FIXED_SIZE(Scalar,Size) \
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW_IF(((Size)!=Eigen::Dynamic) && ((sizeof(Scalar)*(Size))%16==0))

/****************************************************************************/

/** \class aligned_allocator
* \ingroup Core_Module
*
* \brief STL compatible allocator to use with with 16 byte aligned types
*
* Example:
* \code
* // Matrix4f requires 16 bytes alignment:
* std::map< int, Matrix4f, std::less<int>, 
*           aligned_allocator<std::pair<const int, Matrix4f> > > my_map_mat4;
* // Vector3f does not require 16 bytes alignment, no need to use Eigen's allocator:
* std::map< int, Vector3f > my_map_vec3;
* \endcode
*
* \sa \ref TopicStlContainers.
*/
template<class T>
class aligned_allocator
{
public:
    typedef size_t    size_type;
    typedef std::ptrdiff_t difference_type;
    typedef T*        pointer;
    typedef const T*  const_pointer;
    typedef T&        reference;
    typedef const T&  const_reference;
    typedef T         value_type;

    template<class U>
    struct rebind
    {
        typedef aligned_allocator<U> other;
    };

    pointer address( reference value ) const
    {
        return &value;
    }

    const_pointer address( const_reference value ) const
    {
        return &value;
    }

    aligned_allocator() throw()
    {
    }

    aligned_allocator( const aligned_allocator& ) throw()
    {
    }

    template<class U>
    aligned_allocator( const aligned_allocator<U>& ) throw()
    {
    }

    ~aligned_allocator() throw()
    {
    }

    size_type max_size() const throw()
    {
        return std::numeric_limits<size_type>::max();
    }

    pointer allocate( size_type num, const_pointer* hint = 0 )
    {
        static_cast<void>( hint ); // suppress unused variable warning
        return static_cast<pointer>( internal::aligned_malloc( num * sizeof(T) ) );
    }

    void construct( pointer p, const T& value )
    {
        ::new( p ) T( value );
    }

    void destroy( pointer p )
    {
        p->~T();
    }

    void deallocate( pointer p, size_type /*num*/ )
    {
        internal::aligned_free( p );
    }

    bool operator!=(const aligned_allocator<T>& ) const
    { return false; }

    bool operator==(const aligned_allocator<T>& ) const
    { return true; }
};

//---------- Cache sizes ----------

#if defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
#  if defined(__PIC__) && defined(__i386__)
     // Case for x86 with PIC
#    define EIGEN_CPUID(abcd,func,id) \
       __asm__ __volatile__ ("xchgl %%ebx, %%esi;cpuid; xchgl %%ebx,%%esi": "=a" (abcd[0]), "=S" (abcd[1]), "=c" (abcd[2]), "=d" (abcd[3]) : "a" (func), "c" (id));
#  else
     // Case for x86_64 or x86 w/o PIC
#    define EIGEN_CPUID(abcd,func,id) \
       __asm__ __volatile__ ("cpuid": "=a" (abcd[0]), "=b" (abcd[1]), "=c" (abcd[2]), "=d" (abcd[3]) : "a" (func), "c" (id) );
#  endif
#elif defined(_MSC_VER)
#  if (_MSC_VER > 1500)
#    define EIGEN_CPUID(abcd,func,id) __cpuidex((int*)abcd,func,id)
#  endif
#endif

namespace internal {

#ifdef EIGEN_CPUID

inline bool cpuid_is_vendor(int abcd[4], const char* vendor)
{
  return abcd[1]==((int*)(vendor))[0] && abcd[3]==((int*)(vendor))[1] && abcd[2]==((int*)(vendor))[2];
}

inline void queryCacheSizes_intel_direct(int& l1, int& l2, int& l3)
{
  int abcd[4];
  l1 = l2 = l3 = 0;
  int cache_id = 0;
  int cache_type = 0;
  do {
    abcd[0] = abcd[1] = abcd[2] = abcd[3] = 0;
    EIGEN_CPUID(abcd,0x4,cache_id);
    cache_type  = (abcd[0] & 0x0F) >> 0;
    if(cache_type==1||cache_type==3) // data or unified cache
    {
      int cache_level = (abcd[0] & 0xE0) >> 5;  // A[7:5]
      int ways        = (abcd[1] & 0xFFC00000) >> 22; // B[31:22]
      int partitions  = (abcd[1] & 0x003FF000) >> 12; // B[21:12]
      int line_size   = (abcd[1] & 0x00000FFF) >>  0; // B[11:0]
      int sets        = (abcd[2]);                    // C[31:0]

      int cache_size = (ways+1) * (partitions+1) * (line_size+1) * (sets+1);

      switch(cache_level)
      {
        case 1: l1 = cache_size; break;
        case 2: l2 = cache_size; break;
        case 3: l3 = cache_size; break;
        default: break;
      }
    }
    cache_id++;
  } while(cache_type>0 && cache_id<16);
}

inline void queryCacheSizes_intel_codes(int& l1, int& l2, int& l3)
{
  int abcd[4];
  abcd[0] = abcd[1] = abcd[2] = abcd[3] = 0;
  l1 = l2 = l3 = 0;
  EIGEN_CPUID(abcd,0x00000002,0);
  unsigned char * bytes = reinterpret_cast<unsigned char *>(abcd)+2;
  bool check_for_p2_core2 = false;
  for(int i=0; i<14; ++i)
  {
    switch(bytes[i])
    {
      case 0x0A: l1 = 8; break;   // 0Ah   data L1 cache, 8 KB, 2 ways, 32 byte lines
      case 0x0C: l1 = 16; break;  // 0Ch   data L1 cache, 16 KB, 4 ways, 32 byte lines
      case 0x0E: l1 = 24; break;  // 0Eh   data L1 cache, 24 KB, 6 ways, 64 byte lines
      case 0x10: l1 = 16; break;  // 10h   data L1 cache, 16 KB, 4 ways, 32 byte lines (IA-64)
      case 0x15: l1 = 16; break;  // 15h   code L1 cache, 16 KB, 4 ways, 32 byte lines (IA-64)
      case 0x2C: l1 = 32; break;  // 2Ch   data L1 cache, 32 KB, 8 ways, 64 byte lines
      case 0x30: l1 = 32; break;  // 30h   code L1 cache, 32 KB, 8 ways, 64 byte lines
      case 0x60: l1 = 16; break;  // 60h   data L1 cache, 16 KB, 8 ways, 64 byte lines, sectored
      case 0x66: l1 = 8; break;   // 66h   data L1 cache, 8 KB, 4 ways, 64 byte lines, sectored
      case 0x67: l1 = 16; break;  // 67h   data L1 cache, 16 KB, 4 ways, 64 byte lines, sectored
      case 0x68: l1 = 32; break;  // 68h   data L1 cache, 32 KB, 4 ways, 64 byte lines, sectored
      case 0x1A: l2 = 96; break;   // code and data L2 cache, 96 KB, 6 ways, 64 byte lines (IA-64)
      case 0x22: l3 = 512; break;   // code and data L3 cache, 512 KB, 4 ways (!), 64 byte lines, dual-sectored
      case 0x23: l3 = 1024; break;   // code and data L3 cache, 1024 KB, 8 ways, 64 byte lines, dual-sectored
      case 0x25: l3 = 2048; break;   // code and data L3 cache, 2048 KB, 8 ways, 64 byte lines, dual-sectored
      case 0x29: l3 = 4096; break;   // code and data L3 cache, 4096 KB, 8 ways, 64 byte lines, dual-sectored
      case 0x39: l2 = 128; break;   // code and data L2 cache, 128 KB, 4 ways, 64 byte lines, sectored
      case 0x3A: l2 = 192; break;   // code and data L2 cache, 192 KB, 6 ways, 64 byte lines, sectored
      case 0x3B: l2 = 128; break;   // code and data L2 cache, 128 KB, 2 ways, 64 byte lines, sectored
      case 0x3C: l2 = 256; break;   // code and data L2 cache, 256 KB, 4 ways, 64 byte lines, sectored
      case 0x3D: l2 = 384; break;   // code and data L2 cache, 384 KB, 6 ways, 64 byte lines, sectored
      case 0x3E: l2 = 512; break;   // code and data L2 cache, 512 KB, 4 ways, 64 byte lines, sectored
      case 0x40: l2 = 0; break;   // no integrated L2 cache (P6 core) or L3 cache (P4 core)
      case 0x41: l2 = 128; break;   // code and data L2 cache, 128 KB, 4 ways, 32 byte lines
      case 0x42: l2 = 256; break;   // code and data L2 cache, 256 KB, 4 ways, 32 byte lines
      case 0x43: l2 = 512; break;   // code and data L2 cache, 512 KB, 4 ways, 32 byte lines
      case 0x44: l2 = 1024; break;   // code and data L2 cache, 1024 KB, 4 ways, 32 byte lines
      case 0x45: l2 = 2048; break;   // code and data L2 cache, 2048 KB, 4 ways, 32 byte lines
      case 0x46: l3 = 4096; break;   // code and data L3 cache, 4096 KB, 4 ways, 64 byte lines
      case 0x47: l3 = 8192; break;   // code and data L3 cache, 8192 KB, 8 ways, 64 byte lines
      case 0x48: l2 = 3072; break;   // code and data L2 cache, 3072 KB, 12 ways, 64 byte lines
      case 0x49: if(l2!=0) l3 = 4096; else {check_for_p2_core2=true; l3 = l2 = 4096;} break;// code and data L3 cache, 4096 KB, 16 ways, 64 byte lines (P4) or L2 for core2
      case 0x4A: l3 = 6144; break;   // code and data L3 cache, 6144 KB, 12 ways, 64 byte lines
      case 0x4B: l3 = 8192; break;   // code and data L3 cache, 8192 KB, 16 ways, 64 byte lines
      case 0x4C: l3 = 12288; break;   // code and data L3 cache, 12288 KB, 12 ways, 64 byte lines
      case 0x4D: l3 = 16384; break;   // code and data L3 cache, 16384 KB, 16 ways, 64 byte lines
      case 0x4E: l2 = 6144; break;   // code and data L2 cache, 6144 KB, 24 ways, 64 byte lines
      case 0x78: l2 = 1024; break;   // code and data L2 cache, 1024 KB, 4 ways, 64 byte lines
      case 0x79: l2 = 128; break;   // code and data L2 cache, 128 KB, 8 ways, 64 byte lines, dual-sectored
      case 0x7A: l2 = 256; break;   // code and data L2 cache, 256 KB, 8 ways, 64 byte lines, dual-sectored
      case 0x7B: l2 = 512; break;   // code and data L2 cache, 512 KB, 8 ways, 64 byte lines, dual-sectored
      case 0x7C: l2 = 1024; break;   // code and data L2 cache, 1024 KB, 8 ways, 64 byte lines, dual-sectored
      case 0x7D: l2 = 2048; break;   // code and data L2 cache, 2048 KB, 8 ways, 64 byte lines
      case 0x7E: l2 = 256; break;   // code and data L2 cache, 256 KB, 8 ways, 128 byte lines, sect. (IA-64)
      case 0x7F: l2 = 512; break;   // code and data L2 cache, 512 KB, 2 ways, 64 byte lines
      case 0x80: l2 = 512; break;   // code and data L2 cache, 512 KB, 8 ways, 64 byte lines
      case 0x81: l2 = 128; break;   // code and data L2 cache, 128 KB, 8 ways, 32 byte lines
      case 0x82: l2 = 256; break;   // code and data L2 cache, 256 KB, 8 ways, 32 byte lines
      case 0x83: l2 = 512; break;   // code and data L2 cache, 512 KB, 8 ways, 32 byte lines
      case 0x84: l2 = 1024; break;   // code and data L2 cache, 1024 KB, 8 ways, 32 byte lines
      case 0x85: l2 = 2048; break;   // code and data L2 cache, 2048 KB, 8 ways, 32 byte lines
      case 0x86: l2 = 512; break;   // code and data L2 cache, 512 KB, 4 ways, 64 byte lines
      case 0x87: l2 = 1024; break;   // code and data L2 cache, 1024 KB, 8 ways, 64 byte lines
      case 0x88: l3 = 2048; break;   // code and data L3 cache, 2048 KB, 4 ways, 64 byte lines (IA-64)
      case 0x89: l3 = 4096; break;   // code and data L3 cache, 4096 KB, 4 ways, 64 byte lines (IA-64)
      case 0x8A: l3 = 8192; break;   // code and data L3 cache, 8192 KB, 4 ways, 64 byte lines (IA-64)
      case 0x8D: l3 = 3072; break;   // code and data L3 cache, 3072 KB, 12 ways, 128 byte lines (IA-64)

      default: break;
    }
  }
  if(check_for_p2_core2 && l2 == l3)
    l3 = 0;
  l1 *= 1024;
  l2 *= 1024;
  l3 *= 1024;
}

inline void queryCacheSizes_intel(int& l1, int& l2, int& l3, int max_std_funcs)
{
  if(max_std_funcs>=4)
    queryCacheSizes_intel_direct(l1,l2,l3);
  else
    queryCacheSizes_intel_codes(l1,l2,l3);
}

inline void queryCacheSizes_amd(int& l1, int& l2, int& l3)
{
  int abcd[4];
  abcd[0] = abcd[1] = abcd[2] = abcd[3] = 0;
  EIGEN_CPUID(abcd,0x80000005,0);
  l1 = (abcd[2] >> 24) * 1024; // C[31:24] = L1 size in KB
  abcd[0] = abcd[1] = abcd[2] = abcd[3] = 0;
  EIGEN_CPUID(abcd,0x80000006,0);
  l2 = (abcd[2] >> 16) * 1024; // C[31;16] = l2 cache size in KB
  l3 = ((abcd[3] & 0xFFFC000) >> 18) * 512 * 1024; // D[31;18] = l3 cache size in 512KB
}
#endif

/** \internal
 * Queries and returns the cache sizes in Bytes of the L1, L2, and L3 data caches respectively */
inline void queryCacheSizes(int& l1, int& l2, int& l3)
{
  #ifdef EIGEN_CPUID
  int abcd[4];

  // identify the CPU vendor
  EIGEN_CPUID(abcd,0x0,0);
  int max_std_funcs = abcd[1];
  if(cpuid_is_vendor(abcd,"GenuineIntel"))
    queryCacheSizes_intel(l1,l2,l3,max_std_funcs);
  else if(cpuid_is_vendor(abcd,"AuthenticAMD") || cpuid_is_vendor(abcd,"AMDisbetter!"))
    queryCacheSizes_amd(l1,l2,l3);
  else
    // by default let's use Intel's API
    queryCacheSizes_intel(l1,l2,l3,max_std_funcs);

  // here is the list of other vendors:
//   ||cpuid_is_vendor(abcd,"VIA VIA VIA ")
//   ||cpuid_is_vendor(abcd,"CyrixInstead")
//   ||cpuid_is_vendor(abcd,"CentaurHauls")
//   ||cpuid_is_vendor(abcd,"GenuineTMx86")
//   ||cpuid_is_vendor(abcd,"TransmetaCPU")
//   ||cpuid_is_vendor(abcd,"RiseRiseRise")
//   ||cpuid_is_vendor(abcd,"Geode by NSC")
//   ||cpuid_is_vendor(abcd,"SiS SiS SiS ")
//   ||cpuid_is_vendor(abcd,"UMC UMC UMC ")
//   ||cpuid_is_vendor(abcd,"NexGenDriven")
  #else
  l1 = l2 = l3 = -1;
  #endif
}

/** \internal
 * \returns the size in Bytes of the L1 data cache */
inline int queryL1CacheSize()
{
  int l1(-1), l2, l3;
  queryCacheSizes(l1,l2,l3);
  return l1;
}

/** \internal
 * \returns the size in Bytes of the L2 or L3 cache if this later is present */
inline int queryTopLevelCacheSize()
{
  int l1, l2(-1), l3(-1);
  queryCacheSizes(l1,l2,l3);
  return std::max(l2,l3);
}

} // end namespace internal

#endif // EIGEN_MEMORY_H
