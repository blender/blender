#include "BLI_string.h"
#include "MEM_guardedalloc.h"

#include <regex>
#include <string>

#include "BLI_compiler_attrs.h"

using namespace std;

extern "C" {
const char *sculpt_keymap_fix(const char *str)
{
  basic_string repl = regex_replace(str, regex("unified_"), "");
  repl = regex_replace(repl, regex("size"), "radius");

  regex pat1(R"'(tool_settings.sculpt.brush.([a-zA-Z0-9_]+)$)'", regex::flag_type::extended);
  regex pat2(R"'(tool_settings.paint_settings.([a-zA-Z0-9_]+)$)'", regex::flag_type::extended);
  regex pat3(R"'(tool_settings.paint_settings.use_([a-zA-Z_]+)$)'", regex::flag_type::extended);

  basic_string propname = "";
  basic_string tmp = regex_replace(repl, regex("use_"), "");
  bool inherit = regex_search(repl, regex("use_[a-zA-Z_]+", regex::flag_type::extended));

  std::cmatch match;
  if (regex_search(tmp.c_str(), match, pat1)) {
    if (match.size() > 1) {
      propname = match[1];
    }
  }

  basic_string type = "float";

  if (propname == "strength") {
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

  // tool_settings.sculpt.channels.channels["strength"].factor_value

  basic_string sub1 = R"'(tool_settings.sculpt.brush.channels.channels["$1"].)'" + type;
  basic_string sub2 = R"'(tool_settings.sculpt.channels.channels["$1"].)'" + type;
  basic_string sub3 = R"'(tool_settings.sculpt.brush.channels.channels["$1"].)'" + type;

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

  const char *out = repl.c_str();
  size_t len = (size_t)strlen(out);

  char *ret = (char *)malloc(len + 1);
  BLI_strncpy(ret, out, len + 1);

  return ret;
}
}
