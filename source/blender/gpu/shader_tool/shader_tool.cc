/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_tool
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

#include "processor.hh"

using namespace blender::gpu::shader;

static std::vector<std::string> list_files(const std::string &dir)
{
  std::vector<std::string> files;
  for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path(dir))) {
    if (entry.is_regular_file()) {
      std::string filename(entry.path().string());
      /* We only allow including header files or shader files. */
      if (filename.find(".hh") != std::string::npos ||
          filename.find(".msl") != std::string::npos ||
          filename.find(".glsl") != std::string::npos)
      {
        files.push_back(filename);
      }
    }
  }
  return files;
}

static std::vector<metadata::Symbol> scan_external_symbols(
    const std::vector<std::string> &file_list,
    std::vector<std::string> &visited_files,
    const std::string &file_buffer,
    const std::string &file_name)
{
  Language language = language_from_filename(file_name);
  SourceProcessor processor(file_buffer, file_name, language);

  metadata::Source include_data = processor.parse_include_and_symbols();

  bool errors = false;

  for (const auto &dep : include_data.dependencies) {
    std::string file;
    for (const auto &filename : file_list) {
      if (filename.find(dep) != std::string::npos) {
        file = filename;
      }
    }

    if (file.empty()) {
      std::cout << "Error: Included file not found " << dep << std::endl;
      errors = true;
    }
    else if (std::find(visited_files.begin(), visited_files.end(), file) == visited_files.end()) {
      visited_files.emplace_back(file);

      std::ifstream input_file(file);
      if (!input_file) {
        std::cerr << "Error: Could not open file " << file << std::endl;
        errors = true;
      }
      else {
        std::stringstream buffer;
        buffer << input_file.rdbuf();

        std::vector<blender::gpu::shader::metadata::Symbol> symbols = scan_external_symbols(
            file_list, visited_files, buffer.str(), file);

        /* Set line number for each symbol to 0 as they are defined outside of the target file. */
        for (auto &symbol : include_data.symbol_table) {
          symbol.definition_line = 0;
        }

        /* Extend list. */
        include_data.symbol_table.insert(
            include_data.symbol_table.end(), symbols.begin(), symbols.end());
      }
    }
  }

  if (errors) {
    exit(1);
  }
  return include_data.symbol_table;
}

int main(int argc, char **argv)
{
  using namespace blender;
  if (argc < 5) {
    std::cerr << "Usage: shader_tool <data_file_from> <data_file_to> <metadata_file_to> "
                 "<infos_file_to> <include_dir1> <include_dir2> ..."
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

  /* List of files available for include. */
  std::vector<std::string> file_list;
  for (int i = 5; i < argc; i++) {
    auto list = list_files(std::string(argv[i]));
    /* Extend list. */
    file_list.insert(file_list.end(), list.begin(), list.end());
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
  const bool is_info = filename.find("infos.hh") != std::string::npos ||
                       buffer.str().find("#pragma create_info") != std::string::npos;

  using namespace gpu::shader;

  Language language = language_from_filename(filename);

  if (language == Language::GLSL) {
    /* All build-time GLSL files should be considered blender-GLSL. */
    language = Language::BLENDER_GLSL;
  }

  std::vector<metadata::Symbol> external_symbols;
  if (language == Language::BLENDER_GLSL) {
    std::vector<std::string> visited_files{input_file_name};
    external_symbols = scan_external_symbols(file_list, visited_files, buffer.str(), filename);
  }

  SourceProcessor processor(buffer.str(), input_file_name, language, report_error);

  auto [result, metadata] = processor.convert(external_symbols);

  output_file << result;

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
