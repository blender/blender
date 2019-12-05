// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef DRACO_CORE_DRACO_TEST_UTILS_H_
#define DRACO_CORE_DRACO_TEST_UTILS_H_

#include "draco/core/draco_test_base.h"
#include "draco/io/mesh_io.h"
#include "draco/io/point_cloud_io.h"

namespace draco {

// Returns the full path to a given file system entry, such as test file or test
// directory.
std::string GetTestFileFullPath(const std::string &entry_name);

// Returns the full path to a given temporary file (a location where tests store
// generated files).
std::string GetTestTempFileFullPath(const std::string &file_name);

// Generates a new golden file and saves it into the correct folder.
// Returns false if the file couldn't be created.
bool GenerateGoldenFile(const std::string &golden_file_name, const void *data,
                        int data_size);

// Compare a golden file content with the input data.
// Function will log the first byte position where the data differ.
// Returns false if there are any differences.
bool CompareGoldenFile(const std::string &golden_file_name, const void *data,
                       int data_size);

// Loads a mesh / point cloud specified by a |file_name| that is going to be
// automatically converted to the correct path available to the testing
// instance.
inline std::unique_ptr<Mesh> ReadMeshFromTestFile(
    const std::string &file_name) {
  const std::string path = GetTestFileFullPath(file_name);
  return ReadMeshFromFile(path).value();
}
inline std::unique_ptr<Mesh> ReadMeshFromTestFile(const std::string &file_name,
                                                  bool use_metadata) {
  const std::string path = GetTestFileFullPath(file_name);
  return ReadMeshFromFile(path, use_metadata).value();
}
inline std::unique_ptr<Mesh> ReadMeshFromTestFile(const std::string &file_name,
                                                  const Options &options) {
  const std::string path = GetTestFileFullPath(file_name);
  return ReadMeshFromFile(path, options).value();
}

inline std::unique_ptr<PointCloud> ReadPointCloudFromTestFile(
    const std::string &file_name) {
  const std::string path = GetTestFileFullPath(file_name);
  return ReadPointCloudFromFile(path).value();
}

}  // namespace draco

#endif  // DRACO_CORE_DRACO_TEST_UTILS_H_
