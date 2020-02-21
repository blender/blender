

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011-2020 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Helper functions to handle file IO
 *
 ******************************************************************************/

#include "mantaio.h"

#if NO_ZLIB != 1
extern "C" {
#  include <zlib.h>
}

#  if defined(WIN32) || defined(_WIN32)
#    include <windows.h>
#    include <string>
#  endif

using namespace std;

namespace Manta {

#  if defined(WIN32) || defined(_WIN32)
static wstring stringToWstring(const char *str)
{
  const int length_wc = MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), NULL, 0);
  wstring strWide(length_wc, 0);
  MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), &strWide[0], length_wc);
  return strWide;
}
#  endif

void *safeGzopen(const char *filename, const char *mode)
{
  gzFile gzfile;

#  if defined(WIN32) || defined(_WIN32)
  wstring filenameWide = stringToWstring(filename);
  gzfile = gzopen_w(filenameWide.c_str(), mode);
#  else
  gzfile = gzopen(filename, mode);
#  endif

  return gzfile;
}
#endif

}  // namespace
