/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blt
 *
 * Main internationalization functions to set the locale and query available languages.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#  include <clocale>
#endif

#include "RNA_types.h"

#include "BLT_lang.h" /* own include */
#include "BLT_translation.h"

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"

#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#ifdef WITH_INTERNATIONAL

#  include "BLI_fileops.h"
#  include "BLI_linklist.h"

#  include "boost_locale_wrapper.h"

/* Locale options. */
static const char **locales = nullptr;
static int num_locales = 0;
static EnumPropertyItem *locales_menu = nullptr;
static int num_locales_menu = 0;

static void free_locales()
{
  if (locales) {
    int idx = num_locales_menu - 1; /* Last item does not need to be freed! */
    while (idx--) {
      MEM_freeN((void *)locales_menu[idx].identifier);
      MEM_freeN((void *)locales_menu[idx].name);
      MEM_freeN((void *)locales_menu[idx].description); /* Also frees locales's relevant value! */
    }

    MEM_freeN((void *)locales);
    locales = nullptr;
  }
  MEM_SAFE_FREE(locales_menu);
  num_locales = num_locales_menu = 0;
}

static void fill_locales()
{
  const char *const languages_path = BKE_appdir_folder_id(BLENDER_DATAFILES, "locale");
  char languages[FILE_MAX];
  LinkNode *lines = nullptr, *line;
  char *str;
  int idx = 0;

  free_locales();

  BLI_path_join(languages, FILE_MAX, languages_path, "languages");
  line = lines = BLI_file_read_as_lines(languages);

  /* This whole "parsing" code is a bit weak, in that it expects strictly formatted input file...
   * Should not be a problem, though, as this file is script-generated! */

  /* First loop to find highest locale ID */
  while (line) {
    int t;
    str = (char *)line->link;
    if (ELEM(str[0], '#', '\0')) {
      line = line->next;
      continue; /* Comment or void... */
    }
    t = atoi(str);
    if (t >= num_locales) {
      num_locales = t + 1;
    }
    num_locales_menu++;
    line = line->next;
  }
  num_locales_menu++; /* The "closing" void item... */

  /* And now, build locales and locale_menu! */
  locales_menu = static_cast<EnumPropertyItem *>(
      MEM_callocN(num_locales_menu * sizeof(EnumPropertyItem), __func__));
  line = lines;
  /* Do not allocate locales with zero-sized mem,
   * as LOCALE macro uses nullptr locales as invalid marker! */
  if (num_locales > 0) {
    locales = static_cast<const char **>(MEM_callocN(num_locales * sizeof(char *), __func__));
    while (line) {
      int id;
      char *loc, *sep1, *sep2, *sep3;

      str = (char *)line->link;
      if (ELEM(str[0], '#', '\0')) {
        line = line->next;
        continue;
      }

      id = atoi(str);
      sep1 = strchr(str, ':');
      if (sep1) {
        sep1++;
        sep2 = strchr(sep1, ':');
        if (sep2) {
          locales_menu[idx].value = id;
          locales_menu[idx].icon = 0;
          locales_menu[idx].name = BLI_strdupn(sep1, sep2 - sep1);

          sep2++;
          sep3 = strchr(sep2, ':');

          if (sep3) {
            locales_menu[idx].identifier = loc = BLI_strdupn(sep2, sep3 - sep2);
          }
          else {
            locales_menu[idx].identifier = loc = BLI_strdup(sep2);
          }

          if (id == 0) {
            /* The DEFAULT/Automatic item... */
            if (BLI_strnlen(loc, 2)) {
              locales[id] = "";
              /* Keep this tip in sync with the one in rna_userdef
               * (rna_enum_language_default_items). */
              locales_menu[idx].description = BLI_strdup(
                  "Automatically choose system's defined language "
                  "if available, or fall-back to English");
            }
            /* Menu "label", not to be stored in locales! */
            else {
              locales_menu[idx].description = BLI_strdup("");
            }
          }
          else {
            locales[id] = locales_menu[idx].description = BLI_strdup(loc);
          }
          idx++;
        }
      }

      line = line->next;
    }
  }

  /* Add closing item to menu! */
  locales_menu[idx].identifier = nullptr;
  locales_menu[idx].value = locales_menu[idx].icon = 0;
  locales_menu[idx].name = locales_menu[idx].description = "";

  BLI_file_free_lines(lines);
}
#endif /* WITH_INTERNATIONAL */

EnumPropertyItem *BLT_lang_RNA_enum_properties()
{
#ifdef WITH_INTERNATIONAL
  return locales_menu;
#else
  return nullptr;
#endif
}

void BLT_lang_init()
{
#ifdef WITH_INTERNATIONAL
  const char *const messagepath = BKE_appdir_folder_id(BLENDER_DATAFILES, "locale");
#endif

/* Make sure LANG is correct and wouldn't cause #std::runtime_error. */
#ifndef _WIN32
  /* TODO(sergey): This code only ensures LANG is set properly, so later when
   * Cycles will try to use file system API from boost there will be no runtime
   * exception generated by #std::locale() which _requires_ having proper LANG
   * set in the environment.
   *
   * Ideally we also need to ensure LC_ALL, LC_MESSAGES and others are also
   * set to a proper value, but currently it's not a huge deal and doesn't
   * cause any headache.
   *
   * Would also be good to find nicer way to check if LANG is correct.
   */
  const char *lang = BLI_getenv("LANG");
  if (lang != nullptr) {
    char *old_locale = setlocale(LC_ALL, nullptr);
    /* Make a copy so subsequent #setlocale() doesn't interfere. */
    old_locale = BLI_strdup(old_locale);
    if (setlocale(LC_ALL, lang) == nullptr) {
      setenv("LANG", "C", 1);
      printf("Warning: Falling back to the standard locale (\"C\")\n");
    }
    setlocale(LC_ALL, old_locale);
    MEM_freeN(old_locale);
  }
#endif

#ifdef WITH_INTERNATIONAL
  if (messagepath) {
    bl_locale_init(messagepath, TEXT_DOMAIN_NAME);
    fill_locales();
  }
  else {
    printf("%s: 'locale' data path for translations not found, continuing\n", __func__);
  }
#else
#endif
}

void BLT_lang_free()
{
#ifdef WITH_INTERNATIONAL
  free_locales();
#else
#endif
}

#ifdef WITH_INTERNATIONAL
#  define ULANGUAGE \
    ((U.language >= ULANGUAGE_AUTO && U.language < num_locales) ? U.language : ULANGUAGE_ENGLISH)
#  define LOCALE(_id) (locales ? locales[(_id)] : "")
#endif

void BLT_lang_set(const char *str)
{
#ifdef WITH_INTERNATIONAL
  int ulang = ULANGUAGE;
  const char *short_locale = str ? str : LOCALE(ulang);
  const char *short_locale_utf8 = nullptr;

  /* We want to avoid locales like '.UTF-8'! */
  if (short_locale[0]) {
    /* Hooray! Encoding needs to be placed *before* variant! */
    const char *variant = strchr(short_locale, '@');
    if (variant) {
      char *locale = BLI_strdupn(short_locale, variant - short_locale);
      short_locale_utf8 = BLI_sprintfN("%s.UTF-8%s", locale, variant);
      MEM_freeN(locale);
    }
    else {
      short_locale_utf8 = BLI_sprintfN("%s.UTF-8", short_locale);
    }
    bl_locale_set(short_locale_utf8);
    MEM_freeN((void *)short_locale_utf8);
  }
  else {
    bl_locale_set(short_locale);
  }
#else
  (void)str;
#endif
}

const char *BLT_lang_get()
{
#ifdef WITH_INTERNATIONAL
  if (BLT_translate()) {
    const char *locale = LOCALE(ULANGUAGE);
    if (locale[0] == '\0') {
      /* Default locale, we have to find which one we are actually using! */
      locale = bl_locale_get();
    }
    return locale;
  }
  return "en_US"; /* Kind of default locale in Blender when no translation enabled. */
#else
  return "";
#endif
}

#undef LOCALE
#undef ULANGUAGE

void BLT_lang_locale_explode(const char *locale,
                             char **language,
                             char **country,
                             char **variant,
                             char **language_country,
                             char **language_variant)
{
  const char *m1, *m2;
  char *_t = nullptr;

  m1 = strchr(locale, '_');
  m2 = strchr(locale, '@');

  if (language || language_variant) {
    if (m1 || m2) {
      _t = m1 ? BLI_strdupn(locale, m1 - locale) : BLI_strdupn(locale, m2 - locale);
      if (language) {
        *language = _t;
      }
    }
    else if (language) {
      *language = BLI_strdup(locale);
    }
  }
  if (country) {
    if (m1) {
      *country = m2 ? BLI_strdupn(m1 + 1, m2 - (m1 + 1)) : BLI_strdup(m1 + 1);
    }
    else {
      *country = nullptr;
    }
  }
  if (variant) {
    if (m2) {
      *variant = BLI_strdup(m2 + 1);
    }
    else {
      *variant = nullptr;
    }
  }
  if (language_country) {
    if (m1) {
      *language_country = m2 ? BLI_strdupn(locale, m2 - locale) : BLI_strdup(locale);
    }
    else {
      *language_country = nullptr;
    }
  }
  if (language_variant) {
    if (m2) {
      *language_variant = m1 ? BLI_strdupcat(_t, m2) : BLI_strdup(locale);
    }
    else {
      *language_variant = nullptr;
    }
  }
  if (_t && !language) {
    MEM_freeN(_t);
  }
}
