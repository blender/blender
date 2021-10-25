/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* define if you have google gflags library */
#define HAVE_LIB_GFLAGS 1

/* Namespace for Google classes */
#define GOOGLE_NAMESPACE google

/* Stops putting the code inside the Google namespace */
#define _END_GOOGLE_NAMESPACE_ }

/* Puts following code inside the Google namespace */
#define _START_GOOGLE_NAMESPACE_ namespace google {

#if defined(__MINGW32__) || (defined(_MSC_VER) && (_MSC_VER >= 1900))
#  define HAVE_SNPRINTF
#endif

/* Always the empty-string on non-windows systems. On windows, should be
   "__declspec(dllexport)". This way, when we compile the dll, we export our
   functions/classes. It's safe to define this here because config.h is only
   used internally, to compile the DLL, and every DLL source file #includes
   "config.h" before anything else. */
#ifndef GOOGLE_GLOG_DLL_DECL
# define GOOGLE_GLOG_IS_A_DLL  1   /* not set if you're statically linking */
# define GOOGLE_GLOG_DLL_DECL  __declspec(dllexport)
# define GOOGLE_GLOG_DLL_DECL_FOR_UNITTESTS  __declspec(dllimport)
#endif
