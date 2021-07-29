#include "boost_locale_wrapper.h"

#import <Cocoa/Cocoa.h>

#include <cstdlib>

static char* user_locale = NULL;

// get current locale
const char* osx_user_locale()
{
	::free(user_locale);
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	CFLocaleRef myCFLocale = CFLocaleCopyCurrent();
	NSLocale * myNSLocale = (NSLocale *) myCFLocale;
	[myNSLocale autorelease];
	NSString *nsIdentifier = [myNSLocale localeIdentifier];
	user_locale = ::strdup([nsIdentifier UTF8String]);
	[pool drain];

	return user_locale;
}
