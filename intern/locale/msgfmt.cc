// Written by Sergey Sharybin <sergey.vfx@gmail.com>
// Added support for contexts
//
// Based on Python script msgfmt.py from Python source
// code tree, which was written by Written by
// Martin v. Löwis <loewis@informatik.hu-berlin.de>
//
// Generate binary message catalog from textual translation description.
//
// This program converts a textual Uniforum-style message catalog (.po file) into
// a binary GNU catalog (.mo file).  This is essentially the same function as the
// GNU msgfmt program, however, it is a simpler implementation.
//
// Usage: msgfmt input.po output.po

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdlib.h>
#include <string>
#include <vector>

namespace {

std::map<std::string, std::string> MESSAGES;

bool starts_with(const std::string &string,
                 const std::string &prefix) {
  return prefix.size() <= string.size() &&
         string.compare(0, prefix.size(), prefix) == 0;
}

std::string ltrim(const std::string &s) {
  std::string result = s;
  result.erase(result.begin(),
               std::find_if(result.begin(),
                            result.end(),
                            std::not1(std::ptr_fun<int, int>(std::isspace))));
  return result;
}

std::string rtrim(const std::string &s) {
  std::string result = s;
  result.erase(
    std::find_if(result.rbegin(),
                 result.rend(),
                 std::not1(std::ptr_fun<int, int>(std::isspace))).base(),
    result.end());
  return result;
}

std::string trim(const std::string &s) {
  return ltrim(rtrim(s));
}

std::string unescape(const std::string &s) {
  std::string result;
  std::string::const_iterator it = s.begin();
  while (it != s.end()) {
    char current_char = *it++;
    if (current_char == '\\' && it != s.end()) {
      char next_char = *it++;
      if (next_char == '\\') {
        current_char = '\\';
      } else if (next_char == 'n') {
        current_char = '\n';
      } else if (next_char == 't') {
        current_char = '\t';
      } else {
        current_char = next_char;
      }
    }
    result += current_char;
  }

  if (result[0] == '"' && result[result.size() - 1] == '"') {
    result = result.substr(1, result.size() - 2);
  }

  return result;
}

// Add a non-fuzzy translation to the dictionary.
void add(const std::string &msgctxt,
         const std::string &msgid,
         const std::string &msgstr,
         bool fuzzy) {
  if (fuzzy == false && msgstr.empty() == false) {
    if (msgctxt.empty()) {
      MESSAGES[msgid] = msgstr;
    } else {
      MESSAGES[msgctxt + (char)0x04 + msgid] = msgstr;
    }
  }
}

template<typename TKey, typename TValue>
void get_keys(std::map<TKey, TValue> map,
              std::vector<TKey> *keys) {
  for (typename std::map<TKey, TValue>::iterator it = map.begin();
      it != map.end();
      it++) {
    keys->push_back(it->first);
  }
}

std::string intToBytes(int value) {
  std::string result;
  for (unsigned int i = 0; i < sizeof(value); i++) {
    result += (unsigned char) ((value >> (i * 8)) & 0xff);
  }
  return result;
}

typedef enum {
  SECTION_NONE = 0,
  SECTION_CTX  = 1,
  SECTION_ID   = 2,
  SECTION_STR  = 3
} eSectionType;

struct Offset {
  unsigned int o1, l1, o2, l2;
};

// Return the generated output.
std::string generate(void) {
  // The keys are sorted in the .mo file
  std::vector<std::string> keys;

  // Get list of sorted keys.
  get_keys(MESSAGES, &keys);
  std::sort(keys.begin(), keys.end());

  std::vector<Offset> offsets;
  std::string ids = "", strs = "";
  for (std::vector<std::string>::iterator it = keys.begin();
       it != keys.end();
       it++) {
    std::string &id = *it;
    // For each string, we need size and file offset.  Each string is NUL
    // terminated; the NUL does not count into the size.
    Offset offset = {(unsigned int) ids.size(),
                     (unsigned int) id.size(),
                     (unsigned int) strs.size(),
                     (unsigned int) MESSAGES[id].size()};
    offsets.push_back(offset);
    ids += id + '\0';
    strs += MESSAGES[id] + '\0';
  }

  // The header is 7 32-bit unsigned integers.  We don't use hash tables, so
  // the keys start right after the index tables.
  // translated string.
  int keystart = 7 * 4 + 16 * keys.size();
  // and the values start after the keys
  int valuestart = keystart + ids.size();
  std::vector<int> koffsets;
  std::vector<int> voffsets;
  // The string table first has the list of keys, then the list of values.
  // Each entry has first the size of the string, then the file offset.
  for (std::vector<Offset>::iterator it = offsets.begin();
       it != offsets.end();
       it++) {
    Offset &offset = *it;
    koffsets.push_back(offset.l1);
    koffsets.push_back(offset.o1 + keystart);
    voffsets.push_back(offset.l2);
    voffsets.push_back(offset.o2 + valuestart);
  }

  std::vector<int> all_offsets;
  all_offsets.reserve(koffsets.size() + voffsets.size());
  all_offsets.insert(all_offsets.end(), koffsets.begin(), koffsets.end());
  all_offsets.insert(all_offsets.end(), voffsets.begin(), voffsets.end());

  std::string output = "";
  output += intToBytes(0x950412de);  // Magic
  output += intToBytes(0x0);  // Version
  output += intToBytes(keys.size());  // # of entries
  output += intToBytes(7 * 4);  // start of key index
  output += intToBytes(7 * 4 + keys.size() * 8);  // start of value index
  output += intToBytes(0);  // Size of hash table
  output += intToBytes(0);  // Offset of hash table

  for (std::vector<int>::iterator it = all_offsets.begin();
       it != all_offsets.end();
       it++) {
    int offset = *it;
    output += intToBytes(offset);
  }

  output += ids;
  output += strs;

  return output;
}

void make(const char *input_file_name,
          const char *output_file_name) {
  std::map<std::string, std::string> messages;

  // Start off assuming Latin-1, so everything decodes without failure,
  // until we know the exact encoding.
  // TODO(sergey): Support encoding.
  // const char *encoding = "latin-1";

  eSectionType section = SECTION_NONE;
  bool fuzzy = false;
  bool is_plural = false;
  std::string msgctxt, msgid, msgstr;

  std::ifstream input_file_stream(input_file_name);

  // Parse the catalog.
  int lno = 0;
  for (std::string l; getline(input_file_stream, l); ) {
    lno++;
    // If we get a comment line after a msgstr, this is a new entry.
    if (l[0] == '#' && section == SECTION_STR) {
      add(msgctxt, msgid, msgstr, fuzzy);
      section = SECTION_NONE;
      msgctxt = "";
      fuzzy = false;
    }
    // Record a fuzzy mark.
    if (starts_with(l, "#,") && l.find("fuzzy") != std::string::npos) {
      fuzzy = 1;
    }
    // Skip comments
    if (l[0] == '#') {
      continue;
    }
    // Now we are in a msgid section, output previous section.
    if (starts_with(l, "msgctxt")) {
      if (section == SECTION_STR) {
        add(msgctxt, msgid, msgstr, fuzzy);
      }
      section = SECTION_CTX;
      l = l.substr(7, l.size() - 7);
      msgctxt = msgid = msgstr = "";
    }
    else if (starts_with(l, "msgid") && !starts_with(l, "msgid_plural")) {
      if (section == SECTION_STR) {
        add(msgctxt, msgid, msgstr, fuzzy);
        msgctxt = "";
        if (msgid == "") {
#if 0
          // See whether there is an encoding declaration.
          p = HeaderParser();
          charset = p.parsestr(msgstr.decode(encoding)).get_content_charset();
          if (charset) {
            encoding = charset;
          }
#else
          // Not ported to C++ yet.
          std::cerr << "Encoding declarations are not supported yet.\n"
                    << std::endl;
          abort();
#endif
        }
      }
      section = SECTION_ID;
      l = l.substr(5, l.size() - 5);
      msgid = msgstr = "";
      is_plural = false;
    } else if (starts_with(l, "msgid_plural")) {
      // This is a message with plural forms.
      if (section != SECTION_ID) {
        std::cerr << "msgid_plural not preceeded by msgid on"
                  << input_file_name << ":"
                  << lno
                  << std::endl;
        abort();
      }
      l = l.substr(12, l.size() - 12);
      msgid += '\0';  // separator of singular and plural
      is_plural = true;
    } else if (starts_with(l, "msgstr")) {
      // Now we are in a msgstr section
      section = SECTION_STR;
      if (starts_with(l, "msgstr[")) {
        if (is_plural == false) {
          std::cerr << "plural without msgid_plural on "
                    << input_file_name << ":"
                    << lno
                    << std::endl;
          abort();
        }
        int bracket_position = l.find(']');
        if (bracket_position == std::string::npos) {
          std::cerr << "Syntax error on "
                    << input_file_name << ":"
                    << lno
                    << std::endl;
          abort();
        }
        l = l.substr(bracket_position, l.size() - bracket_position);
        if (msgstr != "") {
          msgstr += '\0';  // Separator of the various plural forms;
        }
      } else {
        if (is_plural) {
          std::cerr << "indexed msgstr required for plural on "
                    << input_file_name << ":"
                    << lno
                    << std::endl;
          abort();
        }
        l = l.substr(6, l.size() - 6);
      }
    }
    // Skip empty lines.
    l = trim(l);
    if (l.empty()) {
      continue;
    }
    l = unescape(l);
    if (section == SECTION_CTX) {
      // TODO(sergey): Support encoding.
      // msgid += l.encode(encoding);
      msgctxt += l;
    }
    else if (section == SECTION_ID) {
      // TODO(sergey): Support encoding.
      // msgid += l.encode(encoding);
      msgid += l;
    } else if (section == SECTION_STR) {
      // TODO(sergey): Support encoding.
      // msgstr += l.encode(encoding)
      msgstr += l;
    } else {
      std::cerr << "Syntax error on "
                << input_file_name << ":"
                << lno
                << std::endl;
      abort();
    }
    // Add last entry
    if (section == SECTION_STR) {
      add(msgctxt, msgid, msgstr, fuzzy);
    }
  }

  // Compute output
  std::string output = generate();

  std::ofstream output_file_stream(output_file_name,
    std::ios::out | std::ios::binary);
  output_file_stream << output;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("Usage: %s <input.po> <output.mo>\n", argv[0]);
    return EXIT_FAILURE;
  }
  const char *input_file = argv[1];
  const char *output_file = argv[2];

  make(input_file, output_file);

  return EXIT_SUCCESS;
}
