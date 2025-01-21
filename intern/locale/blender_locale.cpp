/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_locale
 */

#include "blender_locale.h"
#include "messages.h"

static std::string messages_path;
static std::string default_domain;
static std::string locale_str;

void bl_locale_init(const char *_messages_path, const char *_default_domain)
{
  messages_path = _messages_path;
  default_domain = _default_domain;
}

void bl_locale_free()
{
  blender::locale::free();
}

void bl_locale_set(const char *locale_name)
{
  /* Get locale name from system if not specified. */
  std::string locale_full_name = locale_name ? locale_name : "";

  /* Initialize and load .mo file for locale. */
  blender::locale::init(locale_full_name, {default_domain}, {messages_path});

  /* Generate the locale string, to known which one is used in case of default locale. */
  locale_str = blender::locale::full_name();
}

const char *bl_locale_get(void)
{
  return locale_str.c_str();
}

const char *bl_locale_pgettext(const char *msgctxt, const char *msgid)
{
  const char *r = blender::locale::translate(0, msgctxt, msgid);
  return (r) ? r : msgid;
}
