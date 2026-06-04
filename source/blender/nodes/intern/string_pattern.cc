/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLT_translation.hh"

#include "NOD_string_pattern.hh"

namespace blender::nodes {

const EnumPropertyItem string_pattern_mode_items[] = {
    {int(StringPatternMode::Exact),
     "EXACT",
     0,
     N_("Exact"),
     N_("Remove the one attribute with the given name")},
    {int(StringPatternMode::Wildcard),
     "WILDCARD",
     0,
     N_("Wildcard"),
     N_("Remove all attributes that match the pattern which is allowed to contain a single "
        "wildcard (*)")},
    {0, nullptr, 0, nullptr, nullptr},
};

std::optional<StringPattern> StringPattern::from_str(StringPatternMode mode,
                                                     StringRef pattern,
                                                     std::string &r_error)
{
  switch (mode) {
    case StringPatternMode::Exact: {
      return StringPattern(pattern, Exact{pattern});
    }
    case blender::nodes::StringPatternMode::Wildcard: {
      const int wildcard_count = Span(pattern.data(), pattern.size()).count('*');
      if (wildcard_count == 0) {
        return StringPattern(pattern, Exact{pattern});
      }
      if (wildcard_count >= 2) {
        r_error = TIP_("Only one * is supported in the pattern");
        return std::nullopt;
      }
      const int wildcard_index = pattern.find('*');
      const StringRef prefix = StringRef(pattern).substr(0, wildcard_index);
      const StringRef suffix = StringRef(pattern).substr(wildcard_index + 1);
      return StringPattern(pattern, Wildcard{prefix, suffix});
    }
  }
  r_error = TIP_("Invalid pattern");
  return std::nullopt;
}

bool StringPattern::match(const StringRef query) const
{
  return std::visit([&](const auto &variant) { return variant.match(query); }, variant_);
}

}  // namespace blender::nodes
