/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2012, Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Bastien Montagne.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include <boost/locale.hpp>

#include "boost_locale_wrapper.h"

static std::string messages_path;
static std::string default_domain;

void bl_locale_init(const char *_messages_path, const char *_default_domain)
{
	// Avoid using ICU backend, we do not need its power and it's rather heavy!
	boost::locale::localization_backend_manager lman = boost::locale::localization_backend_manager::global(); 
#if defined (_WIN32)
	lman.select("winapi");
#else
	lman.select("posix");
#endif
	boost::locale::localization_backend_manager::global(lman);

	messages_path = _messages_path;
	default_domain = _default_domain;
}

void bl_locale_set(const char *locale)
{
	boost::locale::generator gen;
	// Specify location of dictionaries.
	gen.add_messages_path(messages_path);
	gen.add_messages_domain(default_domain);
	//gen.set_default_messages_domain(default_domain);

	if (locale && locale[0]) {
		std::locale::global(gen(locale));
	}
	else {
		std::locale::global(gen(""));
	}
	// Note: boost always uses "C" LC_NUMERIC by default!
}

const char* bl_locale_pgettext(const char *msgctxt, const char *msgid)
{
	// Note: We cannot use short stuff like boost::locale::gettext, because those return
	//       std::basic_string objects, which c_ptr()-returned char* is no more valid
	//       once deleted (which happens as soons they are out of scope of this func).
	typedef boost::locale::message_format<char> char_message_facet;
	try {
		std::locale l;
		char_message_facet const &facet = std::use_facet<char_message_facet>(l);
		char const *r = facet.get(0, msgctxt, msgid);
		if(r)
			return r;
		return msgid;
	}
	catch(std::exception const &e) {
//		std::cout << "boost_locale_pgettext: " << e.what() << " \n";
		return msgid;
	}
}

