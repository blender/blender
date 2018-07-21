// Copyright 2013 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin
// Contributor(s): Brecht van Lommel

#ifndef OPENSUBDIV_UTIL_H_
#define OPENSUBDIV_UTIL_H_

#include <vector>
#include <string>

namespace opensubdiv_capi {

#define STRINGIFY_ARG(x) "" #x
#define STRINGIFY_APPEND(a, b) "" a #b
#define STRINGIFY(x) STRINGIFY_APPEND("", x)

void stringSplit(std::vector<std::string>* tokens,
                 const std::string& str,
                 const std::string& separators,
                 bool skip_empty);

}  // namespace opensubdiv_capi

#endif  // OPENSUBDIV_UTIL_H_
