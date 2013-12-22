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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file memutil/MEM_CacheLimiter.h
 *  \ingroup memutil
 */


#ifndef __MEM_CACHELIMITER_H__
#define __MEM_CACHELIMITER_H__

/**
 * @section MEM_CacheLimiter
 * This class defines a generic memory cache management system
 * to limit memory usage to a fixed global maximum.
 *
 * Please use the C-API in MEM_CacheLimiterC-Api.h for code written in C.
 *
 * Usage example:
 *
 * class BigFatImage {
 * public:
 *       ~BigFatImage() { tell_everyone_we_are_gone(this); }
 * };
 *
 * void doit() {
 *     MEM_Cache<BigFatImage> BigFatImages;
 *
 *     MEM_Cache_Handle<BigFatImage>* h = BigFatImages.insert(new BigFatImage);
 *
 *     BigFatImages.enforce_limits();
 *     h->ref();
 *
 *     work with image...
 *
 *     h->unref();
 *
 *     leave image in cache.
 */

#include <list>
#include <queue>
#include <vector>
#include "MEM_Allocator.h"

template<class T>
class MEM_CacheLimiter;

#ifndef __MEM_CACHELIMITERC_API_H__
extern "C" {
	void MEM_CacheLimiter_set_maximum(size_t m);
	size_t MEM_CacheLimiter_get_maximum();
	void MEM_CacheLimiter_set_disabled(bool disabled);
	bool MEM_CacheLimiter_is_disabled(void);
};
#endif

template<class T>
class MEM_CacheLimiterHandle {
public:
	explicit MEM_CacheLimiterHandle(T * data_,MEM_CacheLimiter<T> *parent_) :
		data(data_),
		refcount(0),
		parent(parent_)
	{ }

	void ref() {
		refcount++;
	}

	void unref() {
		refcount--;
	}

	T *get() {
		return data;
	}

	const T *get() const {
		return data;
	}

	int get_refcount() const {
		return refcount;
	}

	bool can_destroy() const {
		return !data || !refcount;
	}

	bool destroy_if_possible() {
		if (can_destroy()) {
			delete data;
			data = NULL;
			unmanage();
			return true;
		}
		return false;
	}

	void unmanage() {
		parent->unmanage(this);
	}

	void touch() {
		parent->touch(this);
	}

private:
	friend class MEM_CacheLimiter<T>;

	T * data;
	int refcount;
	typename std::list<MEM_CacheLimiterHandle<T> *, MEM_Allocator<MEM_CacheLimiterHandle<T> *> >::iterator me;
	MEM_CacheLimiter<T> * parent;
};

template<class T>
class MEM_CacheLimiter {
public:
	typedef size_t (*MEM_CacheLimiter_DataSize_Func) (void *data);
	typedef int    (*MEM_CacheLimiter_ItemPriority_Func) (void *item, int default_priority);
	typedef bool   (*MEM_CacheLimiter_ItemDestroyable_Func) (void *item);

	MEM_CacheLimiter(MEM_CacheLimiter_DataSize_Func data_size_func)
		: data_size_func(data_size_func) {
	}

	~MEM_CacheLimiter() {
		for (iterator it = queue.begin(); it != queue.end(); it++) {
			delete *it;
		}
	}

	MEM_CacheLimiterHandle<T> *insert(T * elem) {
		queue.push_back(new MEM_CacheLimiterHandle<T>(elem, this));
		iterator it = queue.end();
		--it;
		queue.back()->me = it;
		return queue.back();
	}

	void unmanage(MEM_CacheLimiterHandle<T> *handle) {
		queue.erase(handle->me);
		delete handle;
	}

	size_t get_memory_in_use() {
		size_t size = 0;
		if (data_size_func) {
			for (iterator it = queue.begin(); it != queue.end(); it++) {
				size += data_size_func((*it)->get()->get_data());
			}
		}
		else {
			size = MEM_get_memory_in_use();
		}
		return size;
	}

	void enforce_limits() {
		size_t max = MEM_CacheLimiter_get_maximum();
		bool is_disabled = MEM_CacheLimiter_is_disabled();
		size_t mem_in_use, cur_size;

		if (is_disabled) {
			return;
		}

		if (max == 0) {
			return;
		}

		mem_in_use = get_memory_in_use();

		if (mem_in_use <= max) {
			return;
		}

		while (!queue.empty() && mem_in_use > max) {
			MEM_CacheElementPtr elem = get_least_priority_destroyable_element();

			if (!elem)
				break;

			if (data_size_func) {
				cur_size = data_size_func(elem->get()->get_data());
			}
			else {
				cur_size = mem_in_use;
			}

			if (elem->destroy_if_possible()) {
				if (data_size_func) {
					mem_in_use -= cur_size;
				}
				else {
					mem_in_use -= cur_size - MEM_get_memory_in_use();
				}
			}
		}
	}

	void touch(MEM_CacheLimiterHandle<T> * handle) {
		/* If we're using custom priority callback re-arranging the queue
		 * doesn't make much sense because we'll iterate it all to get
		 * least priority element anyway.
		 */
		if (item_priority_func == NULL) {
			queue.push_back(handle);
			queue.erase(handle->me);
			iterator it = queue.end();
			--it;
			handle->me = it;
		}
	}

	void set_item_priority_func(MEM_CacheLimiter_ItemPriority_Func item_priority_func) {
		this->item_priority_func = item_priority_func;
	}

	void set_item_destroyable_func(MEM_CacheLimiter_ItemDestroyable_Func item_destroyable_func) {
		this->item_destroyable_func = item_destroyable_func;
	}

private:
	typedef MEM_CacheLimiterHandle<T> *MEM_CacheElementPtr;
	typedef std::list<MEM_CacheElementPtr, MEM_Allocator<MEM_CacheElementPtr> > MEM_CacheQueue;
	typedef typename MEM_CacheQueue::iterator iterator;

	/* Check whether element can be destroyed when enforcing cache limits */
	bool can_destroy_element(MEM_CacheElementPtr &elem) {
		if (!elem->can_destroy()) {
			/* Element is referenced */
			return false;
		}
		if (item_destroyable_func) {
			if (!item_destroyable_func(elem->get()->get_data()))
				return false;
		}
		return true;
	}

	MEM_CacheElementPtr get_least_priority_destroyable_element(void) {
		if (queue.empty())
			return NULL;

		MEM_CacheElementPtr best_match_elem = NULL;

		if (!item_priority_func) {
			for (iterator it = queue.begin(); it != queue.end(); it++) {
				MEM_CacheElementPtr elem = *it;
				if (!can_destroy_element(elem))
					continue;
				best_match_elem = elem;
				break;
			}
		}
		else {
			int best_match_priority = 0;
			iterator it;
			int i;

			for (it = queue.begin(), i = 0; it != queue.end(); it++, i++) {
				MEM_CacheElementPtr elem = *it;

				if (!can_destroy_element(elem))
					continue;

				/* by default 0 means highest priority element */
				/* casting a size type to int is questionable,
				   but unlikely to cause problems */
				int priority = -((int)(queue.size()) - i - 1);
				priority = item_priority_func(elem->get()->get_data(), priority);

				if (priority < best_match_priority || best_match_elem == NULL) {
					best_match_priority = priority;
					best_match_elem = elem;
				}
			}
		}

		return best_match_elem;
	}

	MEM_CacheQueue queue;
	MEM_CacheLimiter_DataSize_Func data_size_func;
	MEM_CacheLimiter_ItemPriority_Func item_priority_func;
	MEM_CacheLimiter_ItemDestroyable_Func item_destroyable_func;
};

#endif  // __MEM_CACHELIMITER_H__
