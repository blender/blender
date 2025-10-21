/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup glsl_preprocess
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

#include "glsl_preprocess.hh"

int main(int argc, char **argv)
{
  if (argc != 5) {
    std::cerr << "Usage: glsl_preprocess <data_file_from> <data_file_to> <metadata_file_to> "
                 "<infos_file_to>"
              << std::endl;
    exit(1);
  }

  const char *input_file_name = argv[1];
  const char *output_file_name = argv[2];
  const char *metadata_file_name = argv[3];
  const char *infos_file_name = argv[4];

  /* Open the input file for reading */
  std::ifstream input_file(input_file_name);
  if (!input_file) {
    std::cerr << "Error: Could not open input file " << input_file_name << std::endl;
    exit(1);
  }

  /* We make the required directories here rather than having the build system
   * do the work for us, as having cmake do it leads to several thousand cmake
   * instances being launched, leading to significant overhead, see pr #141404
   * for details. */
  std::filesystem::path parent_dir = std::filesystem::path(output_file_name).parent_path();
  std::error_code ec;
  if (!std::filesystem::create_directories(parent_dir, ec)) {
    if (ec) {
      std::cerr << "Unable to create " << parent_dir << " : " << ec.message() << std::endl;
      exit(1);
    }
  }

  /* Open the output file for writing */
  std::ofstream output_file(output_file_name, std::ofstream::out | std::ofstream::binary);
  if (!output_file) {
    std::cerr << "Error: Could not open output file " << output_file_name << std::endl;
    input_file.close();
    exit(1);
  }

  /* Open the output file for writing */
  std::ofstream metadata_file(metadata_file_name, std::ofstream::out | std::ofstream::binary);
  if (!output_file) {
    std::cerr << "Error: Could not open output file " << metadata_file_name << std::endl;
    input_file.close();
    exit(1);
  }

  /* Open the output file for writing */
  std::ofstream infos_file(infos_file_name, std::ofstream::out | std::ofstream::binary);
  if (!output_file) {
    std::cerr << "Error: Could not open output file " << infos_file_name << std::endl;
    input_file.close();
    exit(1);
  }

  std::stringstream buffer;
  buffer << input_file.rdbuf();

  int error = 0;

  auto report_error =
      [&](int err_line, int err_char, const std::string &line, const char *err_msg) {
        std::cerr << input_file_name;
        std::cerr << ':' << std::to_string(err_line) << ':' << std::to_string(err_char + 1);
        std::cerr << ": error: " << err_msg << std::endl;
        std::cerr << line << std::endl;
        std::cerr << std::string(err_char, ' ') << '^' << std::endl;

        error++;
      };
  std::string filename(output_file_name);
  const bool is_info = filename.find("infos.hh") != std::string::npos;
  const bool is_glsl = filename.find(".glsl") != std::string::npos;
  const bool is_shared = filename.find("shared.h") != std::string::npos;
  const bool is_library = is_glsl &&
                          (filename.find("gpu_shader_material_") != std::string::npos ||
                           filename.find("gpu_shader_common_") != std::string::npos ||
                           filename.find("gpu_shader_compositor_") != std::string::npos);

  using Preprocessor = blender::gpu::shader::Preprocessor;
  Preprocessor processor;

  Preprocessor::SourceLanguage language = Preprocessor::language_from_filename(filename);

  if (language == Preprocessor::SourceLanguage::GLSL) {
    /* All build-time GLSL files should be considered blender-GLSL. */
    language = Preprocessor::SourceLanguage::BLENDER_GLSL;
  }

  blender::gpu::shader::metadata::Source metadata;
  output_file << processor.process(
      language, buffer.str(), input_file_name, is_library, is_shared, report_error, metadata);

  /* TODO(fclem): Don't use regex for that. */
  std::string metadata_function_name = "metadata_" +
                                       std::regex_replace(
                                           filename, std::regex(R"((?:.*)\/(.*))"), "$1");
  std::replace(metadata_function_name.begin(), metadata_function_name.end(), '.', '_');

  metadata_file << metadata.serialize(metadata_function_name);
  if (is_info) {
    infos_file << metadata.serialize_infos();
  }

  input_file.close();
  output_file.close();
  metadata_file.close();
  infos_file.close();

  return error;
}
