// Copyright 2013 Blender Foundation
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

#ifndef OPENSUBDIV_BASE_TYPE_H_
#define OPENSUBDIV_BASE_TYPE_H_

#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <map>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace blender {
namespace opensubdiv {

using std::map;
using std::pair;
using std::stack;
using std::string;
using std::unordered_map;
using std::vector;

using std::fill;
using std::make_pair;
using std::max;
using std::min;
using std::move;
using std::swap;

}  // namespace opensubdiv
}  // namespace blender

#endif  // OPENSUBDIV_BASE_TYPE_H_
