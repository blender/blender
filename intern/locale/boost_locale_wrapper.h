/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_locale
 *  A thin C wrapper around `boost::locale`.
 */

#ifndef __BOOST_LOCALE_WRAPPER_H__
#define __BOOST_LOCALE_WRAPPER_H__

#ifdef __cplusplus
extern "C" {
#endif

void bl_locale_init(const char *messages_path, const char *default_domain);
void bl_locale_set(const char *locale);
const char *bl_locale_get(void);
const char *bl_locale_pgettext(const char *msgctxt, const char *msgid);

#if defined(__APPLE__) && !defined(WITH_HEADLESS) && !defined(WITH_GHOST_SDL)
const char *osx_user_locale(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BOOST_LOCALE_WRAPPER_H__ */
