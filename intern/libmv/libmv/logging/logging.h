// Copyright (c) 2007, 2008, 2009 libmv authors.
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

#ifndef LIBMV_LOGGING_LOGGING_H
#define LIBMV_LOGGING_LOGGING_H

#include <glog/logging.h>

// Note on logging severity and verbosity level.
//
// Reserve LOG(INFO) for messages which are always to be put to log and don't
// use the INFO severity for the debugging/troubleshooting type of messages.
// Some reasoning behind:
//
//   - Library integration would want to disable "noisy" messages coming from
//     algorithms.
//
//   - It is not possible to disable INFO severity entirely: there is enough
//     of preparation being done for the message stream. What is even worse
//     is that such stream preparation causes measurable time spent in spin
//     lock, ruining multi-threading.

#define LG VLOG(1)

#endif  // LIBMV_LOGGING_LOGGING_H
