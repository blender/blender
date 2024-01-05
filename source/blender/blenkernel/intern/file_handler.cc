/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_file_handler.hh"

#include "BLI_string.h"

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

FileHandlerType *file_handler_find(const StringRef name)
{
  auto itr = std::find_if(file_handlers().begin(),
                          file_handlers().end(),
                          [name](const std::unique_ptr<FileHandlerType> &file_handler) {
                            return name == file_handler->idname;
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

}  // namespace blender::bke
