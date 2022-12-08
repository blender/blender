#pragma once

#include "ply_file_buffer.hh"
#include "ply_file_buffer_ascii.hh"
#include "ply_file_buffer_binary.hh"

namespace blender::io::ply {

class TestingFileBuffer : public FileBuffer {
 public:
  void write_string(StringRef s);
};
}  // namespace blender::io::ply
