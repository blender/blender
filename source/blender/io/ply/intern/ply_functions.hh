/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_fileops.hh"
#include <string>

namespace blender::io::ply {

enum line_ending { CR_LF, LF, CR, LF_CR, UNSET };

/**
 * Reads a line in the ply file in a line-ending safe manner. All different line endings are
 * supported. This also supports a mix of different line endings in the same file. CR (\\r), LF
 * (\\n), CR/LF (\\r\\n), LF/CR (\\n\\r).
 * \param file: The file stream.
 * \param line: The string you want to read to.
 * \return The line ending enum if you're interested.
 */
line_ending safe_getline(fstream &file, std::string &line);

}  // namespace blender::io::ply
