/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef NAN_INCLUDED_LOD_TaggedSetOps_h
#define NAN_INCLUDED_LOD_TaggedSetOps_h

#include "MEM_NonCopyable.h"
#include <vector>

/**
 * This class contains some utility functions for finding the intersection,
 * union, and difference of a collection of stl vector of indices into
 * a set of primitives.
 *
 * These are mainly used as helper functions in the decimation and bsp
 * libraries.
 *
 * This template class assumes that each value of type IndexType encountered
 * in the list is a valid index into an array of primitives. This is not
 * checked at run-time and is left to the user to insure. Prmitives of
 * type ObjectType must have the following public methods to be used by
 * this template class:
 *
 * 	int
 * OpenTag(void) --- return a persistent tag value for the primitive
 *
 *	void
 * SetOpenTag(int bla) --- set the persistent tag value for this primitive to bla.
 *
 *	bool
 * SelectTag() --- return a persistent boolean tag for this primitive
 *
 *	void
 * SetSelectTag(bool bla) --- set the persistent boolean tag for this primitive to bla.
 *
 * Here persistent means that the tag should be associated with the object for the
 * entire lifetime of the primitive. Again none of this stuff is enforced you have
 * to make sure that your primitives do the right thing. Often these tags can be
 * cunningly stowed away inside some of the spare bits in the primitive. See
 * CTR_TaggedIndex for such a class.
 *
 */

template 
<class IndexType, class ObjectType>
class CTR_TaggedSetOps : public MEM_NonCopyable {

public :

	static
		void
	Intersect(
		const std::vector< std::vector<IndexType> > &index_list,
		std::vector<ObjectType> &primitives,
		std::vector<IndexType> &output,
		unsigned int mask,
		unsigned int shift
	) {

		// iterate through vectors in index_list
		// iterate through individual members of each vector
		// mark each obejct that the index points to 

		typename std::vector< std::vector<IndexType> >::const_iterator 
			last_vector = index_list.end();
		typename std::vector< std::vector<IndexType> >::const_iterator 
			start_vector = index_list.begin();

		// FIXME some temporary space

		std::vector<IndexType> temp_union;
		temp_union.reserve(64);

		int tag_num = 0;

		for (; start_vector != last_vector; ++start_vector) {

			typename std::vector<IndexType>::const_iterator 
				last_index = start_vector->end();
			typename std::vector<IndexType>::const_iterator 
				start_index = start_vector->begin();

			for (; start_index != last_index; ++start_index) {

				ObjectType & prim = primitives[*start_index];

				if (!prim.OpenTag()) {
					// compute the union
					temp_union.push_back(*start_index);
				}
				int tag = prim.OpenTag();
				tag = (tag & mask) >> shift;
				tag += 1;
				prim.SetOpenTag((prim.OpenTag() & ~mask)| ((tag << shift) & mask));
			}

			++tag_num;
		}
				
		// now iterate through the union and pull out all those with the right tag
		
		typename std::vector<IndexType>::const_iterator last_index = 
			temp_union.end();
		typename std::vector<IndexType>::const_iterator start_index = 
			temp_union.begin();

		for (; start_index != last_index; ++start_index) {

			ObjectType & prim = primitives[*start_index];

			if (prim.OpenTag() == tag_num) {
				//it's part of the intersection!

				output.push_back(*start_index);
				// because we're iterating through the union 
				// it's safe to remove the tag at this point

				prim.SetOpenTag(prim.OpenTag() & ~mask);
			}
		}
	};
		
	// note not a strict set intersection!
	// if x appears twice in b and is part of the intersection
	// it will appear twice in the intersection

	static
		void
	IntersectPair(
		const std::vector<IndexType> &a,
		const std::vector<IndexType> &b,
		std::vector<ObjectType> &primitives,
		std::vector<IndexType> &output
	) {
		
		typename std::vector<IndexType>::const_iterator last_index = 
			a.end();
		typename std::vector<IndexType>::const_iterator start_index = 
			a.begin();

		for (; start_index != last_index; ++start_index) {
			ObjectType & prim = primitives[*start_index];
			prim.SetSelectTag(true);
		}
		last_index = b.end();
		start_index = b.begin();

		for (; start_index != last_index; ++start_index) {
			ObjectType & prim = primitives[*start_index];
			if (prim.SelectTag()) {
				output.push_back(*start_index);
			}
		}
		// deselect
		last_index = a.end();
		start_index = a.begin();

		for (; start_index != last_index; ++start_index) {
			ObjectType & prim = primitives[*start_index];
			prim.SetSelectTag(false);
		}
	};


	static	
		void
	Union(
		std::vector< std::vector<IndexType> > &index_list,
		std::vector<ObjectType> &primitives,
		std::vector<IndexType> &output
	) {
	
		// iterate through vectors in index_list
		// iterate through individual members of each vector
		// mark each obejct that the index points to 

		typename std::vector< std::vector<IndexType> >::const_iterator 
			last_vector = index_list.end();
		typename std::vector< std::vector<IndexType> >::iterator 
			start_vector = index_list.begin();

		for (; start_vector != last_vector; ++start_vector) {

			typename std::vector<IndexType>::const_iterator 
				last_index = start_vector->end();
			typename std::vector<IndexType>::iterator 
				start_index = start_vector->begin();

			for (; start_index != last_index; ++start_index) {

				ObjectType & prim = primitives[*start_index];

				if (!prim.SelectTag()) {
					// compute the union
					output.push_back(*start_index);
					prim.SetSelectTag(true);
				}
			}
		}
				
		// now iterate through the union and reset the tags
		
		typename std::vector<IndexType>::const_iterator last_index = 
			output.end();
		typename std::vector<IndexType>::iterator start_index = 
			output.begin();

		for (; start_index != last_index; ++start_index) {

			ObjectType & prim = primitives[*start_index];
			prim.SetSelectTag(false);
		}			
	}


	static
		void
	Difference(
		std::vector< IndexType> &a,
		std::vector< IndexType> &b,
		std::vector<ObjectType> &primitives,
		std::vector< IndexType> &output
	) {

		// iterate through b mark all
		// iterate through a and add to output all unmarked 

		typename std::vector<IndexType>::const_iterator last_index = 
			b.end();
		typename std::vector<IndexType>::iterator start_index = 
			b.begin();

		for (; start_index != last_index; ++start_index) {

			ObjectType & prim = primitives[*start_index];
			prim.SetSelectTag(true);
		}
			
		last_index = a.end();
		start_index = a.begin();

		for (; start_index != last_index; ++start_index) {

			ObjectType & prim = primitives[*start_index];
			if (!prim.SelectTag()) {
				output.push_back(*start_index);
			}
		}

		// clean up the tags
	
		last_index = b.end();
		start_index = b.begin();

		for (; start_index != last_index; ++start_index) {

			ObjectType & prim = primitives[*start_index];
			prim.SetSelectTag(false);
		}
	};

private :

	// private constructor - this class is not meant for
	// instantiation

	CTR_TaggedSetOps();

};

#endif

