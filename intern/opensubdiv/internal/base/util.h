/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef OPENSUBDIV_BASE_UTIL_H_
#define OPENSUBDIV_BASE_UTIL_H_

#include <string>
#include <vector>

namespace blender {
namespace opensubdiv {

void stringSplit(std::vector<std::string> *tokens,
                 const std::string &str,
                 const std::string &separators,
                 bool skip_empty);

}  // namespace opensubdiv
}  // namespace blender

#endif  // OPENSUBDIV_BASE_UTIL_H_
