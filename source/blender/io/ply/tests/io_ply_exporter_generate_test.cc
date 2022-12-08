#include <gtest/gtest.h>

#include "IO_ply.h"

#include "ply_export_data.hh"
#include "ply_export_header.hh"

#include "io_ply_exporter_test.hh"
#include "io_ply_exporter_mock_filebuffer.hh"

namespace blender::io::ply {
class PlyExporterGenerateTest : public testing::Test {};

TEST(ply_exporter_writer, header)
{
  std::unique_ptr<FileBuffer> buffer;
  buffer = std::make_unique<FileBufferAscii>("");
  std::unique_ptr<PlyData> plyData(new PlyData());
  struct PLYExportParamsDefault export_params;

  printf(export_params.filepath);
}
}  // namespace blender::io::ply
