/* SPDX-FileCopyrightText: 2009-2015 Artyom Beilis (Tonkikh)
 * SPDX-FileCopyrightText: 2021-2023 Alexander Grund
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: BSL-1.0
 *
 * Adapted from boost::locale */

/** \file
 * \ingroup blt
 */

#include "messages.hh"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include "BLI_assert.h"
#include "BLI_fileops.h"
#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#ifdef _WIN32
#  include "BLI_winstuff.h"
#endif

#include "CLG_log.h"

namespace blender::locale {

static CLG_LogRef LOG = {"translation.messages"};

/* Upper/lower case, intentionally restricted to ASCII. */

static constexpr bool is_upper_ascii(const char c)
{
  return 'A' <= c && c <= 'Z';
}

static constexpr bool is_lower_ascii(const char c)
{
  return 'a' <= c && c <= 'z';
}

static bool make_lower_ascii(char &c)
{
  if (is_upper_ascii(c)) {
    c += 'a' - 'A';
    return true;
  }
  return false;
}

static bool make_upper_ascii(char &c)
{
  if (is_lower_ascii(c)) {
    c += 'A' - 'a';
    return true;
  }
  return false;
}

static constexpr bool is_numeric_ascii(const char c)
{
  return '0' <= c && c <= '9';
}

/* Info about a locale. */

class Info {
 public:
  std::string language = "C";
  std::string script;
  std::string country;
  std::string variant;

  Info(const StringRef locale_full_name)
  {
    std::string locale_name(locale_full_name);

    /* If locale name not specified, try to get the appropriate one from the system. */
#if defined(__APPLE__) && !defined(WITH_HEADLESS) && !defined(WITH_GHOST_SDL)
    if (locale_name.empty()) {
      locale_name = macos_user_locale();
    }
#endif

    if (locale_name.empty()) {
      const char *lc_all = BLI_getenv("LC_ALL");
      if (lc_all) {
        locale_name = lc_all;
      }
    }
    if (locale_name.empty()) {
      const char *lang = BLI_getenv("LANG");
      if (lang) {
        locale_name = lang;
      }
    }

#ifdef _WIN32
    if (locale_name.empty()) {
      char buf[128] = {};
      if (GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, buf, sizeof(buf)) != 0) {
        locale_name = buf;
        if (GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, buf, sizeof(buf)) != 0) {
          locale_name += "_";
          locale_name += buf;
        }
      }
    }
#endif

    parse_from_lang(locale_name);
  }

  std::string to_full_name() const
  {
    std::string result = language;
    if (!script.empty()) {
      result += '_' + script;
    }
    if (!country.empty()) {
      result += '_' + country;
    }
    if (!variant.empty()) {
      result += '@' + variant;
    }
    return result;
  }

 private:
  /* Locale parsing. */
  bool parse_from_variant(const std::string_view input)
  {
    if (language == "C" || input.empty()) {
      return false;
    }
    variant = input;
    /* No assumptions, just make it lowercase. */
    for (char &c : variant) {
      make_lower_ascii(c);
    }
    return true;
  }

  bool parse_from_encoding(const std::string_view input)
  {
    const int64_t end = input.find_first_of('@');
    std::string tmp(input.substr(0, end));
    if (tmp.empty()) {
      return false;
    }
    /* tmp contains encoding, we ignore it. */
    if (end >= input.size()) {
      return true;
    }
    BLI_assert(input[end] == '@');
    return parse_from_variant(input.substr(end + 1));
  }

  bool parse_from_country(const std::string_view input)
  {
    if (language == "C") {
      return false;
    }

    const int64_t end = input.find_first_of("@.");
    std::string tmp(input.substr(0, end));
    if (tmp.empty()) {
      return false;
    }

    for (char &c : tmp) {
      make_upper_ascii(c);
    }

    /* If it's ALL uppercase ASCII, assume ISO 3166 country id. */
    if (std::find_if_not(tmp.begin(), tmp.end(), is_upper_ascii) != tmp.end()) {
      /* else handle special cases:
       *   - en_US_POSIX is an alias for C
       *   - M49 country code: 3 digits */
      if (language == "en" && tmp == "US_POSIX") {
        language = "C";
        tmp.clear();
      }
      else if (tmp.size() != 3u ||
               std::find_if_not(tmp.begin(), tmp.end(), is_numeric_ascii) != tmp.end())
      {
        return false;
      }
    }

    country = tmp;
    if (end >= input.size()) {
      return true;
    }
    if (input[end] == '.') {
      return parse_from_encoding(input.substr(end + 1));
    }
    BLI_assert(input[end] == '@');
    return parse_from_variant(input.substr(end + 1));
  }

  bool parse_from_script(const std::string_view input)
  {
    const int64_t end = input.find_first_of("-_@.");
    std::string tmp(input.substr(0, end));
    /* Script is exactly 4 ASCII characters, otherwise it is not present. */
    if (tmp.length() != 4) {
      return parse_from_country(input);
    }

    for (char &c : tmp) {
      if (!is_lower_ascii(c) && !make_lower_ascii(c)) {
        return parse_from_country(input);
      }
    }
    make_upper_ascii(tmp[0]); /* Capitalize first letter only. */
    script = tmp;

    if (end >= input.size()) {
      return true;
    }
    if (ELEM(input[end], '-', '_')) {
      return parse_from_country(input.substr(end + 1));
    }
    if (input[end] == '.') {
      return parse_from_encoding(input.substr(end + 1));
    }
    BLI_assert(input[end] == '@');
    return parse_from_variant(input.substr(end + 1));
  }

  bool parse_from_lang(const std::string_view input)
  {
    const int64_t end = input.find_first_of("-_@.");
    std::string tmp(input.substr(0, end));
    if (tmp.empty()) {
      return false;
    }
    for (char &c : tmp) {
      if (!is_lower_ascii(c) && !make_lower_ascii(c)) {
        return false;
      }
    }
    if (!ELEM(tmp, "c", "posix")) { /* Keep default if C or POSIX. */
      language = tmp;
    }

    if (end >= input.size()) {
      return true;
    }
    if (ELEM(input[end], '-', '_')) {
      return parse_from_script(input.substr(end + 1));
    }
    if (input[end] == '.') {
      return parse_from_encoding(input.substr(end + 1));
    }
    BLI_assert(input[end] == '@');
    return parse_from_variant(input.substr(end + 1));
  }
};

/* .mo file reader. */

class MOFile {
  uint32_t keys_offset_ = 0;
  uint32_t translations_offset_ = 0;

  Vector<char> data_;
  bool native_byteorder_ = false;
  size_t size_ = false;

  std::string error_;

 public:
  MOFile(const std::string &filepath)
  {
    FILE *file = BLI_fopen(filepath.c_str(), "rb");
    if (!file) {
      return;
    }

    fseek(file, 0, SEEK_END);
    const int64_t len = BLI_ftell(file);
    if (len >= 0) {
      fseek(file, 0, SEEK_SET);
      data_.resize(len);
      if (fread(data_.data(), 1, len, file) != len) {
        data_.clear();
        error_ = "Failed to read file";
      }
    }
    else {
      error_ = "Wrong file object";
    }

    fclose(file);

    if (error_.empty()) {
      read_data();
    }
  }

  const char *key(int id)
  {
    const uint32_t off = get(keys_offset_ + id * 8 + 4);
    return data_.data() + off;
  }

  StringRef value(int id)
  {
    const uint32_t len = get(translations_offset_ + id * 8);
    const uint32_t off = get(translations_offset_ + id * 8 + 4);
    if (len > data_.size() || off > data_.size() - len) {
      error_ = "Bad mo-file format";
      return "";
    }
    return StringRef(&data_[off], len);
  }

  size_t size() const
  {
    return size_;
  }

  bool empty() const
  {
    return size_ == 0;
  }

  const std::string &error() const
  {
    return error_;
  }

 private:
  void read_data()
  {
    if (data_.size() < 4) {
      error_ = "Invalid 'mo' file format - the file is too short";
      return;
    }

    uint32_t magic;
    memcpy(&magic, data_.data(), sizeof(magic));
    if (magic == 0x950412de) {
      native_byteorder_ = true;
    }
    else if (magic == 0xde120495) {
      native_byteorder_ = false;
    }
    else {
      error_ = "Invalid file format - invalid magic number";
      return;
    }

    // Read all format sizes
    size_ = get(8);
    keys_offset_ = get(12);
    translations_offset_ = get(16);
  }

  uint32_t get(int offset)
  {
    if (offset > data_.size() - 4) {
      error_ = "Bad mo-file format";
      return 0;
    }
    uint32_t v;
    memcpy(&v, &data_[offset], 4);
    if (!native_byteorder_) {
      v = ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v & 0xFF0000) >> 8) |
          ((v & 0xFF000000) >> 24);
    }

    return v;
  }
};

/* Message lookup key. */

struct MessageKeyRef {
  StringRef context;
  StringRef str;

  uint64_t hash() const
  {
    return get_default_hash(this->context, this->str);
  }
};

struct MessageKey {
  std::string context;
  std::string str;

  MessageKey(const StringRef c)
  {
    const size_t pos = c.find(char(4));
    if (pos == StringRef::not_found) {
      this->str = c;
    }
    else {
      this->context = c.substr(0, pos);
      this->str = c.substr(pos + 1);
    }
  }

  uint64_t hash() const
  {
    return get_default_hash(this->context, this->str);
  }

  static uint64_t hash_as(const MessageKeyRef &key)
  {
    return key.hash();
  }
};

inline bool operator==(const MessageKey &a, const MessageKey &b)
{
  return a.context == b.context && a.str == b.str;
}

inline bool operator==(const MessageKeyRef &a, const MessageKey &b)
{
  return a.context == b.context && a.str == b.str;
}

/* Messages translation based on .mo files. */

class MOMessages {
  using Catalog = Map<MessageKey, std::string>;
  Vector<Catalog> catalogs_;
  std::string error_;

 public:
  MOMessages(const Info &info,
             const Vector<std::string> &domains,
             const Vector<std::string> &paths)
  {
    const Vector<std::string> catalog_paths = get_catalog_paths(info, paths);
    for (size_t i = 0; i < domains.size(); i++) {
      const std::string &domain_name = domains[i];
      const std::string filename = domain_name + ".mo";
      Catalog catalog;
      for (const std::string &path : catalog_paths) {
        if (load_file(path + "/" + filename, catalog)) {
          break;
        }
      }
      catalogs_.append(std::move(catalog));
    }
  }

  const char *translate(const int domain, const StringRef context, const StringRef str) const
  {
    if (domain < 0 || domain >= catalogs_.size()) {
      return nullptr;
    }
    const MessageKeyRef key{context, str};
    const std::string *result = catalogs_[domain].lookup_ptr_as(key);
    return (result) ? result->c_str() : nullptr;
  }

  const std::string &error()
  {
    return error_;
  }

 private:
  Vector<std::string> get_catalog_paths(const Info &info, const Vector<std::string> &paths)
  {
    /* Find language folders. */
    Vector<std::string> lang_folders;
    if (info.language.empty()) {
      return {};
    }

    /* Blender uses non-standard uppercase script zh_HANS instead of zh_Hans, try both. */
    Vector<std::string> scripts = {info.script};
    if (!info.script.empty()) {
      std::string script_uppercase = info.script;
      for (char &c : script_uppercase) {
        make_upper_ascii(c);
      }
      scripts.append(script_uppercase);
    }

    for (const std::string &script : scripts) {
      std::string language = info.language;
      if (!script.empty()) {
        language += "_" + script;
      }
      if (!info.variant.empty() && !info.country.empty()) {
        lang_folders.append(language + "_" + info.country + "@" + info.variant);
      }
      if (!info.variant.empty()) {
        lang_folders.append(language + "@" + info.variant);
      }
      if (!info.country.empty()) {
        lang_folders.append(language + "_" + info.country);
      }
      lang_folders.append(language);
    }

    /* Find catalogs in language folders. */
    Vector<std::string> result;
    result.reserve(lang_folders.size() * paths.size());
    for (const std::string &lang_folder : lang_folders) {
      for (const std::string &search_path : paths) {
        result.append(search_path + "/" + lang_folder + "/LC_MESSAGES");
      }
    }
    return result;
  }

  bool load_file(const std::string &filepath, Catalog &catalog)
  {
    MOFile mo(filepath);
    if (!mo.error().empty()) {
      error_ = mo.error();
      return false;
    }
    if (mo.empty()) {
      return false;
    }

    /* Only support UTF-8 encoded files, as created by our msgfmt tool. */
    const std::string mo_encoding = extract(mo.value(0), "charset=", " \r\n;");
    if (mo_encoding.empty()) {
      error_ = "Invalid mo-format, encoding is not specified";
      return false;
    }
    if (mo_encoding != "UTF-8") {
      error_ = "supported mo-format, encoding must be UTF-8";
      return false;
    }

    /* Create context + key to translated string mapping. */
    for (size_t i = 0; i < mo.size(); i++) {
      const MessageKey key(mo.key(i));
      catalog.add(std::move(key), std::string(mo.value(i)));
    }

    return true;
  }

  static std::string extract(StringRef meta, const std::string &key, const StringRef separators)
  {
    const size_t pos = meta.find(key);
    if (pos == StringRef::not_found) {
      return "";
    }
    meta = meta.substr(pos + key.size());
    const size_t end_pos = meta.find_first_of(separators);
    return std::string(meta.substr(0, end_pos));
  }
};

/* Public API */

static std::unique_ptr<MOMessages> global_messages;
static std::string global_full_name;

void init(const StringRef locale_full_name,
          const Vector<std::string> &domains,
          const Vector<std::string> &paths)
{
  Info info(locale_full_name);
  if (global_full_name == info.to_full_name()) {
    return;
  }

  global_messages = std::make_unique<MOMessages>(info, domains, paths);
  global_full_name = info.to_full_name();

  if (global_messages->error().empty()) {
    CLOG_INFO(&LOG, 2, "Locale %s used for translation", global_full_name.c_str());
  }
  else {
    CLOG_ERROR(&LOG, "Locale %s: %s", global_full_name.c_str(), global_messages->error().c_str());
    free();
  }
}

void free()
{
  global_messages.reset();
  global_full_name = "";
}

const char *translate(const int domain, const StringRef context, const StringRef key)
{
  if (!global_messages) {
    return nullptr;
  }

  return global_messages->translate(domain, context, key);
}

const char *full_name()
{
  return global_full_name.c_str();
}

}  // namespace blender::locale
