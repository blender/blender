/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "boost_locale_wrapper.h"

/** \file
 * \ingroup intern_locale
 */

#import <Cocoa/Cocoa.h>

#include <cstdlib>

static char *user_locale = NULL;

// get current locale
const char *osx_user_locale()
{
  ::free(user_locale);
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  CFLocaleRef myCFLocale = CFLocaleCopyCurrent();
  NSLocale *myNSLocale = (NSLocale *)myCFLocale;
  [myNSLocale autorelease];

  // This produces gettext-invalid locale in recent macOS versions (11.4),
  // like `ko-Kore_KR` instead of `ko_KR`. See #88877.
  // NSString *nsIdentifier = [myNSLocale localeIdentifier];

  const NSString *nsIdentifier = [myNSLocale languageCode];
  const NSString *const nsIdentifier_country = [myNSLocale countryCode];
  if ([nsIdentifier length] != 0 && [nsIdentifier_country length] != 0) {
    nsIdentifier = [NSString stringWithFormat:@"%@_%@", nsIdentifier, nsIdentifier_country];
  }

  user_locale = ::strdup([nsIdentifier UTF8String]);
  [pool drain];

  return user_locale;
}
