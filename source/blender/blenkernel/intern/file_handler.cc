/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstring>

#include "BKE_file_handler.hh"

#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "BLT_translation.hh"

namespace blender::bke {

static Vector<std::unique_ptr<FileHandlerType>> &file_handlers_vector()
{
  static Vector<std::unique_ptr<FileHandlerType>> file_handlers;
  return file_handlers;
}

Span<std::unique_ptr<FileHandlerType>> file_handlers()
{
  return file_handlers_vector();
}

FileHandlerType *file_handler_find(const StringRef idname)
{
  const auto *itr = std::find_if(file_handlers().begin(),
                                 file_handlers().end(),
                                 [idname](const std::unique_ptr<FileHandlerType> &file_handler) {
                                   return idname == file_handler->idname;
                                 });
  if (itr != file_handlers().end()) {
    return itr->get();
  }
  return nullptr;
}

void file_handler_add(std::unique_ptr<FileHandlerType> file_handler)
{
  BLI_assert(file_handler_find(file_handler->idname) == nullptr);

  /** Load all extensions from the string list into the list. */
  const char char_separator = ';';
  const char *char_begin = file_handler->file_extensions_str;
  const char *char_end = BLI_strchr_or_end(char_begin, char_separator);
  while (char_begin[0]) {
    if (char_end - char_begin > 1) {
      std::string file_extension(char_begin, char_end - char_begin);
      file_handler->file_extensions.append(file_extension);
    }
    char_begin = char_end[0] ? char_end + 1 : char_end;
    char_end = BLI_strchr_or_end(char_begin, char_separator);
  }

  file_handlers_vector().append(std::move(file_handler));
}

void file_handler_remove(FileHandlerType *file_handler)
{
  file_handlers_vector().remove_if(
      [file_handler](const std::unique_ptr<FileHandlerType> &test_file_handler) {
        return test_file_handler.get() == file_handler;
      });
}

Vector<FileHandlerType *> file_handlers_poll_file_drop(const bContext *C,
                                                       const Span<std::string> paths)
{
  Vector<std::string> path_extensions;
  for (const std::string &path : paths) {
    const char *extension = BLI_path_extension(path.c_str());
    if (!extension) {
      continue;
    }
    path_extensions.append_non_duplicates(extension);
  }

  Vector<FileHandlerType *> result;
  for (const std::unique_ptr<FileHandlerType> &file_handler_ptr : file_handlers()) {
    FileHandlerType &file_handler = *file_handler_ptr;
    const auto &file_extensions = file_handler.file_extensions;
    bool support_any_extension = false;
    for (const std::string &extension : path_extensions) {
      auto test_fn = [&extension](const std::string &test_extension) {
        return BLI_strcaseeq(extension.c_str(), test_extension.c_str()) == 1;
      };
      support_any_extension = std::any_of(file_extensions.begin(), file_extensions.end(), test_fn);
      if (support_any_extension) {
        break;
      }
    }
    if (!support_any_extension) {
      continue;
    }

    if (!(file_handler.poll_drop && file_handler.poll_drop(C, &file_handler))) {
      continue;
    }

    result.append(&file_handler);
  }
  return result;
}

Vector<int64_t> FileHandlerType::filter_supported_paths(const Span<std::string> paths) const
{
  Vector<int64_t> indices;

  for (const int idx : paths.index_range()) {
    const char *extension = BLI_path_extension(paths[idx].c_str());
    if (!extension) {
      continue;
    }
    auto test_fn = [extension](const std::string &test_extension) {
      return BLI_strcaseeq(extension, test_extension.c_str()) == 1;
    };

    if (std::any_of(file_extensions.begin(), file_extensions.end(), test_fn)) {
      indices.append(idx);
    }
  }
  return indices;
}

std::string FileHandlerType::get_default_filename(const StringRefNull name)
{
  /* Spaces are supported but do not allow all characters to be blank. */
  const bool all_blank = name.is_empty() ||
                         std::all_of(name.begin(), name.end(), [](char c) { return c == ' '; });

  char filename[FILE_MAXFILE];
  STRNCPY(filename, all_blank ? DATA_("Untitled") : name.c_str());
  BLI_path_extension_ensure(filename,
                            sizeof(filename),
                            file_extensions.is_empty() ? "" : file_extensions.first().c_str());
  BLI_path_make_safe_filename(filename);

  return filename;
}

static std::string extensions_common_prefix(const Span<std::string> extensions)
{
  std::string prefix = extensions.first();
  for (const std::string &extension : extensions.drop_front(1)) {
    const int64_t max_len = std::min<int64_t>(prefix.size(), extension.size());
    int64_t i = 0;
    while (i < max_len && prefix[i] == extension[i]) {
      i++;
    }
    prefix.resize(i);
  }
  return prefix;
}

static bool extensions_group_is_collapsible(const Span<std::string> extensions)
{
  const std::string prefix = extensions_common_prefix(extensions);
  const int64_t prefix_letters = prefix.size() - (!prefix.empty() && prefix[0] == '.' ? 1 : 0);
  return prefix_letters >= 3 && std::ranges::all_of(extensions, [&](const auto &extension) {
           return extension.size() <= prefix.size() + 1;
         });
}

std::string FileHandlerType::label_with_extensions() const
{
  if (file_extensions.is_empty()) {
    return label;
  }

  std::string extensions;
  const Span<std::string> all_extensions = file_extensions.as_span();
  int64_t group_start = 0;
  while (group_start < all_extensions.size()) {
    int64_t group_size = 1;
    while (group_start + group_size < all_extensions.size() &&
           extensions_group_is_collapsible(all_extensions.slice(group_start, group_size + 1)))
    {
      group_size++;
    }

    const Span<std::string> group = all_extensions.slice(group_start, group_size);
    if (!extensions.empty()) {
      extensions += "/";
    }
    if (group.size() > 1) {
      extensions += extensions_common_prefix(group) + "*";
    }
    else {
      extensions += group.first();
    }

    group_start += group_size;
  }

  return std::string(label) + " (" + extensions + ")";
}

}  // namespace blender::bke
