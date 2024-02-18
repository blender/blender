/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "WM_types.hh"

#include "BLI_vector.hh"

struct wmOperator;
struct wmOperatorType;
struct wmDrag;
struct wmDropBox;

namespace blender::bke {
struct FileHanlderType;
}  // namespace blender::bke

namespace blender::ed::io {
/**
 * Shows a import dialog if the operator was invoked with filepath properties set,
 * otherwise invokes the file-select window.
 */
int filesel_drop_import_invoke(bContext *C, wmOperator *op, const wmEvent *event);

bool poll_file_object_drop(const bContext *C, blender::bke::FileHandlerType *fh);

/**
 * Return all paths stored in the pointer.
 * Properties in pointer should include a `directory` #PropertySubType::PROP_FILEPATH property and
 * a `files` #RNA_OperatorFileListElement collection property.
 * If the pointer has a `filepath` property is also returned as fallback.
 */
Vector<std::string> paths_from_operator_properties(PointerRNA *ptr);
}  // namespace blender::ed::io
