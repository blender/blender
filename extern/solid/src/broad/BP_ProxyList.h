/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#ifndef BP_PROXYLIST_H
#define BP_PROXYLIST_H

#include <assert.h>

#include <vector>
#include <algorithm>
#include <utility>

class BP_Proxy;

typedef std::pair<BP_Proxy *, unsigned int> BP_ProxyEntry; 

inline bool operator<(const BP_ProxyEntry& a, const BP_ProxyEntry& b) 
{
	return a.first < b.first;
}

class BP_ProxyList : public std::vector<BP_ProxyEntry> {
public:
   BP_ProxyList(size_t n = 20) : std::vector<BP_ProxyEntry>(n) {}      

	iterator add(BP_Proxy *proxy) 
	{
		BP_ProxyEntry entry = std::make_pair(proxy, (unsigned int)0);
		iterator it = std::lower_bound(begin(), end(), entry);
		if (it == end() || (*it).first != proxy) 
		{
			it = insert(it, entry);
		}
		++(*it).second;
		return it;
	}

	void remove(BP_Proxy *proxy) 
	{
		BP_ProxyEntry entry = std::make_pair(proxy, (unsigned int)0);
		iterator it = std::lower_bound(begin(), end(), entry);
		if (it != end() && (*it).first == proxy && --(*it).second == 0) 
		{
			erase(it);	
		}	
	}
};

#endif
