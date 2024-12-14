/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blt
 */

#include "messages.hh"

#import <Cocoa/Cocoa.h>

#include <cstdlib>
#include <string>

namespace blender::locale {

#if !defined(WITH_HEADLESS) && !defined(WITH_GHOST_SDL)
/* Get current locale. */
std::string macos_user_locale()
{
  std::string result;

  @autoreleasepool {
    CFLocaleRef myCFLocale = CFLocaleCopyCurrent();
    NSLocale *myNSLocale = (NSLocale *)myCFLocale;
    [myNSLocale autorelease];

    /* This produces gettext-invalid locale in recent macOS versions (11.4),
     * like `ko-Kore_KR` instead of `ko_KR`. See #88877. */
    // NSString *nsIdentifier = [myNSLocale localeIdentifier];

    NSString *nsIdentifier = myNSLocale.languageCode;
    NSString *nsIdentifier_country = myNSLocale.countryCode;
    if (nsIdentifier.length != 0 && nsIdentifier_country.length != 0) {
      nsIdentifier = [NSString stringWithFormat:@"%@_%@", nsIdentifier, nsIdentifier_country];
    }

    result = nsIdentifier.UTF8String;
  }

  return result + ".UTF-8";
}
#endif

}  // namespace blender::locale
