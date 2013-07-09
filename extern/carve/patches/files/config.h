#define CARVE_VERSION "2.0.0a"

#undef CARVE_DEBUG
#undef CARVE_DEBUG_WRITE_PLY_DATA

#if defined(__GNUC__)
#  if !defined(HAVE_BOOST_UNORDERED_COLLECTIONS)
#    define HAVE_TR1_UNORDERED_COLLECTIONS
#  endif

#  define HAVE_STDINT_H
#endif
