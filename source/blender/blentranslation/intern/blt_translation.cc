/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blt
 *
 * Manages translation files and provides translation functions.
 * (which are optional and can be disabled as a preference).
 */

#include <cstdlib>
#include <cstring>
#include <optional>

#include "BLT_translation.hh"

#include "DNA_userdef_types.h" /* For user settings. */

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

#ifdef WITH_INTERNATIONAL
#  include "BLI_threads.h"
#  include "messages.hh"
#endif /* WITH_INTERNATIONAL */

using blender::StringRef;
using blender::StringRefNull;

bool BLT_is_default_context(const StringRef msgctxt)
{
  /* We use the "short" test, a more complete one could be:
   * return (!msgctxt || !msgctxt[0] || STREQ(msgctxt, BLT_I18NCONTEXT_DEFAULT_BPYRNA))
   */
  /* NOTE: trying without the void string check for now, it *should* not be necessary... */
  return (msgctxt.is_empty() || msgctxt[0] == BLT_I18NCONTEXT_DEFAULT_BPYRNA[0]);
}

static std::optional<StringRefNull> pgettext(StringRef msgctxt, const StringRef msgid)
{
#ifdef WITH_INTERNATIONAL
  if (msgid.is_empty()) {
    return std::nullopt;
  }
  if (BLT_is_default_context(msgctxt)) {
    msgctxt = BLT_I18NCONTEXT_DEFAULT;
  }
  if (const std::optional<StringRefNull> translation = blender::locale::translate(
          0, msgctxt, msgid))
  {
    return translation;
  }
#  ifdef WITH_PYTHON
  return BPY_app_translations_py_pgettext(msgctxt, msgid);
#  endif
#else
  UNUSED_VARS(msgctxt, msgid);
  return std::nullopt;
#endif
}

const char *BLT_pgettext(const char *msgctxt, const char *msgid)
{
  const std::optional<StringRefNull> translation = pgettext(msgctxt, msgid);
  if (!translation) {
    return msgid;
  }
  return translation->c_str();
}

blender::StringRef BLT_pgettext(blender::StringRef msgctxt, blender::StringRef msgid)
{
  const std::optional<StringRefNull> translation = pgettext(msgctxt, msgid);
  if (!translation) {
    return msgid;
  }
  return *translation;
}

bool BLT_translate()
{
#ifdef WITH_INTERNATIONAL
  return BLI_thread_is_main();
#else
  return false;
#endif
}

bool BLT_translate_iface()
{
#ifdef WITH_INTERNATIONAL
  return BLT_translate() && (U.transopts & USER_TR_IFACE);
#else
  return false;
#endif
}

bool BLT_translate_tooltips()
{
#ifdef WITH_INTERNATIONAL
  return BLT_translate() && (U.transopts & USER_TR_TOOLTIPS);
#else
  return false;
#endif
}

bool BLT_translate_reports()
{
#ifdef WITH_INTERNATIONAL
  return BLT_translate() && (U.transopts & USER_TR_REPORTS);
#else
  return false;
#endif
}

bool BLT_translate_new_dataname()
{
#ifdef WITH_INTERNATIONAL
  return BLT_translate() && (U.transopts & USER_TR_NEWDATANAME);
#else
  return false;
#endif
}

template<typename StringT> StringT translate_do(StringT msgctxt, StringT msgid)
{
#ifdef WITH_INTERNATIONAL
  if (BLT_translate()) {
    return BLT_pgettext(msgctxt, msgid);
  }

  return msgid;

#else
  (void)msgctxt;
  return msgid;
#endif
}

const char *BLT_translate_do(const char *msgctxt, const char *msgid)
{
  return translate_do(msgctxt, msgid);
}

StringRef BLT_translate_do(StringRef msgctxt, StringRef msgid)
{
  return translate_do(msgctxt, msgid);
}

template<typename StringT> StringT translate_do_iface(StringT msgctxt, StringT msgid)
{
#ifdef WITH_INTERNATIONAL
  if (BLT_translate_iface()) {
    return BLT_pgettext(msgctxt, msgid);
  }

  return msgid;

#else
  (void)msgctxt;
  return msgid;
#endif
}

const char *BLT_translate_do_iface(const char *msgctxt, const char *msgid)
{
  return translate_do_iface(msgctxt, msgid);
}

StringRef BLT_translate_do_iface(StringRef msgctxt, StringRef msgid)
{
  return translate_do_iface(msgctxt, msgid);
}

template<typename StringT> StringT translate_do_tooltip(StringT msgctxt, StringT msgid)
{
#ifdef WITH_INTERNATIONAL
  if (BLT_translate_tooltips()) {
    return BLT_pgettext(msgctxt, msgid);
  }

  return msgid;

#else
  (void)msgctxt;
  return msgid;
#endif
}

const char *BLT_translate_do_tooltip(const char *msgctxt, const char *msgid)
{
  return translate_do_tooltip(msgctxt, msgid);
}

StringRef BLT_translate_do_tooltip(StringRef msgctxt, StringRef msgid)
{
  return translate_do_tooltip(msgctxt, msgid);
}

template<typename StringT> StringT translate_do_report(StringT msgctxt, StringT msgid)
{
#ifdef WITH_INTERNATIONAL
  if (BLT_translate_reports()) {
    return BLT_pgettext(msgctxt, msgid);
  }

  return msgid;

#else
  (void)msgctxt;
  return msgid;
#endif
}

const char *BLT_translate_do_report(const char *msgctxt, const char *msgid)
{
  return translate_do_report(msgctxt, msgid);
}

StringRef BLT_translate_do_report(StringRef msgctxt, StringRef msgid)
{
  return translate_do_report(msgctxt, msgid);
}

template<typename StringT> StringT translate_do_new_dataname(StringT msgctxt, StringT msgid)
{
#ifdef WITH_INTERNATIONAL
  if (BLT_translate_new_dataname()) {
    return BLT_pgettext(msgctxt, msgid);
  }

  return msgid;

#else
  (void)msgctxt;
  return msgid;
#endif
}

const char *BLT_translate_do_new_dataname(const char *msgctxt, const char *msgid)
{
  return translate_do_new_dataname(msgctxt, msgid);
}

StringRef BLT_translate_do_new_dataname(StringRef msgctxt, StringRef msgid)
{
  return translate_do_new_dataname(msgctxt, msgid);
}
