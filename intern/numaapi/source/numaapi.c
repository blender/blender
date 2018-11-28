// Copyright (c) 2018, libnumaapi authors
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
//
// Author: Sergey Sharybin (sergey.vfx@gmail.com)

#include "numaapi.h"

#include <assert.h>

const char* numaAPI_ResultAsString(NUMAAPI_Result result) {
  switch (result) {
    case NUMAAPI_SUCCESS: return "SUCCESS";
    case NUMAAPI_NOT_AVAILABLE: return "NOT_AVAILABLE";
    case NUMAAPI_ERROR: return "ERROR";
    case NUMAAPI_ERROR_ATEXIT: return "ERROR_AT_EXIT";
  }
  assert(!"Unknown result was passed to numapi_ResultAsString().");
  return "UNKNOWN";
}
