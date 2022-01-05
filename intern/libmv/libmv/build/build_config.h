// Copyright (c) 2020 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef LIBMV_BUILD_BUILD_CONFIG_H_
#define LIBMV_BUILD_BUILD_CONFIG_H_

#include <climits>
#include <cstdint>

// Initially is based on Chromium's build_config.h, with tweaks and extensions
// needed for this project.
//
// NOTE: All commonly used symbols (which are checked on a "top" level, from
// outside of any platform-specific ifdef block) are to be explicitly defined
// to 0 when they are not "active". This is extra lines of code in this file,
// but is not being edited that often. Such approach helps catching cases when
// one attempted to access build configuration variable without including the
// header by simply using -Wundef compiler attribute.
//
// NOTE: Not having things explicitly defined to 0 is harmless (in terms it
// follows same rules as Google projects) and will simply cause compiler to
// become more noisy, which is simple to correct.

////////////////////////////////////////////////////////////////////////////////
// A set of macros to use for platform detection.

#if defined(__native_client__)
// __native_client__ must be first, so that other OS_ defines are not set.
#  define OS_NACL 1
// OS_NACL comes in two sandboxing technology flavors, SFI or Non-SFI.
// PNaCl toolchain defines __native_client_nonsfi__ macro in Non-SFI build
// mode, while it does not in SFI build mode.
#  if defined(__native_client_nonsfi__)
#    define OS_NACL_NONSFI
#  else
#    define OS_NACL_SFI
#  endif
#elif defined(_AIX)
#  define OS_AIX 1
#elif defined(ANDROID)
#  define OS_ANDROID 1
#elif defined(__APPLE__)
// Only include TargetConditions after testing ANDROID as some android builds
// on mac don't have this header available and it's not needed unless the target
// is really mac/ios.
#  include <TargetConditionals.h>
#  define OS_MACOSX 1
#  if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#    define OS_IOS 1
#  endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#elif defined(__HAIKU__)
#  define OS_HAIKU 1
#elif defined(__hpux)
#  define OS_HPUX 1
#elif defined(__linux__)
#  define OS_LINUX 1
// Include a system header to pull in features.h for glibc/uclibc macros.
#  include <unistd.h>
#  if defined(__GLIBC__) && !defined(__UCLIBC__)
// We really are using glibc, not uClibc pretending to be glibc.
#    define LIBC_GLIBC 1
#  endif
#elif defined(__sgi)
#  define OS_IRIX 1
#elif defined(_WIN32)
#  define OS_WIN 1
#elif defined(__Fuchsia__)
#  define OS_FUCHSIA 1
#elif defined(__FreeBSD__)
#  define OS_FREEBSD 1
#elif defined(__NetBSD__)
#  define OS_NETBSD 1
#elif defined(__OpenBSD__)
#  define OS_OPENBSD 1
#elif defined(__sun)
#  define OS_SOLARIS 1
#elif defined(__QNXNTO__)
#  define OS_QNX 1
#elif defined(__asmjs__) || defined(__wasm__)
#  define OS_ASMJS
#else
#  error Please add support for your platform in build/build_config.h
#endif

#if !defined(OS_AIX)
#  define OS_AIX 0
#endif
#if !defined(OS_ASMJS)
#  define OS_ASMJS 0
#endif
#if !defined(OS_NACL)
#  define OS_NACL 0
#endif
#if !defined(OS_NACL_NONSFI)
#  define OS_NACL_NONSFI 0
#endif
#if !defined(OS_NACL_SFI)
#  define OS_NACL_SFI 0
#endif
#if !defined(OS_ANDROID)
#  define OS_ANDROID 0
#endif
#if !defined(OS_MACOSX)
#  define OS_MACOSX 0
#endif
#if !defined(OS_IOS)
#  define OS_IOS 0
#endif
#if !defined(OS_HAIKU)
#  define OS_HAIKU 0
#endif
#if !defined(OS_HPUX)
#  define OS_HPUX 0
#endif
#if !defined(OS_IRIX)
#  define OS_IRIX 0
#endif
#if !defined(OS_LINUX)
#  define OS_LINUX 0
#endif
#if !defined(LIBC_GLIBC)
#  define LIBC_GLIBC 0
#endif
#if !defined(OS_WIN)
#  define OS_WIN 0
#endif
#if !defined(OS_FUCHSIA)
#  define OS_FUCHSIA 0
#endif
#if !defined(OS_FREEBSD)
#  define OS_FREEBSD 0
#endif
#if !defined(OS_NETBSD)
#  define OS_NETBSD 0
#endif
#if !defined(OS_OPENBSD)
#  define OS_OPENBSD 0
#endif
#if !defined(OS_SOLARIS)
#  define OS_SOLARIS 0
#endif
#if !defined(OS_QNX)
#  define OS_QNX 0
#endif

////////////////////////////////////////////////////////////////////////////////
// Apple OS family detection.

#if defined(OS_MAC) || defined(OS_IOS)
#  define OS_APPLE 1
#else
#  define OS_APPLE 0
#endif

////////////////////////////////////////////////////////////////////////////////
// *BSD OS family detection.
//
// For access to standard BSD features, use OS_BSD instead of a
// more specific macro.

#if OS_FREEBSD || OS_OPENBSD || OS_NETBSD
#  define OS_BSD 1
#else
#  define OS_BSD 0
#endif

////////////////////////////////////////////////////////////////////////////////
// POSIX system detection.
//
// For access to standard POSIXish features use OS_POSIX instead of a
// more specific macro.

#if OS_AIX || OS_ANDROID || OS_ASMJS || OS_FREEBSD || OS_LINUX || OS_MACOSX || \
    OS_NACL || OS_NETBSD || OS_OPENBSD || OS_QNX || OS_SOLARIS
#  define OS_POSIX 1
#else
#  define OS_POSIX 0
#endif

////////////////////////////////////////////////////////////////////////////////
// Compiler detection, including its capabilities.

#if defined(__clang__)
#  define COMPILER_CLANG 1
#elif defined(__GNUC__)
#  define COMPILER_GCC 1
#  define COMPILER_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#elif defined(_MSC_VER)
#  define COMPILER_MSVC 1
#  define COMPILER_MSVC_VERSION (_MSC_VER)
#elif defined(__MINGW32__)
#  define COMPILER_MINGW32 1
#elif defined(__MINGW64__)
#  define COMPILER_MINGW64 1
#else
#  error Please add support for your compiler in build/build_config.h
#endif

#if !defined(COMPILER_CLANG)
#  define COMPILER_CLANG 0
#endif
#if !defined(COMPILER_GCC)
#  define COMPILER_GCC 0
#endif
#if !defined(COMPILER_MSVC)
#  define COMPILER_MSVC 0
#endif
#if !defined(COMPILER_MINGW32)
#  define COMPILER_MINGW32 0
#endif
#if !defined(COMPILER_MINGW64)
#  define COMPILER_MINGW64 0
#endif

// Compiler is any of MinGW family.
#if COMPILER_MINGW32 || COMPILER_MINGW64
#  define COMPILER_MINGW 1
#else
#  define COMPILER_MINGW 0
#endif

// Check what is the latest C++ specification the compiler supports.
//
// NOTE: Use explicit definition here to avoid expansion-to-defined warning from
// being generated. While this will most likely a false-positive warning in this
// particular case, that warning might be helpful to catch errors elsewhere.

// C++11 check.
#if ((defined(__cplusplus) && (__cplusplus > 199711L)) ||                      \
     (defined(_MSC_VER) && (_MSC_VER >= 1800)))
#  define COMPILER_SUPPORTS_CXX11 1
#else
#  define COMPILER_SUPPORTS_CXX11 0
#endif
// C++14 check.
#if (defined(__cplusplus) && (__cplusplus > 201311L))
#  define COMPILER_SUPPORTS_CXX14 1
#else
#  define COMPILER_SUPPORTS_CXX14 0
#endif
// C++17 check.
#if (defined(__cplusplus) && (__cplusplus > 201611L))
#  define COMPILER_SUPPORTS_CXX17 1
#else
#  define COMPILER_SUPPORTS_CXX17 0
#endif
// C++20 check.
#if (defined(__cplusplus) && (__cplusplus > 201911L))
#  define COMPILER_SUPPORTS_CXX20 1
#else
#  define COMPILER_SUPPORTS_CXX20 0
#endif

////////////////////////////////////////////////////////////////////////////////
// Processor architecture detection.
//
// For more info on what's defined, see:
//
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#  define ARCH_CPU_X86_FAMILY 1
#  define ARCH_CPU_X86_64 1
#  define ARCH_CPU_64_BITS 1
#  define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
#  define ARCH_CPU_X86_FAMILY 1
#  define ARCH_CPU_X86 1
#  define ARCH_CPU_32_BITS 1
#  define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__s390x__)
#  define ARCH_CPU_S390_FAMILY 1
#  define ARCH_CPU_S390X 1
#  define ARCH_CPU_64_BITS 1
#  define ARCH_CPU_BIG_ENDIAN 1
#elif defined(__s390__)
#  define ARCH_CPU_S390_FAMILY 1
#  define ARCH_CPU_S390 1
#  define ARCH_CPU_31_BITS 1
#  define ARCH_CPU_BIG_ENDIAN 1
#elif (defined(__PPC64__) || defined(__PPC__)) && /* NOLINT */                 \
    defined(__BIG_ENDIAN__)                       // NOLINT
#  define ARCH_CPU_PPC64_FAMILY 1
#  define ARCH_CPU_PPC64 1
#  define ARCH_CPU_64_BITS 1
#  define ARCH_CPU_BIG_ENDIAN 1
#elif defined(__PPC64__)
#  define ARCH_CPU_PPC64_FAMILY 1
#  define ARCH_CPU_PPC64 1
#  define ARCH_CPU_64_BITS 1
#  define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__ARMEL__)
#  define ARCH_CPU_ARM_FAMILY 1
#  define ARCH_CPU_ARMEL 1
#  define ARCH_CPU_32_BITS 1
#  define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define ARCH_CPU_ARM_FAMILY 1
#  define ARCH_CPU_ARM64 1
#  define ARCH_CPU_64_BITS 1
#  define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__pnacl__) || defined(__asmjs__) || defined(__wasm__)
#  define ARCH_CPU_32_BITS 1
#  define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__MIPSEL__)
#  if defined(__LP64__)
#    define ARCH_CPU_MIPS_FAMILY 1
#    define ARCH_CPU_MIPS64EL 1
#    define ARCH_CPU_64_BITS 1
#    define ARCH_CPU_LITTLE_ENDIAN 1
#  else
#    define ARCH_CPU_MIPS_FAMILY 1
#    define ARCH_CPU_MIPSEL 1
#    define ARCH_CPU_32_BITS 1
#    define ARCH_CPU_LITTLE_ENDIAN 1
#  endif
#elif defined(__MIPSEB__)
#  if defined(__LP64__)
#    define ARCH_CPU_MIPS_FAMILY 1
#    define ARCH_CPU_MIPS64 1
#    define ARCH_CPU_64_BITS 1
#    define ARCH_CPU_BIG_ENDIAN 1
#  else
#    define ARCH_CPU_MIPS_FAMILY 1
#    define ARCH_CPU_MIPS 1
#    define ARCH_CPU_32_BITS 1
#    define ARCH_CPU_BIG_ENDIAN 1
#  endif
#else
#  error Please add support for your architecture in build/build_config.h
#endif

#if !defined(ARCH_CPU_LITTLE_ENDIAN)
#  define ARCH_CPU_LITTLE_ENDIAN 0
#endif
#if !defined(ARCH_CPU_BIG_ENDIAN)
#  define ARCH_CPU_BIG_ENDIAN 0
#endif

#if !defined(ARCH_CPU_32_BITS)
#  define ARCH_CPU_32_BITS 0
#endif
#if !defined(ARCH_CPU_64_BITS)
#  define ARCH_CPU_64_BITS 0
#endif

#if !defined(ARCH_CPU_X86_FAMILY)
#  define ARCH_CPU_X86_FAMILY 0
#endif
#if !defined(ARCH_CPU_ARM_FAMILY)
#  define ARCH_CPU_ARM_FAMILY 0
#endif
#if !defined(ARCH_CPU_MIPS_FAMILY)
#  define ARCH_CPU_MIPS_FAMILY 0
#endif
#if !defined(ARCH_CPU_PPC64_FAMILY)
#  define ARCH_CPU_PPC64_FAMILY 0
#endif
#if !defined(ARCH_CPU_S390_FAMILY)
#  define ARCH_CPU_S390_FAMILY 0
#endif

#endif  // LIBMV_BUILD_BUILD_CONFIG_H_
