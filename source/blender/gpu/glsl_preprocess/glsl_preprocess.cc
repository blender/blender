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

  bool first_comment = true;
  bool inside_comment = false;

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

  blender::gpu::shader::Preprocessor processor(report_error);

  std::string line;
  while (std::getline(input_file, line)) {
    line_index++;

    /* Remove license headers (first comment). */
    if (line.rfind("/*", 0) == 0 && first_comment) {
      first_comment = false;
      inside_comment = true;
    }

    const bool skip_line = inside_comment;

    if (inside_comment && (line.find("*/") != std::string::npos)) {
      inside_comment = false;
    }

    if (skip_line) {
      output_file << "\n";
    }
    else {
      processor << line << '\n';
    }
  }

  output_file << processor.str();

  input_file.close();
  output_file.close();

  return error;
}
