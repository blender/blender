/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup glsl_preprocess
 */

#include <fstream>
#include <iostream>
#include <regex>
#include <string>

#include "glsl_preprocess.hh"

int main(int argc, char **argv)
{
  if (argc != 3) {
    std::cerr << "Usage: glsl_preprocess <data_file_from> <data_file_to>" << std::endl;
    exit(1);
  }

  const char *input_file_name = argv[1];
  const char *output_file_name = argv[2];

  /* Open the input file for reading */
  std::ifstream input_file(input_file_name);
  if (!input_file) {
    std::cerr << "Error: Could not open input file " << input_file_name << std::endl;
    exit(1);
  }

  /* Open the output file for writing */
  std::ofstream output_file(output_file_name, std::ofstream::out | std::ofstream::binary);
  if (!output_file) {
    std::cerr << "Error: Could not open output file " << output_file_name << std::endl;
    input_file.close();
    exit(1);
  }

  std::stringstream buffer;
  buffer << input_file.rdbuf();

  int error = 0;

  auto count_lines = [](const std::string &str) {
    size_t lines = 0;
    for (char c : str) {
      if (c == '\n') {
        lines++;
      }
    }
    return lines;
  };

  auto get_line = [&](size_t line) {
    std::string src = buffer.str();
    size_t start = 0, end;
    for (; line > 1; line--) {
      start = src.find('\n', start + 1);
    }
    end = src.find('\n', start + 1);
    return src.substr(start + 1, end - (start + 1));
  };

  auto report_error = [&](const std::smatch &match, const char *err_msg) {
    size_t remaining_lines = count_lines(match.suffix());
    size_t total_lines = count_lines(buffer.str());

    size_t err_line = 1 + total_lines - remaining_lines;
    size_t err_char = (match.prefix().str().size() - 1) - match.prefix().str().rfind('\n');

    std::cerr << input_file_name;
    std::cerr << ':' << std::to_string(err_line) << ':' << std::to_string(err_char);
    std::cerr << ": error: " << err_msg << std::endl;
    std::cerr << get_line(err_line) << std::endl;
    std::cerr << std::string(err_char, ' ') << '^' << std::endl;

    error++;
  };
  std::string filename(output_file_name);
  const bool is_info = filename.find("info.hh") != std::string::npos;
  const bool is_glsl = filename.find(".glsl") != std::string::npos;
  const bool is_shared = filename.find("shared.h") != std::string::npos;
  const bool is_library = is_glsl &&
                          (filename.find("gpu_shader_material_") != std::string::npos ||
                           filename.find("gpu_shader_common_") != std::string::npos ||
                           filename.find("gpu_shader_compositor_") != std::string::npos);

  if (is_info) {
    std::cerr << "File " << output_file_name
              << " is a create info file and should not be processed as glsl" << std::endl;
    return 1;
  }

  blender::gpu::shader::Preprocessor processor;

  output_file << processor.process(
      buffer.str(), input_file_name, true, is_library, is_glsl, is_glsl, is_shared, report_error);

  input_file.close();
  output_file.close();

  return error;
}
