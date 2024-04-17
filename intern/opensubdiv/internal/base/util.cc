/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "internal/base/util.h"

namespace blender {
namespace opensubdiv {

void stringSplit(std::vector<std::string> *tokens,
                 const std::string &str,
                 const std::string &separators,
                 bool skip_empty)
{
  size_t token_start = 0, token_length = 0;
  for (size_t i = 0; i < str.length(); ++i) {
    const char ch = str[i];
    if (separators.find(ch) == std::string::npos) {
      // Append non-separator char to a token.
      ++token_length;
    }
    else {
      // Append current token to the list (if any).
      if (token_length > 0 || !skip_empty) {
        std::string token = str.substr(token_start, token_length);
        tokens->push_back(token);
      }
      // Re-set token pointers.
      token_start = i + 1;
      token_length = 0;
    }
  }
  // Append token which might be at the end of the std::string.
  if ((token_length != 0) || (!skip_empty && token_start > 0 &&
                              separators.find(str[token_start - 1]) != std::string::npos))
  {
    std::string token = str.substr(token_start, token_length);
    tokens->push_back(token);
  }
}

}  // namespace opensubdiv
}  // namespace blender
