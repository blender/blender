/** Interfacing with Blender
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
  *
  *  $Id$
  *
  * This code is currently messy and an attempt to restructure
  * some Blender kernel level code.
  * Hopefully a template for a future C-API...
  *
  *
  */


#include "BLI_blenlib.h" // mallocs
#include "BLI_arithb.h"

#include "BKE_library.h" 
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_mesh.h"
#include "BKE_ipo.h"

#include "MEM_guardedalloc.h"

#include "Python.h" 
#include "BPY_macros.h" 
#include "structmember.h" 

#include "BDR_editobject.h"

#include "b_interface.h"

/************************************************************************
 * Generic low level routines
 *
 */

/** This just returns a pointer to the global struct.
  *
  * Mainly introduced for debugging purposes..
  *
  */

Global *getGlobal(void)
{
	return &G;
}	

/** define list getters:
	These functions return a linked list pointer (ListBase *) from the global
	Blender-object list.

	Example:
		oblist = getObjectList();
		firstobject = oblist->first;

	 */

/*
DEF_GETLIST(Scene, scene)
DEF_GETLIST(Object, object)
DEF_GETLIST(Mesh, mesh)
DEF_GETLIST(Camera, camera)
DEF_GETLIST(Material, mat)
DEF_GETLIST(Lamp, lamp)
DEF_GETLIST(World, world)
DEF_GETLIST(Ipo, ipo)
DEF_GETLIST(Image, image)
DEF_GETLIST(Texture, tex)
DEF_GETLIST(Text, text)
DEF_GETLIST(Key, key)
*/

/* gets a datablock object from the ID list by name */
ID *getFromList(ListBase *list, char *name) 
{
	ID *id = list->first;

	while (id) {
		if(STREQ(name, getIDName(id))) break;
			id= id->next;
	}
	return id;
}

void printList(ListBase *list)
{
	ID *walk = list->first;
	ID *lastwalk = 0;
	printf("List: %s\n", walk->name);
	while (walk) {
		printf("   %s\n", walk->name);
		lastwalk = walk;
		walk= walk->next;
	}
	if (list->last != lastwalk)
	{
		printf("****: listbase->last pointing to wrong end!\n");
		// list->last = lastwalk;
	}
}


/** (future) garbage collector subroutine */


int gc_mainlist(ListBase *lb)
{
	ID *id = (ID *) lb->first;

	while (id) {
		if (getIDUsers(id) == 0) {
			switch(GET_ID_TYPE(id)) {
			case ID_OB:
				BPY_debug(("free [Object %s]\n", getIDName(id)));
				unlink_object((Object *) id);
				free_libblock(lb, id);
				break;
			default: break;
			}
		}
		id = id->next;
	}
	return 1;
}

	/** Garbage collection function.  EXPERIMENTAL!
	 *  This should free Blender from all unreferenced Objects (i.e. 
	 *  user count == 0). 
	 *  Don't expect too much yet -- User counting isn't done
	 *  consequently in Blender. Neither parenting or bevelcurves
	 *  etc. respect user counts...therefore, no circular references
	 *  show up -- which are in fact possible; example:
	 *
	 *  A BevelCurve is parenting its BevelObject: so there is a 
	 *  reference from the BevelObject to the BevelCurve, and a
	 *  reference back from the Bevelcurve to the BevelObject.
	 *
	 *  There are a lot of cleanup functions in Blender taking care
	 *  of updating (invalidating) references to deleted objects.
	 *  See unlink_object() for more details.
	 *
	 *  This function must not be called inside a script, so don't go
	 *  and create a wrapper for it :-)
	 *  In a hopefully later implementation, the Python garbage collection
	 *  might be used. For the moment, this is better than 'Save and Reload'
	 */

	int garbage_collect(Main *m)
	{
		/* Remember, all descriptor objects must BOB_DECUSER on their raw 
		Blender Datablock in their __del__ method (C-API: dealloc function) */

		gc_mainlist(&m->object);

		/* TODO proper kernel level functions for safely freeing these objects
		 * must first be implemented... 
		gc_mainlist(&m->mesh);
	gc_mainlist(&m->mat);
	gc_mainlist(&m->lamp);
	gc_mainlist(&m->camera);

	.. and this list is far from being complete.
	*/

	return 1;
}

/** expands pointer array of length 'oldsize' to length 'newsize'.
  * A pointer to the (void *) array must be passed as first argument 
  * The array pointer content can be NULL, in this case a new array of length
  * 'newsize' is created.
  */

static int expandPtrArray(void **p, int oldsize, int newsize)
{
	void *newarray;

	if (newsize < oldsize) {
		return 0;
	}	
	newarray = MEM_callocN(newsize * sizeof(void *), "PtrArray");
	if (*p) {
		memcpy(newarray, *p, oldsize);
		MEM_freeN(*p);
	}	
	*p = newarray;
	return 1;
}

/************************************************************************
 * Material object low level routines
 *
 */

/* MAXMAT = maximum number of materials per object/ object data */

#define MATINDEX_CHECK(x) \
	if ((x) < 0 || (x) >= MAXMAT) { printf("illegal matindex!\n"); return 0; }

/** Returns a new material list (material pointer array) of length 'len'
  *
  */

Material **newMaterialList(int len)
{
	Material **matlist = 
		(Material **) MEM_mallocN(len * sizeof(Material *), "MaterialList");
	return matlist;
}

/** releases material list and decrements user count on materials */

int releaseMaterialList(Material **matlist, int len)
{
	int i;
	Material *m;

	MATINDEX_CHECK(len);

	for (i= 0; i < len; i++) {

		m = matlist[i];
		BOB_XDECUSER((ID *) m);
	}
	MEM_freeN(matlist); 
	return 1;
}

/** Synchronizes Object <-> data material lists. Blender just wants it. */

int synchronizeMaterialLists(Object *object, void *data)
{
	// get pointer to data's material array:
	// and            number of data materials
	// ... because they will need modification.

	Material ***p_dataMaterials = give_matarar(object); 
	short *nmaterials = give_totcolp(object);

	if (object->totcol > *nmaterials){ // more object mats than data mats
		*nmaterials = object->totcol;
		return expandPtrArray((void *) p_dataMaterials, *nmaterials, object->totcol);
	} else if (object->totcol < *nmaterials) {
		object->totcol = *nmaterials;
		return expandPtrArray((void *) &object->mat, object->totcol, *nmaterials);
	}
	return 1; // in this case, no synchronization needed; they're of equal
	          // length
}

/************************************************************************
 * Object low level routines
 *
 */

/** creates a new empty object of type OB_ (TODO: should be enum)
  *
  */


Object *object_new(int type)
{
	Object *object;
	char name[32];

	Global *g = getGlobal();

	switch(type) {
		case OB_MESH: strcpy(name, "Mesh"); break;
		case OB_CURVE: strcpy(name, "Curve"); break;
		case OB_SURF: strcpy(name, "Surf"); break;
		case OB_FONT: strcpy(name, "Text"); break;
		case OB_MBALL: strcpy(name, "Mball"); break;
		case OB_CAMERA: strcpy(name, "Camera"); break;
		case OB_LAMP: strcpy(name, "Lamp"); break;
		case OB_IKA: strcpy(name, "Ika"); break;
		case OB_LATTICE: strcpy(name, "Lattice"); break;
		case OB_WAVE: strcpy(name, "Wave"); break;
		case OB_ARMATURE: strcpy(name,"Armature");break;
		default:  strcpy(name, "Empty");
	}

	object = alloc_libblock(getObjectList(), ID_OB, name);

	/* user count is set to 1 by alloc_libblock, we just reset it to 0... */
	BOB_USERCOUNT((ID*) object) = 0; // it's a new object, so no user yet
	object->flag = 0;
	object->type = type;

	/* transforms */
	QuatOne(object->quat);
	QuatOne(object->dquat);
	
    object->col[3]= 1.0;    // alpha 

	object->size[0] = object->size[1] = object->size[2] = 1.0;
	object->loc[0] = object->loc[1] = object->loc[2] = 0.0;
	Mat4One(object->parentinv);
	Mat4One(object->obmat);
	object->dt = OB_SHADED; // drawtype

	object_setdefaults(object);

	object->lay = 1; // Layer, by default visible

	switch(type) {
		case OB_MESH: object->data= add_mesh(); g->totmesh++; break;
		case OB_CAMERA: object->data= add_camera(); break;
		case OB_LAMP: object->data= add_lamp(); g->totlamp++; break;

	// TODO the following types will be supported later
	//	case OB_CURVE: object->data= add_curve(OB_CURVE); g->totcurve++; break;
	//	case OB_SURF: object->data= add_curve(OB_SURF); g->totcurve++; break;
	//	case OB_FONT: object->data= add_curve(OB_FONT); break;
	//	case OB_MBALL: object->data= add_mball(); break;
	//	case OB_IKA: object->data= add_ika(); object->dt= OB_WIRE; break;
	//	case OB_LATTICE: object->data= (void *)add_lattice(); object->dt= OB_WIRE; break;
	//	case OB_WAVE: object->data= add_wave(); break;
	//	case OB_ARMATURE: object->data=add_armature();break;
	}

	g->totobj++; // gee, I *hate* G
	return object;
}

/* returns new Base */
Base *object_newBase(Object *object)
{
	Base *base;
	base = MEM_callocN(sizeof(Base), "newbase");
	if (!base)
		return 0;
	base->object = object;
	base->lay = object->lay;
	base->flag = object->flag;
	return base;
}

Object *object_copy(Object *object)
{
	Object *new;

	new = copy_object(object);
	BOB_USERCOUNT((ID*) new) = 0; // it's a new object, so no user yet
	return new;
}

/* Set draw mode of object */
void object_setDrawMode(Object *object, int modebits)
{
	object->dt = (modebits & 0xff);
	object->dtx = (modebits >> 8);
}

int object_getDrawMode(Object *object)
{
	return (((int) object->dtx) << 8 ) + object->dt;
}

/* link data to Object object */
int object_linkdata(Object *object, void *data)
{
	ID *oldid, *id;
	int valid;

	if (!data) return 0;

	oldid = (ID*) object->data; 
	id = (ID*) data;

	valid = 0;

#define _CASE(objtype, idtype) \
	case objtype:\
		if (GET_ID_TYPE(id) == idtype) \
			valid = 1; \
		break;	

	switch (object->type) {
	_CASE(OB_MESH, ID_ME)
	_CASE(OB_CAMERA, ID_CA)
	_CASE(OB_LAMP, ID_LA)
	default: // not supported
		return 0;
	}
	if (valid) {
		object->data = data;
		BOB_INCUSER(id);
		if (oldid)
			BOB_DECUSER(oldid); // dec users

		// extra check for multi materials on meshes:
		// This is a hack to check whether object material lists are of same
		// length as their data material lists..
		//if (GET_ID_TYPE(id) == ID_ME) {
			//test_object_materials(id);
		//}	
		return 1;
	}
	return 0;
}

/* release data from object object */

int object_unlinkdata(Object *object)
{
	ID *id = object->data;

	BOB_XDECUSER(id);
	return 1;
}

/** set Object materials:
  * takes a list of Material pointers of maximum length MAXMAT
  */

int object_setMaterials(Object *object, Material **matlist, int len)
{
	int i;

	MATINDEX_CHECK(len)
	if (object->mat) {
		releaseMaterialList(object->mat, len);
	}
	// inc user count on all materials
	for (i = 0; i < len; i++) {
		BOB_XINCUSER( (ID *) matlist[i]);
	}

	object->mat = matlist;
	object->totcol = len;
	object->actcol = len - 1; // XXX
	// workaround: blender wants the object's data material list
	// to be of the same length, otherwise colourful fun happens.
	// so, we synchronize this here:

	switch (object->type)
	{
		case OB_MESH:
		case OB_CURVE:
		case OB_FONT:
		case OB_SURF:
		case OB_MBALL:
			synchronizeMaterialLists(object, object->data);
			break;
		default:
			return 0;
	}
	return 1;
}

/** make 'object' the parent of the object 'child' 
  *
  * mode = 1: set parent inverse matrix to _1_ ('clear inverse')
  * fast = 1: Don't update scene base (hierarchy). In this case,
  *           sort_baselist() needs to be called explicitely before redraw.
  */

int object_makeParent(Object *parent, Object *child, int mode, int fast) 
{
	if (test_parent_loop(parent, child)) {
		PyErr_SetString(PyExc_RuntimeError, "parenting loop detected!");
		return 0;
	}
	child->partype = PAROBJECT;
	child->parent = parent;
	if (mode == 1) {
		Mat4One(child->parentinv); // parent inverse = unity
		child->loc[0] = 0.0; child->loc[1] = 0.0; child->loc[2] = 0.0; 
	} else {
		what_does_parent(child);
		Mat4Invert(child->parentinv, parent->obmat); // save inverse
	}

	/* This is some bad global thing again -- we should determine 
	   the current scene
	   another way. Later. */
	if (!fast) 
		sort_baselist(getGlobal()->scene);

	return 1;
}

/** Unlink parenting hierarchy:
  *
  * mode = 2: keep transform
  * fast = 1: don't update scene bases. see makeParent()
  */

int object_clrParent(Object *child, int mode, int fast) 
{
	Object *par;

	par = child->parent;
	child->parent = 0;
	if (mode == 2) { // keep transform
		apply_obmat(child);
	}
	if (!fast) 
		sort_baselist(getGlobal()->scene);
	return 1;
}

/** Set object's defaults */

int object_setdefaults(Object *ob)
{
	if(U.flag & MAT_ON_OB) ob->colbits= -1;
	switch(ob->type) {
		case OB_CAMERA:
		case OB_LAMP:
			ob->trackflag = OB_NEGZ;
			ob->upflag = OB_POSY;
			break;
		default:
			ob->trackflag = OB_POSY;
			ob->upflag = OB_POSZ;
	}
	ob->ipoflag = OB_OFFS_OB + OB_OFFS_PARENT;

	/* duplivert settings */

	ob->dupon = 1; ob->dupoff = 0;
	ob->dupsta = 1; ob->dupend = 100;

	/* Gameengine defaults*/
	ob->mass= ob->inertia= 1.0;
	ob->formfactor= 0.4;
	ob->damping= 0.04;
	ob->rdamping= 0.1;
	ob->anisotropicFriction[0] = 1.0;
	ob->anisotropicFriction[1] = 1.0;
	ob->anisotropicFriction[2] = 1.0;

	/* default to not use fh in new system */
	ob->gameflag= OB_PROP;	/*|OB_DO_FH; */
	
	return 1;
}

/************************************************************************
 * Creation of new data blocks
 * 
 * We [ab|re]use the blender kernel functions, but set the user count to 0,
 * because the object does not have users yet.
 * Currently, the user count is abused as reference count which should be
 * separate in future
 */

Material *material_new(void)
{
	Material *m = add_material("Material");
	BOB_USERCOUNT((ID*) m) = 0; // set 'refcount' to 0, because 
	                            // it's a free material
	return m;
}

Lamp *lamp_new()
{
	Lamp *la;

	la = add_lamp();
	BOB_USERCOUNT((ID*) la) = 0; 

	return la;
}	

Camera *camera_new()
{
	Camera *cam;
        
 	cam = add_camera();
	BOB_USERCOUNT((ID*) cam) = 0; 
	return cam;
}	

Ipo *ipo_new(int type, char *name)
{
	Ipo *ipo;

	ipo = add_ipo(name, type);
	BOB_USERCOUNT((ID*) ipo) = 0; 
	return ipo;
}


/* Finds the ipo curve with channel code 'code' in the datablock 'ipo' 
   and returns it, if found (NULL otherwise) */

IpoCurve *ipo_findcurve(Ipo *ipo, int code) 
{
	IpoCurve *ipocurve;

	ipocurve = ipo->curve.first;
	while(ipocurve) {
		if (ipocurve->adrcode == code) break;
		ipocurve = ipocurve->next;
	}
	return ipocurve;
}	


/** Returns a new Ipo curve */
IpoCurve *ipocurve_new()
{
	IpoCurve *curve;

	curve = MEM_callocN(sizeof(IpoCurve), "new_ipocurve");
	curve->flag = IPO_VISIBLE;
	return curve;
}	

IpoCurve *ipocurve_copy(IpoCurve *curve)
{
	IpoCurve *newcurve;

	newcurve = MEM_callocN(sizeof(IpoCurve), "new_ipocurve");
	memcpy(newcurve, curve, sizeof(IpoCurve));
	// copy bez triples:
	newcurve->bezt= MEM_mallocN(curve->totvert*sizeof(BezTriple), "ipocurve_copy");
	memcpy(newcurve->bezt, curve->bezt, curve->totvert*sizeof(BezTriple)); 
	return newcurve;
}

/** Assign ipo to object object */

/* macros, see b_interface.h */

DEF_ASSIGN_IPO(object, Object)  // defines object_assignIpo()

DEF_ASSIGN_IPO(camera, Camera)  

DEF_ASSIGN_IPO(lamp, Lamp)  

DEF_ASSIGN_IPO(material, Material)  

/************************************************************************
 * Mesh object low level routines
 *
 */

/** Returns a new, free (non owned) mesh.
  * add_mesh() automatically returns a mesh object with users = 1,
  * so we set it to 0. Hack, hack.
  */

Mesh *mesh_new(void)
{
	Mesh *me = add_mesh();
	((ID *) me)->us = 0;
	return me;
}

/** updates drawing properties etc. of mesh */

void mesh_update(Mesh *mesh)
{
	edge_drawflags_mesh(mesh);
	tex_space_mesh(mesh);
}

/************************************************************************
 * Scene object low level routines
 *
 */

/** Returns current Scene */

Scene *scene_getCurrent(void)
{
	return getGlobal()->scene;
}

/* returns base of object 'object' in Scene 'scene', 0 if nonexistant 
 * A base is basically an visual instantiation of an 3D object (Object)
 * in a Scene. See scene_linkObject()
 *
 */

Base *scene_getObjectBase(Scene *scene, Object *object)
{
	Base *base;
	base = scene->base.first;
	while (base)
	{
		if (object == base->object) // it exists
			return base;
		base = base->next;
	}
	return NULL;
}

/* links an object into a scene */
int scene_linkObject(Scene *scene, Object *object)
{
	Base *base, *b;
	b = scene_getObjectBase(scene, object);
	if (b)
		return 0;
	base = object_newBase(object); 
	if (!base) { 
		return 0;
	}	
	BOB_INCUSER((ID *) object); // incref the object 
	BLI_addhead(&scene->base, base);
	return 1;
}

/* unlinks an object from a scene */
int scene_unlinkObject(Scene *scene, Object *object)
{
	Base *base;
	base = scene_getObjectBase(scene, object);
	if (base) {
		BLI_remlink(&scene->base, base);
		BOB_DECUSER((ID *) object);
		MEM_freeN(base);
		scene->basact = 0; // make sure the just deleted object has no longer an 
		                   // active base (which happens if it was selected
		return 1;
	}
	else return 0;
}

