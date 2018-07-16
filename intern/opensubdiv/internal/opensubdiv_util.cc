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

#include "internal/opensubdiv_util.h"

#include <GL/glew.h>
#include <cstring>

#ifdef _MSC_VER
#  include <iso646.h>
#endif

namespace opensubdiv_capi {

void stringSplit(std::vector<std::string>* tokens,
                 const std::string& str,
                 const std::string& separators,
                 bool skip_empty) {
  size_t token_start = 0, token_length = 0;
  for (size_t i = 0; i < str.length(); ++i) {
    const char ch = str[i];
    if (separators.find(ch) == std::string::npos) {
      // Append non-separator char to a token.
      ++token_length;
    } else {
      // Append current token to the list (if any).
      if (token_length > 0 || !skip_empty) {
        std::string token = str.substr(token_start, token_length);
        tokens->push_back(token);
      }
      // Re-set token pointers,
      token_start = i + 1;
      token_length = 0;
    }
  }
  // Append token which might be at the end of the string.
  if ((token_length != 0) ||
      (!skip_empty && token_start > 0 &&
       separators.find(str[token_start-1]) != std::string::npos)) {
    std::string token = str.substr(token_start, token_length);
    tokens->push_back(token);
  }
}

}  // namespace opensubdiv_capi
