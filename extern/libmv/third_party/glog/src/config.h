/* src/config.h.  Generated from config.h.in by configure.  */
/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* Namespace for Google classes */
#if defined(__APPLE__)
 #include "config_mac.h"
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
 #include "config_freebsd.h"
#elif defined(__MINGW32__)
 #include "windows/config.h"
#elif defined(__linux__)
 #include "config_linux.h"
#elif defined(_MSC_VER)
 #include "windows/config.h"
#endif
