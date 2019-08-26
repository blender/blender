/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#ifndef LEMON_ASSERT_H
#define LEMON_ASSERT_H

/// \ingroup exceptions
/// \file
/// \brief Extended assertion handling

#include <lemon/error.h>

namespace lemon {

  inline void assert_fail_abort(const char *file, int line,
                                const char *function, const char* message,
                                const char *assertion)
  {
    std::cerr << file << ":" << line << ": ";
    if (function)
      std::cerr << function << ": ";
    std::cerr << message;
    if (assertion)
      std::cerr << " (assertion '" << assertion << "' failed)";
    std::cerr << std::endl;
    std::abort();
  }

  namespace _assert_bits {


    inline const char* cstringify(const std::string& str) {
      return str.c_str();
    }

    inline const char* cstringify(const char* str) {
      return str;
    }
  }
}

#endif // LEMON_ASSERT_H

#undef LEMON_ASSERT
#undef LEMON_DEBUG

#if (defined(LEMON_ASSERT_ABORT) ? 1 : 0) +               \
  (defined(LEMON_ASSERT_CUSTOM) ? 1 : 0) > 1
#error "LEMON assertion system is not set properly"
#endif

#if ((defined(LEMON_ASSERT_ABORT) ? 1 : 0) +            \
     (defined(LEMON_ASSERT_CUSTOM) ? 1 : 0) == 1 ||     \
     defined(LEMON_ENABLE_ASSERTS)) &&                  \
  (defined(LEMON_DISABLE_ASSERTS) ||                    \
   defined(NDEBUG))
#error "LEMON assertion system is not set properly"
#endif


#if defined LEMON_ASSERT_ABORT
#  undef LEMON_ASSERT_HANDLER
#  define LEMON_ASSERT_HANDLER ::lemon::assert_fail_abort
#elif defined LEMON_ASSERT_CUSTOM
#  undef LEMON_ASSERT_HANDLER
#  ifndef LEMON_CUSTOM_ASSERT_HANDLER
#    error "LEMON_CUSTOM_ASSERT_HANDLER is not set"
#  endif
#  define LEMON_ASSERT_HANDLER LEMON_CUSTOM_ASSERT_HANDLER
#elif defined LEMON_DISABLE_ASSERTS
#  undef LEMON_ASSERT_HANDLER
#elif defined NDEBUG
#  undef LEMON_ASSERT_HANDLER
#else
#  define LEMON_ASSERT_HANDLER ::lemon::assert_fail_abort
#endif

#ifndef LEMON_FUNCTION_NAME
#  if defined __GNUC__
#    define LEMON_FUNCTION_NAME (__PRETTY_FUNCTION__)
#  elif defined _MSC_VER
#    define LEMON_FUNCTION_NAME (__FUNCSIG__)
#  elif __STDC_VERSION__ >= 199901L
#    define LEMON_FUNCTION_NAME (__func__)
#  else
#    define LEMON_FUNCTION_NAME ("<unknown>")
#  endif
#endif

#ifdef DOXYGEN

/// \ingroup exceptions
///
/// \brief Macro for assertion with customizable message
///
/// Macro for assertion with customizable message.
/// \param exp An expression that must be convertible to \c bool.  If it is \c
/// false, then an assertion is raised. The concrete behaviour depends on the
/// settings of the assertion system.
/// \param msg A <tt>const char*</tt> parameter, which can be used to provide
/// information about the circumstances of the failed assertion.
///
/// The assertions are enabled in the default behaviour.
/// You can disable them with the following code:
/// \code
/// #define LEMON_DISABLE_ASSERTS
/// \endcode
/// or with compilation parameters:
/// \code
/// g++ -DLEMON_DISABLE_ASSERTS
/// make CXXFLAGS='-DLEMON_DISABLE_ASSERTS'
/// \endcode
/// The checking is also disabled when the standard macro \c NDEBUG is defined.
///
/// As a default behaviour the failed assertion prints a short log message to
/// the standard error and aborts the execution.
///
/// However, the following modes can be used in the assertion system:
/// - \c LEMON_ASSERT_ABORT The failed assertion prints a short log message to
///   the standard error and aborts the program. It is the default behaviour.
/// - \c LEMON_ASSERT_CUSTOM The user can define own assertion handler
///   function.
///   \code
///     void custom_assert_handler(const char* file, int line,
///                                const char* function, const char* message,
///                                const char* assertion);
///   \endcode
///   The name of the function should be defined as the \c
///   LEMON_CUSTOM_ASSERT_HANDLER macro name.
///   \code
///     #define LEMON_CUSTOM_ASSERT_HANDLER custom_assert_handler
///   \endcode
///   Whenever an assertion is occured, the custom assertion
///   handler is called with appropiate parameters.
///
/// The assertion mode can also be changed within one compilation unit.
/// If the macros are redefined with other settings and the
/// \ref lemon/assert.h "assert.h" file is reincluded, then the
/// behaviour is changed appropiately to the new settings.
#  define LEMON_ASSERT(exp, msg)                                        \
  (static_cast<void> (!!(exp) ? 0 : (                                   \
    LEMON_ASSERT_HANDLER(__FILE__, __LINE__,                            \
                         LEMON_FUNCTION_NAME,                           \
                         ::lemon::_assert_bits::cstringify(msg), #exp), 0)))

/// \ingroup exceptions
///
/// \brief Macro for internal assertions
///
/// Macro for internal assertions, it is used in the library to check
/// the consistency of results of algorithms, several pre- and
/// postconditions and invariants. The checking is disabled by
/// default, but it can be turned on with the macro \c
/// LEMON_ENABLE_DEBUG.
/// \code
/// #define LEMON_ENABLE_DEBUG
/// \endcode
/// or with compilation parameters:
/// \code
/// g++ -DLEMON_ENABLE_DEBUG
/// make CXXFLAGS='-DLEMON_ENABLE_DEBUG'
/// \endcode
///
/// This macro works like the \c LEMON_ASSERT macro, therefore the
/// current behaviour depends on the settings of \c LEMON_ASSERT
/// macro.
///
/// \see LEMON_ASSERT
#  define LEMON_DEBUG(exp, msg)                                         \
  (static_cast<void> (!!(exp) ? 0 : (                                   \
    LEMON_ASSERT_HANDLER(__FILE__, __LINE__,                            \
                         LEMON_FUNCTION_NAME,                           \
                         ::lemon::_assert_bits::cstringify(msg), #exp), 0)))

#else

#  ifndef LEMON_ASSERT_HANDLER
#    define LEMON_ASSERT(exp, msg)  (static_cast<void>(0))
#    define LEMON_DEBUG(exp, msg) (static_cast<void>(0))
#  else
#    define LEMON_ASSERT(exp, msg)                                      \
       (static_cast<void> (!!(exp) ? 0 : (                              \
        LEMON_ASSERT_HANDLER(__FILE__, __LINE__,                        \
                             LEMON_FUNCTION_NAME,                       \
                             ::lemon::_assert_bits::cstringify(msg),    \
                             #exp), 0)))
#    if defined LEMON_ENABLE_DEBUG
#      define LEMON_DEBUG(exp, msg)                                     \
         (static_cast<void> (!!(exp) ? 0 : (                            \
           LEMON_ASSERT_HANDLER(__FILE__, __LINE__,                     \
                                LEMON_FUNCTION_NAME,                    \
                                ::lemon::_assert_bits::cstringify(msg), \
                                #exp), 0)))
#    else
#      define LEMON_DEBUG(exp, msg) (static_cast<void>(0))
#    endif
#  endif

#endif
