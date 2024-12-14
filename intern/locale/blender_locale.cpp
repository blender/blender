/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_locale
 */

#include <iostream>

#include "blender_locale.h"
#include "messages.h"

static std::string messages_path;
static std::string default_domain;
static std::string locale_str;

/* NOTE: We cannot use short stuff like `boost::locale::gettext`, because those return
 * `std::basic_string` objects, which c_ptr()-returned char* is no more valid
 * once deleted (which happens as soons they are out of scope of this func). */
static std::locale locale_global;
static blender::locale::MessageFacet const *facet_global = nullptr;

static void bl_locale_global_cache()
{
  /* Cache facet in global variable. Not only is it better for performance,
   * it also fixes crashes on macOS when doing translation from threads other
   * than main. Likely because of some internal thread local variables. */
  try {
    /* facet_global reference is valid as long as local_global exists,
     * so we store both. */
    locale_global = std::locale();
    facet_global = &std::use_facet<blender::locale::MessageFacet>(locale_global);
  }
  // TODO: verify it's not installed for C case
  /* `if std::has_facet<blender::locale::MessageFacet>(l) == false`, LC_ALL = "C" case. */
  catch (const std::bad_cast &e) {
#ifndef NDEBUG
    std::cout << "bl_locale_global_cache:" << e.what() << " \n";
#endif
    (void)e;
    facet_global = nullptr;
  }
  catch (const std::exception &e) {
#ifndef NDEBUG
    std::cout << "bl_locale_global_cache:" << e.what() << " \n";
#endif
    (void)e;
    facet_global = nullptr;
  }
}

void bl_locale_init(const char *_messages_path, const char *_default_domain)
{
  /* TODO: Do we need to modify locale for other things like numeric or time?
   * And if so, do we need to set it to "C", or to the chosen language? */
  messages_path = _messages_path;
  default_domain = _default_domain;
}

void bl_locale_set(const char *locale_name)
{
  /* Get locale name from system if not specified. */
  std::string locale_full_name = locale_name ? locale_name : "";

  try {
    /* Retrieve and parse full locale name. */
    blender::locale::Info info(locale_full_name);

    /* Load .mo file for locale. */
    std::locale _locale = blender::locale::MessageFacet::install(
        std::locale(), info, {default_domain}, {messages_path});
    std::locale::global(_locale);

    bl_locale_global_cache();

    /* Generate the locale string, to known which one is used in case of default locale. */
    locale_str = info.to_full_name();
  }
  catch (std::exception const &e) {
    std::cout << "bl_locale_set(" << locale_full_name << "): " << e.what() << " \n";
  }
}

const char *bl_locale_get(void)
{
  return locale_str.c_str();
}

const char *bl_locale_pgettext(const char *msgctxt, const char *msgid)
{
  if (facet_global) {
    char const *r = facet_global->translate(0, msgctxt, msgid);
    if (r) {
      return r;
    }
  }

  return msgid;
}
