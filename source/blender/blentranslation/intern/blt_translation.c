/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blt
 *
 * Manages translation files and provides translation functions.
 * (which are optional and can be disabled as a preference).
 */

#include <stdlib.h>
#include <string.h>

#include "BLT_translation.h"

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h" /* For user settings. */

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#ifdef WITH_INTERNATIONAL
#  include "BLI_threads.h"
#  include "boost_locale_wrapper.h"
#endif /* WITH_INTERNATIONAL */

bool BLT_is_default_context(const char *msgctxt)
{
  /* We use the "short" test, a more complete one could be:
   * return (!msgctxt || !msgctxt[0] || STREQ(msgctxt, BLT_I18NCONTEXT_DEFAULT_BPYRNA))
   */
  /* NOTE: trying without the void string check for now, it *should* not be necessary... */
  return (!msgctxt || msgctxt[0] == BLT_I18NCONTEXT_DEFAULT_BPYRNA[0]);
}

const char *BLT_pgettext(const char *msgctxt, const char *msgid)
{
#ifdef WITH_INTERNATIONAL
  const char *ret = msgid;

  if (msgid && msgid[0]) {
    if (BLT_is_default_context(msgctxt)) {
      msgctxt = BLT_I18NCONTEXT_DEFAULT;
    }
    ret = bl_locale_pgettext(msgctxt, msgid);
    /* We assume if the returned string is the same (memory level) as the msgid,
     * no translation was found, and we can try py scripts' ones!
     */
#  ifdef WITH_PYTHON
    if (ret == msgid) {
      ret = BPY_app_translations_py_pgettext(msgctxt, msgid);
    }
#  endif
  }

  return ret;
#else
  (void)msgctxt;
  return msgid;
#endif
}

bool BLT_translate(void)
{
#ifdef WITH_INTERNATIONAL
  return BLI_thread_is_main();
#else
  return false;
#endif
}

bool BLT_translate_iface(void)
{
#ifdef WITH_INTERNATIONAL
  return BLT_translate() && (U.transopts & USER_TR_IFACE);
#else
  return false;
#endif
}

bool BLT_translate_tooltips(void)
{
#ifdef WITH_INTERNATIONAL
  return BLT_translate() && (U.transopts & USER_TR_TOOLTIPS);
#else
  return false;
#endif
}

bool BLT_translate_new_dataname(void)
{
#ifdef WITH_INTERNATIONAL
  return BLT_translate() && (U.transopts & USER_TR_NEWDATANAME);
#else
  return false;
#endif
}

const char *BLT_translate_do(const char *msgctxt, const char *msgid)
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

const char *BLT_translate_do_iface(const char *msgctxt, const char *msgid)
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

const char *BLT_translate_do_tooltip(const char *msgctxt, const char *msgid)
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

const char *BLT_translate_do_new_dataname(const char *msgctxt, const char *msgid)
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
