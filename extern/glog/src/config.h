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
#elif defined(__GNU__)
 #include "config_hurd.h"
#elif defined(__HAIKU__)
 #include "config_haiku.h"
#endif
