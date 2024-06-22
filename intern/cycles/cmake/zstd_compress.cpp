/* SPDX-FileCopyrightText: 2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <cstdint>
#include <fstream>
#include <vector>

#include <zstd.h>

int main(int argc, const char **argv)
{
  if (argc < 3) {
    return -1;
  }

  /* TODO: This might fail for non-ASCII paths on Windows... */
  std::ifstream in(argv[1], std::ios_base::binary);
  std::ofstream out(argv[2], std::ios_base::binary);
  if (!in || !out) {
    return -1;
  }

  in.seekg(0, std::ios_base::end);
  size_t in_size = in.tellg();
  in.seekg(0, std::ios_base::beg);
  if (!in) {
    return -1;
  }

  std::vector<char> in_data(in_size);
  in.read(in_data.data(), in_size);
  if (!in) {
    return -1;
  }

  size_t out_size = ZSTD_compressBound(in_size);
  if (ZSTD_isError(out_size)) {
    return -1;
  }
  std::vector<char> out_data(out_size);

  out_size = ZSTD_compress(out_data.data(), out_data.size(), in_data.data(), in_data.size(), 19);
  if (ZSTD_isError(out_size)) {
    return -1;
  }

  out.write(out_data.data(), out_size);
  if (!out) {
    return -1;
  }

  return 0;
}
