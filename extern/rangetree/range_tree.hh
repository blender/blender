/* This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include <cassert>
#include <climits>
#include <iostream>
#include <set>

#ifndef RANGE_TREE_DEBUG_PRINT_FUNCTION
#    define RANGE_TREE_DEBUG_PRINT_FUNCTION 0
#endif

template <typename T>
struct RangeTree {
	struct Range {
		Range(T min_, T max_)
			: min(min_), max(max_), single(min_ == max_) {
			assert(min_ <= max_);
		}

		Range(T t)
			: min(t), max(t), single(true)
		{}

		Range& operator=(const Range& v) {
			*this = v;
			return *this;
		}

		bool operator<(const Range& v) const {
			return max < v.min;
		}

		const T min;
		const T max;
		const bool single;
	};

	typedef std::set<Range> Tree;
	typedef typename Tree::iterator TreeIter;
	typedef typename Tree::reverse_iterator TreeIterReverse;
	typedef typename Tree::const_iterator TreeIterConst;

	/* Initialize with a single range from 'min' to 'max', inclusive. */
	RangeTree(T min, T max) {
		tree.insert(Range(min, max));
	}

	/* Initialize with a single range from 0 to 'max', inclusive. */
	RangeTree(T max) {
		tree.insert(Range(0, max));
	}

	RangeTree(const RangeTree<T>& src) {
		tree = src.tree;
	}

	/* Remove 't' from the associated range in the tree. Precondition:
	   a range including 't' must exist in the tree. */
	void take(T t) {
		#if RANGE_TREE_DEBUG_PRINT_FUNCTION
		std::cout << __func__ << "(" << t << ")\n";
		#endif

		/* Find the range that includes 't' and its neighbors */
		TreeIter iter = tree.find(Range(t));
		assert(iter != tree.end());
		Range cur = *iter;

		/* Remove the original range (note that this does not
		   invalidate the prev/next iterators) */
		tree.erase(iter);

		/* Construct two new ranges that together cover the original
		   range, except for 't' */
		if (t > cur.min)
			tree.insert(Range(cur.min, t - 1));
		if (t + 1 <= cur.max)
			tree.insert(Range(t + 1, cur.max));
	}

	/* Take the first element out of the first range in the
	   tree. Precondition: tree must not be empty. */
	T take_any() {
		#if RANGE_TREE_DEBUG_PRINT_FUNCTION
		std::cout << __func__ << "()\n";
		#endif

		/* Find the first element */
		TreeIter iter = tree.begin();
		assert(iter != tree.end());
		T first = iter->min;

		/* Take the first element */
		take(first);
		return first;
	}

	/* Return 't' to the tree, either expanding/merging existing
	   ranges or adding a range to cover it. Precondition: 't' cannot
	   be in an existing range. */
	void release(T t) {
		#if RANGE_TREE_DEBUG_PRINT_FUNCTION
		std::cout << __func__ << "(" << t << ")\n";
		#endif

		/* TODO: these cases should be simplified/unified */

		TreeIter right = tree.upper_bound(t);
		if (right != tree.end()) {
			TreeIter left = right;
			if (left != tree.begin())
				--left;

			if (left == right) {
				/* 't' lies before any existing ranges */
				if (t + 1 == left->min) {
					/* 't' lies directly before the first range,
					   resize and replace that range */
					const Range r(t, left->max);
					tree.erase(left);
					tree.insert(r);
				}
				else {
					/* There's a gap between 't' and the first range,
					   add a new range */
					tree.insert(Range(t));
				}
			}
			else if ((left->max + 1 == t) &&
				(t + 1 == right->min)) {
				/* 't' fills a hole. Remove left and right, and insert a
				   new range that covers both. */
				const Range r(left->min, right->max);
				tree.erase(left);
				tree.erase(right);
				tree.insert(r);
			}
			else if (left->max + 1 == t) {
				/* 't' lies directly after 'left' range, resize and
				   replace that range */
				const Range r(left->min, t);
				tree.erase(left);
				tree.insert(r);
			}
			else if (t + 1 == right->min) {
				/* 't' lies directly before 'right' range, resize and
				   replace that range */
				const Range r(t, right->max);
				tree.erase(right);
				tree.insert(r);
			}
			else {
				/* There's a gap between 't' and both adjacent ranges,
				   add a new range */
				tree.insert(Range(t));
			}
		}
		else {
			/* 't' lies after any existing ranges */
			right = tree.end();
			right--;
			if (right->max + 1 == t) {
				/* 't' lies directly after last range, resize and
				   replace that range */
				const Range r(right->min, t);
				tree.erase(right);
				tree.insert(r);
			}
			else {
				/* There's a gap between the last range and 't', add a
				   new range */
				tree.insert(Range(t));
			}
		}
	}

	bool has(T t) const {
		TreeIterConst iter = tree.find(Range(t));
		return (iter != tree.end()) && (t <= iter->max);
	}

	bool has_range(T min, T max) const {
		TreeIterConst iter = tree.find(Range(min, max));
		return (iter != tree.end()) && (min == iter->min && max == iter->max);
	}

	bool empty() const {
		return tree.empty();
	}

	int size() const {
		return tree.size();
	}

	void print() const {
		std::cout << "RangeTree:\n";
		for (TreeIterConst iter = tree.begin(); iter != tree.end(); ++iter) {
			const Range& r = *iter;
			if (r.single)
				std::cout << "  [" << r.min << "]\n";
			else
				std::cout << "  [" << r.min << ", " << r.max << "]\n";
		}
		if (empty())
			std::cout << "  <empty>";
		std::cout << "\n";
	}

	unsigned int allocation_lower_bound() const {
		return tree.size() * sizeof(Range);
	}

private:
	Tree tree;
};
