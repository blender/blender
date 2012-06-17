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
 * Contributor(s): Peter Schlaile <peter@schlaile.de> 2005
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 */

/** \file memutil/intern/MEM_CacheLimiterC-Api.cpp
 *  \ingroup memutil
 */

#include <cstddef>

#include "MEM_CacheLimiter.h"
#include "MEM_CacheLimiterC-Api.h"

static size_t & get_max()
{
	static size_t m = 32*1024*1024;
	return m;
}

void MEM_CacheLimiter_set_maximum(size_t m)
{
	get_max() = m;
}

size_t MEM_CacheLimiter_get_maximum()
{
	return get_max();
}

class MEM_CacheLimiterHandleCClass;
class MEM_CacheLimiterCClass;

typedef MEM_CacheLimiterHandle<MEM_CacheLimiterHandleCClass> handle_t;
typedef MEM_CacheLimiter<MEM_CacheLimiterHandleCClass> cache_t;
typedef std::list<MEM_CacheLimiterHandleCClass*,
		  MEM_Allocator<MEM_CacheLimiterHandleCClass* > > list_t;

class MEM_CacheLimiterCClass {
public:
	MEM_CacheLimiterCClass(MEM_CacheLimiter_Destruct_Func data_destructor_, MEM_CacheLimiter_DataSize_Func data_size)
		: data_destructor(data_destructor_), cache(data_size) {
	}
	~MEM_CacheLimiterCClass();
	
	handle_t * insert(void * data);

	void destruct(void * data,
	              list_t::iterator it);

	cache_t * get_cache() {
		return &cache;
	}
private:
	MEM_CacheLimiter_Destruct_Func data_destructor;

	MEM_CacheLimiter<MEM_CacheLimiterHandleCClass> cache;
	
	list_t cclass_list;
};

class MEM_CacheLimiterHandleCClass {
public:
	MEM_CacheLimiterHandleCClass(void * data_,
	                             MEM_CacheLimiterCClass * parent_)
	    : data(data_), parent(parent_) { }
	~MEM_CacheLimiterHandleCClass();
	void set_iter(list_t::iterator it_) {
		it = it_;
	}
	void set_data(void * data_) {
		data = data_;
	}
	void * get_data() const {
		return data;
	}
private:
	void * data;
	MEM_CacheLimiterCClass * parent;
	list_t::iterator it;
};

handle_t * MEM_CacheLimiterCClass::insert(void * data) 
{
	cclass_list.push_back(new MEM_CacheLimiterHandleCClass(data, this));
	list_t::iterator it = cclass_list.end();
	--it;
	cclass_list.back()->set_iter(it);
	
	return cache.insert(cclass_list.back());
}

void MEM_CacheLimiterCClass::destruct(void * data, list_t::iterator it) 
{
	data_destructor(data);
	cclass_list.erase(it);
}

MEM_CacheLimiterHandleCClass::~MEM_CacheLimiterHandleCClass()
{
	if (data) {
		parent->destruct(data, it);
	}
}

MEM_CacheLimiterCClass::~MEM_CacheLimiterCClass()
{
	// should not happen, but don't leak memory in this case...
	for (list_t::iterator it = cclass_list.begin();
	     it != cclass_list.end(); it++) {
		(*it)->set_data(0);
		delete *it;
	}
}

// ----------------------------------------------------------------------

static inline MEM_CacheLimiterCClass* cast(MEM_CacheLimiterC * l)
{
	return (MEM_CacheLimiterCClass*) l;
}

static inline handle_t* cast(MEM_CacheLimiterHandleC * l)
{
	return (handle_t*) l;
}

MEM_CacheLimiterC * new_MEM_CacheLimiter(
	MEM_CacheLimiter_Destruct_Func data_destructor,
	MEM_CacheLimiter_DataSize_Func data_size)
{
	return (MEM_CacheLimiterC*) new MEM_CacheLimiterCClass(
		data_destructor,
		data_size);
}

void delete_MEM_CacheLimiter(MEM_CacheLimiterC * This)
{
	delete cast(This);
}

MEM_CacheLimiterHandleC * MEM_CacheLimiter_insert(
	MEM_CacheLimiterC * This, void * data)
{
	return (MEM_CacheLimiterHandleC *) cast(This)->insert(data);
}

void MEM_CacheLimiter_enforce_limits(MEM_CacheLimiterC * This)
{
	cast(This)->get_cache()->enforce_limits();
}
	
void MEM_CacheLimiter_unmanage(MEM_CacheLimiterHandleC * handle)
{
	cast(handle)->unmanage();
}
	
void MEM_CacheLimiter_touch(MEM_CacheLimiterHandleC * handle)
{
	cast(handle)->touch();
}
	
void MEM_CacheLimiter_ref(MEM_CacheLimiterHandleC * handle)
{
	cast(handle)->ref();
}
	
void MEM_CacheLimiter_unref(MEM_CacheLimiterHandleC * handle)
{
	cast(handle)->unref();
}

int MEM_CacheLimiter_get_refcount(MEM_CacheLimiterHandleC * handle)
{
	return cast(handle)->get_refcount();
}

	
void * MEM_CacheLimiter_get(MEM_CacheLimiterHandleC * handle)
{
	return cast(handle)->get()->get_data();
}
