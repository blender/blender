/* include/carve/config.h.  Generated from config.h.in by configure.  */
#pragma once

#include <math.h>

/* Define if using boost collections. Preferred, because the visual C++ unordered collections are slow and memory hungry. */
#define HAVE_BOOST_UNORDERED_COLLECTIONS 

#if defined(_MSC_VER)
#  pragma warning(disable:4201)
#endif

#include <math.h>

static inline double round(double value) {
  return (value >= 0) ? floor(value + 0.5) : ceil(value - 0.5);
}
