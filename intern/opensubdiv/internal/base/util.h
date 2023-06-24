/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef OPENSUBDIV_BASE_UTIL_H_
#define OPENSUBDIV_BASE_UTIL_H_

#include "internal/base/type.h"

namespace blender {
namespace opensubdiv {

void stringSplit(vector<string> *tokens,
                 const string &str,
                 const string &separators,
                 bool skip_empty);

}  // namespace opensubdiv
}  // namespace blender

#endif  // OPENSUBDIV_BASE_UTIL_H_
