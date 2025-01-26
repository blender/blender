/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Based on C++ version by `Sergey Sharybin <sergey.vfx@gmail.com>`.
 * Based on Python script `msgfmt.py` from Python source code tree, which was written by
 * `Martin v. Löwis <loewis@informatik.hu-berlin.de>`.
 *
 * Generate binary message catalog from textual translation description.
 *
 * This program converts a textual Uniform-style message catalog (.po file)
 * into a binary GNU catalog (.mo file).
 * This is essentially the same function as the GNU msgfmt program,
 * however, it is a simpler implementation.
 *
 * Usage: msgfmt input.po output.po
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

/* Stupid stub necessary because some BLI files includes winstuff.h, which uses G a bit... */
#ifdef WIN32
struct Global {
  void *dummy;
};

Global G;
#endif

enum eSectionType {
  SECTION_NONE = 0,
  SECTION_CTX = 1,
  SECTION_ID = 2,
  SECTION_STR = 3,
};

struct Message {
  std::string ctxt;
  std::string id;
  std::string str;

  bool is_fuzzy = false;
};

static blender::StringRef unescape(std::string &str)
{
  int curr, next;
  for (curr = next = 0; next < str.size(); curr++, next++) {
    if (str[next] == '\\') {
      /* Get rid of trailing escape char. */
      if (next == str.size() - 1) {
        curr--;
        continue;
      }
      switch (str[next + 1]) {
        case '\\':
          str[curr] = '\\';
          next++;
          break;
        case 'n':
          str[curr] = '\n';
          next++;
          break;
        case 't':
          str[curr] = '\t';
          next++;
          break;
        default:
          /* Get rid of useless escape char. */
          next++;
          str[curr] = str[next];
      }
    }
    else if (curr != next) {
      str[curr] = str[next];
    }
  }
  blender::StringRef ret_str = str;
  BLI_assert(curr <= str.size());

  if (ret_str[0] == '"' && ret_str[curr - 1] == '"') {
    return ret_str.substr(1, curr - 2);
  }
  return ret_str.substr(0, curr);
}

BLI_INLINE size_t uint32_to_bytes(const int value, char *bytes)
{
  size_t i;
  for (i = 0; i < sizeof(value); i++) {
    bytes[i] = char((value >> (int(i) * 8)) & 0xff);
  }
  return i;
}

BLI_INLINE size_t msg_to_bytes(const std::string &msg, char *bytes, uint32_t size)
{
  BLI_assert(msg.size() == size - 1);
  memcpy(bytes, msg.c_str(), size);
  return size;
}

struct Offset {
  uint32_t key_offset, key_len, val_offset, val_len;
};

/* Return the generated binary output. */
static char *generate(blender::Map<std::string, std::string> &messages, size_t *r_output_size)
{
  using MapItem = blender::Map<std::string, std::string>::MutableItem;
  struct Item {
    blender::StringRef key;
    blender::StringRef value;

    Item(const MapItem &other) : key(other.key), value(other.value) {}
    Item(const Item &other) = default;
    Item &operator=(const Item &other) = default;
  };
  const uint32_t num_keys = messages.size();

  /* Get a vector of (key, value) pairs sorted by their keys. */
  blender::Vector<Item> items = {};
  for (const auto message_items_iter : messages.items()) {
    items.append(Item(message_items_iter));
  }
  std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) -> bool {
    return a.key < b.key;
  });

  Offset *offsets = MEM_cnew_array<Offset>(num_keys, __func__);
  uint32_t tot_keys_len = 0;
  uint32_t tot_vals_len = 0;

  for (int i = 0; i < num_keys; i++) {
    Offset &off = offsets[i];

    /* For each string, we need size and file offset.
     * Each string is nullptr terminated; the nullptr does not count into the size. */
    off.key_offset = tot_keys_len;
    off.key_len = uint32_t(items[i].key.size());
    tot_keys_len += off.key_len + 1;

    off.val_offset = tot_vals_len;
    off.val_len = uint32_t(items[i].value.size());
    tot_vals_len += off.val_len + 1;
  }

  /* The header is 7 32-bit unsigned integers.
   * Then comes the keys index table, then the values index table. */
  const uint32_t idx_keystart = 7 * 4;
  const uint32_t idx_valstart = idx_keystart + 8 * num_keys;
  /* We don't use hash tables, so the keys start right after the index tables. */
  const uint32_t keystart = idx_valstart + 8 * num_keys;
  /* and the values start after the keys */
  const uint32_t valstart = keystart + tot_keys_len;

  /* Final buffer representing the binary MO file. */
  *r_output_size = valstart + tot_vals_len;
  char *output = MEM_cnew_array<char>(*r_output_size, __func__);
  char *h = output;
  char *ik = output + idx_keystart;
  char *iv = output + idx_valstart;
  char *k = output + keystart;
  char *v = output + valstart;

  h += uint32_to_bytes(0x950412de, h);   /* Magic */
  h += uint32_to_bytes(0x0, h);          /* Version */
  h += uint32_to_bytes(num_keys, h);     /* Number of entries */
  h += uint32_to_bytes(idx_keystart, h); /* Start of key index */
  h += uint32_to_bytes(idx_valstart, h); /* Start of value index */
  h += uint32_to_bytes(0, h);            /* Size of hash table */
  h += uint32_to_bytes(0, h);            /* Offset of hash table */

  BLI_assert(h == ik);

  for (int i = 0; i < num_keys; i++) {
    const Offset &off = offsets[i];

    /* The index table first has the list of keys, then the list of values.
     * Each entry has first the size of the string, then the file offset. */
    ik += uint32_to_bytes(off.key_len, ik);
    ik += uint32_to_bytes(off.key_offset + keystart, ik);
    iv += uint32_to_bytes(off.val_len, iv);
    iv += uint32_to_bytes(off.val_offset + valstart, iv);

    k += msg_to_bytes(items[i].key, k, off.key_len + 1);
    v += msg_to_bytes(items[i].value, v, off.val_len + 1);
  }

  BLI_assert(ik == output + idx_valstart);
  BLI_assert(iv == output + keystart);
  BLI_assert(k == output + valstart);

  MEM_freeN(offsets);

  return output;
}

static void clear(Message &msg)
{
  msg.ctxt.clear();
  msg.id.clear();
  msg.str.clear();
  msg.is_fuzzy = false;
}

/* Add a non-fuzzy translation to the dictionary. */
static void add(blender::Map<std::string, std::string> &messages, Message &msg)
{
  if (!msg.is_fuzzy && !msg.str.empty()) {
    std::string msgkey;
    if (msg.ctxt.empty()) {
      msgkey = std::move(msg.id);
    }
    else {
      /* '\x04' is the context/msgid separator. */
      msgkey = msg.ctxt + "\x04" + msg.id;
    }

    messages.add(std::move(msgkey), std::move(msg.str));
  }
  clear(msg);
}

static int make(const char *input_file_name, const char *output_file_name)
{
  blender::Map<std::string, std::string> messages;

  const char *msgctxt_kw = "msgctxt";
  const char *msgid_kw = "msgid";
  const char *msgid_plural_kw = "msgid_plural";
  const char *msgstr_kw = "msgstr";
  const size_t msgctxt_len = strlen(msgctxt_kw);
  const size_t msgid_len = strlen(msgid_kw);
  const size_t msgid_plural_len = strlen(msgid_plural_kw);
  const size_t msgstr_len = strlen(msgstr_kw);

  /* NOTE: For now, we assume file encoding is always utf-8. */

  eSectionType section = SECTION_NONE;
  bool is_plural = false;

  Message msg{};

  LinkNode *input_file_lines = BLI_file_read_as_lines(input_file_name);
  LinkNode *ifl = input_file_lines;

  /* Parse the catalog. */
  for (int lno = 1; ifl; ifl = ifl->next, lno++) {
    std::string line = static_cast<char *>(ifl->link);
    blender::StringRef l = line;
    if (l.is_empty()) {
      continue;
    }
    const bool is_comment = (l[0] == '#');
    /* If we get a comment line after a msgstr, this is a new entry. */
    if (is_comment) {
      if (section == SECTION_STR) {
        add(messages, msg);
        section = SECTION_NONE;
      }
      /* Record a fuzzy mark. */
      if (l[1] == ',' && l.find("fuzzy") != blender::StringRef::not_found) {
        msg.is_fuzzy = true;
      }
      /* Skip comments */
      continue;
    }
    if (l.startswith(msgctxt_kw)) {
      if (section == SECTION_STR) {
        /* New message, output previous section. */
        add(messages, msg);
      }
      if (!ELEM(section, SECTION_NONE, SECTION_STR)) {
        printf("msgctxt not at start of new message on %s:%d\n", input_file_name, lno);
        return EXIT_FAILURE;
      }
      section = SECTION_CTX;
      l = l.substr(msgctxt_len);
      clear(msg);
    }
    else if (l.startswith(msgid_plural_kw)) {
      /* This is a message with plural forms. */
      if (section != SECTION_ID) {
        printf("msgid_plural not preceded by msgid on %s:%d\n", input_file_name, lno);
        return EXIT_FAILURE;
      }
      l = l.substr(msgid_plural_len);
      msg.id += "\0"; /* separator of singular and plural */
      is_plural = true;
    }
    else if (l.startswith(msgid_kw)) {
      if (section == SECTION_STR) {
        add(messages, msg);
      }
      if (section != SECTION_CTX) {
        clear(msg);
      }
      section = SECTION_ID;
      l = l.substr(msgid_len);
      is_plural = false;
    }
    else if (l.startswith(msgstr_kw)) {
      l = l.substr(msgstr_len);
      /* Now we are in a `msgstr` section. */
      section = SECTION_STR;
      if (l[0] == '[') {
        if (!is_plural) {
          printf("plural without msgid_plural on %s:%d\n", input_file_name, lno);
          return EXIT_FAILURE;
        }
        int64_t close_bracket_idx = l.find(']');
        if (close_bracket_idx == blender::StringRef::not_found) {
          printf("Syntax error on %s:%d\n", input_file_name, lno);
          return EXIT_FAILURE;
        }
        l = l.substr(close_bracket_idx + 1);
        if (!msg.str.empty()) {
          msg.str += "\0"; /* Separator of the various plural forms. */
        }
      }
      else {
        if (is_plural) {
          printf("indexed msgstr required for plural on %s:%d\n", input_file_name, lno);
          return EXIT_FAILURE;
        }
      }
    }
    /* Skip empty lines. */
    l = l.trim();
    if (l.is_empty()) {
      if (section == SECTION_STR) {
        add(messages, msg);
      }
      section = SECTION_NONE;
      continue;
    }
    line = l;
    l = unescape(line);
    if (section == SECTION_CTX) {
      msg.ctxt += l;
    }
    else if (section == SECTION_ID) {
      msg.id += l;
    }
    else if (section == SECTION_STR) {
      msg.str += l;
    }
    else {
      printf("Syntax error on %s:%d\n", input_file_name, lno);
      return EXIT_FAILURE;
    }
  }
  /* Add last entry */
  if (section == SECTION_STR) {
    add(messages, msg);
  }

  BLI_file_free_lines(input_file_lines);

  /* Compute output */
  size_t output_size;
  char *output = generate(messages, &output_size);

  FILE *fp = BLI_fopen(output_file_name, "wb");
  fwrite(output, 1, output_size, fp);
  fclose(fp);

  MEM_freeN(output);

  return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
  if (argc != 3) {
    printf("Usage: %s <input.po> <output.mo>\n", argv[0]);
    return EXIT_FAILURE;
  }
  const char *input_file = argv[1];
  const char *output_file = argv[2];

  return make(input_file, output_file);
}
