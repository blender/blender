#define CARVE_VERSION "2.0.0a"

#undef CARVE_DEBUG
#undef CARVE_DEBUG_WRITE_PLY_DATA

#if defined(__GNUC__)
#  if !defined(HAVE_BOOST_UNORDERED_COLLECTIONS)
#    define HAVE_TR1_UNORDERED_COLLECTIONS
#  endif

#  define HAVE_STDINT_H
#endif

// Support for latest Clang/LLVM on FreeBSD which does have different libcxx.
//
// TODO(sergey): Move it some some more generic header with platform-specific
//               declarations.

// Indicates whether __is_heap is available
#undef HAVE_IS_HEAP

#ifdef __GNUC__
// NeyBSD doesn't have __is_heap
#  ifndef __NetBSD__
#    define HAVE_IS_HEAP
#    ifdef _LIBCPP_VERSION
#      define __is_heap is_heap
#    endif  // _LIBCPP_VERSION
#  endif  // !__NetBSD__
#endif  // __GNUC__
