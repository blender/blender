/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup glsl_preprocess
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

  std::string line;
  while (std::getline(input_file, line)) {
    /* Remove licence headers (first comment). */
    if (line.rfind("/*", 0) == 0 && first_comment) {
      first_comment = false;
      inside_comment = true;
    }

    const bool skip_line = inside_comment;

    if (inside_comment && (line.find("*/") != std::string::npos)) {
      inside_comment = false;
    }

    if (skip_line) {
      line = "";
    }
#if 0 /* Wait until we support this new syntax. */
    else if (line.rfind("#include ", 0) == 0 || line.rfind("#pragma once", 0) == 0) {
      line[0] = line[1] = '/';
    }
#endif

    output_file << line << "\n";
  }

  input_file.close();
  output_file.close();

  return 0;
}
