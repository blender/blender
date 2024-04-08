/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_windowmanager_types.h"

#include "RNA_types.hh"

struct bContext;

namespace blender::bke {

#define FH_MAX_FILE_EXTENSIONS_STR 512

struct FileHandlerType {
  /** Unique name. */
  char idname[OP_MAX_TYPENAME];
  /** For UI text. */
  char label[OP_MAX_TYPENAME];
  /** Import operator name. */
  char import_operator[OP_MAX_TYPENAME];
  /** Export operator name. */
  char export_operator[OP_MAX_TYPENAME];
  /** Formatted string of file extensions supported by the file handler, each extension should
   * start with a `.` and be separated by `;`. For Example: `".blend;.ble"`. */
  char file_extensions_str[FH_MAX_FILE_EXTENSIONS_STR];

  /** Check if file handler can be used on file drop. */
  bool (*poll_drop)(const bContext *C, FileHandlerType *file_handle_type);

  /** List of file extensions supported by the file handler. */
  Vector<std::string> file_extensions;

  /** RNA integration. */
  ExtensionRNA rna_ext;

  /**
   * Return a vector of indices in #paths of file paths supported by the file handler.
   */
  blender::Vector<int64_t> filter_supported_paths(const blender::Span<std::string> paths) const;

  /**
   * Generate a default file name for use with this file handler.
   */
  std::string get_default_filename(const StringRefNull name);
};

/**
 * Adds a new `file_handler` to the `file_handlers` list, also loads all the file extensions from
 * the formatted `FileHandlerType.file_extensions_str` string to `FileHandlerType.file_extensions`
 * list.
 *
 * The new  `file_handler` is expected to have a unique `FileHandlerType.idname`.
 */
void file_handler_add(std::unique_ptr<FileHandlerType> file_handler);

/** Returns a `file_handler` that have a specific `idname`, otherwise return `nullptr`. */
FileHandlerType *file_handler_find(StringRef idname);

/**
 * Removes and frees a specific `file_handler` from the `file_handlers` list, the `file_handler`
 * pointer will be not longer valid for use.
 */
void file_handler_remove(FileHandlerType *file_handler);

/** Return pointers to all registered file handlers. */
Span<std::unique_ptr<FileHandlerType>> file_handlers();

/**
 * Return a vector of file handlers that support any file path in `paths` and the call to
 * `poll_drop` returns #true. Caller must check if each file handler have a valid
 * `import_operator`.
 */
blender::Vector<FileHandlerType *> file_handlers_poll_file_drop(
    const bContext *C, const blender::Span<std::string> paths);

}  // namespace blender::bke
