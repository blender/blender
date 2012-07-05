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
			data = 0;
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

	void set_priority(int priority) {
		this->priority = priority;
	}

	int get_priority(void) {
		return this->priority;
	}

private:
	friend class MEM_CacheLimiter<T>;

	T * data;
	int refcount;
	int priority;
	typename std::list<MEM_CacheLimiterHandle<T> *, MEM_Allocator<MEM_CacheLimiterHandle<T> *> >::iterator me;
	MEM_CacheLimiter<T> * parent;
};

template<class T>
class MEM_CacheLimiter {
public:
	typedef size_t (*MEM_CacheLimiter_DataSize_Func) (void *data);
	typedef int    (*MEM_CacheLimiter_ItemPriority_Func) (void *item, int default_priority);

	MEM_CacheLimiter(MEM_CacheLimiter_DataSize_Func getDataSize_)
		: getDataSize(getDataSize_) {
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

	void enforce_limits() {
		MEM_CachePriorityQueue priority_queue;
		size_t max = MEM_CacheLimiter_get_maximum();
		size_t mem_in_use, cur_size;

		if (max == 0) {
			return;
		}

		if(getDataSize) {
			mem_in_use = total_size();
		} else {
			mem_in_use = MEM_get_memory_in_use();
		}

		if (mem_in_use <= max) {
			return;
		}

		priority_queue = get_priority_queue();

		while (!priority_queue.empty() && mem_in_use > max) {
			MEM_CacheElementPtr elem = priority_queue.top();

			priority_queue.pop();

			if(getDataSize) {
				cur_size = getDataSize(elem->get()->get_data());
			} else {
				cur_size = mem_in_use;
			}

			if (elem->destroy_if_possible()) {
				if (getDataSize) {
					mem_in_use -= cur_size;
				} else {
					mem_in_use -= cur_size - MEM_get_memory_in_use();
				}
			}
		}
	}

	void touch(MEM_CacheLimiterHandle<T> * handle) {
		queue.push_back(handle);
		queue.erase(handle->me);
		iterator it = queue.end();
		--it;
		handle->me = it;
	}

	void set_item_priority_func(MEM_CacheLimiter_ItemPriority_Func item_priority_func) {
		getItemPriority = item_priority_func;
	}

private:
	typedef MEM_CacheLimiterHandle<T> *MEM_CacheElementPtr;
	typedef std::list<MEM_CacheElementPtr, MEM_Allocator<MEM_CacheElementPtr> > MEM_CacheQueue;
	typedef typename MEM_CacheQueue::iterator iterator;

	struct compare_element_priority : public std::binary_function<MEM_CacheElementPtr, MEM_CacheElementPtr, bool> {
		bool operator()(const MEM_CacheElementPtr left_elem, const MEM_CacheElementPtr right_elem) const {
			return left_elem->get_priority() > right_elem->get_priority();
		}
	};

	typedef std::priority_queue<MEM_CacheElementPtr, std::vector<MEM_CacheElementPtr>, compare_element_priority > MEM_CachePriorityQueue;

	size_t total_size() {
		size_t size = 0;
		for (iterator it = queue.begin(); it != queue.end(); it++) {
			size+= getDataSize((*it)->get()->get_data());
		}
		return size;
	}

	MEM_CachePriorityQueue get_priority_queue(void) {
		MEM_CachePriorityQueue priority_queue;
		iterator it;
		int i;

		for (it = queue.begin(), i = 0; it != queue.end(); it++, i++) {
			MEM_CacheElementPtr elem = *it;
			int priority;

			/* by default 0 means higherst priority element */
			priority = -(queue.size() - i - 1);

			if (getItemPriority) {
				priority = getItemPriority(elem->get()->get_data(), priority);
			}

			elem->set_priority(priority);

			priority_queue.push(elem);
		}

		return priority_queue;
	}

	MEM_CacheQueue queue;
	MEM_CacheLimiter_DataSize_Func getDataSize;
	MEM_CacheLimiter_ItemPriority_Func getItemPriority;
};

#endif // __MEM_CACHELIMITER_H__
