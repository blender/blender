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
  std::ofstream output_file(output_file_name);
  if (!output_file) {
    std::cerr << "Error: Could not open output file " << output_file_name << std::endl;
    input_file.close();
    exit(1);
  }

  std::stringstream buffer;
  buffer << input_file.rdbuf();

  int error = 0;
  size_t line_index = 0;

  auto report_error =
      [&](const std::string &src_line, const std::smatch &match, const char *err_msg) {
        size_t err_line = line_index;
        size_t err_char = match.position();

        std::cerr << input_file_name;
        std::cerr << ':' << std::to_string(err_line) << ':' << std::to_string(err_char);
        std::cerr << ": error: " << err_msg << std::endl;
        std::cerr << src_line << std::endl;
        std::cerr << std::string(err_char, ' ') << '^' << std::endl;

        error++;
      };

  blender::gpu::shader::Preprocessor processor;

  output_file << processor.process(buffer.str(), report_error);

  input_file.close();
  output_file.close();

  return error;
}
