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

#include "internal/base/util.h"

namespace blender {
namespace opensubdiv {

void stringSplit(vector<string> *tokens,
                 const string &str,
                 const string &separators,
                 bool skip_empty)
{
  size_t token_start = 0, token_length = 0;
  for (size_t i = 0; i < str.length(); ++i) {
    const char ch = str[i];
    if (separators.find(ch) == string::npos) {
      // Append non-separator char to a token.
      ++token_length;
    }
    else {
      // Append current token to the list (if any).
      if (token_length > 0 || !skip_empty) {
        string token = str.substr(token_start, token_length);
        tokens->push_back(token);
      }
      // Re-set token pointers.
      token_start = i + 1;
      token_length = 0;
    }
  }
  // Append token which might be at the end of the string.
  if ((token_length != 0) ||
      (!skip_empty && token_start > 0 && separators.find(str[token_start - 1]) != string::npos))
  {
    string token = str.substr(token_start, token_length);
    tokens->push_back(token);
  }
}

}  // namespace opensubdiv
}  // namespace blender
