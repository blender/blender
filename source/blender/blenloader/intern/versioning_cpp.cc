#include "BLI_string.h"
#include "MEM_guardedalloc.h"

#include <regex>
#include <string>

#include "BLI_compiler_attrs.h"
#include <cstdio>
#include <vector>

using namespace std;

extern "C" {
const char *sculpt_keymap_fix_intern(const char *str)
{
  basic_string repl = regex_replace(str, regex("unified_"), "");
  repl = regex_replace(repl, regex("size"), "radius");

  regex pat1(R"'(tool_settings.sculpt.brush.([a-zA-Z0-9_]+)$)'");
  regex pat2(R"'(tool_settings.paint_settings.([a-zA-Z0-9_]+)$)'");
  regex pat3(R"'(tool_settings.paint_settings.use_([a-zA-Z_]+)$)'");

  basic_string propname = "";
  basic_string tmp = regex_replace(repl, regex("use_"), "");
  bool inherit = regex_search(repl, regex("use_[a-zA-Z_]+"));

  std::cmatch match;
  if (regex_search(tmp.c_str(), match, pat1)) {
    if (match.size() > 1) {
      propname = match[1];
    }
  }

  basic_string type = "float";

  if (regex_search(str, regex("strength"))) {
    type = "factor";
  }
  else if (regex_search(str, regex("color"))) {
    type = "color4";
  }

  if (!inherit) {
    type += "_value";
  }
  else {
    type = "inherit";
  }

  // tool_settings.sculpt.channels["strength"].factor_value

  basic_string sub1 = R"'(tool_settings.sculpt.brush.channels["$1"].)'" + type;
  basic_string sub2 = R"'(tool_settings.sculpt.channels["$1"].)'" + type;
  basic_string sub3 = R"'(tool_settings.sculpt.brush.channels["$1"].)'" + type;

  // sub += type;

  if (inherit) {
    repl = std::regex_replace(repl, pat3, sub1);
    repl = std::regex_replace(repl, pat1, sub1);
    repl = std::regex_replace(repl, pat2, sub2);
  }
  else {
    repl = std::regex_replace(repl, pat1, sub1);
    repl = std::regex_replace(repl, pat2, sub2);
  }

  repl = regex_replace(repl, regex(R"'(\.channels\.channels)'"), ".channels");

  const char *out = repl.c_str();
  size_t len = (size_t)strlen(out);

  char *ret = (char *)malloc(len + 1);
  BLI_strncpy(ret, out, len + 1);

  return ret;
}

static void test_regexp()
{
  // TODO: figure out blender's testing framework

  std::vector<basic_string<char>> strings = {
      "tool_settings.sculpt.brush.size",
      "tool_settings.unified_paint_settings.size",
      "tool_settings.unified_paint_settings.use_unified_size",
      "tool_settings.sculpt.brush.color",
      "tool_settings.unified_paint_settings.color",
      "tool_settings.unified_paint_settings.use_unified_color",
      "tool_settings.sculpt.brush.strength",
      "tool_settings.unified_paint_settings.strength",
      "tool_settings.unified_paint_settings.use_unified_strength",
  };

  for (auto str : strings) {
    printf("\n%s\n%s\n\n", str.c_str(), sculpt_keymap_fix_intern(str.c_str()));
  }

  fflush(stdout);
}

const char *sculpt_keymap_fix(const char *str)
{
  // test_regexp();

  return sculpt_keymap_fix_intern(str);
}
}
