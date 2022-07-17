
#ifndef CERES_EXPORT_H
#define CERES_EXPORT_H

#ifdef CERES_STATIC_DEFINE
#  define CERES_EXPORT
#  define CERES_NO_EXPORT
#else
#  ifndef CERES_EXPORT
#    ifdef ceres_EXPORTS
        /* We are building this library */
#      define CERES_EXPORT 
#    else
        /* We are using this library */
#      define CERES_EXPORT 
#    endif
#  endif

#  ifndef CERES_NO_EXPORT
#    define CERES_NO_EXPORT 
#  endif
#endif

#ifndef CERES_DEPRECATED
#  define CERES_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef CERES_DEPRECATED_EXPORT
#  define CERES_DEPRECATED_EXPORT CERES_EXPORT CERES_DEPRECATED
#endif

#ifndef CERES_DEPRECATED_NO_EXPORT
#  define CERES_DEPRECATED_NO_EXPORT CERES_NO_EXPORT CERES_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef CERES_NO_DEPRECATED
#    define CERES_NO_DEPRECATED
#  endif
#endif

#endif /* CERES_EXPORT_H */
