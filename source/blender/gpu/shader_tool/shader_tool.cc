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

static metadata::Source scan_external_symbols(const std::vector<std::string> &file_list,
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

        metadata::Source source = scan_external_symbols(
            file_list, visited_files, buffer.str(), file);

        /* Extend list. */
        include_data.symbol_table.insert(include_data.symbol_table.end(),
                                         source.symbol_table.begin(),
                                         source.symbol_table.end());
        include_data.template_definitions.insert(include_data.template_definitions.end(),
                                                 source.template_definitions.begin(),
                                                 source.template_definitions.end());
      }
    }
  }

  if (errors) {
    exit(1);
  }
  return include_data;
}

int main(int argc, char **argv)
{
  using namespace blender;
  if (argc < 6) {
    std::cerr << "Usage: shader_tool <data_file_from> <data_file_to> <metadata_file_to> "
                 "<infos_file_to> <dep_file_to> <include_dir1> <include_dir2> ..."
              << std::endl;
    exit(1);
  }

  const char *input_file_name = argv[1];
  const char *output_file_name = argv[2];
  const char *metadata_file_name = argv[3];
  const char *infos_file_name = argv[4];
  const char *dep_file_name = argv[5];

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
  if (!metadata_file) {
    std::cerr << "Error: Could not open output file " << metadata_file_name << std::endl;
    input_file.close();
    exit(1);
  }

  /* Open the output file for writing */
  std::ofstream infos_file(infos_file_name, std::ofstream::out | std::ofstream::binary);
  if (!infos_file) {
    std::cerr << "Error: Could not open output file " << infos_file_name << std::endl;
    input_file.close();
    exit(1);
  }

  /* Open the output file for writing */
  std::ofstream dep_file(dep_file_name, std::ofstream::out | std::ofstream::binary);
  if (!dep_file) {
    std::cerr << "Error: Could not open output file " << dep_file_name << std::endl;
    input_file.close();
    exit(1);
  }

  /* List of files available for include. */
  std::vector<std::string> file_list;
  for (int i = 6; i < argc; i++) {
    auto list = list_files(std::string(argv[i]));
    /* Extend list. */
    file_list.insert(file_list.end(), list.begin(), list.end());
  }

  std::stringstream buffer;
  buffer << input_file.rdbuf();

  std::string filename(input_file_name);
  const bool is_info = filename.ends_with("infos.hh") || filename.ends_with(".bsl.hh");

  using namespace gpu::shader;

  Language language = language_from_filename(filename);

  if (language == Language::GLSL) {
    /* All build-time GLSL files should be considered blender-GLSL. */
    language = Language::BLENDER_GLSL;
  }

  metadata::Source external_symbols;
  std::vector<std::string> visited_files{input_file_name};
  if (language == Language::BLENDER_GLSL || language == Language::BSL) {
    external_symbols = scan_external_symbols(file_list, visited_files, buffer.str(), filename);
  }

  /* Escape path according to the depfile syntax. */
  auto escape_path = [](std::string filepath) {
    size_t pos = 0;
    while ((pos = filepath.find(' ', pos)) != std::string::npos) {
      filepath.replace(pos, 1, "\\ ");
      pos += 2;
    }
    return filepath;
  };

  dep_file << output_file_name << " : ";
  for (const auto &file : visited_files) {
    dep_file << escape_path(file) << " ";
  }
  dep_file << "\n";

  SourceProcessor processor(buffer.str(), input_file_name, language);

  auto [result, metadata, error] = processor.convert(external_symbols);

  output_file << result;

  /* TODO(fclem): Don't use regex for that. */
  size_t last_slash = filename.find_last_of('/');
  std::string name = (last_slash == std::string::npos) ? filename :
                                                         filename.substr(last_slash + 1);
  std::string metadata_function_name = "metadata_" + name + "_tmp";
  std::ranges::replace(metadata_function_name, '.', '_');

  metadata_file << metadata.serialize(metadata_function_name);
  if (is_info) {
    infos_file << metadata.serialize_infos();
  }

  input_file.close();
  output_file.close();
  metadata_file.close();
  infos_file.close();
  dep_file.close();

  if (error) {
    std::cerr << error.value().full_report << std::endl;
  }

  return error.has_value() ? 1 : 0;
}
