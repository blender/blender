/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** Method used to reference paths. Equivalent of bpy_extras.io_utils.path_reference_mode. */
enum ePathReferenceMode {
  /** Use relative paths with subdirectories only. */
  PATH_REFERENCE_AUTO = 0,
  /** Always write absolute paths. */
  PATH_REFERENCE_ABSOLUTE = 1,
  /** Write relative paths where possible. */
  PATH_REFERENCE_RELATIVE = 2,
  /** Match absolute/relative setting with input path. */
  PATH_REFERENCE_MATCH = 3,
  /** Filename only. */
  PATH_REFERENCE_STRIP = 4,
  /** Copy the file to the destination path. */
  PATH_REFERENCE_COPY = 5,
};
