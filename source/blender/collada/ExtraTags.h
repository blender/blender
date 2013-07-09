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
 * Contributor(s): Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/ExtraTags.h
 *  \ingroup collada
 */

#include <string>
#include <map>
#include <vector>

/** \brief Class for saving \<extra\> tags for a specific UniqueId.
 */
class ExtraTags
{
public:
	/** Constructor. */
	ExtraTags(const std::string profile);

	/** Destructor. */
	virtual ~ExtraTags();

	/** Handle the beginning of an element. */
	bool addTag(std::string tag, std::string data);
	
	/** Set given short pointer to value of tag, if it exists. */
	void setData(std::string tag, short *data);
	
	/** Set given int pointer to value of tag, if it exists. */
	void setData(std::string tag, int *data);
	
	/** Set given float pointer to value of tag, if it exists. */
	void setData(std::string tag, float *data);
	
	/** Set given char pointer to value of tag, if it exists. */
	void setData(std::string tag, char *data);
	
	/** Return true if the extra tags is for specified profile. */
	bool isProfile(std::string profile);
	
private:
	/** Disable default copy constructor. */
	ExtraTags(const ExtraTags& pre);
	/** Disable default assignment operator. */
	const ExtraTags& operator= ( const ExtraTags& pre );
	
	/** The profile for which the tags are. */
	std::string profile;
	
	/** Map of tag and text pairs. */
	std::map<std::string, std::string> tags;
	
	/** Get text data for tag as an int. */
	int asInt(std::string tag, bool *ok);
	/** Get text data for tag as a float. */
	float asFloat(std::string tag, bool *ok);
	/** Get text data for tag as a string. */
	std::string asString(std::string tag, bool *ok);
};
