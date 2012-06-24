/* for BLENDER - we hand-maintain this,
 * for the original OpenJpeg package it is generated,
 * the endian check is a blender define */

/* create config.h for CMake */
#define PACKAGE_VERSION "1.5.0"

#define HAVE_INTTYPES_H
#define HAVE_MEMORY_H
#define HAVE_STDINT_H
#define HAVE_STDLIB_H
#define HAVE_STRINGS_H
#define HAVE_STRING_H
#define HAVE_SYS_STAT_H
#define HAVE_SYS_TYPES_H
#define HAVE_UNISTD_H
/* #define HAVE_LIBPNG */
/* #define HAVE_PNG_H */
/* #define HAVE_LIBTIFF */
/* #define HAVE_TIFF_H */

/* #undef HAVE_LIBLCMS1 */
/* #undef HAVE_LIBLCMS2 */
/* #undef HAVE_LCMS1_H */
/* #undef HAVE_LCMS2_H */

/* Byte order.  */
/* All compilers that support Mac OS X define either __BIG_ENDIAN__ or
__LITTLE_ENDIAN__ to match the endianness of the architecture being
compiled for. This is not necessarily the same as the architecture of the
machine doing the building. In order to support Universal Binaries on
Mac OS X, we prefer those defines to decide the endianness.
On other platforms we use the result of the TRY_RUN. */
#if defined(__BIG_ENDIAN__)
#  define OPJ_BIG_ENDIAN
#else
#  undef OPJ_BIG_ENDIAN
#endif
