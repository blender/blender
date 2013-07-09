// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: keir@google.com (Keir Mierle)
//
// Really simple file IO.

#include "ceres/file.h"

#include <cstdio>
#include "glog/logging.h"

namespace ceres {
namespace internal {

using std::string;

void WriteStringToFileOrDie(const string &data, const string &filename) {
  FILE* file_descriptor = fopen(filename.c_str(), "wb");
  if (!file_descriptor) {
    LOG(FATAL) << "Couldn't write to file: " << filename;
  }
  fwrite(data.c_str(), 1, data.size(), file_descriptor);
  fclose(file_descriptor);
}

void ReadFileToStringOrDie(const string &filename, string *data) {
  FILE* file_descriptor = fopen(filename.c_str(), "r");

  if (!file_descriptor) {
    LOG(FATAL) << "Couldn't read file: " << filename;
  }

  // Resize the input buffer appropriately.
  fseek(file_descriptor, 0L, SEEK_END);
  int num_bytes = ftell(file_descriptor);
  data->resize(num_bytes);

  // Read the data.
  fseek(file_descriptor, 0L, SEEK_SET);
  int num_read = fread(&((*data)[0]),
                       sizeof((*data)[0]),
                       num_bytes,
                       file_descriptor);
  if (num_read != num_bytes) {
    LOG(FATAL) << "Couldn't read all of " << filename
               << "expected bytes: " << num_bytes * sizeof((*data)[0])
               << "actual bytes: " << num_read;
  }
  fclose(file_descriptor);
}

string JoinPath(const string& dirname, const string& basename) {
#ifdef _WIN32
    static const char separator = '\\';
#else
    static const char separator = '/';
#endif  // _WIN32

  if ((!basename.empty() && basename[0] == separator) || dirname.empty()) {
    return basename;
  } else if (dirname[dirname.size() - 1] == separator) {
    return dirname + basename;
  } else {
    return dirname + string(&separator, 1) + basename;
  }
}

}  // namespace internal
}  // namespace ceres
