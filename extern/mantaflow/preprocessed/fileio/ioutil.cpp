

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

namespace Manta {

//! helper to handle non ascii filenames correctly, mainly problematic on windows
void *safeGzopen(const char *filename, const char *mode)
{
  gzFile gzfile;
#  if 0
  UTF16_ENCODE(filename);

  // gzopen_w() is supported since zlib v1.2.7
  gzfile = gzopen_w(filename_16, mode);
  UTF16_UN_ENCODE(filename);
#  else
  gzfile = gzopen(filename, mode);
#  endif
  return gzfile;
}
#endif

}  // namespace
