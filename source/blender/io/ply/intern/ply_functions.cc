/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_functions.hh"

namespace blender::io::ply {

line_ending safe_getline(fstream &file, std::string &line)
{
  line.clear();
  std::streambuf *sb = file.rdbuf();
  std::istream::sentry se(file, true);

  line_ending possible = UNSET;
  char c;
  while (sb->sgetc() != std::streambuf::traits_type::eof()) {
    c = char(sb->sgetc());
    switch (c) {
      case '\n':
        if (possible == UNSET) {
          possible = LF;
        }
        else if (possible == CR) {
          possible = CR_LF;
        }
        break;
      case '\r':
        if (possible == UNSET) {
          possible = CR;
        }
        else if (possible == LF) {
          possible = LF_CR;
        }
        break;
      default:
        /* If a different character is encountered after the line ending is set, we know to return.
         */
        if (possible != UNSET) {
          return possible;
        }
        line += c;
        break;
    }
    sb->sbumpc();
  }
  return possible;
}
}  // namespace blender::io::ply
