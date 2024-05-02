/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functionality to interact with keying sets.
 */

namespace blender::animrig {

/** Mode for modify_keyframes. */
enum class ModifyKeyMode {
  INSERT = 0,
  DELETE,
};

/** Return codes for errors (with Relative KeyingSets). */
enum class ModifyKeyReturn {
  SUCCESS = 0,
  /** Context info was invalid for using the Keying Set. */
  INVALID_CONTEXT = -1,
  /** There isn't any type-info for generating paths from context. */
  MISSING_TYPEINFO = -2,
};

}  // namespace blender::animrig
