/**
 * $Id$
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
 */
#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_text_types.h"
#include "DNA_curve_types.h"
#include "DNA_screen_types.h"
#include "DNA_camera_types.h"
#include "DNA_ipo_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_scriptlink_types.h"
#include "DNA_userdef_types.h" /* for userdata struct; U.pythondir */

#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_global.h"

/* DEFINES */

#define ASSIGN_IPO(prefix, type)            \
	prefix##_assignIpo(type *obj, Ipo *ipo)

// example DEF_ASSIGN_IPO(Object, obj) ->
//    int object_assignIpo(Object *obj, Ipo *ipo)

#define DEF_ASSIGN_IPO(prefix, type)            \
	int prefix##_assignIpo(type *obj, Ipo *ipo) \
	{                                           \
		BOB_XDECUSER((ID*) obj->ipo);            \
		BOB_XINCUSER((ID*) ipo);                  \
		obj->ipo = ipo;                         \
		return 1;                               \
	}                                           \

// defined prototypes:

#define FUNC_ASSIGN_IPO(prefix, arg1, arg2) \
	prefix##_assignIpo(arg1, arg2)

#define object_assignIpo(arg1, arg2) FUNC_ASSIGN_IPO(object, arg1, arg2)
#define material_assignIpo(arg1, arg2) FUNC_ASSIGN_IPO(material, arg1, arg2)
#define camera_assignIpo(arg1, arg2) FUNC_ASSIGN_IPO(camera, arg1, arg2)
#define lamp_assignIpo(arg1, arg2) FUNC_ASSIGN_IPO(lamp, arg1, arg2)

/** Defines for List getters */

/*
#define PROTO_GETLIST(name, member)  \
	ListBase *get##name##List(void) 

#define DEF_GETLIST(name, member)    \
	PROTO_GETLIST(name, member)      \
	{                                \
		return &(G.main->member);       \
	}
*/

/* PROTOS  */

#define _GETMAINLIST(x) \
	(&(G.main->x))

#define getSceneList()  _GETMAINLIST(scene)
#define getObjectList()  _GETMAINLIST(object)
#define getMeshList()  _GETMAINLIST(mesh)
#define getMaterialList()  _GETMAINLIST(mat)
#define getCameraList()  _GETMAINLIST(camera)
#define getLampList()  _GETMAINLIST(lamp)
#define getWorldList()  _GETMAINLIST(world)
#define getIpoList()  _GETMAINLIST(ipo)
#define getImageList()  _GETMAINLIST(image)
#define getTextureList()  _GETMAINLIST(tex)
#define getTextList()  _GETMAINLIST(text)
#define getKeyList()  _GETMAINLIST(key)
#define getLatticeList()  _GETMAINLIST(latt)

/*
PROTO_GETLIST(Scene, scene);
PROTO_GETLIST(Object, object);
PROTO_GETLIST(Mesh, mesh);
PROTO_GETLIST(Camera, camera);
PROTO_GETLIST(Material, mat);
PROTO_GETLIST(Lamp, lamp);
PROTO_GETLIST(World, world);
PROTO_GETLIST(Ipo, ipo);
PROTO_GETLIST(Image, image);
PROTO_GETLIST(Texture, tex);
PROTO_GETLIST(Text, text);
PROTO_GETLIST(Key, key);  */


Global    *getGlobal(void); // get Global struct

ID        *getFromList(ListBase *list, char *name);

int        garbage_collect(Main *m);


Material **newMaterialList(int len);
int        releaseMaterialList(struct Material **matlist, int len);
int        synchronizeMaterialLists(Object *object, void *data);

// Datablock management

Material  *material_new(void);
int        material_assignIpo(Material *, Ipo *);

Lamp      *lamp_new(void);
int        lamp_assignIpo(Lamp *, Ipo *);

Camera    *camera_new(void);
int        camera_assignIpo(Camera *, Ipo *);

Ipo       *ipo_new(int type, char *name);
IpoCurve  *ipo_findcurve(Ipo *ipo, int code);
IpoCurve  *ipocurve_new(void);
IpoCurve  *ipocurve_copy(IpoCurve *curve);

// Object management
Base      *object_newBase(Object *obj);
int        object_linkdata(Object *obj, void *data);
int        object_unlinkdata(Object *obj);
int        object_setMaterials(Object *object, Material **matlist, int len);
int        object_setdefaults(Object *ob);
int        object_copyMaterialsTo(Object *object, Material **matlist, int len);
int        object_makeParent(Object *parent, Object *child, int noninverse, int fast);
int        object_clrParent(Object *child, int mode, int fast);
Object    *object_new(int type);
Object    *object_copy(Object *obj);
void       object_setDrawMode(Object *object, int val);
int        object_getDrawMode(Object *object);

int        object_assignIpo(Object *, Ipo *);

Scene     *scene_getCurrent(void);
int        scene_linkObject(Scene *scene, Object *obj);
int        scene_unlinkObject(Scene *scene, Object *object);
Base      *scene_getObjectBase(Scene *scene, Object *object);

Mesh      *mesh_new(void);
void       mesh_update(Mesh *me);

/* blender's program name */
extern char bprogname[];  /* init in creator.c */

