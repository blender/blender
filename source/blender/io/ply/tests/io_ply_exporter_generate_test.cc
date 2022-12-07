#include "testing/testing.h"

#include "ply_export_header.hh"
#include "ply_export_data.hh"

namespace blender::io::ply
{
  class PlyExporterGenerateTest : public testing::Test
  {

  };



  TEST(ply_exporter_writer, header) {
    write_header()
  }
} // namespace blender::io::ply
