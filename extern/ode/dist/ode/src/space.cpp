/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

/*

simple space 
------------

reports all n^2 object intersections


multi-resolution hash table
---------------------------

the current implementation rebuilds a new hash table each time collide()
is called. we don't keep any state between calls. this is wasteful if there
are unmoving objects in the space.


TODO
----

less memory wasting may to prevent multiple collision callbacks for the
same pair?

better virtual address function.

the collision search can perhaps be optimized - as we search chains we can
come across other candidate intersections at other levels, perhaps we should
do the intersection check straight away? --> save on list searching time only,
which is not too significant.

*/

//****************************************************************************

#include <ode/common.h>
#include <ode/space.h>
#include <ode/geom.h>
#include <ode/error.h>
#include <ode/memory.h>
#include "objects.h"
#include "geom_internal.h"

//****************************************************************************
// space base class

struct dxSpace : public dBase {
  int type;			// don't want to use RTTI
  virtual void destroy()=0;
  virtual void add (dGeomID)=0;
  virtual void remove (dGeomID)=0;
  virtual void collide (void *data, dNearCallback *callback)=0;
  virtual int query (dGeomID)=0;
};

#define TYPE_SIMPLE 0xbad
#define TYPE_HASH 0xbabe

//****************************************************************************
// stuff common to all spaces

#define ALLOCA(x) dALLOCA16(x)


// collide two AABBs together. for the hash table space, this is called if
// the two AABBs inhabit the same hash table cells. this only calls the
// callback function if the boxes actually intersect. if a geom has an
// AABB test function, that is called to provide a further refinement of
// the intersection.

static inline void collideAABBs (dReal bounds1[6], dReal bounds2[6],
				 dxGeom *g1, dxGeom *g2,
				 void *data, dNearCallback *callback)
{
  // no contacts if both geoms on the same body, and the body is not 0
  if (g1->body == g2->body && g1->body) return;

  if (bounds1[0] > bounds2[1] ||
      bounds1[1] < bounds2[0] ||
      bounds1[2] > bounds2[3] ||
      bounds1[3] < bounds2[2] ||
      bounds1[4] > bounds2[5] ||
      bounds1[5] < bounds2[4]) return;
  if (g1->_class->aabb_test) {
    if (g1->_class->aabb_test (g1,g2,bounds2) == 0) return;
  }
  if (g2->_class->aabb_test) {
    if (g2->_class->aabb_test (g2,g1,bounds1) == 0) return;
  }
  callback (data,g1,g2);
}

//****************************************************************************
// simple space - reports all n^2 object intersections

struct dxSimpleSpace : public dxSpace {
  dGeomID first;
  void destroy();
  void add (dGeomID);
  void remove (dGeomID);
  void collide (void *data, dNearCallback *callback);
  int query (dGeomID);
};


dSpaceID dSimpleSpaceCreate()
{
  dxSimpleSpace *w = new dxSimpleSpace;
  w->type = TYPE_SIMPLE;
  w->first = 0;
  return w;
}


void dxSimpleSpace::destroy()
{
  // destroying each geom will call remove(). this will be efficient if
  // we destroy geoms in list order.
  dAASSERT (this);
  dGeomID g,n;
  g = first;
  while (g) {
    n = g->space.next;
    dGeomDestroy (g);
    g = n;
  }
  delete this;
}


void dxSimpleSpace::add (dGeomID obj)
{
  dAASSERT (this && obj);
  dUASSERT (obj->spaceid == 0 && obj->space.next == 0,
	    "object is already in a space");
  obj->space.next = first;
  first = obj;
  obj->spaceid = this;
}


void dxSimpleSpace::remove (dGeomID geom_to_remove)
{
  dAASSERT (this && geom_to_remove);
  dUASSERT (geom_to_remove->spaceid,"object is not in a space");
  dGeomID last=0,g=first;
  while (g) {
    if (g==geom_to_remove) {
      if (last) last->space.next = g->space.next;
      else first = g->space.next;
      geom_to_remove->space.next = 0;
      geom_to_remove->spaceid = 0;
      return;
    }
    last = g;
    g = g->space.next;
  }
}


void dxSimpleSpace::collide (void *data, dNearCallback *callback)
{
  dAASSERT (this && callback);
  dxGeom *g1,*g2;
  int i,j,n;

  // count the number of objects
  n=0;
  for (g1=first; g1; g1=g1->space.next) n++;

  // allocate and fill bounds array
  dReal *bounds = (dReal*) ALLOCA (6 * n * sizeof(dReal));
  i=0;
  for (g1=first; g1; g1=g1->space.next) {
    g1->_class->aabb (g1,bounds + i);
    g1->space_aabb = bounds + i;
    i += 6;
  }

  // intersect all bounding boxes
  i=0;
  for (g1=first; g1; g1=g1->space.next) {
    j=i+6;
    for (g2=g1->space.next; g2; g2=g2->space.next) {
      collideAABBs (bounds+i,bounds+j,g1,g2,data,callback);
      j += 6;
    }
    i += 6;
  }

  // reset the aabb fields of the geoms back to 0
  for (g1=first; g1; g1=g1->space.next) g1->space_aabb = 0;
}


// @@@ NOT FLEXIBLE ENOUGH
//
//int dSpaceCollide (dSpaceID space, dContactGeom **contact_array)
//{
//  int n = 0;
//  dContactGeom *base = (dContact*) dStackAlloc (sizeof(dContact));
//  dContactGeom *c = base;
//  for (dxGeom *g1=space->first; g1; g1=g1->space.next) {
//    for (dxGeom *g2=g1->space.next; g2; g2=g2->space.next) {
//      // generate at most 1 contact for this pair
//      c->o1 = g1;
//      c->o2 = g2;
//      if (dCollide (0,c)) {
//	c = (dContactGeom*) dStackAlloc (sizeof(dContactGeom));
//	n++;
//      }
//    }
//  }
//  *contact_array = base;
//  return n;
//}


int dxSimpleSpace::query (dGeomID obj)
{
  dAASSERT (this && obj);
  if (obj->spaceid != this) return 0;
  dGeomID compare = first;
  while (compare) {
    if (compare == obj) return 1;
    compare = compare->space.next;
  }
  dDebug (0,"object is not in the space it thinks it is in");
  return 0;
}

//****************************************************************************
// hash table space

// kind of silly, but oh well...
#define MAXINT ((int)((((unsigned int)(-1)) << 1) >> 1))


// prime[i] is the largest prime smaller than 2^i
#define NUM_PRIMES 31
static long int prime[NUM_PRIMES] = {1L,2L,3L,7L,13L,31L,61L,127L,251L,509L,
  1021L,2039L,4093L,8191L,16381L,32749L,65521L,131071L,262139L,
  524287L,1048573L,2097143L,4194301L,8388593L,16777213L,33554393L,
  67108859L,134217689L,268435399L,536870909L,1073741789L};


// currently the space 'container' is just a list of the geoms in the space.

struct dxHashSpace : public dxSpace {
  dxGeom *first;
  int global_minlevel;	// smallest hash table level to put AABBs in
  int global_maxlevel;	// objects that need a level larger than this will be
			// put in a "big objects" list instead of a hash table
  void destroy();
  void add (dGeomID);
  void remove (dGeomID);
  void collide (void *data, dNearCallback *callback);
  int query (dGeomID);
};


// an axis aligned bounding box
struct dxAABB {
  dxAABB *next;		// next in the list of all AABBs
  dReal bounds[6];	// minx, maxx, miny, maxy, minz, maxz
  int level;		// the level this is stored in (cell size = 2^level)
  int dbounds[6];	// AABB bounds, discretized to cell size
  dxGeom *geom;		// corresponding geometry object
  int index;		// index of this AABB, starting from 0
};


// a hash table node that represents an AABB that intersects a particular cell
// at a particular level
struct Node {
  Node *next;		// next node in hash table collision list, 0 if none
  int x,y,z;		// cell position in space, discretized to cell size
  dxAABB *aabb;		// axis aligned bounding box that intersects this cell
};


// return the `level' of an AABB. the AABB will be put into cells at this
// level - the cell size will be 2^level. the level is chosen to be the
// smallest value such that the AABB occupies no more than 8 cells, regardless
// of its placement. this means that:
//	size/2 < q <= size
// where q is the maximum AABB dimension.

static int findLevel (dReal bounds[6])
{
  // compute q
  dReal q,q2;
  q = bounds[1] - bounds[0];	// x bounds
  q2 = bounds[3] - bounds[2];	// y bounds
  if (q2 > q) q = q2;
  q2 = bounds[5] - bounds[4];	// z bounds
  if (q2 > q) q = q2;

  if (q == dInfinity) return MAXINT;

  // find level such that 0.5 * 2^level < q <= 2^level
  int level;
  frexp (q,&level);	// q = (0.5 .. 1.0) * 2^level (definition of frexp)
  return level;
}


// find a virtual memory address for a cell at the given level and x,y,z
// position.
// @@@ currently this is not very sophisticated, e.g. the scaling
// factors could be better designed to avoid collisions, and they should
// probably depend on the hash table physical size.

static unsigned long getVirtualAddress (int level, int x, int y, int z)
{
  return level*1000 + x*100 + y*10 + z;
}

//****************************************************************************
// hash space public functions

dSpaceID dHashSpaceCreate()
{
  dxHashSpace *w = new dxHashSpace;
  w->type = TYPE_HASH;
  w->first = 0;
  w->global_minlevel = -3;
  w->global_maxlevel = 10;
  return w;
}


void dxHashSpace::destroy()
{
  // destroying each geom will call remove(). this will be efficient if
  // we destroy geoms in list order.
  dAASSERT (this);
  dGeomID g,n;
  g = first;
  while (g) {
    n = g->space.next;
    dGeomDestroy (g);
    g = n;
  }
  delete this;
}


void dHashSpaceSetLevels (dxSpace *space, int minlevel, int maxlevel)
{
  dUASSERT (minlevel <= maxlevel,"must have minlevel <= maxlevel");
  dUASSERT (space->type == TYPE_HASH,"must be a hash space");
  dxHashSpace *hspace = (dxHashSpace*) space;
  hspace->global_minlevel = minlevel;
  hspace->global_maxlevel = maxlevel;
}


void dxHashSpace::add (dGeomID obj)
{
  dAASSERT (this && obj);
  dUASSERT (obj->spaceid == 0 && obj->space.next == 0,
	    "object is already in a space");
  obj->space.next = first;
  first = obj;
  obj->spaceid = this;
}


void dxHashSpace::remove (dGeomID geom_to_remove)
{
  dAASSERT (this && geom_to_remove);
  dUASSERT (geom_to_remove->spaceid,"object is not in a space");
  dGeomID last=0,g=first;
  while (g) {
    if (g==geom_to_remove) {
      if (last) last->space.next = g->space.next;
      else first = g->space.next;
      geom_to_remove->space.next = 0;
      geom_to_remove->spaceid = 0;
      return;
    }
    last = g;
    g = g->space.next;
  }
}


void dxHashSpace::collide (void *data, dNearCallback *callback)
{
  dAASSERT(this && callback);
  dxGeom *geom;
  dxAABB *aabb;
  int i,maxlevel;

  // create a list of axis aligned bounding boxes for all geoms. count the
  // number of AABBs as we go. set the level for all AABBs. put AABBs larger
  // than the space's global_maxlevel in the big_boxes list, check everything
  // else against that list at the end. for AABBs that are not too big,
  // record the maximum level that we need.

  int n = 0;			// number of AABBs in main list
  int ntotal = 0;		// total number of AABBs
  dxAABB *first_aabb = 0;	// list of AABBs in hash table
  dxAABB *big_boxes = 0;	// list of AABBs too big for hash table
  maxlevel = global_minlevel - 1;
  for (geom = first; geom; geom=geom->space.next) {
    ntotal++;
    dxAABB *aabb = (dxAABB*) ALLOCA (sizeof(dxAABB));
    geom->_class->aabb (geom,aabb->bounds);
    geom->space_aabb = aabb->bounds;
    aabb->geom = geom;
    // compute level, but prevent cells from getting too small
    int level = findLevel (aabb->bounds);
    if (level < global_minlevel) level = global_minlevel;
    if (level <= global_maxlevel) {
      // aabb goes in main list
      aabb->next = first_aabb;
      first_aabb = aabb;
      aabb->level = level;
      if (level > maxlevel) maxlevel = level;
      // cellsize = 2^level
      dReal cellsize = (dReal) ldexp (1.0,level);
      // discretize AABB position to cell size
      for (i=0; i < 6; i++) aabb->dbounds[i] = (int)
			      floor (aabb->bounds[i]/cellsize);
      // set AABB index
      aabb->index = n;
      n++;
    }
    else {
      // aabb is too big, put it in the big_boxes list. we don't care about
      // setting level, dbounds, index, or the maxlevel
      aabb->next = big_boxes;
      big_boxes = aabb;
    }
  }

  // 0 or 1 boxes can't collide with anything
  if (ntotal < 2) return;

  // for `n' objects, an n*n array of bits is used to record if those objects
  // have been intersection-tested against each other yet. this array can
  // grow large with high n, but oh well...
  int tested_rowsize = (n+7) >> 3;	// number of bytes needed for n bits
  unsigned char *tested = (unsigned char *) alloca (n * tested_rowsize);
  memset (tested,0,n * tested_rowsize);

  // create a hash table to store all AABBs. each AABB may take up to 8 cells.
  // we use chaining to resolve collisions, but we use a relatively large table
  // to reduce the chance of collisions.

  // compute hash table size sz to be a prime > 8*n
  for (i=0; i<NUM_PRIMES; i++) {
    if (prime[i] >= (8*n)) break;
  }
  if (i >= NUM_PRIMES) i = NUM_PRIMES-1;	// probably pointless
  int sz = prime[i];

  // allocate and initialize hash table node pointers
  Node **table = (Node **) ALLOCA (sizeof(Node*) * sz);
  for (i=0; i<sz; i++) table[i] = 0;

  // add each AABB to the hash table (may need to add it to up to 8 cells)
  for (aabb=first_aabb; aabb; aabb=aabb->next) {
    int *dbounds = aabb->dbounds;
    for (int xi = dbounds[0]; xi <= dbounds[1]; xi++) {
      for (int yi = dbounds[2]; yi <= dbounds[3]; yi++) {
	for (int zi = dbounds[4]; zi <= dbounds[5]; zi++) {
	  // get the hash index
	  unsigned long hi = getVirtualAddress (aabb->level,xi,yi,zi) % sz;
	  // add a new node to the hash table
	  Node *node = (Node*) alloca (sizeof (Node));
	  node->x = xi;
	  node->y = yi;
	  node->z = zi;
	  node->aabb = aabb;
	  node->next = table[hi];
	  table[hi] = node;
	}
      }
    }
  }

  // now that all AABBs are loaded into the hash table, we do the actual
  // collision detection. for all AABBs, check for other AABBs in the
  // same cells for collisions, and then check for other AABBs in all
  // intersecting higher level cells.

  int db[6];			// discrete bounds at current level
  for (aabb=first_aabb; aabb; aabb=aabb->next) {
    // we are searching for collisions with aabb
    for (i=0; i<6; i++) db[i] = aabb->dbounds[i];
    for (int level = aabb->level; level <= maxlevel; level++) {
      for (int xi = db[0]; xi <= db[1]; xi++) {
	for (int yi = db[2]; yi <= db[3]; yi++) {
	  for (int zi = db[4]; zi <= db[5]; zi++) {
	    // get the hash index
	    unsigned long hi = getVirtualAddress (level,xi,yi,zi) % sz;
	    // search all nodes at this index
	    Node *node;
	    for (node = table[hi]; node; node=node->next) {
	      // node points to an AABB that may intersect aabb
	      if (node->aabb == aabb) continue;
	      if (node->aabb->level == level &&
		  node->x == xi && node->y == yi && node->z == zi) {
		// see if aabb and node->aabb have already been tested
		// against each other
		unsigned char mask;
		if (aabb->index <= node->aabb->index) {
		  i = (aabb->index * tested_rowsize)+(node->aabb->index >> 3);
		  mask = 1 << (node->aabb->index & 7);
		}
		else {
		  i = (node->aabb->index * tested_rowsize)+(aabb->index >> 3);
		  mask = 1 << (aabb->index & 7);
		}
		dIASSERT (i >= 0 && i < (tested_rowsize*n));
		if ((tested[i] & mask)==0) {
		  collideAABBs (aabb->bounds,node->aabb->bounds,
				aabb->geom,node->aabb->geom,
				data,callback);
		}
		tested[i] |= mask;
	      }
	    }
	  }
	}
      }
      // get the discrete bounds for the next level up
      for (i=0; i<6; i++) db[i] >>= 1;
    }
  }

  // every AABB in the normal list must now be intersected against every
  // AABB in the big_boxes list. so let's hope there are not too many objects
  // in the big_boxes list.
  for (aabb=first_aabb; aabb; aabb=aabb->next) {
    for (dxAABB *aabb2=big_boxes; aabb2; aabb2=aabb2->next) {
      collideAABBs (aabb->bounds,aabb2->bounds,aabb->geom,aabb2->geom,
		    data,callback);
    }
  }

  // intersected all AABBs in the big_boxes list together
  for (aabb=big_boxes; aabb; aabb=aabb->next) {
    for (dxAABB *aabb2=aabb->next; aabb2; aabb2=aabb2->next) {
      collideAABBs (aabb->bounds,aabb2->bounds,aabb->geom,aabb2->geom,
		    data,callback);
    }
  }

  // reset the aabb fields of the geoms back to 0
  for (geom=first; geom; geom=geom->space.next) geom->space_aabb = 0;
}


int dxHashSpace::query (dGeomID obj)
{
  dAASSERT (this && obj);
  if (obj->spaceid != this) return 0;
  dGeomID compare = first;
  while (compare) {
    if (compare == obj) return 1;
    compare = compare->space.next;
  }
  dDebug (0,"object is not in the space it thinks it is in");
  return 0;
}

//****************************************************************************
// space functions

void dSpaceDestroy (dxSpace * space)
{
  space->destroy();
}


void dSpaceAdd (dxSpace * space, dxGeom *g)
{
  space->add (g);
}


void dSpaceRemove (dxSpace * space, dxGeom *g)
{
  space->remove (g);
}


int dSpaceQuery (dxSpace * space, dxGeom *g)
{
  return space->query (g);
}


void dSpaceCollide (dxSpace * space, void *data, dNearCallback *callback)
{
  space->collide (data,callback);
}
