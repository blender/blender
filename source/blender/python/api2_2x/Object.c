/* 
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 *
 * The Object module provides generic access to Objects of various types via
 * the Python interface.
 *
 *
 * Contributor(s): Michel Selten, Willian Germano, Jacques Guignot,
 * Joseph Gilbert, Stephen Swaney, Bala Gi, Campbell Barton, Johnny Matthews,
 * Ken Hughes, Alex Mole, Jean-Michel Soler, Cedric Paille
 *
 * ***** END GPL LICENSE BLOCK *****
*/

struct SpaceIpo;
struct rctf;

#include "Object.h" /*This must come first */

#include "DNA_object_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_force.h"
#include "DNA_userdef_types.h"
#include "DNA_key_types.h" /* for pinShape and activeShape */

#include "BKE_action.h"
#include "BKE_anim.h" /* used for dupli-objects */
#include "BKE_depsgraph.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_property.h"
#include "BKE_mball.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BKE_armature.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_nla.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_idprop.h"
#include "BKE_object.h"
#include "BKE_key.h" /* for setting the activeShape */
#include "BKE_displist.h"
#include "BKE_pointcache.h"
#include "BKE_particle.h"

#include "BSE_editipo.h"
#include "BSE_edit.h"

#include "BIF_space.h"
#include "BIF_editview.h"
#include "BIF_drawscene.h"
#include "BIF_meshtools.h"
#include "BIF_editarmature.h"
#include "BIF_editaction.h"
#include "BIF_editnla.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BDR_editobject.h"
#include "BDR_editcurve.h"
#include "BDR_drawobject.h"

#include "MEM_guardedalloc.h"

#include "mydevice.h"
#include "blendef.h"
#include "Scene.h"
#include "Mathutils.h"
#include "Mesh.h"
#include "NMesh.h"
#include "Curve.h"
#include "Ipo.h"
#include "Armature.h"
#include "Pose.h"
#include "Camera.h"
#include "Lamp.h"
#include "Lattice.h"
#include "Text.h"
#include "Text3d.h"
#include "Metaball.h"
#include "Draw.h"
#include "NLA.h"
#include "logic.h"
#include "Effect.h"
#include "Group.h"
#include "Modifier.h"
#include "Constraint.h"
#include "gen_utils.h"
#include "gen_library.h"
#include "EXPP_interface.h"
#include "BIF_editkey.h"
#include "IDProp.h"
#include "Particle.h"

/* Defines for insertIpoKey */

#define IPOKEY_LOC              0
#define IPOKEY_ROT              1
#define IPOKEY_SIZE             2
#define IPOKEY_LOCROT           3
#define IPOKEY_LOCROTSIZE       4
#define IPOKEY_PI_STRENGTH      5
#define IPOKEY_PI_FALLOFF       6
#define IPOKEY_PI_MAXDIST       7 /*Not Ready Yet*/
#define IPOKEY_PI_SURFACEDAMP   8
#define IPOKEY_PI_RANDOMDAMP    9
#define IPOKEY_PI_PERM          10
#define IPOKEY_LAYER			19

#define PFIELD_FORCE	1
#define PFIELD_VORTEX	2
#define PFIELD_MAGNET	3
#define PFIELD_WIND		4

enum obj_consts {
	EXPP_OBJ_ATTR_LOC_X = 0,
	EXPP_OBJ_ATTR_LOC_Y,
	EXPP_OBJ_ATTR_LOC_Z,
	EXPP_OBJ_ATTR_DLOC_X,
	EXPP_OBJ_ATTR_DLOC_Y,
	EXPP_OBJ_ATTR_DLOC_Z,
	EXPP_OBJ_ATTR_ROT_X,
	EXPP_OBJ_ATTR_ROT_Y,
	EXPP_OBJ_ATTR_ROT_Z,
	EXPP_OBJ_ATTR_DROT_X,
	EXPP_OBJ_ATTR_DROT_Y,
	EXPP_OBJ_ATTR_DROT_Z,
	EXPP_OBJ_ATTR_SIZE_X,
	EXPP_OBJ_ATTR_SIZE_Y,
	EXPP_OBJ_ATTR_SIZE_Z,
	EXPP_OBJ_ATTR_DSIZE_X,
	EXPP_OBJ_ATTR_DSIZE_Y,
	EXPP_OBJ_ATTR_DSIZE_Z,
	EXPP_OBJ_ATTR_LOC,
	EXPP_OBJ_ATTR_DLOC,
	EXPP_OBJ_ATTR_DROT,
	EXPP_OBJ_ATTR_SIZE,
	EXPP_OBJ_ATTR_DSIZE,
	EXPP_OBJ_ATTR_LAYERMASK,
	EXPP_OBJ_ATTR_COLBITS,
	EXPP_OBJ_ATTR_DRAWMODE,
	EXPP_OBJ_ATTR_DRAWTYPE,
	EXPP_OBJ_ATTR_DUPON,
	EXPP_OBJ_ATTR_DUPOFF,
	EXPP_OBJ_ATTR_DUPSTA,
	EXPP_OBJ_ATTR_DUPEND,
 	EXPP_OBJ_ATTR_DUPFACESCALEFAC,
	EXPP_OBJ_ATTR_TIMEOFFSET,
	EXPP_OBJ_ATTR_DRAWSIZE,
	EXPP_OBJ_ATTR_PARENT_TYPE,
	EXPP_OBJ_ATTR_PASSINDEX,
	EXPP_OBJ_ATTR_ACT_MATERIAL,
	EXPP_OBJ_ATTR_ACT_SHAPE,
	
	EXPP_OBJ_ATTR_PI_SURFACEDAMP,	/* these need to stay together */
	EXPP_OBJ_ATTR_PI_RANDOMDAMP,	/* and in order */
	EXPP_OBJ_ATTR_PI_PERM,
	EXPP_OBJ_ATTR_PI_STRENGTH,
	EXPP_OBJ_ATTR_PI_FALLOFF,
	EXPP_OBJ_ATTR_PI_MAXDIST,
	EXPP_OBJ_ATTR_PI_SBDAMP,
	EXPP_OBJ_ATTR_PI_SBIFACETHICK,
	EXPP_OBJ_ATTR_PI_SBOFACETHICK,

	EXPP_OBJ_ATTR_SB_NODEMASS,	/* these need to stay together */
	EXPP_OBJ_ATTR_SB_GRAV,		/* and in order */
	EXPP_OBJ_ATTR_SB_MEDIAFRICT,
	EXPP_OBJ_ATTR_SB_RKLIMIT,
	EXPP_OBJ_ATTR_SB_PHYSICSSPEED,
	EXPP_OBJ_ATTR_SB_GOALSPRING,
	EXPP_OBJ_ATTR_SB_GOALFRICT,
	EXPP_OBJ_ATTR_SB_MINGOAL,
	EXPP_OBJ_ATTR_SB_MAXGOAL,
	EXPP_OBJ_ATTR_SB_DEFGOAL,
	EXPP_OBJ_ATTR_SB_INSPRING,
	EXPP_OBJ_ATTR_SB_INFRICT,

};

#define EXPP_OBJECT_DRAWSIZEMIN         0.01f
#define EXPP_OBJECT_DRAWSIZEMAX         10.0f

/* clamping and range values for particle interaction settings */
#define EXPP_OBJECT_PIDAMP_MIN           0.0f
#define EXPP_OBJECT_PIDAMP_MAX           1.0f
#define EXPP_OBJECT_PIRDAMP_MIN          0.0f
#define EXPP_OBJECT_PIRDAMP_MAX          1.0f
#define EXPP_OBJECT_PIPERM_MIN           0.0f
#define EXPP_OBJECT_PIPERM_MAX           1.0f
#define EXPP_OBJECT_PISTRENGTH_MIN       0.0f
#define EXPP_OBJECT_PISTRENGTH_MAX    1000.0f
#define EXPP_OBJECT_PIPOWER_MIN          0.0f
#define EXPP_OBJECT_PIPOWER_MAX         10.0f
#define EXPP_OBJECT_PIMAXDIST_MIN        0.0f
#define EXPP_OBJECT_PIMAXDIST_MAX     1000.0f
#define EXPP_OBJECT_PISBDAMP_MIN         0.0f
#define EXPP_OBJECT_PISBDAMP_MAX         1.0f
#define EXPP_OBJECT_PISBIFTMIN         0.001f
#define EXPP_OBJECT_PISBIFTMAX           1.0f
#define EXPP_OBJECT_PISBOFTMIN         0.001f
#define EXPP_OBJECT_PISBOFTMAX           1.0f

/* clamping and range values for softbody settings */
#define EXPP_OBJECT_SBMASS_MIN           0.0f
#define EXPP_OBJECT_SBMASS_MAX          50.0f
#define EXPP_OBJECT_SBGRAVITY_MIN        0.0f
#define EXPP_OBJECT_SBGRAVITY_MAX       10.0f
#define EXPP_OBJECT_SBFRICTION_MIN       0.0f
#define EXPP_OBJECT_SBFRICTION_MAX      10.0f
#define EXPP_OBJECT_SBSPEED_MIN          0.01f
#define EXPP_OBJECT_SBSPEED_MAX        100.0f
#define EXPP_OBJECT_SBERRORLIMIT_MIN     0.01f
#define EXPP_OBJECT_SBERRORLIMIT_MAX     1.0f
#define EXPP_OBJECT_SBGOALSPRING_MIN     0.0f
#define EXPP_OBJECT_SBGOALSPRING_MAX     0.999f
#define EXPP_OBJECT_SBGOALFRICT_MIN      0.0f
#define EXPP_OBJECT_SBGOALFRICT_MAX     10.0f
#define EXPP_OBJECT_SBMINGOAL_MIN        0.0f
#define EXPP_OBJECT_SBMINGOAL_MAX        1.0f
#define EXPP_OBJECT_SBMAXGOAL_MIN        0.0f
#define EXPP_OBJECT_SBMAXGOAL_MAX        1.0f
#define EXPP_OBJECT_SBINSPRING_MIN       0.0f
#define EXPP_OBJECT_SBINSPRING_MAX     0.999f
#define EXPP_OBJECT_SBINFRICT_MIN        0.0f
#define EXPP_OBJECT_SBINFRICT_MAX       10.0f
#define EXPP_OBJECT_SBDEFGOAL_MIN        0.0f
#define EXPP_OBJECT_SBDEFGOAL_MAX        1.0f
#define EXPP_OBJECT_SBNODEMASSMIN      0.001f
#define EXPP_OBJECT_SBNODEMASSMAX       50.0f
#define EXPP_OBJECT_SBGRAVMIN            0.0f
#define EXPP_OBJECT_SBGRAVMAX           10.0f
#define EXPP_OBJECT_SBMEDIAFRICTMIN      0.0f
#define EXPP_OBJECT_SBMEDIAFRICTMAX     10.0f
#define EXPP_OBJECT_SBRKLIMITMIN        0.01f
#define EXPP_OBJECT_SBRKLIMITMAX         1.0f
#define EXPP_OBJECT_SBPHYSICSSPEEDMIN   0.01f
#define EXPP_OBJECT_SBPHYSICSSPEEDMAX  100.0f
#define EXPP_OBJECT_SBGOALSPRINGMIN      0.0f
#define EXPP_OBJECT_SBGOALSPRINGMAX    0.999f
#define EXPP_OBJECT_SBGOALFRICTMIN       0.0f
#define EXPP_OBJECT_SBGOALFRICTMAX      10.0f
#define EXPP_OBJECT_SBMINGOALMIN         0.0f
#define EXPP_OBJECT_SBMINGOALMAX         1.0f
#define EXPP_OBJECT_SBMAXGOALMIN         0.0f
#define EXPP_OBJECT_SBMAXGOALMAX         1.0f
#define EXPP_OBJECT_SBDEFGOALMIN         0.0f
#define EXPP_OBJECT_SBDEFGOALMAX         1.0f
#define EXPP_OBJECT_SBINSPRINGMIN        0.0f
#define EXPP_OBJECT_SBINSPRINGMAX      0.999f
#define EXPP_OBJECT_SBINFRICTMIN         0.0f
#define EXPP_OBJECT_SBINFRICTMAX        10.0f
#define EXPP_OBJECT_DUPFACESCALEFACMIN  0.001f
#define EXPP_OBJECT_DUPFACESCALEFACMAX  10000.0f

/*****************************************************************************/
/* Python API function prototypes for the Blender module.		 */
/*****************************************************************************/
static PyObject *M_Object_New( PyObject * self, PyObject * args );
PyObject *M_Object_Get( PyObject * self, PyObject * args );
static PyObject *M_Object_GetSelected( PyObject * self );
static PyObject *M_Object_Duplicate( PyObject * self, PyObject * args, PyObject *kwd);

/* HELPER FUNCTION FOR PARENTING */
static PyObject *internal_makeParent(Object *parent, PyObject *py_child, int partype, int noninverse, int fast, int v1, int v2, int v3, char *bonename);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.	 */
/* In Python these will be written to the console when doing a		 */
/* Blender.Object.__doc__						 */
/*****************************************************************************/
char M_Object_doc[] = "The Blender Object module\n\n\
This module provides access to **Object Data** in Blender.\n";

char M_Object_New_doc[] =
	"(type) - Add a new object of type 'type' in the current scene";

char M_Object_Get_doc[] =
	"(name) - return the object with the name 'name', returns None if not\
	found.\n\
	If 'name' is not specified, it returns a list of all objects in the\n\
	current scene.";

char M_Object_GetSelected_doc[] =
	"() - Returns a list of selected Objects in the active layer(s)\n\
The active object is the first in the list, if visible";

char M_Object_Duplicate_doc[] =
	"(linked) - Duplicate all selected, visible objects in the current scene";


/*****************************************************************************/
/* Python method structure definition for Blender.Object module:	 */
/*****************************************************************************/
struct PyMethodDef M_Object_methods[] = {
	{"New", ( PyCFunction ) M_Object_New, METH_VARARGS,
	 M_Object_New_doc},
	{"Get", ( PyCFunction ) M_Object_Get, METH_VARARGS,
	 M_Object_Get_doc},
	{"GetSelected", ( PyCFunction ) M_Object_GetSelected, METH_NOARGS,
	 M_Object_GetSelected_doc},
	{"Duplicate", ( PyCFunction ) M_Object_Duplicate, METH_VARARGS | METH_KEYWORDS,
	 M_Object_Duplicate_doc},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* Python BPy_Object methods declarations:				   */
/*****************************************************************************/
static int setupSB(Object* ob); /*Make sure Softbody Pointer is initialized */
static int setupPI(Object* ob);

static PyObject *Object_getParticleSys( BPy_Object * self );
/* fixme Object_newParticleSys( self, default-partsys-name ) */
static PyObject *Object_addVertexGroupsFromArmature( BPy_Object * self, PyObject * args);
static PyObject *Object_newParticleSys( BPy_Object * self );
static PyObject *Object_buildParts( BPy_Object * self );
static PyObject *Object_clearIpo( BPy_Object * self );
static PyObject *Object_clrParent( BPy_Object * self, PyObject * args );
static PyObject *Object_clearTrack( BPy_Object * self, PyObject * args );
static PyObject *Object_getData(BPy_Object *self, PyObject *args, PyObject *kwd);
static PyObject *Object_getDeltaLocation( BPy_Object * self );
static PyObject *Object_getDrawMode( BPy_Object * self );
static PyObject *Object_getDrawType( BPy_Object * self );
static PyObject *Object_GetEuler( BPy_Object * self, PyObject * args );
static PyObject *Object_getInverseMatrix( BPy_Object * self );
static PyObject *Object_getIpo( BPy_Object * self );
static PyObject *Object_getLocation( BPy_Object * self, PyObject * args );
static PyObject *Object_getMaterials( BPy_Object * self, PyObject * args );
static PyObject *Object_getMatrix( BPy_Object * self, PyObject * args );
static PyObject *Object_getParent( BPy_Object * self );
static PyObject *Object_getParentBoneName( BPy_Object * self );
static int Object_setParentBoneName( BPy_Object * self, PyObject * value );
static PyObject *Object_getParentVertexIndex( BPy_Object * self );
static int Object_setParentVertexIndex( BPy_Object * self, PyObject * value );
static PyObject *Object_getSize( BPy_Object * self, PyObject * args );
static PyObject *Object_getTimeOffset( BPy_Object * self );
static PyObject *Object_getTracked( BPy_Object * self );
static PyObject *Object_getType( BPy_Object * self );
static PyObject *Object_getBoundBox( BPy_Object * self, PyObject *args );
static PyObject *Object_getBoundBox_noargs( BPy_Object * self );
static PyObject *Object_getAction( BPy_Object * self );
static PyObject *Object_getPose( BPy_Object * self );
static PyObject *Object_evaluatePose( BPy_Object * self, PyObject *args );
static PyObject *Object_getSelected( BPy_Object * self );
static PyObject *Object_makeDisplayList( BPy_Object * self );
static PyObject *Object_link( BPy_Object * self, PyObject * args );
static PyObject *Object_makeParent( BPy_Object * self, PyObject * args );
static PyObject *Object_join( BPy_Object * self, PyObject * args );
static PyObject *Object_makeParentDeform( BPy_Object * self, PyObject * args );
static PyObject *Object_makeParentVertex( BPy_Object * self, PyObject * args );
static PyObject *Object_makeParentBone( BPy_Object * self, PyObject * args );
static PyObject *Object_materialUsage( void );
static PyObject *Object_getDupliObjects ( BPy_Object * self);
static PyObject *Object_getEffects( BPy_Object * self );
static PyObject *Object_setDeltaLocation( BPy_Object * self, PyObject * args );
static PyObject *Object_SetDrawMode( BPy_Object * self, PyObject * args );
static PyObject *Object_SetDrawType( BPy_Object * self, PyObject * args );
static PyObject *Object_SetEuler( BPy_Object * self, PyObject * args );
static PyObject *Object_SetMatrix( BPy_Object * self, PyObject * args );
static PyObject *Object_SetIpo( BPy_Object * self, PyObject * args );
static PyObject *Object_insertIpoKey( BPy_Object * self, PyObject * args );
static PyObject *Object_insertPoseKey( BPy_Object * self, PyObject * args );
static PyObject *Object_insertCurrentPoseKey( BPy_Object * self, PyObject * args );
static PyObject *Object_setConstraintInfluenceForBone( BPy_Object * self, PyObject * args );
static PyObject *Object_setLocation( BPy_Object * self, PyObject * args );
static PyObject *Object_setMaterials( BPy_Object * self, PyObject * args );
static PyObject *Object_setSize( BPy_Object * self, PyObject * args );
static PyObject *Object_setTimeOffset( BPy_Object * self, PyObject * args );
static PyObject *Object_makeTrack( BPy_Object * self, PyObject * args );
static PyObject *Object_shareFrom( BPy_Object * self, PyObject * args );
static PyObject *Object_Select( BPy_Object * self, PyObject * args );
static PyObject *Object_getAllProperties( BPy_Object * self );
static PyObject *Object_addProperty( BPy_Object * self, PyObject * args );
static PyObject *Object_removeProperty( BPy_Object * self, PyObject * args );
static PyObject *Object_getProperty( BPy_Object * self, PyObject * args );
static PyObject *Object_removeAllProperties( BPy_Object * self );
static PyObject *Object_copyAllPropertiesTo( BPy_Object * self,
					     PyObject * args );
static PyObject *Object_getScriptLinks( BPy_Object * self, PyObject * value );
static PyObject *Object_addScriptLink( BPy_Object * self, PyObject * args );
static PyObject *Object_clearScriptLinks( BPy_Object * self, PyObject *args );
static PyObject *Object_getPIStrength( BPy_Object * self );
static PyObject *Object_setPIStrength( BPy_Object * self, PyObject * args );
static PyObject *Object_getPIFalloff( BPy_Object * self );
static PyObject *Object_setPIFalloff( BPy_Object * self, PyObject * args );
static PyObject *Object_getPIMaxDist( BPy_Object * self );
static PyObject *Object_setPIMaxDist( BPy_Object * self, PyObject * args );
static PyObject *Object_getPIUseMaxDist( BPy_Object * self );
static PyObject *Object_SetPIUseMaxDist( BPy_Object * self, PyObject * args );
static PyObject *Object_getPIType( BPy_Object * self );
static PyObject *Object_SetPIType( BPy_Object * self, PyObject * args );
static PyObject *Object_getPIPerm( BPy_Object * self );
static PyObject *Object_SetPIPerm( BPy_Object * self, PyObject * args );
static PyObject *Object_getPIRandomDamp( BPy_Object * self );
static PyObject *Object_setPIRandomDamp( BPy_Object * self, PyObject * args );
static PyObject *Object_getPISurfaceDamp( BPy_Object * self );
static PyObject *Object_SetPISurfaceDamp( BPy_Object * self, PyObject * args );
static PyObject *Object_getPIDeflection( BPy_Object * self );
static PyObject *Object_SetPIDeflection( BPy_Object * self, PyObject * args );

static int Object_setRBMass( BPy_Object * self, PyObject * args );
static int Object_setRBFlags( BPy_Object * self, PyObject * args );
static int Object_setRBShapeBoundType( BPy_Object * self, PyObject * args );

static PyObject *Object_getSBMass( BPy_Object * self );
static PyObject *Object_setSBMass( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBGravity( BPy_Object * self );
static PyObject *Object_setSBGravity( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBFriction( BPy_Object * self );
static PyObject *Object_setSBFriction( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBErrorLimit( BPy_Object * self );
static PyObject *Object_setSBErrorLimit( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBGoalSpring( BPy_Object * self );
static PyObject *Object_setSBGoalSpring( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBGoalFriction( BPy_Object * self );
static PyObject *Object_setSBGoalFriction( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBMinGoal( BPy_Object * self );
static PyObject *Object_setSBMinGoal( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBMaxGoal( BPy_Object * self );
static PyObject *Object_setSBMaxGoal( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBInnerSpring( BPy_Object * self );
static PyObject *Object_setSBInnerSpring( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBInnerSpringFriction( BPy_Object * self );
static PyObject *Object_setSBInnerSpringFriction( BPy_Object * self, PyObject * args );

static PyObject *Object_isSB( BPy_Object * self );
static PyObject *Object_getSBDefaultGoal( BPy_Object * self );
static PyObject *Object_setSBDefaultGoal( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBUseGoal( BPy_Object * self );
static PyObject *Object_SetSBUseGoal( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBUseEdges( BPy_Object * self );
static PyObject *Object_SetSBUseEdges( BPy_Object * self, PyObject * args );
static PyObject *Object_getSBStiffQuads( BPy_Object * self );
static PyObject *Object_SetSBStiffQuads( BPy_Object * self, PyObject * args );
static PyObject *Object_insertShapeKey(BPy_Object * self);
static PyObject *Object_copyNLA( BPy_Object * self, PyObject * args );
static PyObject *Object_convertActionToStrip( BPy_Object * self );
static PyObject *Object_copy(BPy_Object * self); /* __copy__ */
static PyObject *Object_trackAxis(BPy_Object * self);
static PyObject *Object_upAxis(BPy_Object * self);

/*****************************************************************************/
/* Python BPy_Object methods table:					   */
/*****************************************************************************/
static PyMethodDef BPy_Object_methods[] = {
	/* name, method, flags, doc */
	{"getParticleSystems", ( PyCFunction ) Object_getParticleSys, METH_NOARGS,
	 "Return a list of particle systems"},
 	{"newParticleSystem", ( PyCFunction ) Object_newParticleSys, METH_NOARGS,
	 "Create and link a new particle system"},
	{"addVertexGroupsFromArmature" , ( PyCFunction ) Object_addVertexGroupsFromArmature, METH_VARARGS,
	 "Add vertex groups from armature using the bone heat method"},
	{"buildParts", ( PyCFunction ) Object_buildParts, METH_NOARGS,
	 "Recalcs particle system (if any), (depricated, will always return an empty list in version 2.46)"},
	{"getIpo", ( PyCFunction ) Object_getIpo, METH_NOARGS,
	 "Returns the ipo of this object (if any) "},
	{"clrParent", ( PyCFunction ) Object_clrParent, METH_VARARGS,
	 "Clears parent object. Optionally specify:\n\
mode\n\tnonzero: Keep object transform\nfast\n\t>0: Don't update scene \
hierarchy (faster)"},
	{"clearTrack", ( PyCFunction ) Object_clearTrack, METH_VARARGS,
	 "Make this object not track another anymore. Optionally specify:\n\
mode\n\t2: Keep object transform\nfast\n\t>0: Don't update scene \
hierarchy (faster)"},
	{"getData", ( PyCFunction ) Object_getData, METH_VARARGS | METH_KEYWORDS,
	 "(name_only = 0, mesh = 0) - Returns the datablock object containing the object's \
data, e.g. Mesh.\n\
If 'name_only' is nonzero or True, only the name of the datablock is returned"},
	{"getDeltaLocation", ( PyCFunction ) Object_getDeltaLocation,
	 METH_NOARGS,
	 "Returns the object's delta location (x, y, z)"},
	{"getDrawMode", ( PyCFunction ) Object_getDrawMode, METH_NOARGS,
	 "Returns the object draw modes"},
	{"getDrawType", ( PyCFunction ) Object_getDrawType, METH_NOARGS,
	 "Returns the object draw type"},
	{"getAction", ( PyCFunction ) Object_getAction, METH_NOARGS,
	 "Returns the active action for this object"},
	{"evaluatePose", ( PyCFunction ) Object_evaluatePose, METH_VARARGS,
	"(framenum) - Updates the pose to a certain frame number when the Object is\
	bound to an Action"},
	{"getPose", ( PyCFunction ) Object_getPose, METH_NOARGS,
	"() - returns the pose from an object if it exists, else None"},
	{"isSelected", ( PyCFunction ) Object_getSelected, METH_NOARGS,
	 "Return a 1 or 0 depending on whether the object is selected"},
	{"getEuler", ( PyCFunction ) Object_GetEuler, METH_VARARGS,
	 "(space = 'localspace' / 'worldspace') - Returns the object's rotation as Euler rotation vector\n\
(rotX, rotY, rotZ)"},
	{"getInverseMatrix", ( PyCFunction ) Object_getInverseMatrix,
	 METH_NOARGS,
	 "Returns the object's inverse matrix"},
	{"getLocation", ( PyCFunction ) Object_getLocation, METH_VARARGS,
	 "(space = 'localspace' / 'worldspace') - Returns the object's location (x, y, z)\n\
"},
	{"getMaterials", ( PyCFunction ) Object_getMaterials, METH_VARARGS,
	 "(i = 0) - Returns list of materials assigned to the object.\n\
if i is nonzero, empty slots are not ignored: they are returned as None's."},
	{"getMatrix", ( PyCFunction ) Object_getMatrix, METH_VARARGS,
	 "(str = 'worldspace') - Returns the object matrix.\n\
(str = 'worldspace') - the desired matrix: worldspace (default), localspace\n\
or old_worldspace.\n\
\n\
'old_worldspace' was the only behavior before Blender 2.34.  With it the\n\
matrix is not updated for changes made by the script itself\n\
(like obj.LocX = 10) until a redraw happens, either called by the script or\n\
automatic when the script finishes."},
	{"getName", ( PyCFunction ) GenericLib_getName, METH_NOARGS,
	 "Returns the name of the object"},
	{"getParent", ( PyCFunction ) Object_getParent, METH_NOARGS,
	 "Returns the object's parent object"},
	{"getParentBoneName", ( PyCFunction ) Object_getParentBoneName, METH_NOARGS,
	 "Returns None, or the 'sub-name' of the parent (eg. Bone name)"},
	{"getSize", ( PyCFunction ) Object_getSize, METH_VARARGS,
	 "(space = 'localspace' / 'worldspace') - Returns the object's size (x, y, z)"},
	{"getTimeOffset", ( PyCFunction ) Object_getTimeOffset, METH_NOARGS,
	 "Returns the object's time offset"},
	{"getTracked", ( PyCFunction ) Object_getTracked, METH_NOARGS,
	 "Returns the object's tracked object"},
	{"getType", ( PyCFunction ) Object_getType, METH_NOARGS,
	 "Returns type of string of Object"},
/* Particle Interaction */
	 
	{"getPIStrength", ( PyCFunction ) Object_getPIStrength, METH_NOARGS,
	 "Returns Particle Interaction Strength"},
	{"setPIStrength", ( PyCFunction ) Object_setPIStrength, METH_VARARGS,
	 "Sets Particle Interaction Strength"},
	{"getPIFalloff", ( PyCFunction ) Object_getPIFalloff, METH_NOARGS,
	 "Returns Particle Interaction Falloff"},
	{"setPIFalloff", ( PyCFunction ) Object_setPIFalloff, METH_VARARGS,
	 "Sets Particle Interaction Falloff"},
	{"getPIMaxDist", ( PyCFunction ) Object_getPIMaxDist, METH_NOARGS,
	 "Returns Particle Interaction Max Distance"},
	{"setPIMaxDist", ( PyCFunction ) Object_setPIMaxDist, METH_VARARGS,
	 "Sets Particle Interaction Max Distance"},
	{"getPIUseMaxDist", ( PyCFunction ) Object_getPIUseMaxDist, METH_NOARGS,
	 "Returns bool for Use Max Distace in Particle Interaction "},
	{"setPIUseMaxDist", ( PyCFunction ) Object_SetPIUseMaxDist, METH_VARARGS,
	 "Sets if Max Distance should be used in Particle Interaction"},
	{"getPIType", ( PyCFunction ) Object_getPIType, METH_NOARGS,
	 "Returns Particle Interaction Type"},
	{"setPIType", ( PyCFunction ) Object_SetPIType, METH_VARARGS,
	 "sets Particle Interaction Type"},
	{"getPIPerm", ( PyCFunction ) Object_getPIPerm, METH_NOARGS,
	 "Returns Particle Interaction Permiability"},
	{"setPIPerm", ( PyCFunction ) Object_SetPIPerm, METH_VARARGS,
	 "Sets Particle Interaction Permiability"},
	{"getPISurfaceDamp", ( PyCFunction ) Object_getPISurfaceDamp, METH_NOARGS,
	 "Returns Particle Interaction Surface Damping"},
	{"setPISurfaceDamp", ( PyCFunction ) Object_SetPISurfaceDamp, METH_VARARGS,
	 "Sets Particle Interaction Surface Damping"},
	{"getPIRandomDamp", ( PyCFunction ) Object_getPIRandomDamp, METH_NOARGS,
	 "Returns Particle Interaction Random Damping"},
	{"setPIRandomDamp", ( PyCFunction ) Object_setPIRandomDamp, METH_VARARGS,
	 "Sets Particle Interaction Random Damping"},
	{"getPIDeflection", ( PyCFunction ) Object_getPIDeflection, METH_NOARGS,
	 "Returns Particle Interaction Deflection"},
	{"setPIDeflection", ( PyCFunction ) Object_SetPIDeflection, METH_VARARGS,
	 "Sets Particle Interaction Deflection"},  

/* Softbody */

	{"isSB", ( PyCFunction ) Object_isSB, METH_NOARGS,
	 "True if object is a soft body"},
	{"getSBMass", ( PyCFunction ) Object_getSBMass, METH_NOARGS,
	 "Returns SB Mass"},
	{"setSBMass", ( PyCFunction ) Object_setSBMass, METH_VARARGS,
	 "Sets SB Mass"}, 
	{"getSBGravity", ( PyCFunction ) Object_getSBGravity, METH_NOARGS,
	 "Returns SB Gravity"},
	{"setSBGravity", ( PyCFunction ) Object_setSBGravity, METH_VARARGS,
	 "Sets SB Gravity"}, 
	{"getSBFriction", ( PyCFunction ) Object_getSBFriction, METH_NOARGS,
	 "Returns SB Friction"},
	{"setSBFriction", ( PyCFunction ) Object_setSBFriction, METH_VARARGS,
	 "Sets SB Friction"}, 
	{"getSBErrorLimit", ( PyCFunction ) Object_getSBErrorLimit, METH_NOARGS,
	 "Returns SB ErrorLimit"},
	{"setSBErrorLimit", ( PyCFunction ) Object_setSBErrorLimit, METH_VARARGS,
	 "Sets SB ErrorLimit"}, 
	{"getSBGoalSpring", ( PyCFunction ) Object_getSBGoalSpring, METH_NOARGS,
	 "Returns SB GoalSpring"},
	{"setSBGoalSpring", ( PyCFunction ) Object_setSBGoalSpring, METH_VARARGS,
	 "Sets SB GoalSpring"}, 
	{"getSBGoalFriction", ( PyCFunction ) Object_getSBGoalFriction, METH_NOARGS,
	 "Returns SB GoalFriction"},
	{"setSBGoalFriction", ( PyCFunction ) Object_setSBGoalFriction, METH_VARARGS,
	 "Sets SB GoalFriction"}, 
	{"getSBMinGoal", ( PyCFunction ) Object_getSBMinGoal, METH_NOARGS,
	 "Returns SB MinGoal"},
	{"setSBMinGoal", ( PyCFunction ) Object_setSBMinGoal, METH_VARARGS,
	 "Sets SB MinGoal "}, 
	{"getSBMaxGoal", ( PyCFunction ) Object_getSBMaxGoal, METH_NOARGS,
	 "Returns SB MaxGoal"},
	{"setSBMaxGoal", ( PyCFunction ) Object_setSBMaxGoal, METH_VARARGS,
	 "Sets SB MaxGoal"},  
	{"getSBInnerSpring", ( PyCFunction ) Object_getSBInnerSpring, METH_NOARGS,
	 "Returns SB InnerSpring"},
	{"setSBInnerSpring", ( PyCFunction ) Object_setSBInnerSpring, METH_VARARGS,
	 "Sets SB InnerSpring"}, 	 
	{"getSBInnerSpringFriction", ( PyCFunction ) Object_getSBInnerSpringFriction, METH_NOARGS,
	 "Returns SB InnerSpringFriction"},
	{"setSBInnerSpringFriction", ( PyCFunction ) Object_setSBInnerSpringFriction, METH_VARARGS,
	 "Sets SB InnerSpringFriction"}, 	
	{"getSBDefaultGoal", ( PyCFunction ) Object_getSBDefaultGoal, METH_NOARGS,
	 "Returns SB DefaultGoal"},
	{"setSBDefaultGoal", ( PyCFunction ) Object_setSBDefaultGoal, METH_VARARGS,
	 "Sets SB DefaultGoal"}, 		 
	{"getSBUseGoal", ( PyCFunction ) Object_getSBUseGoal, METH_NOARGS,
	 "Returns SB UseGoal"},
	{"setSBUseGoal", ( PyCFunction ) Object_SetSBUseGoal, METH_VARARGS,
	 "Sets SB UseGoal"}, 
	{"getSBUseEdges", ( PyCFunction ) Object_getSBUseEdges, METH_NOARGS,
	 "Returns SB UseEdges"},
	{"setSBUseEdges", ( PyCFunction ) Object_SetSBUseEdges, METH_VARARGS,
	 "Sets SB UseEdges"}, 
	{"getSBStiffQuads", ( PyCFunction ) Object_getSBStiffQuads, METH_NOARGS,
	 "Returns SB StiffQuads"},
	{"setSBStiffQuads", ( PyCFunction ) Object_SetSBStiffQuads, METH_VARARGS,
	 "Sets SB StiffQuads"},
	{"getBoundBox", ( PyCFunction ) Object_getBoundBox, METH_VARARGS,
	 "Returns the object's bounding box"},
	{"makeDisplayList", ( PyCFunction ) Object_makeDisplayList, METH_NOARGS,
	 "Update this object's Display List. Some changes like turning\n\
'SubSurf' on for a mesh need this method (followed by a Redraw) to\n\
show the changes on the 3d window."},
	{"link", ( PyCFunction ) Object_link, METH_VARARGS,
	 "Links Object with data provided in the argument. The data must\n\
match the Object's type, so you cannot link a Lamp to a Mesh type object."},
	{"makeParent", ( PyCFunction ) Object_makeParent, METH_VARARGS,
	 "Makes the object the parent of the objects provided in the\n\
argument which must be a list of valid Objects. Optional extra arguments:\n\
mode:\n\t0: make parent with inverse\n\t1: without inverse\n\
fast:\n\t0: update scene hierarchy automatically\n\t\
don't update scene hierarchy (faster). In this case, you must\n\t\
explicitly update the Scene hierarchy."},
	{"join", ( PyCFunction ) Object_join, METH_VARARGS,
	 "(object_list) - Joins the objects in object list of the same type, into this object."},
	{"makeParentDeform", ( PyCFunction ) Object_makeParentDeform, METH_VARARGS,
	 "Makes the object the deformation parent of the objects provided in the \n\
argument which must be a list of valid Objects. Optional extra arguments:\n\
mode:\n\t0: make parent with inverse\n\t1: without inverse\n\
fast:\n\t0: update scene hierarchy automatically\n\t\
don't update scene hierarchy (faster). In this case, you must\n\t\
explicitly update the Scene hierarchy."},
	{"makeParentVertex", ( PyCFunction ) Object_makeParentVertex, METH_VARARGS,
	 "Makes the object the vertex parent of the objects provided in the \n\
argument which must be a list of valid Objects. \n\
The second argument is a tuple of 1 or 3 positive integers which corresponds \
to the index of the vertex you are parenting to.\n\
Optional extra arguments:\n\
mode:\n\t0: make parent with inverse\n\t1: without inverse\n\
fast:\n\t0: update scene hierarchy automatically\n\t\
don't update scene hierarchy (faster). In this case, you must\n\t\
explicitly update the Scene hierarchy."},
	{"makeParentBone", ( PyCFunction ) Object_makeParentBone, METH_VARARGS,
	 "Makes this armature objects bone, the parent of the objects provided in the \n\
argument which must be a list of valid Objects. Optional extra arguments:\n\
mode:\n\t0: make parent with inverse\n\t1: without inverse\n\
fast:\n\t0: update scene hierarchy automatically\n\t\
don't update scene hierarchy (faster). In this case, you must\n\t\
explicitely update the Scene hierarchy."},

	{"materialUsage", ( PyCFunction ) Object_materialUsage, METH_NOARGS,
	 "Determines the way the material is used and returns status.\n\
Possible arguments (provide as strings):\n\
\tData:   Materials assigned to the object's data are shown. (default)\n\
\tObject: Materials assigned to the object are shown."},
	{"setDeltaLocation", ( PyCFunction ) Object_setDeltaLocation,
	 METH_VARARGS,
	 "Sets the object's delta location which must be a vector triple."},
	{"setDrawMode", ( PyCFunction ) Object_SetDrawMode, METH_VARARGS,
	 "Sets the object's drawing mode. The argument can be a sum of:\n\
2: axis\n4: texspace\n8: drawname\n16: drawimage\n32: drawwire\n64: drawxray\n128: drawtransp"},
	{"setDrawType", ( PyCFunction ) Object_SetDrawType, METH_VARARGS,
	 "Sets the object's drawing type. The argument must be one of:\n\
1: Bounding box\n2: Wire\n3: Solid\n4: Shaded\n5: Textured"},
	{"setEuler", ( PyCFunction ) Object_SetEuler, METH_VARARGS,
	 "Set the object's rotation according to the specified Euler\n\
angles. The argument must be a vector triple"},
	{"setMatrix", ( PyCFunction ) Object_SetMatrix, METH_VARARGS,
	 "Set and apply a new local matrix for the object"},
	{"setLocation", ( PyCFunction ) Object_setLocation, METH_VARARGS,
	 "Set the object's location. The first argument must be a vector\n\
triple."},
	{"setMaterials", ( PyCFunction ) Object_setMaterials, METH_VARARGS,
	 "Sets materials. The argument must be a list of valid material\n\
objects."},
	{"setName", ( PyCFunction ) GenericLib_setName_with_method, METH_VARARGS,
	 "Sets the name of the object"},
	{"setSize", ( PyCFunction ) Object_setSize, METH_VARARGS,
	 "Set the object's size. The first argument must be a vector\n\
triple."},
	{"setTimeOffset", ( PyCFunction ) Object_setTimeOffset, METH_VARARGS,
	 "Set the object's time offset."},
	{"makeTrack", ( PyCFunction ) Object_makeTrack, METH_VARARGS,
	 "(trackedobj, fast = 0) - Make this object track another.\n\
	 (trackedobj) - the object that will be tracked.\n\
	 (fast = 0) - if 0: update the scene hierarchy automatically.  If you\n\
	 set 'fast' to a nonzero value, don't forget to update the scene yourself\n\
	 (see scene.update())."},
	{"shareFrom", ( PyCFunction ) Object_shareFrom, METH_VARARGS,
	 "Link data of self with object specified in the argument. This\n\
works only if self and the object specified are of the same type."},
	{"select", ( PyCFunction ) Object_Select, METH_VARARGS,
	 "( 1 or 0 )  - Set the selected state of the object.\n\
   1 is selected, 0 not selected "},
	{"setIpo", ( PyCFunction ) Object_SetIpo, METH_VARARGS,
	 "(Blender Ipo) - Sets the object's ipo"},
	{"clearIpo", ( PyCFunction ) Object_clearIpo, METH_NOARGS,
	 "() - Unlink ipo from this object"},

	 {"insertIpoKey", ( PyCFunction ) Object_insertIpoKey, METH_VARARGS,
	 "( Object IPO type ) - Inserts a key into IPO"},
	 {"insertPoseKey", ( PyCFunction ) Object_insertPoseKey, METH_VARARGS,
	 "( Object Pose type ) - Inserts a key into Action"},
	 {"insertCurrentPoseKey", ( PyCFunction ) Object_insertCurrentPoseKey, METH_VARARGS,
	 "( Object Pose type ) - Inserts a key into Action based on current pose"},
	 {"setConstraintInfluenceForBone", ( PyCFunction ) Object_setConstraintInfluenceForBone, METH_VARARGS,
	  "(  ) - sets a constraint influence for a certain bone in this (armature)object."},
	 {"copyNLA", ( PyCFunction ) Object_copyNLA, METH_VARARGS,
	  "(  ) - copies all NLA strips from another object to this object."},
	{"convertActionToStrip", ( PyCFunction ) Object_convertActionToStrip, METH_NOARGS,
	 "(  ) - copies all NLA strips from another object to this object."},
	{"getAllProperties", ( PyCFunction ) Object_getAllProperties, METH_NOARGS,
	 "() - Get all the properties from this object"},
	{"addProperty", ( PyCFunction ) Object_addProperty, METH_VARARGS,
	 "() - Add a property to this object"},
	{"removeProperty", ( PyCFunction ) Object_removeProperty, METH_VARARGS,
	 "() - Remove a property from  this object"},
	{"getProperty", ( PyCFunction ) Object_getProperty, METH_VARARGS,
	 "() - Get a property from this object by name"},
	{"removeAllProperties", ( PyCFunction ) Object_removeAllProperties,
	 METH_NOARGS,
	 "() - removeAll a properties from this object"},
	{"copyAllPropertiesTo", ( PyCFunction ) Object_copyAllPropertiesTo,
	 METH_VARARGS,
	 "() - copy all properties from this object to another object"},
	{"getScriptLinks", ( PyCFunction ) Object_getScriptLinks, METH_O,
	 "(eventname) - Get a list of this object's scriptlinks (Text names) "
	 "of the given type\n"
	 "(eventname) - string: FrameChanged, Redraw or Render."},
	{"addScriptLink", ( PyCFunction ) Object_addScriptLink, METH_VARARGS,
	 "(text, evt) - Add a new object scriptlink.\n"
	 "(text) - string: an existing Blender Text name;\n"
	 "(evt) string: FrameChanged, Redraw or Render."},
	{"clearScriptLinks", ( PyCFunction ) Object_clearScriptLinks,
	 METH_VARARGS,
	 "() - Delete all scriptlinks from this object.\n"
	 "([s1<,s2,s3...>]) - Delete specified scriptlinks from this object."},
	{"insertShapeKey", ( PyCFunction ) Object_insertShapeKey, METH_NOARGS,
	 "() - Insert a Shape Key in the current object"},
	{"__copy__", ( PyCFunction ) Object_copy, METH_NOARGS,
	 "() - Return a copy of this object."},
	{"copy", ( PyCFunction ) Object_copy, METH_NOARGS,
	 "() - Return a copy of this object."},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* PythonTypeObject callback function prototypes			 */
/*****************************************************************************/
static void Object_dealloc( BPy_Object * obj );
static PyObject *Object_repr( BPy_Object * obj );
static int Object_compare( BPy_Object * a, BPy_Object * b );

/*****************************************************************************/
/* Function:			  M_Object_New				 */
/* Python equivalent:	  Blender.Object.New				 */
/*****************************************************************************/

/*
 * Note: if this method is called without later linking object data to it, 
 * errors can be caused elsewhere in Blender.  Future versions of the API
 * will designate obdata as a parameter to this method to prevent this, and
 * eventually this method will be deprecated.
 *
 * When we can guarantee that objects will always have valid obdata, 
 * unlink_object() should be edited to remove checks for NULL pointers and
 * debugging messages.
 */

PyObject *M_Object_New( PyObject * self_unused, PyObject * args )
{
	struct Object *object;
	int type;
	char *str_type;
	char *name = NULL;
	PyObject *py_object;
	BPy_Object *blen_object;

	if( !PyArg_ParseTuple( args, "s|s", &str_type, &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"string expected as argument" );

	if( strcmp( str_type, "Armature" ) == 0 )
		type = OB_ARMATURE;
	else if( strcmp( str_type, "Camera" ) == 0 )
		type = OB_CAMERA;
	else if( strcmp( str_type, "Curve" ) == 0 )
		type = OB_CURVE;
	else if (strcmp (str_type, "Text") == 0)	
		type = OB_FONT;
	else if( strcmp( str_type, "Lamp" ) == 0 )
		type = OB_LAMP;
	else if( strcmp( str_type, "Lattice" ) == 0 )
		type = OB_LATTICE;
	else if( strcmp( str_type, "Mball" ) == 0 )
		type = OB_MBALL;
	else if( strcmp( str_type, "Mesh" ) == 0 )
		type = OB_MESH;
	else if( strcmp( str_type, "Surf" ) == 0 )
		type = OB_SURF;
/*	else if (strcmp (str_type, "Wave") == 0)	type = OB_WAVE; */
	else if( strcmp( str_type, "Empty" ) == 0 )
		type = OB_EMPTY;
	else
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"Unknown type specified" );

	/* Create a new object. */
	if( name == NULL ) {
	/* No name is specified, set the name to the type of the object. */
		name = str_type;
	}
	object = add_only_object(type, name);

	object->flag = 0;
	object->lay = 1;	/* Layer, by default visible*/
	object->data = NULL;

	/* user count is incremented in Object_CreatePyObject */
	object->id.us = 0;

	/* Create a Python object from it. */
	py_object = Object_CreatePyObject( object );
	blen_object = (BPy_Object *)py_object;

	/* store the real object type in the PyObject, treat this as an Empty
	 * until it has some obdata */
	blen_object->realtype = object->type;
	object->type = OB_EMPTY;

	return py_object;
}

/*****************************************************************************/
/* Function:	  M_Object_Get						*/
/* Python equivalent:	  Blender.Object.Get				*/
/*****************************************************************************/
PyObject *M_Object_Get( PyObject * self_unused, PyObject * args )
{
	struct Object *object;
	PyObject *blen_object;
	char *name = NULL;

	PyArg_ParseTuple( args, "|s", &name );

	if( name != NULL ) {
		object = ( Object * ) GetIdFromList( &( G.main->object ), name );

			/* No object exists with the name specified in the argument name. */
		if( !object ){
			char buffer[128];
			PyOS_snprintf( buffer, sizeof(buffer),
						   "object \"%s\" not found", name);
			return EXPP_ReturnPyObjError( PyExc_ValueError,
										  buffer );
		}

		/* objects used in pydriver expressions need this */
		if (bpy_during_pydriver())
			bpy_pydriver_appendToList(object);
 
		return Object_CreatePyObject( object );
	} else {
		/* No argument has been given. Return a list of all objects. */
		PyObject *obj_list;
		Link *link;
		int index;

		/* do not allow Get() (w/o arguments) inside pydriver, otherwise
		 * we'd have to update all objects in the DAG */
		if (bpy_during_pydriver())
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"Object.Get requires an argument when used in pydrivers" );

		obj_list = PyList_New( BLI_countlist( &( G.main->object ) ) );

		if( !obj_list )
			return EXPP_ReturnPyObjError( PyExc_SystemError,
				"List creation failed." );

		link = G.main->object.first;
		index = 0;
		while( link ) {
			object = ( Object * ) link;
			blen_object = Object_CreatePyObject( object );
			if( !blen_object ) {
				Py_DECREF( obj_list );
				Py_RETURN_NONE;
			}
			PyList_SetItem( obj_list, index, blen_object );
			index++;
			link = link->next;
		}
		return obj_list;
	}
}

/*****************************************************************************/
/* Function:	  M_Object_GetSelected				*/
/* Python equivalent:	  Blender.Object.GetSelected		*/
/*****************************************************************************/
static PyObject *M_Object_GetSelected( PyObject * self_unused )
{
	PyObject *blen_object;
	PyObject *list;
	Base *base_iter;

	list = PyList_New( 0 );

	if( G.vd == NULL ) {
		/* No 3d view has been initialized yet, simply return an empty list */
		return list;
	}
	
	if( ( G.scene->basact ) &&
	    ( ( G.scene->basact->flag & SELECT ) &&
	      ( G.scene->basact->lay & G.vd->lay ) ) ) {

		/* Active object is first in the list. */
		blen_object = Object_CreatePyObject( G.scene->basact->object );
		if( !blen_object ) {
			Py_DECREF( list );
			Py_RETURN_NONE;
		}
		PyList_Append( list, blen_object );
		Py_DECREF( blen_object );
	}

	base_iter = G.scene->base.first;
	while( base_iter ) {
		if( ( ( base_iter->flag & SELECT ) &&
				( base_iter->lay & G.vd->lay ) ) &&
				( base_iter != G.scene->basact ) ) {

			blen_object = Object_CreatePyObject( base_iter->object );
			if( blen_object ) {
				PyList_Append( list, blen_object );
				Py_DECREF( blen_object );
			}
		}
		base_iter = base_iter->next;
	}
	return list;
}


/*****************************************************************************/
/* Function:			  M_Object_Duplicate				 */
/* Python equivalent:	  Blender.Object.Duplicate				 */
/*****************************************************************************/
static PyObject *M_Object_Duplicate( PyObject * self_unused,
		PyObject * args, PyObject *kwd )
{
	int dupflag= 0; /* this a flag, passed to adduplicate() and used instead of U.dupflag sp python can set what is duplicated */	

	/* the following variables are bools, if set true they will modify the dupflag to pass to adduplicate() */
	int mesh_dupe = 0;
	int surface_dupe = 0;
	int curve_dupe = 0;
	int text_dupe = 0;
	int metaball_dupe = 0;
	int armature_dupe = 0;
	int lamp_dupe = 0;
	int material_dupe = 0;
	int texture_dupe = 0;
	int ipo_dupe = 0;
	
	static char *kwlist[] = {"mesh", "surface", "curve",
			"text", "metaball", "armature", "lamp", "material", "texture", "ipo", NULL};
	
	/* duplicating in background causes segfaults */
	if( G.background == 1 )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"cannot duplicate objects in background mode" );
	
	
	if (!PyArg_ParseTupleAndKeywords(args, kwd, "|iiiiiiiiii", kwlist,
		&mesh_dupe, &surface_dupe, &curve_dupe, &text_dupe, &metaball_dupe,
		&armature_dupe, &lamp_dupe, &material_dupe, &texture_dupe, &ipo_dupe))
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected nothing or bool keywords 'mesh', 'surface', 'curve', 'text', 'metaball', 'armature', 'lamp' 'material', 'texture' and 'ipo' as arguments" );
	
	/* USER_DUP_ACT for actions is not supported in the UI so dont support it here */
	if (mesh_dupe)		dupflag |= USER_DUP_MESH;
	if (surface_dupe)	dupflag |= USER_DUP_SURF;
	if (curve_dupe)		dupflag |= USER_DUP_CURVE;
	if (text_dupe)		dupflag |= USER_DUP_FONT;
	if (metaball_dupe)	dupflag |= USER_DUP_MBALL;
	if (armature_dupe)	dupflag |= USER_DUP_ARM;
	if (lamp_dupe)		dupflag |= USER_DUP_LAMP;
	if (material_dupe)	dupflag |= USER_DUP_MAT;
	if (texture_dupe)	dupflag |= USER_DUP_TEX;
	if (ipo_dupe)		dupflag |= USER_DUP_IPO;
	adduplicate(2, dupflag); /* 2 is a mode with no transform and no redraw, Duplicate the current selection, context sensitive */
	Py_RETURN_NONE;
}


/*****************************************************************************/
/* Python BPy_Object methods:					*/
/*****************************************************************************/

PyObject *Object_getParticleSys( BPy_Object * self ){
	ParticleSystem *blparticlesys = 0;
	Object *ob = self->object;
	PyObject *partsyslist,*current;

	blparticlesys = ob->particlesystem.first;

	partsyslist = PyList_New( 0 );

	if (!blparticlesys)
		return partsyslist;

/* fixme:  for(;;) */
	current = ParticleSys_CreatePyObject( blparticlesys, ob );
	PyList_Append(partsyslist,current);
	Py_DECREF(current);

	while((blparticlesys = blparticlesys->next)){
		current = ParticleSys_CreatePyObject( blparticlesys, ob );
		PyList_Append(partsyslist,current);
		Py_DECREF(current);
	}

	return partsyslist;
}

PyObject *Object_newParticleSys( BPy_Object * self ){
	ParticleSystem *psys = 0;
	ParticleSystem *rpsys = 0;
	ModifierData *md;
	ParticleSystemModifierData *psmd;
	Object *ob = self->object;
/*	char *name = NULL;  optional name param */
	ID *id;
	int nr;

	id = (ID *)psys_new_settings("PSys", G.main);

	psys = MEM_callocN(sizeof(ParticleSystem), "particle_system");
	psys->pointcache = BKE_ptcache_add();
	psys->flag |= PSYS_ENABLED;
	BLI_addtail(&ob->particlesystem,psys);

	md = modifier_new(eModifierType_ParticleSystem);
	sprintf(md->name, "ParticleSystem %i", BLI_countlist(&ob->particlesystem));
	psmd = (ParticleSystemModifierData*) md;
	psmd->psys=psys;
	BLI_addtail(&ob->modifiers, md);

	psys->part=(ParticleSettings*)id;
	psys->totpart=0;
	psys->flag=PSYS_ENABLED|PSYS_CURRENT;
	psys->cfra=bsystem_time(ob,(float)G.scene->r.cfra+1,0.0);
	rpsys = psys;

	/* check need for dupliobjects */

	nr=0;
	for(psys=ob->particlesystem.first; psys; psys=psys->next){
		if(ELEM(psys->part->draw_as,PART_DRAW_OB,PART_DRAW_GR))
			nr++;
	}
	if(nr)
		ob->transflag |= OB_DUPLIPARTS;
	else
		ob->transflag &= ~OB_DUPLIPARTS;

	BIF_undo_push("Browse Particle System");

	DAG_scene_sort(G.scene);
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	return ParticleSys_CreatePyObject(rpsys,ob);
}

/*****************************************************************************/
/* attribute:           addVertexGroupsFromArmature                          */
/* Description:         evaluate and add vertex groups to the current object */
/*                      for each bone of the selected armature               */   
/* Data:                self Object, Bpy armature                            */
/* Return:              nothing                                              */
/*****************************************************************************/
static PyObject *Object_addVertexGroupsFromArmature( BPy_Object * self, PyObject * args)
{
	
	Object *ob = self->object;
	BPy_Object *arm;

	if( ob->type != OB_MESH )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"Only useable on Mesh type Objects" );
	
	if( G.obedit != NULL)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"Not useable when inside edit mode" );
	
	/* Check if the arguments passed to makeParent are valid. */
	if( !PyArg_ParseTuple( args, "O!",&Object_Type, &arm ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"An armature object is expected." );
	
	if( arm->object->type != OB_ARMATURE ) 
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"An armature object is expected." );
			
	add_verts_to_dgroups(ob, arm->object, 1, 0);
	ob->recalc |= OB_RECALC_OB;  
	
	Py_RETURN_NONE;
}

static PyObject *Object_buildParts( BPy_Object * self )
{
	/* This is now handles by modifiers */
	Py_RETURN_NONE;
}

static PyObject *Object_clearIpo( BPy_Object * self )
{
	Object *ob = self->object;
	Ipo *ipo = ( Ipo * ) ob->ipo;

	if( ipo ) {
		ID *id = &ipo->id;
		if( id->us > 0 )
			id->us--;
		ob->ipo = NULL;

		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE; /* no ipo found */
}

static PyObject *Object_clrParent( BPy_Object * self, PyObject * args )
{
	int mode = 0;
	int fast = 0;

	if( !PyArg_ParseTuple( args, "|ii", &mode, &fast ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected one or two optional integers as arguments" );

	/* Remove the link only, the object is still in the scene. */
	self->object->parent = NULL;

	if( mode == 2 ) {
		/* Keep transform */
		apply_obmat( self->object );
	}

	if( !fast )
		DAG_scene_sort( G.scene );

	Py_RETURN_NONE;
}

static PyObject *Object_clearTrack( BPy_Object * self, PyObject * args )
{
	int mode = 0;
	int fast = 0;

	if( !PyArg_ParseTuple( args, "|ii", &mode, &fast ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected one or two optional integers as arguments" );

	/* Remove the link only, the object is still in the scene. */
	self->object->track = NULL;

	if( mode ) {
		/* Keep transform */
		apply_obmat( self->object );
	}

	if( !fast )
		DAG_scene_sort( G.scene );

	Py_RETURN_NONE;
}

/* adds object data to a Blender object, if object->data = NULL */
int EXPP_add_obdata( struct Object *object )
{
	if( object->data != NULL )
		return -1;

	switch ( object->type ) {
	case OB_ARMATURE:
		/* TODO: Do we need to add something to G? (see the OB_LAMP case) */
		object->data = add_armature( "Armature" );
		break;
	case OB_CAMERA:
		/* TODO: Do we need to add something to G? (see the OB_LAMP case) */
		object->data = add_camera( "Camera" );
		break;
	case OB_CURVE:
		object->data = add_curve( "Curve", OB_CURVE );
		G.totcurve++;
		break;
	case OB_LAMP:
		object->data = add_lamp( "Lamp" );
		G.totlamp++;
		break;
	case OB_MESH:
		object->data = add_mesh( "Mesh" );
		G.totmesh++;
		break;
	case OB_LATTICE:
		object->data = ( void * ) add_lattice( "Lattice" );
		object->dt = OB_WIRE;
		break;
	case OB_MBALL:
		object->data = add_mball( "Meta" );
		break;

		/* TODO the following types will be supported later,
		   be sure to update Scene_link when new types are supported
		   case OB_SURF:
		   object->data = add_curve(OB_SURF);
		   G.totcurve++;
		   break;
		   case OB_FONT:
		   object->data = add_curve(OB_FONT);
		   break;
		   case OB_WAVE:
		   object->data = add_wave();
		   break;
		 */
	default:
		break;
	}

	if( !object->data )
		return -1;

	return 0;
}

static PyObject *Object_getDeltaLocation( BPy_Object * self )
{
	return Py_BuildValue( "fff", self->object->dloc[0],
			self->object->dloc[1], self->object->dloc[2] );
}

static PyObject *Object_getAction( BPy_Object * self )
{
	if( self->object->action )
		return Action_CreatePyObject( self->object->action );
	Py_RETURN_NONE;
}

static int Object_setAction( BPy_Object * self, PyObject * value )
{
	return GenericLib_assignData(value, (void **) &self->object->action, 0, 1, ID_AC, 0);
}

static PyObject *Object_evaluatePose(BPy_Object *self, PyObject *args)
{
	int frame = 1;
	if( !PyArg_ParseTuple( args, "i", &frame ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int argument" );

	frame = EXPP_ClampInt(frame, MINFRAME, MAXFRAME);
	G.scene->r.cfra = frame;
	do_all_pose_actions(self->object);
	where_is_pose (self->object);

	Py_RETURN_NONE;
}

static PyObject * Object_getPose(BPy_Object *self)
{
	/*if there is no pose will return PyNone*/
	return PyPose_FromPose(self->object->pose, self->object->id.name+2);
}

static PyObject *Object_getSelected( BPy_Object * self )
{
	Base *base;

	base = FIRSTBASE;
	while( base ) {
		if( base->object == self->object ) {
			if( base->flag & SELECT ) {
				Py_RETURN_TRUE;
			} else {
				Py_RETURN_FALSE;
			}
		}
		base = base->next;
	}
	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"could not find object's selection state" );
}

static int Object_setSelect( BPy_Object * self, PyObject * value )
{
	Base *base;
	int param = PyObject_IsTrue( value );

	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );

	base = FIRSTBASE;
	while( base ) {
		if( base->object == self->object ) {
			if( param ) {
				base->flag |= SELECT;
				self->object->flag = (short)base->flag;
				set_active_base( base );
			} else {
				base->flag &= ~SELECT;
				self->object->flag = (short)base->flag;
			}
			break;
		}
		base = base->next;
	}
	if (base) { /* was the object selected? */
		countall(  );
	}
	return 0;
}

static PyObject *Object_GetEuler( BPy_Object * self, PyObject * args )
{
	char *space = "localspace";	/* default to local */
	float eul[3];
	
	if( !PyArg_ParseTuple( args, "|s", &space ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a string or nothing" );
	
	if( BLI_streq( space, "worldspace" ) ) {	/* Worldspace matrix */
		float mat3[3][3];
		disable_where_script( 1 );
		where_is_object( self->object );
		Mat3CpyMat4(mat3, self->object->obmat);
		Mat3ToEul(mat3, eul);
		disable_where_script( 0 );
	} else if( BLI_streq( space, "localspace" ) ) {	/* Localspace matrix */
		eul[0] = self->object->rot[0];
		eul[1] = self->object->rot[1];
		eul[2] = self->object->rot[2];
	} else {
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected either nothing, 'localspace' (default) or 'worldspace'" );
	}

	return ( PyObject * ) newEulerObject( eul, Py_NEW );
}

static PyObject *Object_getInverseMatrix( BPy_Object * self )
{
	MatrixObject *inverse =
		( MatrixObject * ) newMatrixObject( NULL, 4, 4, Py_NEW );
	Mat4Invert( (float ( * )[4])*inverse->matrix, self->object->obmat );

	return ( ( PyObject * ) inverse );
}

static PyObject *Object_getIpo( BPy_Object * self )
{
	struct Ipo *ipo = self->object->ipo;

	if( ipo )
		return Ipo_CreatePyObject( ipo );
	Py_RETURN_NONE;
}

static PyObject *Object_getLocation( BPy_Object * self, PyObject * args )
{
	char *space = "localspace";	/* default to local */
	PyObject *attr;
	if( !PyArg_ParseTuple( args, "|s", &space ) ) 
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a string or nothing" );

	if( BLI_streq( space, "worldspace" ) ) {	/* Worldspace matrix */
		disable_where_script( 1 );
		where_is_object( self->object );
		
		attr = Py_BuildValue( "fff",
					self->object->obmat[3][0],
					self->object->obmat[3][1],
					self->object->obmat[3][2] );
		
		disable_where_script( 0 );
	} else if( BLI_streq( space, "localspace" ) ) {	/* Localspace matrix */
		attr = Py_BuildValue( "fff",
					self->object->loc[0],
					self->object->loc[1],
					self->object->loc[2] );
	} else {
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"expected either nothing, 'localspace' (default) or 'worldspace'" );
	}

	return attr;
}

static PyObject *Object_getMaterials( BPy_Object * self, PyObject * args )
{
	int all = 0;

	if( !PyArg_ParseTuple( args, "|i", &all ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an int or nothing" );

	return EXPP_PyList_fromMaterialList( self->object->mat,
					       self->object->totcol, all );
}

static PyObject *Object_getParent( BPy_Object * self )
{
	return Object_CreatePyObject( self->object->parent );
}

static PyObject *Object_getParentBoneName( BPy_Object * self )
{
	if( self->object->parent && self->object->parent->type==OB_ARMATURE && self->object->parsubstr[0] != '\0' )
		return PyString_FromString( self->object->parsubstr );
	Py_RETURN_NONE;
}

static int Object_setParentBoneName( BPy_Object * self, PyObject *value )
{
	char *bonename;
	
	if (!PyString_Check(value))
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int or nothing" );
	
	if (
		self->object->parent &&
		self->object->parent->type == OB_ARMATURE &&
		self->object->partype == PARBONE
	) {/* its all good */} else
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"can only set the parent bone name for objects that already have a bone parent" );
	
	bonename = PyString_AsString(value);

	if (!get_named_bone(self->object->parent->data, bonename))
		return EXPP_ReturnIntError( PyExc_ValueError,
				"cannot parent to this bone: invalid bone name" );
	
	strcpy(self->object->parsubstr, bonename);
	DAG_scene_sort( G.scene );
	return 0;
}

static PyObject *Object_getParentVertexIndex( BPy_Object * self )
{
	PyObject *pyls = NULL;
	
	if( self->object->parent) {
		if (self->object->partype==PARVERT1) {
			pyls = PyList_New(1);
			PyList_SET_ITEM( pyls, 0, PyInt_FromLong( self->object->par1 ));
			return pyls;
		} else if (self->object->partype==PARVERT3) {
			pyls = PyList_New(3);
			PyList_SET_ITEM( pyls, 0, PyInt_FromLong( self->object->par1 ));
			PyList_SET_ITEM( pyls, 1, PyInt_FromLong( self->object->par2 ));
			PyList_SET_ITEM( pyls, 2, PyInt_FromLong( self->object->par3 ));
			return pyls;
		}
	}
	return PyList_New(0);
}

static int Object_setParentVertexIndex( BPy_Object * self, PyObject *value )
{
	PyObject *item;
	int val[3] = {0,0,0};
	if( !self->object->parent) {
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			"This object has no vertex parent, cant set the vertex parent indicies" );
	}
	if (self->object->partype==PARVERT1) {
		if (PySequence_Length(value) != 1)
			return EXPP_ReturnIntError( PyExc_RuntimeError,
				"Vertex parented to 1 vertex, can only assign a sequence with 1 vertex parent index" );
		item = PySequence_GetItem(value, 0);
		if (item) {
			val[0] = PyInt_AsLong(item);
			Py_DECREF(item);
		}
	} else if (self->object->partype==PARVERT3) {
		int i;
		if (PySequence_Length(value) != 3)
			return EXPP_ReturnIntError( PyExc_RuntimeError,
				"Vertex parented to 3 verts, can only assign a sequence with 3 verts parent index" );
		
		for (i=0; i<3; i++) {
			item = PySequence_GetItem(value, i);
			if (item) {
				val[i] = PyInt_AsLong(item);
				Py_DECREF(item);
			}
		}
	} else {
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			"This object has no vertex parent, cant set the vertex parent indicies" );
	}
	
	if (PyErr_Occurred()) {
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			"This object has no vertex parent, cant set the vertex parent indicies" );
	} else {
		if (self->object->partype==PARVERT1) {
			if (val[0] < 0) {
				return EXPP_ReturnIntError( PyExc_RuntimeError,
					"vertex index less then zero" );
			}
			
			self->object->par1 = val[0];
		} else if (self->object->partype==PARVERT3) {
			if (val[0]==val[1] || val[0]==val[2] || val[1]==val[2]) {
				return EXPP_ReturnIntError( PyExc_RuntimeError,
					"duplicate indicies in vertex parent assignment" );
			}
			if (val[0] < 0 || val[1] < 0 || val[2] < 0) {
				return EXPP_ReturnIntError( PyExc_RuntimeError,
					"vertex index less then zero" );
			}
		
			self->object->par1 = val[0];
			self->object->par2 = val[1];
			self->object->par3 = val[2];
		}
	}

	return 0;
}


static PyObject *Object_getSize( BPy_Object * self, PyObject * args )
{
	char *space = "localspace";	/* default to local */
	PyObject *attr;
	if( !PyArg_ParseTuple( args, "|s", &space ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or nothing" );

	if( BLI_streq( space, "worldspace" ) ) {	/* Worldspace matrix */
		float rot[3];
		float mat[3][3], imat[3][3], tmat[3][3];
		disable_where_script( 1 );
		where_is_object( self->object );
		
		Mat3CpyMat4(mat, self->object->obmat);
		
		/* functionality copied from editobject.c apply_obmat */
		Mat3ToEul(mat, rot);
		EulToMat3(rot, tmat);
		Mat3Inv(imat, tmat);
		Mat3MulMat3(tmat, imat, mat);
		
		attr = Py_BuildValue( "fff",
					tmat[0][0],
					tmat[1][1],
					tmat[2][2] );
		disable_where_script( 0 );
	} else if( BLI_streq( space, "localspace" ) ) {	/* Localspace matrix */
		attr = Py_BuildValue( "fff",
					self->object->size[0],
					self->object->size[1],
					self->object->size[2] );
	} else {
		return EXPP_ReturnPyObjError( PyExc_ValueError,
			"expected either nothing, 'localspace' (default) or 'worldspace'" );
	}
	return attr;
}

static PyObject *Object_getTimeOffset( BPy_Object * self )
{
	return PyFloat_FromDouble ( (double) self->object->sf );
}

static PyObject *Object_getTracked( BPy_Object * self )
{
	return Object_CreatePyObject( self->object->track );
}

static PyObject *Object_getType( BPy_Object * self )
{
	char *str;
	int type = self->object->type;
	
	/* if object not yet linked to data, return the stored type */
	if( self->realtype != OB_EMPTY )
		type = self->realtype;
	
	switch ( type ) {
	case OB_ARMATURE:
		str = "Armature";
		break;
	case OB_CAMERA:
		str = "Camera";
		break;
	case OB_CURVE:
		str = "Curve";
		break;
	case OB_EMPTY:
		str = "Empty";
		break;
	case OB_FONT:
		str = "Text";
		break;
	case OB_LAMP:
		str = "Lamp";
		break;
	case OB_LATTICE:
		str = "Lattice";
		break;
	case OB_MBALL:
		str = "MBall";
		break;
	case OB_MESH:
		str = "Mesh";
		break;
	case OB_SURF:
		str = "Surf";
		break;
	case OB_WAVE:
		str = "Wave";
		break;
	default:
		str = "unknown";
		break;
	}

	return PyString_FromString( str );
}

static PyObject *Object_getBoundBox( BPy_Object * self, PyObject *args )
{
	float *vec = NULL;
	PyObject *vector, *bbox;
	int worldspace = 1;
	
	if( !PyArg_ParseTuple( args, "|i", &worldspace ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected an int or nothing" );

	if( !self->object->data )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"This object isn't linked to any object data (mesh, curve, etc) yet" );

	if( !self->object->bb ) {	/* if no ob bbox, we look in obdata */
		Mesh *me;
		Curve *curve;
		switch ( self->object->type ) {
		case OB_MESH:
			me = self->object->data;
			vec = (float*) mesh_get_bb(self->object)->vec;
			break;
		case OB_CURVE:
		case OB_FONT:
		case OB_SURF:
			curve = self->object->data;
			if( !curve->bb )
				tex_space_curve( curve );
			vec = ( float * ) curve->bb->vec;
			break;
		default:
			Py_RETURN_NONE;
		}
	} else {		/* the ob bbox exists */
		vec = ( float * ) self->object->bb->vec;
	}


	{	/* transform our obdata bbox by the obmat.
		   the obmat is 4x4 homogeneous coords matrix.
		   each bbox coord is xyz, so we make it homogenous
		   by padding it with w=1.0 and doing the matrix mult.
		   afterwards we divide by w to get back to xyz.
		 */
		/* printmatrix4( "obmat", self->object->obmat); */

		float tmpvec[4];	/* tmp vector for homogenous coords math */
		int i;
		float *from;

		bbox = PyList_New( 8 );
		if( !bbox )
			return EXPP_ReturnPyObjError
				( PyExc_MemoryError,
				  "couldn't create pylist" );
		for( i = 0, from = vec; i < 8; i++, from += 3 ) {
			memcpy( tmpvec, from, 3 * sizeof( float ) );
			tmpvec[3] = 1.0f;	/* set w coord */
			
			if (worldspace) {
				Mat4MulVec4fl( self->object->obmat, tmpvec );
				/* divide x,y,z by w */
				tmpvec[0] /= tmpvec[3];
				tmpvec[1] /= tmpvec[3];
				tmpvec[2] /= tmpvec[3];

#if 0
				{	/* debug print stuff */
					int i;
	
					printf( "\nobj bbox transformed\n" );
					for( i = 0; i < 4; ++i )
						printf( "%f ", tmpvec[i] );
	
					printf( "\n" );
				}
#endif
			}
			/* because our bounding box is calculated and
			   does not have its own memory,
			   we must create vectors that allocate space */

			vector = newVectorObject( NULL, 3, Py_NEW);
			memcpy( ( ( VectorObject * ) vector )->vec,
				tmpvec, 3 * sizeof( float ) );
			PyList_SET_ITEM( bbox, i, vector );
		}
	}

	return bbox;
}

static PyObject *Object_getBoundBox_noargs( BPy_Object * self )
{
	return Object_getBoundBox(self, PyTuple_New(0));
}

static PyObject *Object_makeDisplayList( BPy_Object * self )
{
	Object *ob = self->object;

	if( ob->type == OB_FONT ) {
		Curve *cu = ob->data;
		freedisplist( &cu->disp );
		text_to_curve( ob, 0 );
	}

	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	Py_RETURN_NONE;
}

static PyObject *Object_link( BPy_Object * self, PyObject * args )
{
	PyObject *py_data;
	ID *id;
	ID *oldid;
	int obj_id;
	void *data = NULL;
	int ok;

	if( !PyArg_ParseTuple( args, "O", &py_data ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an object as argument" );

	if( BPy_Armature_Check( py_data ) )
		data = ( void * ) PyArmature_AsArmature((BPy_Armature*)py_data);
	else if( BPy_Camera_Check( py_data ) )
		data = ( void * ) Camera_FromPyObject( py_data );
	else if( BPy_Lamp_Check( py_data ) )
		data = ( void * ) Lamp_FromPyObject( py_data );
	else if( BPy_Curve_Check( py_data ) )
		data = ( void * ) Curve_FromPyObject( py_data );
	else if( BPy_NMesh_Check( py_data ) ) {
		data = ( void * ) NMesh_FromPyObject( py_data, self->object );
		if( !data )		/* NULL means there is already an error */
			return NULL;
	} else if( BPy_Mesh_Check( py_data ) )
		data = ( void * ) Mesh_FromPyObject( py_data, self->object );
	else if( BPy_Lattice_Check( py_data ) )
		data = ( void * ) Lattice_FromPyObject( py_data );
	else if( BPy_Metaball_Check( py_data ) )
		data = ( void * ) Metaball_FromPyObject( py_data );
	else if( BPy_Text3d_Check( py_data ) )
		data = ( void * ) Text3d_FromPyObject( py_data );

	/* have we set data to something good? */
	if( !data )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"link argument type is not supported " );

	oldid = ( ID * ) self->object->data;
	id = ( ID * ) data;
	obj_id = MAKE_ID2( id->name[0], id->name[1] );

	/* if the object object has not been linked to real data before, we
	 * can now let it assume its real type */
	if( self->realtype != OB_EMPTY ) {
		self->object->type = self->realtype;
		self->realtype = OB_EMPTY;
	}

	ok = 1;
	switch ( obj_id ) {
	case ID_AR:
		if( self->object->type != OB_ARMATURE ) {
			ok = 0;
		}
		break;
	case ID_CA:
		if( self->object->type != OB_CAMERA ) {
			ok = 0;
		}
		break;
	case ID_LA:
		if( self->object->type != OB_LAMP ) {
			ok = 0;
		}
		break;
	case ID_ME:
		if( self->object->type != OB_MESH ) {
			ok = 0;
		}
		break;
	case ID_CU:
		if( self->object->type != OB_CURVE && self->object->type != OB_FONT ) {
			ok = 0;
		}
		break;
	case ID_LT:
		if( self->object->type != OB_LATTICE ) {
			ok = 0;
		}
		break;
	case ID_MB:
		if( self->object->type != OB_MBALL ) {
			ok = 0;
		}
		break;
	default:
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"Linking this object type is not supported" );
	}

	if( !ok )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"The 'link' object is incompatible with the base object" );
	self->object->data = data;

	/* creates the curve for the text object */
	if (self->object->type == OB_FONT) {
		text_to_curve(self->object, 0);
	} else if (self->object->type == OB_ARMATURE) {
		armature_rebuild_pose(self->object, (bArmature *)data);
	}
	id_us_plus( id );
	if( oldid ) {
		if( oldid->us > 0 ) {
			oldid->us--;
		} else {
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"old object reference count below 0" );
		}
	}

	/* make sure data and object materials are consistent */
	test_object_materials( id );

	Py_RETURN_NONE;
}

static PyObject *Object_makeParentVertex( BPy_Object * self, PyObject * args )
{
	PyObject *list;
	PyObject *vlist;
	PyObject *py_child;
	PyObject *ret_val;
	Object *parent;
	int noninverse = 0;
	int fast = 0;
	int partype;
	int v1, v2=0, v3=0;
	int i;

	/* Check if the arguments passed to makeParent are valid. */
	if( !PyArg_ParseTuple( args, "OO|ii", &list, &vlist, &noninverse, &fast ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a list of objects, a tuple of integers and one or two integers as arguments" );

	if( !PySequence_Check( list ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected a list of objects" );

	if (!PyTuple_Check( vlist ))
		return EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected a tuple of integers" );

	switch( PyTuple_Size( vlist ) ) {
	case 1:
		if( !PyArg_ParseTuple( vlist, "i", &v1 ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a tuple of 1 or 3 integers" );

		if ( v1 < 0 )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"indices must be strictly positive" );

		partype = PARVERT1;
		break;
	case 3:
		if( !PyArg_ParseTuple( vlist, "iii", &v1, &v2, &v3 ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a tuple of 1 or 3 integers" );

		if ( v1 < 0 || v2 < 0 || v3 < 0)
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					   	"indices must be strictly positive" );
		partype = PARVERT3;
		break;
	default:
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a tuple of 1 or 3 integers" );
	}

	parent = ( Object * ) self->object;

	if (!ELEM3(parent->type, OB_MESH, OB_CURVE, OB_SURF))
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"Parent Vertex only applies to curve, mesh or surface objects" );

	if (parent->id.us == 0)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"object must be linked to a scene before it can become a parent");

	/* Check if the PyObject passed in list is a Blender object. */
	for( i = 0; i < PySequence_Length( list ); i++ ) {
		py_child = PySequence_GetItem( list, i );

		ret_val = internal_makeParent(parent, py_child, partype, noninverse, fast, v1, v2, v3, NULL);
		Py_DECREF (py_child);

		if (ret_val)
			Py_DECREF(ret_val);
		else {
			if (!fast)	/* need to sort when interrupting in the middle of the list */
				DAG_scene_sort( G.scene );
			return NULL; /* error has been set already */
		}
	}

	if (!fast) /* otherwise, only sort at the end */
		DAG_scene_sort( G.scene );

	Py_RETURN_NONE;
}

static PyObject *Object_makeParentDeform( BPy_Object * self, PyObject * args )
{
	PyObject *list;
	PyObject *py_child;
	PyObject *ret_val;
	Object *parent;
	int noninverse = 0;
	int fast = 0;
	int i;

	/* Check if the arguments passed to makeParent are valid. */
	if( !PyArg_ParseTuple( args, "O|ii", &list, &noninverse, &fast ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a list of objects and one or two integers as arguments" );

	if( !PySequence_Check( list ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a list of objects" );

	parent = ( Object * ) self->object;

	if (parent->type != OB_CURVE && parent->type != OB_ARMATURE)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"Parent Deform only applies to curve or armature objects" );

	if (parent->id.us == 0)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"object must be linked to a scene before it can become a parent");

	/* Check if the PyObject passed in list is a Blender object. */
	for( i = 0; i < PySequence_Length( list ); i++ ) {
		py_child = PySequence_GetItem( list, i );

		ret_val = internal_makeParent(parent, py_child, PARSKEL, noninverse, fast, 0, 0, 0, NULL);
		Py_DECREF (py_child);

		if (ret_val)
			Py_DECREF(ret_val);
		else {
			if (!fast)	/* need to sort when interupting in the middle of the list */
				DAG_scene_sort( G.scene );
			return NULL; /* error has been set already */
		}
	}

	if (!fast) /* otherwise, only sort at the end */
		DAG_scene_sort( G.scene );

	Py_RETURN_NONE;
}


static PyObject *Object_makeParentBone( BPy_Object * self, PyObject * args )
{
	char *bonename;
	PyObject *list;
	PyObject *py_child;
	PyObject *ret_val;
	Object *parent;
	int noninverse = 0;
	int fast = 0;
	int i;
	
	/* Check if the arguments passed to makeParent are valid. */
	if( !PyArg_ParseTuple( args, "Os|ii", &list, &bonename, &noninverse, &fast ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a list of objects, bonename and optionally two integers as arguments" );
	
	parent = ( Object * ) self->object;
	
	if (parent->type != OB_ARMATURE)
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"Parent Bone only applies to armature objects" );

	if (parent->id.us == 0)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"object must be linked to a scene before it can become a parent");
	
	if (!parent->data)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"object must be linked to armature data");
	
	if (!get_named_bone(parent->data, bonename))
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"Parent Bone Name is not in the armature" );
	
	/* Check if the PyObject passed in list is a Blender object. */
	for( i = 0; i < PySequence_Length( list ); i++ ) {
		py_child = PySequence_GetItem( list, i );

		ret_val = internal_makeParent(parent, py_child, PARBONE, noninverse, fast, 0, 0, 0, bonename);
		Py_DECREF (py_child);

		if (ret_val)
			Py_DECREF(ret_val);
		else {
			if (!fast)	/* need to sort when interupting in the middle of the list */
				DAG_scene_sort( G.scene );
			return NULL; /* error has been set already */
		}
	}

	if (!fast) /* otherwise, only sort at the end */
		DAG_scene_sort( G.scene );

	Py_RETURN_NONE;
}


static PyObject *Object_makeParent( BPy_Object * self, PyObject * args )
{
	PyObject *list;
	PyObject *py_child;
	PyObject *ret_val;
	Object *parent;
	int noninverse = 0;
	int fast = 0;
	int i;

	/* Check if the arguments passed to makeParent are valid. */
	if( !PyArg_ParseTuple( args, "O|ii", &list, &noninverse, &fast ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a list of objects and one or two integers as arguments" );

	if( !PySequence_Check( list ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a list of objects" );

	parent = ( Object * ) self->object;

	if (parent->id.us == 0)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"object must be linked to a scene before it can become a parent");

	/* Check if the PyObject passed in list is a Blender object. */
	for( i = 0; i < PySequence_Length( list ); i++ ) {
		py_child = PySequence_GetItem( list, i );

		ret_val = internal_makeParent(parent, py_child, PAROBJECT, noninverse, fast, 0, 0, 0, NULL);
		Py_DECREF (py_child);

		if (ret_val)
			Py_DECREF(ret_val);
		else {
			if (!fast)	/* need to sort when interupting in the middle of the list */
				DAG_scene_sort( G.scene );
			return NULL; /* error has been set already */
		}
	}

	if (!fast) /* otherwise, only sort at the end */
		DAG_scene_sort( G.scene );

	Py_RETURN_NONE;
}

static PyObject *Object_join( BPy_Object * self, PyObject * args )
{
	PyObject *list;
	PyObject *py_child;
	Object *parent;
	Object *child;
	Scene *temp_scene;
	Scene *orig_scene;
	Base *temp_base;
	short type;
	int i, ok=0, ret_value=0, list_length=0;

	/* joining in background causes segfaults */
	if( G.background == 1 )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"cannot join objects in background mode" );

	/* Check if the arguments passed to makeParent are valid. */
	if( !PyArg_ParseTuple( args, "O", &list ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a list of objects" );
	
	if( !PySequence_Check( list ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a list of objects" );
	
	list_length = PySequence_Length( list ); /* if there are no objects to join then exit silently */
	
	if( !list_length ) {
		Py_RETURN_NONE;
	}
	
	parent = ( Object * ) self->object;
	type = parent->type;
	
	/* Only these object types are sypported */
	if( type!=OB_MESH && type!=OB_CURVE && type!=OB_SURF && type!=OB_ARMATURE )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
						"Base object is not a type Blender can join" );
	
	if( !object_in_scene( parent, G.scene ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"object must be in the current scene" );

	/* exit editmode so join can be done */
	if( G.obedit )
		exit_editmode( EM_FREEDATA );
	
	temp_scene = add_scene( "Scene" ); /* make the new scene */
	temp_scene->lay= 1; /* first layer on */
	
	/* TODO: use EXPP_check_sequence_consistency here */

	/* Check if the PyObject passed in list is a Blender object. */
	for( i = 0; i < list_length; i++ ) {
		py_child = PySequence_GetItem( list, i );
		if( !BPy_Object_Check( py_child ) ) {
			/* Cleanup */
			free_libblock( &G.main->scene, temp_scene );
			Py_DECREF( py_child );
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a list of objects, one or more of the list items is not a Blender Object." );
		} else {
			/* List item is an object, is it the same type? */
			child = ( Object * ) Object_FromPyObject( py_child );
			Py_DECREF( py_child );
			if( parent->type == child->type ) {
				if( !object_in_scene( child, G.scene ) ) {
					free_libblock( &G.main->scene, temp_scene );
					return EXPP_ReturnPyObjError( PyExc_AttributeError,
							"object must be in the current scene" );
				}

				ok =1;
				/* Add a new base, then link the base to the temp_scene */
				temp_base = MEM_callocN( sizeof( Base ), "pynewbase" );
				/* we know these types are the same, link to the temp scene
				 * for joining */
				temp_base->object = child;	/* link object to the new base */
				temp_base->flag |= SELECT;
				temp_base->lay = 1; /*1 layer on */
				
				BLI_addhead( &temp_scene->base, temp_base );	/* finally, link new base to scene */
				child->id.us += 1; /*Would usually increase user count but in this case it's ok not to */
				
				/*DAG_object_flush_update(temp_scene, temp_base->object, OB_RECALC_DATA);*/
			}
		}
	}
	
	orig_scene = G.scene; /* backup our scene */
	
	/* Add the main object into the temp_scene */
	temp_base = MEM_callocN( sizeof( Base ), "pynewbase" );
	temp_base->object = parent;	/* link object to the new base */
	temp_base->flag |= SELECT;
	temp_base->lay = 1; /*1 layer on */
	BLI_addhead( &temp_scene->base, temp_base );	/* finally, link new base to scene */
	parent->id.us += 1;
	
	/* all objects in the scene, set it active and the active object */
	set_scene( temp_scene );
	set_active_base( temp_base );
	
	/* Do the joining now we know everythings OK. */
	if(type == OB_MESH)
		ret_value = join_mesh();
	else if(type == OB_CURVE)
		ret_value = join_curve(OB_CURVE);
	else if(type == OB_SURF)
		ret_value = join_curve(OB_SURF);
	else if(type == OB_ARMATURE)
		ret_value = join_armature();
	
	/* May use this for correcting object user counts later on */
	/*
	if (!ret_value) {
		temp_base = temp_scene->base.first;
		while( base ) {
			object = base->object;
			object->id.us +=1
			base = base->next;
		}
	}*/

	/* remove old scene */
	set_scene( orig_scene );
	free_libblock( &G.main->scene, temp_scene );

	/* no objects were of the correct type, return None */
	if (!ok) {
		Py_RETURN_NONE;
	}

	/* If the join failed then raise an error */
	if (!ret_value)
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
"Blender failed to join the objects, this is not a script error.\n\
Please add exception handling to your script with a RuntimeError exception\n\
letting the user know that their data could not be joined." ) );

	Py_RETURN_NONE;
}

static PyObject *internal_makeParent(Object *parent, PyObject *py_child,
		int partype,                /* parenting type */
		int noninverse, int fast,   /* parenting arguments */
		int v1, int v2, int v3,     /* for vertex parent */
		char *bonename)             /* for bone parents - assume the name is already checked to be a valid bone name*/
{
	Object *child = NULL;

	if( BPy_Object_Check( py_child ) )
		child = ( Object * ) Object_FromPyObject( py_child );

	if( child == NULL )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					"Object Type expected" );

	if( test_parent_loop( parent, child ) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"parenting loop detected - parenting failed" );

	if (partype == PARSKEL && child->type != OB_MESH)
		child->partype = PAROBJECT;
	else
		child->partype = (short)partype;

	if (partype == PARVERT3) {
		child->par1 = v1;
		child->par2 = v2;
		child->par3 = v3;
	}
	else if (partype == PARVERT1) {
		child->par1 = v1;
	} else if (partype == PARBONE) {
		strcpy( child->parsubstr, bonename );
	}
	
	

	child->parent = parent;
	/* py_obj_child = (BPy_Object *) py_child; */
	if( noninverse == 1 ) {
		Mat4One(child->parentinv);
		/* Parent inverse = unity */
		child->loc[0] = 0.0;
		child->loc[1] = 0.0;
		child->loc[2] = 0.0;
	} else {
		what_does_parent( child );
		Mat4Invert( child->parentinv, workob.obmat );
		clear_workob();
	}

	if( !fast )
		child->recalc |= OB_RECALC_OB;

	Py_RETURN_NONE;
}

static PyObject *Object_materialUsage( void )
{
	return EXPP_ReturnPyObjError( PyExc_NotImplementedError,
			"materialUsage: not yet implemented" );
}

static PyObject *Object_setDeltaLocation( BPy_Object * self, PyObject * args )
{
	float dloc1;
	float dloc2;
	float dloc3;
	int status;

	if( PyObject_Length( args ) == 3 )
		status = PyArg_ParseTuple( args, "fff", &dloc1, &dloc2,
					   &dloc3 );
	else
		status = PyArg_ParseTuple( args, "(fff)", &dloc1, &dloc2,
					   &dloc3 );

	if( !status )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected sequence argument of 3 floats" );

	self->object->dloc[0] = dloc1;
	self->object->dloc[1] = dloc2;
	self->object->dloc[2] = dloc3;

	/* since we have messed with object, we need to flag for DAG recalc */
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

#define DTX_MASK ( OB_AXIS | OB_TEXSPACE | OB_DRAWNAME | \
		OB_DRAWIMAGE | OB_DRAWWIRE | OB_DRAWXRAY | OB_DRAWTRANSP )

static PyObject *Object_getDrawMode( BPy_Object * self )
{
	return PyInt_FromLong( (long)(self->object->dtx & DTX_MASK) );
}

static int Object_setDrawMode( BPy_Object * self, PyObject * args )
{
	PyObject* integer = PyNumber_Int( args );
	int value;

	if( !integer )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected integer argument" );

	value = ( int )PyInt_AS_LONG( integer );
	Py_DECREF( integer );
	if( value & ~DTX_MASK )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"undefined bit(s) set in bitfield" );

	self->object->dtx = value;
	self->object->recalc |= OB_RECALC_OB;  

	return 0;
}

static PyObject *Object_getDrawType( BPy_Object * self )
{
	return PyInt_FromLong( (long)self->object->dt );
}

static int Object_setDrawType( BPy_Object * self, PyObject * value )
{
	/* since we mess with object, we need to flag for DAG recalc */
	self->object->recalc |= OB_RECALC_OB;  

	return EXPP_setIValueRange( value, &self->object->dt,
			OB_BOUNDBOX, OB_TEXTURE, 'b' );
}

static int Object_setEuler( BPy_Object * self, PyObject * args )
{
	float rot1, rot2, rot3;
	int status = 0;		/* failure */

	if( PyTuple_Check( args ) && PyTuple_Size( args ) == 1 )
		args = PyTuple_GET_ITEM( args, 0 );

	if( EulerObject_Check( args ) ) {
		rot1 = ( ( EulerObject * ) args )->eul[0];
		rot2 = ( ( EulerObject * ) args )->eul[1];
		rot3 = ( ( EulerObject * ) args )->eul[2];
		status = 1;
	} else if( PySequence_Check( args ) && PySequence_Size( args ) == 3 ) {
		if( PyList_Check( args ) )
			args = PySequence_Tuple( args );
		else
			Py_INCREF( args );
		status = PyArg_ParseTuple( args, "fff", &rot1, &rot2, &rot3 );
		Py_DECREF( args );
	}

	if( !status )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected euler or sequence of 3 floats" );

	self->object->rot[0] = rot1;
	self->object->rot[1] = rot2;
	self->object->rot[2] = rot3;

	/* since we have messed with object, we need to flag for DAG recalc */
	self->object->recalc |= OB_RECALC_OB;  

	return 0;
}

static int Object_setMatrix( BPy_Object * self, MatrixObject * mat )
#if 0
{
	int x, y;

	if( !MatrixObject_Check( mat ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected matrix object as argument" );

	if( mat->rowSize == 4 && mat->colSize == 4 ) {
		for( x = 0; x < 4; x++ ) {
			for( y = 0; y < 4; y++ ) {
				self->object->obmat[x][y] = mat->matrix[x][y];
			}
		}
	} else if( mat->rowSize == 3 && mat->colSize == 3 ) {
		for( x = 0; x < 3; x++ ) {
			for( y = 0; y < 3; y++ ) {
				self->object->obmat[x][y] = mat->matrix[x][y];
			}
		}
		/* if a 3x3 matrix, clear the fourth row/column */
		for( x = 0; x < 3; x++ )
			self->object->obmat[x][3] = self->object->obmat[3][x] = 0.0;
		self->object->obmat[3][3] = 1.0;
	} else 
		return EXPP_ReturnIntError( PyExc_ValueError,
				"expected 3x3 or 4x4 matrix" );

	apply_obmat( self->object );

	/* since we have messed with object, we need to flag for DAG recalc */
	self->object->recalc |= OB_RECALC_OB;  

	return 0;
}
#endif
{
	int x, y;
	float matrix[4][4]; /* for the result */
	float invmat[4][4]; /* for the result */

	if( !MatrixObject_Check( mat ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected matrix object as argument" );

	if( mat->rowSize == 4 && mat->colSize == 4 ) {
		for( x = 0; x < 4; x++ ) {
			for( y = 0; y < 4; y++ ) {
				matrix[x][y] = mat->matrix[x][y];
			}
		}
	} else if( mat->rowSize == 3 && mat->colSize == 3 ) {
		for( x = 0; x < 3; x++ ) {
			for( y = 0; y < 3; y++ ) {
				matrix[x][y] = mat->matrix[x][y];
			}
		}
		/* if a 3x3 matrix, clear the fourth row/column */
		for( x = 0; x < 3; x++ )
			matrix[x][3] = matrix[3][x] = 0.0;
		matrix[3][3] = 1.0;
	} else 
		return EXPP_ReturnIntError( PyExc_ValueError,
				"expected 3x3 or 4x4 matrix" );

	/* localspace matrix is truly relative to the parent, but parameters
	 * stored in object are relative to parentinv matrix.  Undo the parent
	 * inverse part before updating obmat and calling apply_obmat() */
	if( self->object->parent ) {
		Mat4Invert( invmat, self->object->parentinv );
		Mat4MulMat4( self->object->obmat, matrix, invmat );
	} else
		Mat4CpyMat4( self->object->obmat, matrix );

	apply_obmat( self->object );

	/* since we have messed with object, we need to flag for DAG recalc */
	self->object->recalc |= OB_RECALC_OB;  

	return 0;
}


/*
 * Object_insertIpoKey()
 *  inserts Object IPO key for LOC, ROT, SIZE, LOCROT, LOCROTSIZE, or LAYER
 *  Note it also inserts actions! 
 */

static PyObject *Object_insertIpoKey( BPy_Object * self, PyObject * args )
{
	Object *ob= self->object;
	int key = 0;
	char *actname= NULL;

	if( !PyArg_ParseTuple( args, "i", &key ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected int argument" );

	if(ob->ipoflag & OB_ACTION_OB)
		actname= "Object";
	
	if (key == IPOKEY_LOC || key == IPOKEY_LOCROT || key == IPOKEY_LOCROTSIZE){
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_LOC_X, 0);
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_LOC_Y, 0);
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_LOC_Z, 0);      
	}
	if (key == IPOKEY_ROT || key == IPOKEY_LOCROT || key == IPOKEY_LOCROTSIZE){
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_ROT_X, 0);
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_ROT_Y, 0);
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_ROT_Z, 0);      
	}
	if (key == IPOKEY_SIZE || key == IPOKEY_LOCROTSIZE ){
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_SIZE_X, 0);
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_SIZE_Y, 0);
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_SIZE_Z, 0);      
	}
	if (key == IPOKEY_LAYER ){
		insertkey((ID *)ob, ID_OB, actname, NULL,OB_LAY, 0);
	}

	if (key == IPOKEY_PI_STRENGTH ){
		insertkey((ID *)ob, ID_OB, actname, NULL, OB_PD_FSTR, 0);   
	} else if (key == IPOKEY_PI_FALLOFF ){
		insertkey((ID *)ob, ID_OB, actname, NULL, OB_PD_FFALL, 0);   
	} else if (key == IPOKEY_PI_SURFACEDAMP ){
		insertkey((ID *)ob, ID_OB, actname, NULL, OB_PD_SDAMP, 0);   
	} else if (key == IPOKEY_PI_RANDOMDAMP ){
		insertkey((ID *)ob, ID_OB, actname, NULL, OB_PD_RDAMP, 0);   
	} else if (key == IPOKEY_PI_PERM ){
		insertkey((ID *)ob, ID_OB, actname, NULL, OB_PD_PERM, 0);   
	}

	allspace(REMAKEIPO, 0);
	EXPP_allqueue(REDRAWIPO, 0);
	EXPP_allqueue(REDRAWVIEW3D, 0);
	EXPP_allqueue(REDRAWACTION, 0);
	EXPP_allqueue(REDRAWNLA, 0);

	Py_RETURN_NONE;
}

/*
 * Object_insertPoseKey()
 * inserts a Action Pose key from a given pose (sourceaction, frame) to the
 * active action to a given framenum
 */

static PyObject *Object_insertPoseKey( BPy_Object * self, PyObject * args )
{
	Object *ob= self->object;
	BPy_Action *sourceact;
	char *chanName;
	int actframe;


	/* for doing the time trick, similar to editaction bake_action_with_client() */
	int oldframe;
	int curframe;

	if( !PyArg_ParseTuple( args, "O!sii", &Action_Type, &sourceact,
				&chanName, &actframe, &curframe ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expects an action to copy poses from, a string for chan/bone name, an int argument for frame to extract from the action and finally another int for the frame where to put the new key in the active object.action" );

	extract_pose_from_action(ob->pose, sourceact->action, (float)actframe);

	oldframe = G.scene->r.cfra;
	G.scene->r.cfra = curframe;

	/* XXX: must check chanName actually exists, otherwise segfaults! */
	//achan = get_action_channel(sourceact->action, chanName);

	insertkey(&ob->id, ID_PO, chanName, NULL, AC_LOC_X, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_LOC_Y, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_LOC_Z, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_QUAT_X, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_QUAT_Y, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_QUAT_Z, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_QUAT_W, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_SIZE_X, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_SIZE_Y, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_SIZE_Z, 0);
	
	G.scene->r.cfra = oldframe;

	allspace(REMAKEIPO, 0);
	EXPP_allqueue(REDRAWIPO, 0);
	EXPP_allqueue(REDRAWVIEW3D, 0);
	EXPP_allqueue(REDRAWACTION, 0);
	EXPP_allqueue(REDRAWNLA, 0);

	/* restore, but now with the new action in place */
	/*extract_pose_from_action(ob->pose, ob->action, G.scene->r.cfra);
	where_is_pose(ob);*/
	
	EXPP_allqueue(REDRAWACTION, 1);

	Py_RETURN_NONE;
}

static PyObject *Object_insertCurrentPoseKey( BPy_Object * self, PyObject * args )
{
	Object *ob= self->object;
	char *chanName;

	/* for doing the time trick, similar to editaction bake_action_with_client() */
	int oldframe;
	int curframe;

	if( !PyArg_ParseTuple( args, "si", &chanName, &curframe ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected chan/bone name, and a time (int) argument" );

	oldframe = G.scene->r.cfra;
	G.scene->r.cfra = curframe;

	/* XXX: must check chanName actually exists, otherwise segfaults! */

	insertkey(&ob->id, ID_PO, chanName, NULL, AC_LOC_X, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_LOC_Y, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_LOC_Z, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_QUAT_X, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_QUAT_Y, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_QUAT_Z, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_QUAT_W, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_SIZE_X, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_SIZE_Y, 0);
	insertkey(&ob->id, ID_PO, chanName, NULL, AC_SIZE_Z, 0);

	G.scene->r.cfra = oldframe;

	allspace(REMAKEIPO, 0);
	EXPP_allqueue(REDRAWIPO, 0);
	EXPP_allqueue(REDRAWVIEW3D, 0);
	EXPP_allqueue(REDRAWACTION, 0);
	EXPP_allqueue(REDRAWNLA, 0);

	/* restore */
	extract_pose_from_action(ob->pose, ob->action, (float)G.scene->r.cfra);
	where_is_pose(ob);

	EXPP_allqueue(REDRAWACTION, 1);

	Py_RETURN_NONE;
}  

static PyObject *Object_setConstraintInfluenceForBone( BPy_Object * self,
		PyObject * args )
{
	char *boneName, *constName;
	float influence;
	IpoCurve *icu;

	if( !PyArg_ParseTuple( args, "ssf", &boneName, &constName, &influence ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expects bonename, constraintname, influenceval" );
	
	icu = verify_ipocurve((ID *)self->object, ID_CO, boneName, constName, NULL,
			CO_ENFORCE);
	
	if (!icu)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"cannot get a curve from this IPO, may be using libdata" );		
	
	insert_vert_icu(icu, (float)CFRA, influence, 0);
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_copyNLA( BPy_Object * self, PyObject * args ) {
	BPy_Object *bpy_fromob;

	if( !PyArg_ParseTuple( args, "O", &bpy_fromob ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					"requires a Blender Object to copy NLA strips from." );
	copy_nlastrips(&self->object->nlastrips, &bpy_fromob->object->nlastrips);
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

/*Now that  BPY has a Strip type, return the created strip.*/
static PyObject *Object_convertActionToStrip( BPy_Object * self )
{
	bActionStrip *strip = convert_action_to_strip( self->object );
	return ActionStrip_CreatePyObject( strip );
}

static PyObject *Object_setLocation( BPy_Object * self, PyObject * args )
{
	float loc1;
	float loc2;
	float loc3;
	int status;

	if( PyObject_Length( args ) == 3 )
		status = PyArg_ParseTuple( args, "fff", &loc1, &loc2, &loc3 );
	else
		status = PyArg_ParseTuple( args, "(fff)", &loc1, &loc2,
					   &loc3 );

	if( !status )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected list argument of 3 floats" );

	self->object->loc[0] = loc1;
	self->object->loc[1] = loc2;
	self->object->loc[2] = loc3;

	/* since we have messed with object, we need to flag for DAG recalc */
	self->object->recalc |= OB_RECALC_OB;  
	DAG_object_flush_update(G.scene, self->object, OB_RECALC_DATA);

	Py_RETURN_NONE;
}

static PyObject *Object_setMaterials( BPy_Object * self, PyObject * args )
{
	PyObject *list;
	int len;
	int i;
	Material **matlist = NULL;

	if (!self->object->data)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"object must be linked to object data (e.g. to a mesh) first" );

	if( !PyArg_ParseTuple( args, "O!", &PyList_Type, &list ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a list (of materials or None) as argument" );

	len = PyList_Size(list);

	/* Object_getMaterials can return '[]' (zero-length list), so that must
	 * also be accepted by this method for
	 * ob2.setMaterials(ob1.getMaterials()) to always work.
	 * In other words, list can be '[]' and so len can be zero. */
	if (len > 0) {
		if( len > MAXMAT )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"list must have from 1 up to 16 materials" );

		matlist = EXPP_newMaterialList_fromPyList( list );
		if( !matlist )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"material list must be a list of valid materials!" );
	}

	if( self->object->mat )
		EXPP_releaseMaterialList( self->object->mat, self->object->totcol );

	/* Increase the user count on all materials */
	for( i = 0; i < len; i++ ) {
		if( matlist[i] )
			id_us_plus( ( ID * ) matlist[i] );
	}
	self->object->mat = matlist;
	self->object->totcol = (char)len;
	self->object->actcol = (char)len;

	switch ( self->object->type ) {
		case OB_CURVE:	/* fall through */
		case OB_FONT:	/* fall through */
		case OB_MESH:	/* fall through */
		case OB_MBALL:	/* fall through */
		case OB_SURF:
			EXPP_synchronizeMaterialLists( self->object );
			break;
		default:
			break;
	}

	/* since we have messed with object, we need to flag for DAG recalc */
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_setSize( BPy_Object * self, PyObject * args )
{
	float sizex;
	float sizey;
	float sizez;
	int status;

	if( PyObject_Length( args ) == 3 )
		status = PyArg_ParseTuple( args, "fff", &sizex, &sizey,
					   &sizez );
	else
		status = PyArg_ParseTuple( args, "(fff)", &sizex, &sizey,
					   &sizez );

	if( !status )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected list argument of 3 floats" );

	self->object->size[0] = sizex;
	self->object->size[1] = sizey;
	self->object->size[2] = sizez;

	/* since we have messed with object, we need to flag for DAG recalc */
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_makeTrack( BPy_Object * self, PyObject * args )
{
	BPy_Object *tracked = NULL;
	Object *ob = self->object;
	int fast = 0;

	if( !PyArg_ParseTuple( args, "O!|i", &Object_Type, &tracked, &fast ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an object and optionally also an int as arguments." );

	ob->track = tracked->object;

	if( !fast )
		DAG_scene_sort( G.scene );

	Py_RETURN_NONE;
}

static PyObject *Object_shareFrom( BPy_Object * self, PyObject * args )
{
	BPy_Object *object;
	ID *id;
	ID *oldid;

	if( !PyArg_ParseTuple( args, "O!", &Object_Type, &object ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected an object argument" );

	if( !object->object->data )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "Object argument has no data linked yet or is an empty" );
	
	if( self->object->type != object->object->type &&
		self->realtype != object->object->type)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "objects are not of same data type" );

	switch ( object->object->type ) {
	case OB_MESH:
	case OB_LAMP:
	case OB_CAMERA:	/* we can probably add the other types, too */
	case OB_ARMATURE:
	case OB_CURVE:
	case OB_SURF:
	case OB_LATTICE:
		
		/* if this object had no data, we need to enable the realtype */
		if (self->object->type == OB_EMPTY) {
			self->object->type= self->realtype;
			self->realtype = OB_EMPTY;
		}
	
		oldid = ( ID * ) self->object->data;
		id = ( ID * ) object->object->data;
		self->object->data = object->object->data;

		if( self->object->type == OB_MESH && id ) {
			self->object->totcol = 0;
			EXPP_synchronizeMaterialLists( self->object );
		}

		id_us_plus( id );
		if( oldid ) {
			if( oldid->us > 0 ) {
				oldid->us--;
			} else {
				return EXPP_ReturnPyObjError ( PyExc_RuntimeError,
					   "old object reference count below 0" );
			}
		}
		
		Py_RETURN_NONE;
	default:
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"object type not supported" );
	}
}

static PyObject *Object_getAllProperties( BPy_Object * self )
{
	PyObject *prop_list, *pyval;
	bProperty *prop = NULL;

	prop_list = PyList_New( 0 );

	prop = self->object->prop.first;
	while( prop ) {
		pyval = Property_CreatePyObject( prop );
		PyList_Append( prop_list, pyval );
		Py_DECREF(pyval);
		prop = prop->next;
	}
	return prop_list;
}

static PyObject *Object_getProperty( BPy_Object * self, PyObject * args )
{
	char *prop_name = NULL;
	bProperty *prop = NULL;

	if( !PyArg_ParseTuple( args, "s", &prop_name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a string" );

	prop = get_property( self->object, prop_name );
	if( prop )
		return Property_CreatePyObject( prop );

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"couldn't find the property" );
}

static PyObject *Object_addProperty( BPy_Object * self, PyObject * args )
{
	bProperty *prop = NULL;
	char *prop_name = NULL;
	PyObject *prop_data = Py_None;
	char *prop_type = NULL;
	short type = -1;
	BPy_Property *py_prop = NULL;
	int argslen = PyObject_Length( args );

	if( argslen == 3 || argslen == 2 ) {
		if( !PyArg_ParseTuple( args, "sO|s", &prop_name, &prop_data,
					&prop_type ) ) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expecting string, data, and optional string" );
		}
	} else if( argslen == 1 ) {
		if( !PyArg_ParseTuple( args, "O!", &property_Type, &py_prop ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expecting a Property" );

		if( py_prop->property != NULL )
			return EXPP_ReturnPyObjError( PyExc_ValueError,
					"Property is already added to an object" );
	} else {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected 1,2 or 3 arguments" );
	}

	/*parse property type*/
	if( !py_prop ) {
		if( prop_type ) {
			if( BLI_streq( prop_type, "BOOL" ) )
				type = PROP_BOOL;
			else if( BLI_streq( prop_type, "INT" ) )
				type = PROP_INT;
			else if( BLI_streq( prop_type, "FLOAT" ) )
				type = PROP_FLOAT;
			else if( BLI_streq( prop_type, "TIME" ) )
				type = PROP_TIME;
			else if( BLI_streq( prop_type, "STRING" ) )
				type = PROP_STRING;
			else
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					   "BOOL, INT, FLOAT, TIME or STRING expected" );
		} else {
			/*use the default*/
			if( PyInt_Check( prop_data ) )
				type = PROP_INT;
			else if( PyFloat_Check( prop_data ) )
				type = PROP_FLOAT;
			else if( PyString_Check( prop_data ) )
				type = PROP_STRING;
		}
	} else {
		type = py_prop->type;
	}

	/*initialize a new bProperty of the specified type*/
	prop = new_property( type );

	/*parse data*/
	if( !py_prop ) {
		BLI_strncpy( prop->name, prop_name, 32 );
		if( PyInt_Check( prop_data ) ) {
			*( ( int * ) &prop->data ) =
				( int ) PyInt_AsLong( prop_data );
		} else if( PyFloat_Check( prop_data ) ) {
			*( ( float * ) &prop->data ) =
				( float ) PyFloat_AsDouble( prop_data );
		} else if( PyString_Check( prop_data ) ) {
			BLI_strncpy( prop->poin,
				     PyString_AsString( prop_data ),
				     MAX_PROPSTRING );
		}
	} else {
		py_prop->property = prop;

		/* this should never be able to happen is we just assigned a valid
		 * proper to py_prop->property */

		if( !updateProperyData( py_prop ) ) {
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
							"Could not update property data" );
		}
	}

	/*add to property listbase for the object*/
	BLI_addtail( &self->object->prop, prop );

	Py_RETURN_NONE;
}

static PyObject *Object_removeProperty( BPy_Object * self, PyObject * args )
{
	char *prop_name = NULL;
	BPy_Property *py_prop = NULL;
	bProperty *prop = NULL;

	/* we accept either a property stringname or actual object */
	if( PyTuple_Size( args ) == 1 ) {
		PyObject *prop = PyTuple_GET_ITEM( args, 0 );
		if( BPy_Property_Check( prop ) )
			py_prop = (BPy_Property *)prop;
		else
			prop_name = PyString_AsString( prop );
	}
	if( !py_prop && !prop_name )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a Property or a string" );

	/*remove the link, free the data, and update the py struct*/
	if( py_prop ) {
		BLI_remlink( &self->object->prop, py_prop->property );
		if( updatePyProperty( py_prop ) ) {
			free_property( py_prop->property );
			py_prop->property = NULL;
		}
	} else {
		prop = get_property( self->object, prop_name );
		if( prop ) {
			BLI_remlink( &self->object->prop, prop );
			free_property( prop );
		}
	}
	Py_RETURN_NONE;
}

static PyObject *Object_removeAllProperties( BPy_Object * self )
{
	free_properties( &self->object->prop );
	Py_RETURN_NONE;
}

static PyObject *Object_copyAllPropertiesTo( BPy_Object * self,
					     PyObject * args )
{
	PyObject *dest;
	bProperty *prop = NULL;
	bProperty *propn = NULL;

	if( !PyArg_ParseTuple( args, "O!", &Object_Type, &dest ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected an Object" );

	/*make a copy of all its properties*/
	prop = self->object->prop.first;
	while( prop ) {
		propn = copy_property( prop );
		BLI_addtail( &( ( BPy_Object * ) dest )->object->prop, propn );
		prop = prop->next;
	}

	Py_RETURN_NONE;
}

static PyObject *Object_addScriptLink( BPy_Object * self, PyObject * args )
{
	Object *obj = self->object;
	ScriptLink *slink = &obj->scriptlink;
	return EXPP_addScriptLink( slink, args, 0 );
}

static PyObject *Object_clearScriptLinks( BPy_Object * self, PyObject * args )
{
	Object *obj = self->object;
	ScriptLink *slink = &obj->scriptlink;
	return EXPP_clearScriptLinks( slink, args );
}

static PyObject *Object_getScriptLinks( BPy_Object * self, PyObject * value )
{
	Object *obj = self->object;
	ScriptLink *slink = &obj->scriptlink;
	return EXPP_getScriptLinks( slink, value, 0 );
}

static PyObject *Object_getNLAflagBits ( BPy_Object * self ) 
{
	if (self->object->nlaflag & OB_NLA_OVERRIDE)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static int Object_setNLAflagBits ( BPy_Object * self, PyObject * value ) 
{
	int param;

	param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );

	if (param)
		self->object->nlaflag |= OB_NLA_OVERRIDE;
	else 
		self->object->nlaflag &= ~OB_NLA_OVERRIDE;
	
	self->object->recalc |= OB_RECALC_OB;  

	return 0;
}

static PyObject *Object_getDupliObjects( BPy_Object * self )
{
	Object *ob= self->object;
	
	if(ob->transflag & OB_DUPLI) {
		/* before make duplis, update particle for current frame */
		/* TODO, build particles for particle dupli's */
		if(ob->type!=OB_MBALL) {
			PyObject *list;
			DupliObject *dupob;
			int index;
			ListBase *duplilist = object_duplilist(G.scene, ob);

			list = PyList_New( BLI_countlist(duplilist) );
			if( !list )
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"PyList_New() failed" );

			for(dupob= duplilist->first, index=0; dupob; dupob= dupob->next, index++) {
				PyObject *pair;
				pair = PyTuple_New( 2 );
				
				PyTuple_SET_ITEM( pair, 0, Object_CreatePyObject(dupob->ob) );
				PyTuple_SET_ITEM( pair, 1, newMatrixObject((float*)dupob->mat,4,4,Py_NEW) );
				PyList_SET_ITEM( list, index, pair);
			}
			free_object_duplilist(duplilist);
			return list;
		}
	}
	return PyList_New( 0 );
}

static PyObject *Object_getDupliGroup( BPy_Object * self )
{
	Object *ob= self->object;

	if( ob->dup_group )
		return Group_CreatePyObject( ob->dup_group );

	Py_RETURN_NONE;
}

static int Object_setDupliGroup( BPy_Object * self, PyObject * value )
{
	return GenericLib_assignData(value, (void **) &self->object->dup_group, 0, 1, ID_GR, 0);
}

static PyObject *Object_getEffects( BPy_Object * self )
{
	return PyList_New( 0 );
}

static PyObject *Object_getActionStrips( BPy_Object * self )
{
	return ActionStrips_CreatePyObject( self->object );
}

static PyObject *Object_getConstraints( BPy_Object * self )
{
	return ObConstraintSeq_CreatePyObject( self->object );
}

static PyObject *Object_getModifiers( BPy_Object * self )
{
	return ModSeq_CreatePyObject( self->object, NULL );
}

static int Object_setModifiers( BPy_Object * self, PyObject * value )
{
	BPy_ModSeq *pymodseq;
	ModifierData *md;
	
	if (!BPy_ModSeq_Check(value))
		return EXPP_ReturnIntError( PyExc_TypeError,
				"can only assign another objects modifiers" );
	
	pymodseq = ( BPy_ModSeq * ) value;
	
	if (self->object->type != pymodseq->object->type)
		return EXPP_ReturnIntError( PyExc_TypeError,
				"can only assign modifiers between objects of the same type" );
	
	if (self->object == pymodseq->object)
		return 0;
	
	object_free_modifiers(self->object);
	for (md=pymodseq->object->modifiers.first; md; md=md->next) {
		if (md->type!=eModifierType_Hook) {
			ModifierData *nmd = modifier_new(md->type);
			modifier_copyData(md, nmd);
			BLI_addtail(&self->object->modifiers, nmd);
		}
	}
	
	DAG_object_flush_update(G.scene, self->object, OB_RECALC_DATA);
	return 0;
}

static PyObject *Object_insertShapeKey(BPy_Object * self)
{
	insert_shapekey(self->object);
	Py_RETURN_NONE;
}

/* __copy__() */
static  PyObject *Object_copy(BPy_Object * self)
{
	/* copy_object never returns NULL */
	struct Object *object= copy_object( self->object );
	object->id.us= 0; /*is 1 by default, not sure why */
	
	/* Create a Python object from it. */
	return Object_CreatePyObject( object );
}

/*****************************************************************************/
/* Function:	Object_CreatePyObject					 */
/* Description: This function will create a new BlenObject from an existing  */
/*		Object structure.					 */
/*****************************************************************************/
PyObject *Object_CreatePyObject( struct Object * obj )
{
	BPy_Object *blen_object;

	if( !obj ) Py_RETURN_NONE;
	
	blen_object =
		( BPy_Object * ) PyObject_NEW( BPy_Object, &Object_Type );

	if( blen_object == NULL ) {
		return ( NULL );
	}
	blen_object->object = obj;
	blen_object->realtype = OB_EMPTY;
	obj->id.us++;
	return ( ( PyObject * ) blen_object );
}

/*****************************************************************************/
/* Function:	Object_FromPyObject					 */
/* Description: This function returns the Blender object from the given	 */
/*		PyObject.						 */
/*****************************************************************************/
struct Object *Object_FromPyObject( PyObject * py_obj )
{
	BPy_Object *blen_obj;

	blen_obj = ( BPy_Object * ) py_obj;
	return ( blen_obj->object );
}

/*****************************************************************************/
/* Function:    Object_dealloc                                               */
/* Description: This is a callback function for the BlenObject type. It is   */
/*      the destructor function.                                             */
/*****************************************************************************/
static void Object_dealloc( BPy_Object * self )
{
	if( self->realtype != OB_EMPTY ) 
		free_libblock_us( &G.main->object, self->object );
	else 
		self->object->id.us--;

#if 0	/* this will adjust the ID and if zero delete the object */
	free_libblock_us( &G.main->object, self->object );
#endif
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Function:	Object_compare						 */
/* Description: This is a callback function for the BPy_Object type. It	 */
/*		compares two Object_Type objects. Only the "==" and "!="  */
/*		comparisons are meaninful. Returns 0 for equality and -1 if  */
/*		they don't point to the same Blender Object struct.	 */
/*		In Python it becomes 1 if they are equal, 0 otherwise.	 */
/*****************************************************************************/
static int Object_compare( BPy_Object * a, BPy_Object * b )
{
	return ( a->object == b->object ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:	Object_repr						 */
/* Description: This is a callback function for the BPy_Object type. It	 */
/*		builds a meaninful string to represent object objects.	 */
/*****************************************************************************/
static PyObject *Object_repr( BPy_Object * self )
{
	return PyString_FromFormat( "[Object \"%s\"]",
				    self->object->id.name + 2 );
}

/* Particle Deflection functions */

static PyObject *Object_getPIDeflection( BPy_Object * self )
{  
    if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	return PyBool_FromLong( ( long ) self->object->pd->deflect );
}

static int Object_setPIDeflection( BPy_Object * self, PyObject * value )
{
	int param;

    if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected true/false argument" );

	self->object->pd->deflect = (short)param;
	self->object->recalc |= OB_RECALC_OB;  

	return 0;
}

static PyObject *Object_getPIType( BPy_Object * self )
{
    if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

    return PyInt_FromLong( ( long )self->object->pd->forcefield );
}

static int Object_setPIType( BPy_Object * self, PyObject * value )
{
	int status;
	int oldforcefield;

    if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	oldforcefield = self->object->pd->forcefield;
	status = EXPP_setIValueRange( value, &self->object->pd->forcefield,
			PFIELD_FORCE, PFIELD_GUIDE, 'h' );

	/*
	 * if value was set successfully but is PFIELD_MAGNET, restore the old
	 * value and throw exception
	 */
	if( !status ) {
		if ( self->object->pd->forcefield == PFIELD_MAGNET ) {
			self->object->pd->forcefield = oldforcefield;
			return EXPP_ReturnIntError( PyExc_ValueError,
					"PFIELD_MAGNET not supported" );
		}
		self->object->recalc |= OB_RECALC_OB;  
	}
	return status;
}

static PyObject *Object_getPIUseMaxDist( BPy_Object * self )
{  
    if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	return PyBool_FromLong( ( long )self->object->pd->flag );
}

static int Object_setPIUseMaxDist( BPy_Object * self, PyObject * value )
{
	int param;

    if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected true/false argument" );

	self->object->pd->flag = (short)param;
	self->object->recalc |= OB_RECALC_OB;  

	return 0;
}

/* RIGIDBODY FUNCTIONS */

static PyObject *Object_getRBMass( BPy_Object * self )
{
    return PyFloat_FromDouble( (double)self->object->mass );
}

static int Object_setRBMass( BPy_Object * self, PyObject * args )
{
    float value;
	PyObject* flt = PyNumber_Float( args );

	if( !flt )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected float argument" );
	value = (float)PyFloat_AS_DOUBLE( flt );
	Py_DECREF( flt );

	if( value < 0.0f )
		return EXPP_ReturnIntError( PyExc_ValueError,
			"acceptable values are non-negative, 0.0 or more" );

	self->object->mass = value;
	self->object->recalc |= OB_RECALC_OB;  

	return 0;
}

/* this is too low level, possible to add helper methods */

#define GAMEFLAG_MASK ( OB_DYNAMIC | OB_CHILD | OB_ACTOR | OB_DO_FH | \
		OB_ROT_FH | OB_ANISOTROPIC_FRICTION | OB_GHOST | OB_RIGID_BODY | \
		OB_BOUNDS | OB_COLLISION_RESPONSE | OB_SECTOR | OB_PROP | \
		OB_MAINACTOR )

static PyObject *Object_getRBFlags( BPy_Object * self )
{
    return PyInt_FromLong( (long)( self->object->gameflag & GAMEFLAG_MASK ) );
}

static int Object_setRBFlags( BPy_Object * self, PyObject * args )
{
	PyObject* integer = PyNumber_Int( args );
	int value;

	if( !integer )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected integer argument" );

	value = ( int )PyInt_AS_LONG( integer );
	Py_DECREF( integer );
	if( value & ~GAMEFLAG_MASK )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"undefined bit(s) set in bitfield" );

	self->object->gameflag = value;
	self->object->recalc |= OB_RECALC_OB;  

	return 0;
}

static PyObject *Object_getRBShapeBoundType( BPy_Object * self )
{
	return PyInt_FromLong( (long)self->object->boundtype );
}

static int Object_setRBShapeBoundType( BPy_Object * self, PyObject * args )
{
	self->object->recalc |= OB_RECALC_OB;  
	return EXPP_setIValueRange( args, &self->object->boundtype,
			0, OB_BOUND_DYN_MESH, 'h' );
}

/*  SOFTBODY FUNCTIONS */

PyObject *Object_isSB(BPy_Object *self)
{
	if( self->object->soft )
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static PyObject *Object_getSBUseGoal( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    if( self->object->softflag & OB_SB_GOAL )
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static int Object_setSBUseGoal( BPy_Object * self, PyObject * value )
{
	int setting = PyObject_IsTrue( value );

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( setting == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected true/false argument" );

    if( setting )
		self->object->softflag |= OB_SB_GOAL;
    else
		self->object->softflag &= ~OB_SB_GOAL; 

	self->object->recalc |= OB_RECALC_OB;  
	return 0;
}

static PyObject *Object_getSBUseEdges( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    
    
    if( self->object->softflag & OB_SB_EDGES )
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static int Object_setSBUseEdges( BPy_Object * self, PyObject * value )
{
	int setting = PyObject_IsTrue( value );

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( setting == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected true/false argument" );

    if( setting )
		self->object->softflag |= OB_SB_EDGES;
    else
		self->object->softflag &= ~OB_SB_EDGES; 

	self->object->recalc |= OB_RECALC_OB;  
	return 0;
}

static PyObject *Object_getSBStiffQuads( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    
    
    if( self->object->softflag & OB_SB_QUADS )
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static int Object_setSBStiffQuads( BPy_Object * self, PyObject * value )
{
	int setting = PyObject_IsTrue( value );

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( setting == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected true/false argument" );

    if( setting )
		self->object->softflag |= OB_SB_QUADS;
    else
		self->object->softflag &= ~OB_SB_QUADS; 

	self->object->recalc |= OB_RECALC_OB;  
	return 0;
}

static int setupSB( Object* ob )
{
	ob->soft= sbNew();
	ob->softflag |= OB_SB_GOAL|OB_SB_EDGES;

	if( !ob->soft ) 	
		return 0;
	/* all this is initialized in sbNew() */
#if 0
	ob->soft->mediafrict = 0.5f;	
	ob->soft->nodemass   = 1.0f;		
	ob->soft->grav       = 0.0f;			
	ob->soft->rklimit    = 0.1f;		

	ob->soft->goalspring = 0.5f;	
	ob->soft->goalfrict  = 0.0f;	
	ob->soft->mingoal    = 0.0f;		
	ob->soft->maxgoal    = 1.0f;		
	ob->soft->defgoal    = 0.7f;		

	ob->soft->inspring   = 0.5f;	
	ob->soft->infrict    = 0.5f;	
#endif
	return 1;
} 

static int setupPI( Object* ob )
{
	if( ob->pd==NULL ) {
		ob->pd= MEM_callocN(sizeof(PartDeflect), "PartDeflect");
		/* and if needed, init here */
	}

	if( !ob->pd )
		return 0;

	ob->pd->deflect      =0;		
	ob->pd->forcefield   =0;	
	ob->pd->flag         =0;	
	ob->pd->pdef_damp    =0;		
	ob->pd->pdef_rdamp   =0;		
	ob->pd->pdef_perm    =0;	
	ob->pd->f_strength   =0;	
	ob->pd->f_power      =0;	
	ob->pd->maxdist      =0;	       
	return 1;
}

/*
 * scan list of Objects looking for matching obdata.
 * if found, set OB_RECALC_DATA flag.
 * call this from a bpy type update() method.
 */

void Object_updateDag( void *data )
{
	Object *ob;

	if( !data )
		return;

	for( ob = G.main->object.first; ob; ob= ob->id.next ){
		if( ob->data == data ) {
			ob->recalc |= OB_RECALC_DATA;
		}
	}
}

/*
 * utilities routines for handling generic getters and setters
 */

/*
 * get integer attributes
 */

static PyObject *getIntAttr( BPy_Object *self, void *type )
{
	int param;
	struct Object *object = self->object;

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_OBJ_ATTR_LAYERMASK:
		param = object->lay;
		break;
	case EXPP_OBJ_ATTR_COLBITS:
		param = object->colbits;
		if( param < 0 ) param += 65536;
		break;
	case EXPP_OBJ_ATTR_DRAWMODE:
		param = object->dtx;
		break;
	case EXPP_OBJ_ATTR_DRAWTYPE:
		param = object->dt;
		break;
	case EXPP_OBJ_ATTR_PARENT_TYPE:
		param = object->partype;
		break;
	case EXPP_OBJ_ATTR_DUPON:
		param = object->dupon;
		break;
	case EXPP_OBJ_ATTR_DUPOFF:
		param = object->dupoff;
		break;
	case EXPP_OBJ_ATTR_DUPSTA:
		param = object->dupsta;
		break;
	case EXPP_OBJ_ATTR_DUPEND:
		param = object->dupend;
		break;
	case EXPP_OBJ_ATTR_PASSINDEX:
		param = object->index;
		break;
	case EXPP_OBJ_ATTR_ACT_MATERIAL:
		param = object->actcol;
		break;
	case EXPP_OBJ_ATTR_ACT_SHAPE:
		param = object->shapenr;
		break;		
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"undefined type in getIntAttr" );
	}

	return PyInt_FromLong( param );
}

/*
 * set integer attributes which require clamping
 */

static int setIntAttrClamp( BPy_Object *self, PyObject *value, void *type )
{
	void *param;
	struct Object *object = self->object;
	int min, max, size;

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_OBJ_ATTR_DUPON:
		min = 1;
		max = 1500;
		size = 'H';			/* in case max is later made > 32767 */
		param = (void *)&object->dupon;
		break;
	case EXPP_OBJ_ATTR_DUPOFF:
		min = 0;
		max = 1500;
		size = 'H';			/* in case max is later made > 32767 */
		param = (void *)&object->dupoff;
		break;
	case EXPP_OBJ_ATTR_DUPSTA:
		min = 1;
		max = 32767;
		size = 'H';			/* in case max is later made > 32767 */
		param = (void *)&object->dupsta;
		break;
	case EXPP_OBJ_ATTR_DUPEND:
		min = 1;
		max = 32767;
		size = 'H';			/* in case max is later made > 32767 */
		param = (void *)&object->dupend;
		break;
	case EXPP_OBJ_ATTR_PASSINDEX:
		min = 0;
		max = 1000;
		size = 'H';			/* in case max is later made > 32767 */
		param = (void *)&object->index;
		break;
	case EXPP_OBJ_ATTR_ACT_MATERIAL:
		min = 1;
		max = object->totcol;
		size = 'b';			/* in case max is later made > 128 */
		param = (void *)&object->actcol;
		break;
	case EXPP_OBJ_ATTR_ACT_SHAPE:
	{
		Key *key= ob_get_key(object);
		KeyBlock *kb;
		min = 1;
		max = 0;
		if (key) {
			max= 1;
			for (kb = key->block.first; kb; kb=kb->next, max++);
		}
		size = 'h';			/* in case max is later made > 128 */
		param = (void *)&object->shapenr;
		break;
	}
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type in setIntAttrClamp");
	}

	self->object->recalc |= OB_RECALC_OB;  
	return EXPP_setIValueClamped( value, param, min, max, size );
}

/*
 * set integer attributes which require range checking
 */

static int setIntAttrRange( BPy_Object *self, PyObject *value, void *type )
{
	void *param;
	struct Object *object = self->object;
	int min, max, size;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					"expected integer argument" );

	/* these parameters require clamping */

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_OBJ_ATTR_COLBITS:
		min = 0;
		max = 0xffff;
		size = 'H';
		param = (void *)&object->colbits;
		break;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type in setIntAttrRange" );
	}

	self->object->recalc |= OB_RECALC_OB;  
	return EXPP_setIValueRange( value, param, min, max, size );
}

/*
 * get floating point attributes
 */

static PyObject *getFloatAttr( BPy_Object *self, void *type )
{
	float param;
	struct Object *object = self->object;

	if( GET_INT_FROM_POINTER(type) >= EXPP_OBJ_ATTR_PI_SURFACEDAMP &&
			GET_INT_FROM_POINTER(type) <= EXPP_OBJ_ATTR_PI_SBOFACETHICK ) {
    	if( !self->object->pd && !setupPI(self->object) )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );
	}
	else if( GET_INT_FROM_POINTER(type) >= EXPP_OBJ_ATTR_SB_NODEMASS &&
			GET_INT_FROM_POINTER(type) <= EXPP_OBJ_ATTR_SB_INFRICT ) {
		if( !self->object->soft && !setupSB(self->object) )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"softbody could not be accessed" );    
    }

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_OBJ_ATTR_LOC_X: 
		param = object->loc[0];
		break;
	case EXPP_OBJ_ATTR_LOC_Y: 
		param = object->loc[1];
		break;
	case EXPP_OBJ_ATTR_LOC_Z: 
		param = object->loc[2];
		break;
	case EXPP_OBJ_ATTR_DLOC_X: 
		param = object->dloc[0];
		break;
	case EXPP_OBJ_ATTR_DLOC_Y: 
		param = object->dloc[1];
		break;
	case EXPP_OBJ_ATTR_DLOC_Z: 
		param = object->dloc[2];
		break;
	case EXPP_OBJ_ATTR_ROT_X: 
		param = object->rot[0];
		break;
	case EXPP_OBJ_ATTR_ROT_Y: 
		param = object->rot[1];
		break;
	case EXPP_OBJ_ATTR_ROT_Z: 
		param = object->rot[2];
		break;
	case EXPP_OBJ_ATTR_DROT_X: 
		param = object->drot[0];
		break;
	case EXPP_OBJ_ATTR_DROT_Y: 
		param = object->drot[1];
		break;
	case EXPP_OBJ_ATTR_DROT_Z: 
		param = object->drot[2];
		break;
	case EXPP_OBJ_ATTR_SIZE_X: 
		param = object->size[0];
		break;
	case EXPP_OBJ_ATTR_SIZE_Y: 
		param = object->size[1];
		break;
	case EXPP_OBJ_ATTR_SIZE_Z: 
		param = object->size[2];
		break;
	case EXPP_OBJ_ATTR_DSIZE_X: 
		param = object->dsize[0];
		break;
	case EXPP_OBJ_ATTR_DSIZE_Y:
		param = object->dsize[1];
		break;
	case EXPP_OBJ_ATTR_DSIZE_Z:
		param = object->dsize[2];
		break;
	case EXPP_OBJ_ATTR_TIMEOFFSET:
		param = object->sf;
		break;
	case EXPP_OBJ_ATTR_DRAWSIZE:
		param = object->empty_drawsize;
		break;
	case EXPP_OBJ_ATTR_PI_SURFACEDAMP:
		param = object->pd->pdef_perm;
		break;
	case EXPP_OBJ_ATTR_PI_RANDOMDAMP:
		param = object->pd->pdef_rdamp;
		break;
	case EXPP_OBJ_ATTR_PI_PERM:
		param = object->pd->pdef_perm;
		break;
	case EXPP_OBJ_ATTR_PI_STRENGTH:
		param = object->pd->f_strength;
		break;
	case EXPP_OBJ_ATTR_PI_FALLOFF:
		param = object->pd->f_power;
		break;
	case EXPP_OBJ_ATTR_PI_MAXDIST:
		param = object->pd->maxdist;
		break;
	case EXPP_OBJ_ATTR_PI_SBDAMP:
		param = object->pd->pdef_sbdamp;
		break;
	case EXPP_OBJ_ATTR_PI_SBIFACETHICK:
		param = object->pd->pdef_sbift;
		break;
	case EXPP_OBJ_ATTR_PI_SBOFACETHICK:
		param = object->pd->pdef_sboft;
		break;
	case EXPP_OBJ_ATTR_SB_NODEMASS:
    	param = self->object->soft->nodemass;
		break;
	case EXPP_OBJ_ATTR_SB_GRAV:
    	param = self->object->soft->grav;
		break;
	case EXPP_OBJ_ATTR_SB_MEDIAFRICT:
    	param = self->object->soft->mediafrict;
		break;
	case EXPP_OBJ_ATTR_SB_RKLIMIT:
    	param = object->soft->rklimit;
		break;
	case EXPP_OBJ_ATTR_SB_PHYSICSSPEED:
    	param = object->soft->physics_speed;
		break;
	case EXPP_OBJ_ATTR_SB_GOALSPRING:
    	param = object->soft->goalspring;
		break;
	case EXPP_OBJ_ATTR_SB_GOALFRICT:
    	param = object->soft->goalfrict;
		break;
	case EXPP_OBJ_ATTR_SB_MINGOAL:
    	param = object->soft->mingoal;
		break;
	case EXPP_OBJ_ATTR_SB_MAXGOAL:
    	param = object->soft->maxgoal;
		break;
	case EXPP_OBJ_ATTR_SB_DEFGOAL:
    	param = object->soft->defgoal;
		break;
	case EXPP_OBJ_ATTR_SB_INSPRING:
    	param = object->soft->inspring;
		break;
	case EXPP_OBJ_ATTR_SB_INFRICT:
    	param = object->soft->infrict;
		break;
	case EXPP_OBJ_ATTR_DUPFACESCALEFAC:
		param = object->dupfacesca;
		break;
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError, 
				"undefined type in getFloatAttr" );
	}

	return PyFloat_FromDouble( param );
}

/*
 * set floating point attributes which require clamping
 */

static int setFloatAttrClamp( BPy_Object *self, PyObject *value, void *type )
{
	float *param;
	struct Object *object = self->object;
	float min, max;

	if( GET_INT_FROM_POINTER(type) >= EXPP_OBJ_ATTR_PI_SURFACEDAMP &&
			GET_INT_FROM_POINTER(type) <= EXPP_OBJ_ATTR_PI_SBOFACETHICK ) {
    	if( !self->object->pd && !setupPI(self->object) )
			return EXPP_ReturnIntError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );
	}
	else if( GET_INT_FROM_POINTER(type) >= EXPP_OBJ_ATTR_SB_NODEMASS &&
			GET_INT_FROM_POINTER(type) <= EXPP_OBJ_ATTR_SB_INFRICT ) {
		if( !self->object->soft && !setupSB(self->object) )
			return EXPP_ReturnIntError( PyExc_RuntimeError,
						"softbody could not be accessed" );    
    }

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_OBJ_ATTR_DRAWSIZE:
		min = EXPP_OBJECT_DRAWSIZEMIN;
		max = EXPP_OBJECT_DRAWSIZEMAX;
		param = &object->empty_drawsize;
		break;
	case EXPP_OBJ_ATTR_TIMEOFFSET:
		min = -MAXFRAMEF;
		max = MAXFRAMEF;
		param = &object->sf;
		break;
	case EXPP_OBJ_ATTR_PI_SURFACEDAMP:
		min = EXPP_OBJECT_PIDAMP_MIN;
		max = EXPP_OBJECT_PIDAMP_MAX;
		param = &object->pd->pdef_perm;
		break;
	case EXPP_OBJ_ATTR_PI_RANDOMDAMP:
		min = EXPP_OBJECT_PIRDAMP_MIN;
		max = EXPP_OBJECT_PIRDAMP_MAX;
		param = &object->pd->pdef_rdamp;
		break;
	case EXPP_OBJ_ATTR_PI_PERM:
		min = EXPP_OBJECT_PIPERM_MIN;
		max = EXPP_OBJECT_PIPERM_MAX;
		param = &object->pd->pdef_perm;
		break;
	case EXPP_OBJ_ATTR_PI_STRENGTH:
		min = EXPP_OBJECT_PISTRENGTH_MIN;
		max = EXPP_OBJECT_PISTRENGTH_MAX;
		param = &object->pd->f_strength;
		break;
	case EXPP_OBJ_ATTR_PI_FALLOFF:
		min = EXPP_OBJECT_PIPOWER_MIN;
		max = EXPP_OBJECT_PIPOWER_MAX;
		param = &object->pd->f_power;
		break;
	case EXPP_OBJ_ATTR_PI_MAXDIST:
		min = EXPP_OBJECT_PIMAXDIST_MIN;
		max = EXPP_OBJECT_PIMAXDIST_MAX;
		param = &object->pd->maxdist;
		break;
	case EXPP_OBJ_ATTR_PI_SBDAMP:
		min = EXPP_OBJECT_PISBDAMP_MIN;
		max = EXPP_OBJECT_PISBDAMP_MAX;
		param = &object->pd->pdef_sbdamp;
		break;
	case EXPP_OBJ_ATTR_PI_SBIFACETHICK:
		min = EXPP_OBJECT_PISBIFTMIN;
		max = EXPP_OBJECT_PISBIFTMAX;
		param = &object->pd->pdef_sbift;
		break;
	case EXPP_OBJ_ATTR_PI_SBOFACETHICK:
		min = EXPP_OBJECT_PISBOFTMIN;
		max = EXPP_OBJECT_PISBOFTMAX;
		param = &object->pd->pdef_sboft;
		break;
	case EXPP_OBJ_ATTR_SB_NODEMASS:
		min = EXPP_OBJECT_SBNODEMASSMIN;
		max = EXPP_OBJECT_SBNODEMASSMAX;
    	param = &self->object->soft->nodemass;
		break;
	case EXPP_OBJ_ATTR_SB_GRAV:
		min = EXPP_OBJECT_SBGRAVMIN;
		max = EXPP_OBJECT_SBGRAVMAX;
    	param = &self->object->soft->grav;
		break;
	case EXPP_OBJ_ATTR_SB_MEDIAFRICT:
		min = EXPP_OBJECT_SBMEDIAFRICTMIN;
		max = EXPP_OBJECT_SBMEDIAFRICTMAX;
    	param = &self->object->soft->mediafrict;
		break;
	case EXPP_OBJ_ATTR_SB_RKLIMIT:
		min = EXPP_OBJECT_SBRKLIMITMIN;
		max = EXPP_OBJECT_SBRKLIMITMAX;
    	param = &self->object->soft->rklimit;
		break;
	case EXPP_OBJ_ATTR_SB_PHYSICSSPEED:
		min = EXPP_OBJECT_SBPHYSICSSPEEDMIN;
		max = EXPP_OBJECT_SBPHYSICSSPEEDMAX;
    	param = &self->object->soft->physics_speed;
		break;
	case EXPP_OBJ_ATTR_SB_GOALSPRING:
		min = EXPP_OBJECT_SBGOALSPRINGMIN;
		max = EXPP_OBJECT_SBGOALSPRINGMAX;
    	param = &self->object->soft->goalspring;
		break;
	case EXPP_OBJ_ATTR_SB_GOALFRICT:
		min = EXPP_OBJECT_SBGOALFRICTMIN;
		max = EXPP_OBJECT_SBGOALFRICTMAX;
    	param = &self->object->soft->goalfrict;
		break;
	case EXPP_OBJ_ATTR_SB_MINGOAL:
		min = EXPP_OBJECT_SBMINGOALMIN;
		max = EXPP_OBJECT_SBMINGOALMAX;
    	param = &self->object->soft->mingoal;
		break;
	case EXPP_OBJ_ATTR_SB_MAXGOAL:
		min = EXPP_OBJECT_SBMAXGOALMIN;
		max = EXPP_OBJECT_SBMAXGOALMAX;
    	param = &self->object->soft->maxgoal;
		break;
	case EXPP_OBJ_ATTR_SB_DEFGOAL:
		min = EXPP_OBJECT_SBDEFGOALMIN;
		max = EXPP_OBJECT_SBDEFGOALMAX;
    	param = &self->object->soft->defgoal;
		break;
	case EXPP_OBJ_ATTR_SB_INSPRING:
		min = EXPP_OBJECT_SBINSPRINGMIN;
		max = EXPP_OBJECT_SBINSPRINGMAX;
    	param = &self->object->soft->inspring;
		break;
	case EXPP_OBJ_ATTR_SB_INFRICT:
		min = EXPP_OBJECT_SBINFRICTMIN;
		max = EXPP_OBJECT_SBINFRICTMAX;
    	param = &self->object->soft->infrict;
		break;
	case EXPP_OBJ_ATTR_DUPFACESCALEFAC:
		min = EXPP_OBJECT_DUPFACESCALEFACMIN;
		max = EXPP_OBJECT_DUPFACESCALEFACMAX;
		param = &self->object->dupfacesca;
		break;
		
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type in setFloatAttrClamp" );
	}

	self->object->recalc |= OB_RECALC_OB;  
	return EXPP_setFloatClamped( value, param, min, max );
}

/*
 * set floating point attributes
 */

static int setFloatAttr( BPy_Object *self, PyObject *value, void *type )
{
	float param;
	struct Object *object = self->object;

	if( !PyNumber_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					"expected float argument" );

	param = (float)PyFloat_AsDouble( value );

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_OBJ_ATTR_LOC_X: 
		object->loc[0] = param;
		break;
	case EXPP_OBJ_ATTR_LOC_Y: 
		object->loc[1] = param;
		break;
	case EXPP_OBJ_ATTR_LOC_Z: 
		object->loc[2] = param;
		break;
	case EXPP_OBJ_ATTR_DLOC_X: 
		object->dloc[0] = param;
		break;
	case EXPP_OBJ_ATTR_DLOC_Y: 
		object->dloc[1] = param;
		break;
	case EXPP_OBJ_ATTR_DLOC_Z: 
		object->dloc[2] = param;
		break;
	case EXPP_OBJ_ATTR_ROT_X: 
		object->rot[0] = param;
		break;
	case EXPP_OBJ_ATTR_ROT_Y: 
		object->rot[1] = param;
		break;
	case EXPP_OBJ_ATTR_ROT_Z: 
		object->rot[2] = param;
		break;
	case EXPP_OBJ_ATTR_DROT_X: 
		object->drot[0] = param;
		break;
	case EXPP_OBJ_ATTR_DROT_Y: 
		object->drot[1] = param;
		break;
	case EXPP_OBJ_ATTR_DROT_Z: 
		object->drot[2] = param;
		break;
	case EXPP_OBJ_ATTR_SIZE_X: 
		object->size[0] = param;
		break;
	case EXPP_OBJ_ATTR_SIZE_Y: 
		object->size[1] = param;
		break;
	case EXPP_OBJ_ATTR_SIZE_Z: 
		object->size[2] = param;
		break;
	case EXPP_OBJ_ATTR_DSIZE_X: 
		object->dsize[0] = param;
		break;
	case EXPP_OBJ_ATTR_DSIZE_Y: 
		object->dsize[1] = param;
		break;
	case EXPP_OBJ_ATTR_DSIZE_Z: 
		object->dsize[2] = param;
		break;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type in setFloatAttr	" );
	}
	self->object->recalc |= OB_RECALC_OB;  
	return 0;
}

/*
 * get 3-tuple floating point attributes
 */

static PyObject *getFloat3Attr( BPy_Object *self, void *type )
{
	float *param;
	struct Object *object = self->object;

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_OBJ_ATTR_LOC: 
		param = object->loc;
		break;
	case EXPP_OBJ_ATTR_DLOC: 
		param = object->dloc;
		break;
	case EXPP_OBJ_ATTR_DROT: 
		param = object->drot;
		break;
	case EXPP_OBJ_ATTR_SIZE: 
		param = object->size;
		break;
	case EXPP_OBJ_ATTR_DSIZE: 
		param = object->dsize;
		break;
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"undefined type in getFloat3Attr" );
	}

	return Py_BuildValue( "(fff)", param[0], param[1], param[2] );
}

/*
 * set 3-tuple floating point attributes
 */

static int setFloat3Attr( BPy_Object *self, PyObject *value, void *type )
{
	int i;
	float *dst, param[3];
	struct Object *object = self->object;

	value = PySequence_Tuple( value );

	if( !value || !PyArg_ParseTuple( value, "fff", &param[0], &param[1], &param[2] ) ) {
		Py_XDECREF( value );
		return EXPP_ReturnIntError( PyExc_TypeError,
					"expected a list or tuple of 3 floats" );
	}

	Py_DECREF( value );
	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_OBJ_ATTR_LOC: 
		dst = object->loc;
		break;
	case EXPP_OBJ_ATTR_DLOC: 
		dst = object->dloc;
		break;
	case EXPP_OBJ_ATTR_DROT: 
		dst = object->drot;
		break;
	case EXPP_OBJ_ATTR_SIZE: 
		dst = object->size;
		break;
	case EXPP_OBJ_ATTR_DSIZE: 
		dst = object->dsize;
		break;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type in setFloat3Attr" );
	}

	for( i = 0; i < 3; ++i )
		dst[i] = param[i];

	self->object->recalc |= OB_RECALC_OB;  
	return 0;
}

/*****************************************************************************/
/* BPy_Object methods and attribute handlers                                 */
/*****************************************************************************/

static PyObject *Object_getShapeFlag( BPy_Object *self, void *type )
{
	if (self->object->shapeflag & GET_INT_FROM_POINTER(type))
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static int Object_setShapeFlag( BPy_Object *self, PyObject *value,
		void *type )
{
	if (PyObject_IsTrue(value) )
		self->object->shapeflag |= GET_INT_FROM_POINTER(type);
	else
		self->object->shapeflag &= ~GET_INT_FROM_POINTER(type);
	
	self->object->recalc |= OB_RECALC_OB;
	return 0;
}

static PyObject *Object_getRestricted( BPy_Object *self, void *type )
{
	if (self->object->restrictflag & GET_INT_FROM_POINTER(type))
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static int Object_setRestricted( BPy_Object *self, PyObject *value,
		void *type )
{
	int param = PyObject_IsTrue( value );
	if( param == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	if ( param )
		self->object->restrictflag |= GET_INT_FROM_POINTER(type);
	else
		self->object->restrictflag &= ~GET_INT_FROM_POINTER(type);
	
	return 0;
}

static PyObject *Object_getDrawModeBits( BPy_Object *self, void *type )
{
	return EXPP_getBitfield( (void *)&self->object->dtx, GET_INT_FROM_POINTER(type), 'b' );
}

static int Object_setDrawModeBits( BPy_Object *self, PyObject *value,
		void *type )
{
	self->object->recalc |= OB_RECALC_OB;  
	return EXPP_setBitfield( value, (void *)&self->object->dtx,
			GET_INT_FROM_POINTER(type), 'b' );
}

static PyObject *Object_getTransflagBits( BPy_Object *self, void *type )
{
	return EXPP_getBitfield( (void *)&self->object->transflag,
			GET_INT_FROM_POINTER(type), 'h' );
}

static int Object_setTransflagBits( BPy_Object *self, PyObject *value,
		void *type )
{
	self->object->recalc |= OB_RECALC_OB;  
	return EXPP_setBitfield( value, (void *)&self->object->transflag,
			GET_INT_FROM_POINTER(type), 'h' );
}

static PyObject *Object_getLayers( BPy_Object * self )
{
	int layers, bit;
	PyObject *laylist = PyList_New( 0 );

	if( !laylist )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
				"PyList_New() failed" );

	layers = self->object->lay & 0xfffff;		/* get layer bitmask */

	/*
	 * starting with the first layer, and until there are no more layers,
	 * find which layers are visible
	 */

	for( bit = 1; layers; ++bit ) {
		if( layers & 1 ) {	/* if layer is visible, add to list */
			PyObject *item = PyInt_FromLong( bit );
			PyList_Append( laylist, item );
			Py_DECREF( item );
		}
		layers >>= 1;		/* go to the next layer */
	}
	return laylist;
}

/*
 * usage note: caller of this func needs to do a Blender.Redraw(-1)
 * to update and redraw the interface
 */

static int Object_setLayers( BPy_Object * self, PyObject *value )
{
	int layers = 0, val, i, len_list, local;
	Base *base;

	if( !PyList_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected a list of integers in the range [1, 20]" );

	len_list = PyList_Size( value );

	/* build a bitmask, check for values outside of range */

	for( i = 0; i < len_list; i++ ) {
		PyObject* integer = PyNumber_Int( PyList_GetItem( value, i ) );
		val = PyInt_AsLong( integer );
		Py_XDECREF( integer );
		if( !integer )
			return EXPP_ReturnIntError( PyExc_TypeError,
				  "list must contain only integer numbers" );
		if( val < 1 || val > 20 )
			return EXPP_ReturnIntError ( PyExc_ValueError,
				  "layer values must be in the range [1, 20]" );
		layers |= 1 << ( val - 1 );
	}

	/* do this, to ensure layers are set for objects not in current scene */
	self->object->lay= layers;
	
	/* update any bases pointing to our object */
	base = FIRSTBASE;  /* first base in current scene */
	while( base ) {
		if( base->object == self->object ) {
			base->lay &= 0xFFF00000;
			local = base->lay;
			base->lay = local | layers;
			self->object->lay = base->lay;
			break;
		}
		base = base->next;
	}
	
	/* these to calls here are overkill! (ton) */
	if (base) { /* The object was found? */
		countall();
		DAG_scene_sort( G.scene );
	}
	return 0;
}

static int Object_setLayersMask( BPy_Object *self, PyObject *value )
{
	int layers = 0, local;
	Base *base;

	if( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected an integer (bitmask) as argument" );

	layers = PyInt_AS_LONG( value );

	/* make sure some bits are set, and only those bits are set */

	if( !( layers & 0xFFFFF ) || ( layers & 0xFFF00000 ) )
		return EXPP_ReturnIntError( PyExc_ValueError,
			"bitmask must have between 1 and 20 bits set" );

	/* update any bases pointing to our object */

	base = FIRSTBASE;  /* first base in current scene */
	while( base ) {
		if( base->object == self->object ) {
			base->lay &= 0xFFF00000;
			local = base->lay;
			base->lay = local | layers;
			self->object->lay = base->lay;
			break;
		}
		base = base->next;
	}
	if (base) { /* The object was found? */
		countall();
		DAG_scene_sort( G.scene );
	}
	return 0;
}

/*
 * this should accept a Py_None argument and just delete the Ipo link
 * (as Object_clearIpo() does)
 */

static int Object_setIpo( BPy_Object * self, PyObject * value )
{
	return GenericLib_assignData(value, (void **) &self->object->ipo, 0, 1, ID_IP, ID_OB);
}

static int Object_setTracked( BPy_Object * self, PyObject * value )
{
	int ret;
	ret = GenericLib_assignData(value, (void **) &self->object->track, 0, 0, ID_OB, 0);
	if (ret==0) {
		self->object->recalc |= OB_RECALC_OB;  
		DAG_scene_sort( G.scene );
	}
	return ret;
}

/* Localspace matrix */

static PyObject *Object_getMatrixLocal( BPy_Object * self )
{
	if( self->object->parent ) {
		float matrix[4][4]; /* for the result */
		float invmat[4][4]; /* for inverse of parent's matrix */
  	 
		Mat4Invert(invmat, self->object->parent->obmat );
		Mat4MulMat4(matrix, self->object->obmat, invmat);
		return newMatrixObject((float*)matrix,4,4,Py_NEW);
	} else { /* no parent, so return world space matrix */
		disable_where_script( 1 );
		where_is_object( self->object );
		disable_where_script( 0 );
		return newMatrixObject((float*)self->object->obmat,4,4,Py_WRAP);
	}
}

/* Worldspace matrix */

static PyObject *Object_getMatrixWorld( BPy_Object * self )
{
	disable_where_script( 1 );
	where_is_object( self->object );
	disable_where_script( 0 );
	return newMatrixObject((float*)self->object->obmat,4,4,Py_WRAP);
}

/* Parent Inverse matrix */

static PyObject *Object_getMatrixParentInverse( BPy_Object * self )
{
	return newMatrixObject((float*)self->object->parentinv,4,4,Py_WRAP);
}

/*
 * Old behavior, prior to Blender 2.34, where eventual changes made by the
 * script itself were not taken into account until a redraw happened, either
 * called by the script or upon its exit.
 */

static PyObject *Object_getMatrixOldWorld( BPy_Object * self )
{
	return newMatrixObject((float*)self->object->obmat,4,4,Py_WRAP);
}

/*
 * get one of three different matrix representations
 */

static PyObject *Object_getMatrix( BPy_Object * self, PyObject * args )
{
	char *space = "worldspace";	/* default to world */
	char *errstr = "expected nothing, 'worldspace' (default), 'localspace' or 'old_worldspace'";

	if( !PyArg_ParseTuple( args, "|s", &space ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError, errstr );

	if( BLI_streq( space, "worldspace" ) )
		return Object_getMatrixWorld( self );
	else if( BLI_streq( space, "localspace" ) )
		return Object_getMatrixLocal( self );
	else if( BLI_streq( space, "old_worldspace" ) )
		return Object_getMatrixOldWorld( self );
	else
		return EXPP_ReturnPyObjError( PyExc_ValueError, errstr );
}

static PyObject *get_obj_data( BPy_Object *self, int mesh )
{
	Object *object = self->object;
	PyObject *data_object = NULL;

	switch ( object->type ) {
	case OB_ARMATURE:
		data_object = Armature_CreatePyObject( object->data );
		break;
	case OB_CAMERA:
		data_object = Camera_CreatePyObject( object->data );
		break;
	case OB_CURVE:
	case OB_SURF:
		data_object = Curve_CreatePyObject( object->data );
		break;
	case ID_IM:
		data_object = Image_CreatePyObject( object->data );
		break;
	case ID_IP:
		data_object = Ipo_CreatePyObject( object->data );
		break;
	case OB_LAMP:
		data_object = Lamp_CreatePyObject( object->data );
		break;
	case OB_LATTICE:
		data_object = Lattice_CreatePyObject( object->data );
		break;
	case ID_MA:
		break;
	case OB_MESH:
		if( !mesh ) /* get as NMesh (default) */
			data_object = NMesh_CreatePyObject( object->data, object );
		else		/* else get as Mesh */
			data_object = Mesh_CreatePyObject( object->data, object );
		break;
	case OB_MBALL:
		data_object = Metaball_CreatePyObject( object->data );
		break;
	case ID_OB:
		data_object = Object_CreatePyObject( object->data );
		break;
	case ID_SCE:
		break;
	case OB_FONT:
		data_object = Text3d_CreatePyObject( object->data );
		break;		
	case ID_WO:
		break;
	default:
		break;
	}

	if( data_object )
		return data_object;

	Py_RETURN_NONE;
}

static PyObject *Object_getData( BPy_Object *self, PyObject *args,
		PyObject *kwd )
{
	Object *object = self->object;
	int name_only = 0;
	int mesh = 0;       /* default mesh type = NMesh */
	static char *kwlist[] = {"name_only", "mesh", NULL};

	if( !PyArg_ParseTupleAndKeywords(args, kwd, "|ii", kwlist,
				&name_only, &mesh) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected nothing or bool keywords 'name_only' or 'mesh' as argument" );

	/* if there's no obdata, try to create it */
	if( object->data == NULL ) {
		int tmptype = object->type;			/* save current type */

			/* if we have no data and are faking an empty, set the type */
		if( self->realtype != OB_EMPTY )	
			object->type = self->realtype;

		if( EXPP_add_obdata( object ) != 0 ) {	/* couldn't create obdata */
			object->type = tmptype;			/* restore previous type */
			Py_RETURN_NONE;
		}

			/* if we set data successfully, clear the fake type */
		self->realtype = OB_EMPTY;
	}

	/* user wants only the name of the data object */
	if( name_only ) {
		ID *id = object->data;
		return PyString_FromString( id->name+2 );
	}

	return get_obj_data( self, mesh );
}

static PyObject *Object_getEuler( BPy_Object * self )
{
	return ( PyObject * ) newEulerObject( self->object->rot, Py_WRAP );
}

#define PROTFLAGS_MASK ( OB_LOCK_LOCX | OB_LOCK_LOCY | OB_LOCK_LOCZ | \
		OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | \
		OB_LOCK_SCALEX | OB_LOCK_SCALEY | OB_LOCK_SCALEZ )

static PyObject *Object_getProtectFlags( BPy_Object * self )
{
	return PyInt_FromLong( (long)(self->object->protectflag & PROTFLAGS_MASK) );
}

static int Object_setProtectFlags( BPy_Object * self, PyObject * args )
{
	PyObject* integer = PyNumber_Int( args );
	short value;

	if( !integer )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected integer argument" );

	value = ( short )PyInt_AS_LONG( integer );
	Py_DECREF( integer );
	if( value & ~PROTFLAGS_MASK )
		return EXPP_ReturnIntError( PyExc_ValueError,
				"undefined bit(s) set in bitfield" );

	self->object->protectflag = value;
	self->object->recalc |= OB_RECALC_OB;  
	return 0;
}

static PyObject *Object_getRBRadius( BPy_Object * self )
{
    return PyFloat_FromDouble( (double) self->object->inertia );
}

static int Object_setRBRadius( BPy_Object * self, PyObject * args )
{
    float value;
    PyObject* flt = PyNumber_Float( args );

    if( !flt )
        return EXPP_ReturnIntError( PyExc_TypeError,
                "expected float argument" );
    value = (float)PyFloat_AS_DOUBLE( flt );
	Py_DECREF( flt );

    if( value < 0.0f )
        return EXPP_ReturnIntError( PyExc_ValueError,
            "acceptable values are non-negative, 0.0 or more" );

    self->object->inertia = value;
	self->object->recalc |= OB_RECALC_OB;  

    return 0;
}

static PyObject *Object_getRBHalfExtents( BPy_Object * self )
{
	float center[3], extents[3];

	get_local_bounds( self->object, center, extents );
	return Py_BuildValue( "[fff]", extents[0], extents[1], extents[2] );
}

static PyGetSetDef BPy_Object_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"LocX",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The X location coordinate of the object",
	 (void *)EXPP_OBJ_ATTR_LOC_X},
	{"LocY",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The Y location coordinate of the object",
	 (void *)EXPP_OBJ_ATTR_LOC_Y},
	{"LocZ",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The Z location coordinate of the object",
	 (void *)EXPP_OBJ_ATTR_LOC_Z},
	{"dLocX",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The delta X location coordinate of the object",
	 (void *)EXPP_OBJ_ATTR_DLOC_X},
	{"dLocY",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The delta Y location coordinate of the object",
	 (void *)EXPP_OBJ_ATTR_DLOC_Y},
	{"dLocZ",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The delta Z location coordinate of the object",
	 (void *)EXPP_OBJ_ATTR_DLOC_Z},
	{"RotX",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The X rotation angle (in radians) of the object",
	 (void *)EXPP_OBJ_ATTR_ROT_X},
	{"RotY",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The Y rotation angle (in radians) of the object",
	 (void *)EXPP_OBJ_ATTR_ROT_Y},
	{"RotZ",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The Z rotation angle (in radians) of the object",
	 (void *)EXPP_OBJ_ATTR_ROT_Z},
	{"dRotX",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The delta X rotation angle (in radians) of the object",
	 (void *)EXPP_OBJ_ATTR_DROT_X},
	{"dRotY",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The delta Y rotation angle (in radians) of the object",
	 (void *)EXPP_OBJ_ATTR_DROT_Y},
	{"dRotZ",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The delta Z rotation angle (in radians) of the object",
	 (void *)EXPP_OBJ_ATTR_DROT_Z},
	{"SizeX",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The X size of the object",
	 (void *)EXPP_OBJ_ATTR_SIZE_X},
	{"SizeY",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The Y size of the object",
	 (void *)EXPP_OBJ_ATTR_SIZE_Y},
	{"SizeZ",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The Z size of the object",
	 (void *)EXPP_OBJ_ATTR_SIZE_Z},
	{"dSizeX",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The delta X size of the object",
	 (void *)EXPP_OBJ_ATTR_DSIZE_X},
	{"dSizeY",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The delta Y size of the object",
	 (void *)EXPP_OBJ_ATTR_DSIZE_Y},
	{"dSizeZ",
	 (getter)getFloatAttr, (setter)setFloatAttr,
	 "The delta Z size of the object",
	 (void *)EXPP_OBJ_ATTR_DSIZE_Z},

	{"loc",
	 (getter)getFloat3Attr, (setter)setFloat3Attr,
	 "The (X,Y,Z) location coordinates of the object",
	 (void *)EXPP_OBJ_ATTR_LOC},
	{"dloc",
	 (getter)getFloat3Attr, (setter)setFloat3Attr,
	 "The delta (X,Y,Z) location coordinates of the object",
	 (void *)EXPP_OBJ_ATTR_DLOC},
	{"rot",
	 (getter)Object_getEuler, (setter)Object_setEuler,
	 "The (X,Y,Z) rotation angles (in degrees) of the object",
	 NULL},
	{"drot",
	 (getter)getFloat3Attr, (setter)setFloat3Attr,
	 "The delta (X,Y,Z) rotation angles (in radians) of the object",
	 (void *)EXPP_OBJ_ATTR_DROT},
	{"size",
	 (getter)getFloat3Attr, (setter)setFloat3Attr,
	 "The (X,Y,Z) size of the object",
	 (void *)EXPP_OBJ_ATTR_SIZE},
	{"dsize",
	 (getter)getFloat3Attr, (setter)setFloat3Attr,
	 "The delta (X,Y,Z) size of the object",
	 (void *)EXPP_OBJ_ATTR_DSIZE},
	{"Layer",
	 (getter)getIntAttr, (setter)Object_setLayersMask,
	 "The object layers (bitfield)",
	 (void *)EXPP_OBJ_ATTR_LAYERMASK},
	{"Layers",
	 (getter)getIntAttr, (setter)Object_setLayersMask,
	 "The object layers (bitfield)",
	 (void *)EXPP_OBJ_ATTR_LAYERMASK},
	{"layers",
	 (getter)Object_getLayers, (setter)Object_setLayers,
	 "The object layers (list of ints)",
	 NULL},
	{"ipo",
	 (getter)Object_getIpo, (setter)Object_setIpo,
	 "Object's Ipo data",
	 NULL},
	{"colbits",
	 (getter)getIntAttr, (setter)setIntAttrRange,
	 "The Material usage bitfield",
	 (void *)EXPP_OBJ_ATTR_COLBITS},
	{"drawMode",
	 (getter)getIntAttr, (setter)Object_setDrawMode,
	 "The object's drawing mode bitfield",
	 (void *)EXPP_OBJ_ATTR_DRAWMODE},
	{"drawType",
	 (getter)getIntAttr, (setter)Object_setDrawType,
	 "The object's drawing type",
	 (void *)EXPP_OBJ_ATTR_DRAWTYPE},
	{"parentType",
	 (getter)getIntAttr, (setter)NULL,
	 "The object's parent type",
	 (void *)EXPP_OBJ_ATTR_PARENT_TYPE},
	{"DupOn",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "DupOn setting (for DupliFrames)",
	 (void *)EXPP_OBJ_ATTR_DUPON},
	{"DupOff",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "DupOff setting (for DupliFrames)",
	 (void *)EXPP_OBJ_ATTR_DUPOFF},
	{"DupSta",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "Starting frame (for DupliFrames)",
	 (void *)EXPP_OBJ_ATTR_DUPSTA},
	{"DupEnd",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "Ending frame (for DupliFrames)",
	 (void *)EXPP_OBJ_ATTR_DUPEND},
	{"passIndex",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "Index for object masks in the compositor",
	 (void *)EXPP_OBJ_ATTR_PASSINDEX},
	{"activeMaterial",
	 (getter)getIntAttr, (setter)setIntAttrClamp,
	 "Index for the active material (displayed in the material panel)",
	 (void *)EXPP_OBJ_ATTR_ACT_MATERIAL},
	{"mat",
	 (getter)Object_getMatrixWorld, (setter)NULL,
	 "worldspace matrix: absolute, takes vertex parents, tracking and Ipos into account",
	 NULL},
	{"matrix",
	 (getter)Object_getMatrixWorld, (setter)NULL,
	 "worldspace matrix: absolute, takes vertex parents, tracking and Ipos into account",
	 NULL},
	{"matrixWorld",
	 (getter)Object_getMatrixWorld, (setter)NULL,
	 "worldspace matrix: absolute, takes vertex parents, tracking and Ipos into account",
	 NULL},
	{"matrixLocal",
	 (getter)Object_getMatrixLocal, (setter)Object_setMatrix,
	 "localspace matrix: relative to the object's parent",
	 NULL},
	{"matrixParentInverse",
	 (getter)Object_getMatrixParentInverse, (setter)NULL,
	 "parents inverse matrix: parents localspace inverted matrix",
	 NULL},
	{"matrixOldWorld",
	 (getter)Object_getMatrixOldWorld, (setter)NULL,
	 "old-type worldspace matrix (prior to Blender 2.34)",
	 NULL},
	{"data",
	 (getter)get_obj_data, (setter)NULL,
	 "The Datablock object linked to this object",
	 NULL},
	{"sel",
	 (getter)Object_getSelected, (setter)Object_setSelect,
	 "The object's selection state",
	 NULL},
	{"parent",
	 (getter)Object_getParent, (setter)NULL,
	 "The object's parent object (if parented)",
	 NULL},
	{"parentbonename",
	 (getter)Object_getParentBoneName, (setter)Object_setParentBoneName,
	 "The object's parent object's sub name",
	 NULL},
	{"parentVertexIndex",
	 (getter)Object_getParentVertexIndex, (setter)Object_setParentVertexIndex,
	 "Indicies used for vertex parents",
	 NULL},
	{"track",
	 (getter)Object_getTracked, (setter)Object_setTracked,
	 "The object's tracked object",
	 NULL},
	{"timeOffset",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "The time offset of the object's animation",
	 (void *)EXPP_OBJ_ATTR_TIMEOFFSET},
	{"type",
	 (getter)Object_getType, (setter)NULL,
	 "The object's type",
	 NULL},
	{"boundingBox",
	 (getter)Object_getBoundBox_noargs, (setter)NULL,
	 "The bounding box of this object",
	 NULL},
	{"action",
	 (getter)Object_getAction, (setter)Object_setAction,
	 "The action associated with this object (if defined)",
	 NULL},
	{"game_properties",
	 (getter)Object_getAllProperties, (setter)NULL,
	 "The object's properties",
	 NULL},
	 
	{"piFalloff",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "The particle interaction falloff power",
	 (void *)EXPP_OBJ_ATTR_PI_FALLOFF},
	{"piMaxDist",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Max distance for the particle interaction field to work",
	 (void *)EXPP_OBJ_ATTR_PI_MAXDIST},
	{"piPermeability",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Probability that a particle will pass through the mesh",
	 (void *)EXPP_OBJ_ATTR_PI_PERM},
	{"piRandomDamp",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Random variation of particle interaction damping",
	 (void *)EXPP_OBJ_ATTR_PI_RANDOMDAMP},
	{"piStrength",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Particle interaction force field strength",
	 (void *)EXPP_OBJ_ATTR_PI_STRENGTH},
	{"piSurfaceDamp",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Amount of damping during particle collision",
	 (void *)EXPP_OBJ_ATTR_PI_SURFACEDAMP},
	{"piSoftbodyDamp",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Damping factor for softbody deflection",
	 (void *)EXPP_OBJ_ATTR_PI_SBDAMP},
	{"piSoftbodyIThick",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Inner face thickness for softbody deflection",
	 (void *)EXPP_OBJ_ATTR_PI_SBIFACETHICK},
	{"piSoftbodyOThick",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Outer face thickness for softbody deflection",
	 (void *)EXPP_OBJ_ATTR_PI_SBOFACETHICK},

	{"piDeflection",
	 (getter)Object_getPIDeflection, (setter)Object_setPIDeflection,
	 "Deflects particles based on collision",
	 NULL},
	{"piType",
	 (getter)Object_getPIType, (setter)Object_setPIType,
	 "Type of particle interaction (force field, wind, etc)",
	 NULL},
	{"piUseMaxDist",
	 (getter)Object_getPIUseMaxDist, (setter)Object_setPIUseMaxDist,
	 "Use a maximum distance for the field to work",
	 NULL},

	{"sbMass",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Softbody point mass (heavier is slower)",
	 (void *)EXPP_OBJ_ATTR_SB_NODEMASS}, 
	{"sbGrav",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Apply gravitation to softbody point movement",
	 (void *)EXPP_OBJ_ATTR_SB_GRAV},
	{"sbFriction",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "General media friction for softbody point movements",
	 (void *)EXPP_OBJ_ATTR_SB_MEDIAFRICT}, 
	{"sbSpeed",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Tweak timing for physics to control softbody frequency and speed",
	 (void *)EXPP_OBJ_ATTR_SB_MEDIAFRICT}, 
	{"sbErrorLimit",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Softbody Runge-Kutta ODE solver error limit (low values give more precision)", 
	 (void *)EXPP_OBJ_ATTR_SB_RKLIMIT}, 
	{"sbGoalSpring",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Softbody goal (vertex target position) spring stiffness",
	 (void *)EXPP_OBJ_ATTR_SB_GOALSPRING},
	{"sbGoalFriction",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Softbody goal (vertex target position) friction",
	 (void *)EXPP_OBJ_ATTR_SB_GOALFRICT},
	{"sbMinGoal",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Softbody goal minimum (vertex group weights scaled to match this range)", 
	 (void *)EXPP_OBJ_ATTR_SB_MINGOAL},
	{"sbMaxGoal",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Softbody goal maximum (vertex group weights scaled to match this range)", 
	 (void *)EXPP_OBJ_ATTR_SB_MAXGOAL},
	{"sbDefaultGoal",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Default softbody goal value, when no vertex group used", 		 
	 (void *)EXPP_OBJ_ATTR_SB_DEFGOAL},
	{"sbInnerSpring",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Softbody edge spring stiffness",
	 (void *)EXPP_OBJ_ATTR_SB_INSPRING},
	{"sbInnerSpringFrict",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "Softbody edge spring friction", 	
	 (void *)EXPP_OBJ_ATTR_SB_INFRICT},
	{"isSoftBody",
	 (getter)Object_isSB, (setter)NULL,
	 "True if object is a soft body",
	 NULL},
	{"sbUseGoal",
	 (getter)Object_getSBUseGoal, (setter)Object_setSBUseGoal,
	 "Softbody forces for vertices to stick to animated position enabled", 
	 NULL},
	{"sbUseEdges",
	 (getter)Object_getSBUseEdges, (setter)Object_setSBUseEdges,
	 "Softbody use edges as springs enabled", 
	 NULL},
	{"sbStiffQuads",
	 (getter)Object_getSBStiffQuads, (setter)Object_setSBStiffQuads,
	 "Softbody adds diagonal springs on 4-gons enabled",
	 NULL},

	{"axis",
	 (getter)Object_getDrawModeBits, (setter)Object_setDrawModeBits,
	 "Display of active object's center and axis enabled",
	 (void *)OB_AXIS},
	{"texSpace",
	 (getter)Object_getDrawModeBits, (setter)Object_setDrawModeBits,
	 "Display of active object's texture space enabled",
	 (void *)OB_TEXSPACE},
	{"nameMode",
	 (getter)Object_getDrawModeBits, (setter)Object_setDrawModeBits,
	 "Display of active object's name enabled",
	 (void *)OB_DRAWNAME},
	{"wireMode",
	 (getter)Object_getDrawModeBits, (setter)Object_setDrawModeBits,
	 "Add the active object's wireframe over solid drawing enabled",
	 (void *)OB_DRAWWIRE},
	{"xRay",
	 (getter)Object_getDrawModeBits, (setter)Object_setDrawModeBits,
	 "Draw the active object in front of others enabled",
	 (void *)OB_DRAWXRAY},
	{"transp",
	 (getter)Object_getDrawModeBits, (setter)Object_setDrawModeBits,
	 "Transparent materials for the active object (mesh only) enabled",
	 (void *)OB_DRAWTRANSP},

	{"enableNLAOverride",
	 (getter)Object_getNLAflagBits, (setter)Object_setNLAflagBits,
	 "Toggles Action-NLA based animation",
	 (void *)OB_NLA_OVERRIDE},

	{"enableDupVerts",
	 (getter)Object_getTransflagBits, (setter)Object_setTransflagBits,
	 "Duplicate child objects on all vertices",
	 (void *)OB_DUPLIVERTS},
	{"enableDupFaces",
	 (getter)Object_getTransflagBits, (setter)Object_setTransflagBits,
	 "Duplicate child objects on all faces",
	 (void *)OB_DUPLIFACES},
	{"enableDupFacesScale",
	 (getter)Object_getTransflagBits, (setter)Object_setTransflagBits,
	 "Use face scale to scale all dupliFaces",
	 (void *)OB_DUPLIFACES_SCALE},
	{"dupFacesScaleFac",
 	 (getter)getFloatAttr, (setter)setFloatAttr,
	"Use face scale to scale all dupliFaces",
	 (void *)EXPP_OBJ_ATTR_DUPFACESCALEFAC},
	{"enableDupFrames",
	 (getter)Object_getTransflagBits, (setter)Object_setTransflagBits,
	 "Make copy of object for every frame",
	 (void *)OB_DUPLIFRAMES},
	{"enableDupGroup",
	 (getter)Object_getTransflagBits, (setter)Object_setTransflagBits,
	 "Enable group instancing",
	 (void *)OB_DUPLIGROUP},
	{"enableDupRot",
	 (getter)Object_getTransflagBits, (setter)Object_setTransflagBits,
	 "Rotate dupli according to vertex normal",
	 (void *)OB_DUPLIROT},
	{"enableDupNoSpeed",
	 (getter)Object_getTransflagBits, (setter)Object_setTransflagBits,
	 "Set dupliframes to still, regardless of frame",
	 (void *)OB_DUPLINOSPEED},
	{"DupObjects",
	 (getter)Object_getDupliObjects, (setter)NULL,
	 "Get a list of tuple pairs (object, matrix), for getting dupli objects",
	 NULL},
	{"DupGroup",
	 (getter)Object_getDupliGroup, (setter)Object_setDupliGroup,
	 "Get a list of tuples for object duplicated by dupliframe",
	 NULL},

	{"effects",
	 (getter)Object_getEffects, (setter)NULL, 
	 "The list of particle effects associated with the object, (depricated, will always return an empty list in version 2.46)",
	 NULL},
	{"actionStrips",
	 (getter)Object_getActionStrips, (setter)NULL, 
	 "The action strips associated with the object",
	 NULL},
	{"constraints",
	 (getter)Object_getConstraints, (setter)NULL, 
	 "The constraints associated with the object",
	 NULL},
	{"modifiers",
	 (getter)Object_getModifiers, (setter)Object_setModifiers, 
	 "The modifiers associated with the object",
	 NULL},
	{"protectFlags",
	 (getter)Object_getProtectFlags, (setter)Object_setProtectFlags, 
	 "The \"transform locking\" bitfield for the object",
	 NULL},
	{"drawSize",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp, 
	 "The size to display the Empty",
	 (void *)EXPP_OBJ_ATTR_DRAWSIZE},

	{"rbFlags",
	 (getter)Object_getRBFlags, (setter)Object_setRBFlags, 
	 "Rigid body flags",
	 NULL},
	{"rbMass",
	 (getter)Object_getRBMass, (setter)Object_setRBMass, 
	 "Rigid body object mass",
	 NULL},
	{"rbRadius",
	 (getter)Object_getRBRadius, (setter)Object_setRBRadius, 
	 "Rigid body bounding sphere size",
	 NULL},
	{"rbShapeBoundType",
	 (getter)Object_getRBShapeBoundType, (setter)Object_setRBShapeBoundType, 
	 "Rigid body physics bounds object type",
	 NULL},
	{"rbHalfExtents",
	 (getter)Object_getRBHalfExtents, (setter)NULL, 
	 "Rigid body physics bounds object type",
	 NULL},
	{"trackAxis",
	 (getter)Object_trackAxis, (setter)NULL, 
	 "track axis 'x' | 'y' | 'z' | '-x' | '-y' | '-z' (string. readonly)",
	 NULL},
 	{"upAxis",
	 (getter)Object_upAxis, (setter)NULL, 
	 "up axis 'x' | 'y' | 'z' (string. readonly)",
	 NULL},
	{"restrictDisplay",
	 (getter)Object_getRestricted, (setter)Object_setRestricted, 
	 "Toggle object restrictions",
	 (void *)OB_RESTRICT_VIEW},
	{"restrictSelect",
	 (getter)Object_getRestricted, (setter)Object_setRestricted, 
	 "Toggle object restrictions",
	 (void *)OB_RESTRICT_SELECT},
	{"restrictRender",
	 (getter)Object_getRestricted, (setter)Object_setRestricted, 
	 "Toggle object restrictions",
	 (void *)OB_RESTRICT_RENDER},

	{"pinShape",
	 (getter)Object_getShapeFlag, (setter)Object_setShapeFlag, 
	 "Set the state for pinning this object",
	 (void *)OB_SHAPE_LOCK},
	{"activeShape",
	 (getter)getIntAttr, (setter)setIntAttrClamp, 
	 "set the index for the active shape key",
	 (void *)EXPP_OBJ_ATTR_ACT_SHAPE},
	 
	 
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Object_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject Object_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Object",           /* char *tp_name; */
	sizeof( BPy_Object ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) Object_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) Object_compare, /* cmpfunc tp_compare; */
	( reprfunc ) Object_repr,   /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) GenericLib_hash,	/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_Object_methods,         /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Object_getseters,       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};


static PyObject *M_Object_DrawModesDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "AXIS", PyInt_FromLong( OB_AXIS ) );
		PyConstant_Insert( d, "TEXSPACE", PyInt_FromLong( OB_TEXSPACE ) );
		PyConstant_Insert( d, "NAME", PyInt_FromLong( OB_DRAWNAME ) );
		PyConstant_Insert( d, "WIRE", PyInt_FromLong( OB_DRAWWIRE ) );
		PyConstant_Insert( d, "XRAY", PyInt_FromLong( OB_DRAWXRAY ) );
		PyConstant_Insert( d, "TRANSP", PyInt_FromLong( OB_DRAWTRANSP ) );
	}
	return M;
}

static PyObject *M_Object_DrawTypesDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "BOUNDBOX", PyInt_FromLong( OB_BOUNDBOX ) );
		PyConstant_Insert( d, "WIRE", PyInt_FromLong( OB_WIRE ) );
		PyConstant_Insert( d, "SOLID", PyInt_FromLong( OB_SOLID ) );
		PyConstant_Insert( d, "SHADED", PyInt_FromLong( OB_SHADED ) );
	}
	return M;
}

static PyObject *M_Object_ParentTypesDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "OBJECT", PyInt_FromLong( PAROBJECT ) );
		PyConstant_Insert( d, "CURVE", PyInt_FromLong( PARCURVE ) );
		
		/*	2.43 was released as LATTICE as PARKEY, my bad,
			lattice uses PARSKEL also - Campbell */
		PyConstant_Insert( d, "LATTICE", PyInt_FromLong( PARSKEL ) ); 
		
		PyConstant_Insert( d, "ARMATURE", PyInt_FromLong( PARSKEL ) );
		PyConstant_Insert( d, "VERT1", PyInt_FromLong( PARVERT1 ) );
		PyConstant_Insert( d, "VERT3", PyInt_FromLong( PARVERT3 ) );
		PyConstant_Insert( d, "BONE", PyInt_FromLong( PARBONE ) );
	}
	return M;
}

static PyObject *M_Object_PITypesDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "NONE", PyInt_FromLong( 0 ) );
		PyConstant_Insert( d, "FORCE", PyInt_FromLong( PFIELD_FORCE ) );
		PyConstant_Insert( d, "VORTEX", PyInt_FromLong( PFIELD_VORTEX ) );
		PyConstant_Insert( d, "WIND", PyInt_FromLong( PFIELD_WIND ) );
		PyConstant_Insert( d, "GUIDE", PyInt_FromLong( PFIELD_GUIDE ) );
	}
	return M;
}

static PyObject *M_Object_ProtectDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "LOCX", PyInt_FromLong( OB_LOCK_LOCX ) );
		PyConstant_Insert( d, "LOCY", PyInt_FromLong( OB_LOCK_LOCY ) );
		PyConstant_Insert( d, "LOCZ", PyInt_FromLong( OB_LOCK_LOCZ ) );
		PyConstant_Insert( d, "LOC", PyInt_FromLong( OB_LOCK_LOC ) );
		PyConstant_Insert( d, "ROTX", PyInt_FromLong( OB_LOCK_ROTX ) );
		PyConstant_Insert( d, "ROTY", PyInt_FromLong( OB_LOCK_ROTY ) );
		PyConstant_Insert( d, "ROTZ", PyInt_FromLong( OB_LOCK_ROTZ ) );
		PyConstant_Insert( d, "ROT",
				PyInt_FromLong( OB_LOCK_ROTX|OB_LOCK_ROTY|OB_LOCK_ROTZ ) );
		PyConstant_Insert( d, "SCALEX", PyInt_FromLong( OB_LOCK_SCALEX ) );
		PyConstant_Insert( d, "SCALEY", PyInt_FromLong( OB_LOCK_SCALEY ) );
		PyConstant_Insert( d, "SCALEZ", PyInt_FromLong( OB_LOCK_SCALEZ ) );
		PyConstant_Insert( d, "SCALE",
				PyInt_FromLong( OB_LOCK_SCALEX|OB_LOCK_SCALEY|OB_LOCK_SCALEZ ) );
	}
	return M;
}

static PyObject *M_Object_RBFlagsDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "DYNAMIC", PyInt_FromLong( OB_DYNAMIC ) );
		PyConstant_Insert( d, "CHILD", PyInt_FromLong( OB_CHILD ) );
		PyConstant_Insert( d, "ACTOR", PyInt_FromLong( OB_ACTOR ) );
		PyConstant_Insert( d, "USEFH", PyInt_FromLong(  OB_DO_FH ) );
		PyConstant_Insert( d, "ROTFH", PyInt_FromLong( OB_ROT_FH ) );
		PyConstant_Insert( d, "ANISOTROPIC",
				PyInt_FromLong( OB_ANISOTROPIC_FRICTION ) );
		PyConstant_Insert( d, "GHOST", PyInt_FromLong( OB_GHOST ) );
		PyConstant_Insert( d, "RIGIDBODY", PyInt_FromLong( OB_RIGID_BODY ) );
		PyConstant_Insert( d, "BOUNDS", PyInt_FromLong( OB_BOUNDS ) );
		PyConstant_Insert( d, "COLLISION_RESPONSE",
				PyInt_FromLong( OB_COLLISION_RESPONSE ) );
		PyConstant_Insert( d, "SECTOR", PyInt_FromLong( OB_SECTOR ) );
		PyConstant_Insert( d, "PROP", PyInt_FromLong( OB_PROP ) );
		PyConstant_Insert( d, "MAINACTOR", PyInt_FromLong( OB_MAINACTOR ) );
	}
	return M;
}

static PyObject *M_Object_RBShapeBoundDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "BOX", PyInt_FromLong( OB_BOUND_BOX ) );
		PyConstant_Insert( d, "SPHERE", PyInt_FromLong( OB_BOUND_SPHERE ) );
		PyConstant_Insert( d, "CYLINDER", PyInt_FromLong( OB_BOUND_CYLINDER ) );
		PyConstant_Insert( d, "CONE", PyInt_FromLong( OB_BOUND_CONE ) );
		PyConstant_Insert( d, "POLYHEDERON", PyInt_FromLong( OB_BOUND_POLYH ) );
	}
	return M;
}

static PyObject *M_Object_IpoKeyTypesDict( void )
{
	PyObject *M = PyConstant_New(  );

	if( M ) {
		BPy_constant *d = ( BPy_constant * ) M;
		PyConstant_Insert( d, "LOC", PyInt_FromLong( IPOKEY_LOC ) );
		PyConstant_Insert( d, "ROT", PyInt_FromLong( IPOKEY_ROT ) );
		PyConstant_Insert( d, "SIZE", PyInt_FromLong( IPOKEY_SIZE ) );
		PyConstant_Insert( d, "LOCROT", PyInt_FromLong( IPOKEY_LOCROT ) );
		PyConstant_Insert( d, "LOCROTSIZE", PyInt_FromLong( IPOKEY_LOCROTSIZE ) );
		PyConstant_Insert( d, "LAYER", PyInt_FromLong( IPOKEY_LAYER ) );
		
		PyConstant_Insert( d, "PI_STRENGTH", PyInt_FromLong( IPOKEY_PI_STRENGTH ) );
		PyConstant_Insert( d, "PI_FALLOFF", PyInt_FromLong( IPOKEY_PI_FALLOFF ) );
		PyConstant_Insert( d, "PI_SURFACEDAMP", PyInt_FromLong( IPOKEY_PI_SURFACEDAMP ) );
		PyConstant_Insert( d, "PI_RANDOMDAMP", PyInt_FromLong( IPOKEY_PI_RANDOMDAMP ) );
		PyConstant_Insert( d, "PI_PERM", PyInt_FromLong( IPOKEY_PI_PERM ) );
	}
	return M;
}

/*****************************************************************************/
/* Function:	 initObject						*/
/*****************************************************************************/
PyObject *Object_Init( void )
{
	PyObject *module, *dict;
	PyObject *DrawModesDict = M_Object_DrawModesDict( );
	PyObject *DrawTypesDict = M_Object_DrawTypesDict( );
	PyObject *ParentTypesDict = M_Object_ParentTypesDict( );
	PyObject *ProtectDict = M_Object_ProtectDict( );
	PyObject *PITypesDict = M_Object_PITypesDict( );
	PyObject *RBFlagsDict = M_Object_RBFlagsDict( );
	PyObject *RBShapesDict = M_Object_RBShapeBoundDict( );
	PyObject *IpoKeyTypesDict = M_Object_IpoKeyTypesDict( );

	PyType_Ready( &Object_Type ) ;

	module = Py_InitModule3( "Blender.Object", M_Object_methods,
				 M_Object_doc );
	
	
	/* We Should Remove these!!!! */
	PyModule_AddIntConstant( module, "LOC", IPOKEY_LOC );
	PyModule_AddIntConstant( module, "ROT", IPOKEY_ROT );
	PyModule_AddIntConstant( module, "SIZE", IPOKEY_SIZE );
	PyModule_AddIntConstant( module, "LOCROT", IPOKEY_LOCROT );
	PyModule_AddIntConstant( module, "LOCROTSIZE", IPOKEY_LOCROTSIZE );
	PyModule_AddIntConstant( module, "LAYER", IPOKEY_LAYER );
	
	PyModule_AddIntConstant( module, "PI_STRENGTH", IPOKEY_PI_STRENGTH );
	PyModule_AddIntConstant( module, "PI_FALLOFF", IPOKEY_PI_FALLOFF );
	PyModule_AddIntConstant( module, "PI_SURFACEDAMP", IPOKEY_PI_SURFACEDAMP );
	PyModule_AddIntConstant( module, "PI_RANDOMDAMP", IPOKEY_PI_RANDOMDAMP );
	PyModule_AddIntConstant( module, "PI_PERM", IPOKEY_PI_PERM );

	PyModule_AddIntConstant( module, "NONE",0 );
	PyModule_AddIntConstant( module, "FORCE",PFIELD_FORCE );
	PyModule_AddIntConstant( module, "VORTEX",PFIELD_VORTEX );
	PyModule_AddIntConstant( module, "MAGNET",PFIELD_MAGNET );
	PyModule_AddIntConstant( module, "WIND",PFIELD_WIND );
	/* Only keeping above so as not to break compat */
	
	
	if( DrawModesDict )
		PyModule_AddObject( module, "DrawModes", DrawModesDict );
	if( DrawTypesDict )
		PyModule_AddObject( module, "DrawTypes", DrawTypesDict );
	if( ParentTypesDict )
		PyModule_AddObject( module, "ParentTypes", ParentTypesDict );
	if( PITypesDict )
		PyModule_AddObject( module, "PITypes", PITypesDict );
	if( ProtectDict )
		PyModule_AddObject( module, "ProtectFlags", ProtectDict );
	if( RBFlagsDict )
		PyModule_AddObject( module, "RBFlags", RBFlagsDict );
	if( RBShapesDict )
		PyModule_AddObject( module, "RBShapes", RBShapesDict );
	if( IpoKeyTypesDict )
		PyModule_AddObject( module, "IpoKeyTypes", IpoKeyTypesDict );	

		/*Add SUBMODULES to the module*/
	dict = PyModule_GetDict( module ); /*borrowed*/
	PyDict_SetItemString(dict, "Pose", Pose_Init()); /*creates a *new* module*/
	/*PyDict_SetItemString(dict, "Constraint", Constraint_Init()); */ /*creates a *new* module*/

	return ( module );
}

/* #####DEPRECATED###### */

static PyObject *Object_SetIpo( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)Object_setIpo );
}

static PyObject *Object_Select( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)Object_setSelect );
}

static PyObject *Object_SetDrawMode( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Object_setDrawMode );
}

static PyObject *Object_SetDrawType( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Object_setDrawType );
}

static PyObject *Object_SetMatrix( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Object_setMatrix );
}

static PyObject *Object_SetEuler( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapperTuple( (void *)self, args,
			(setter)Object_setEuler );
}

static PyObject *Object_setTimeOffset( BPy_Object * self, PyObject * args )
{
	float newTimeOffset;

	if( !PyArg_ParseTuple( args, "f", &newTimeOffset ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a float as argument" );

	self->object->sf = newTimeOffset;
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}


/*************************************************************************/
/* particle defection methods                                            */
/*************************************************************************/

static PyObject *Object_SetPIDeflection( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Object_setPIDeflection );
}

static PyObject *Object_SetPIType( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Object_setPIType );
}

static PyObject *Object_SetPIUseMaxDist( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Object_setPIUseMaxDist );
}

static PyObject *Object_getPISurfaceDamp( BPy_Object * self )
{
    if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );
    
    return PyFloat_FromDouble( ( double ) self->object->pd->pdef_damp );
}

static PyObject *Object_SetPISurfaceDamp( BPy_Object * self, PyObject * args )
{
	float value;

	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return NULL;

	self->object->pd->pdef_damp = EXPP_ClampFloat( value,
			EXPP_OBJECT_PIDAMP_MIN, EXPP_OBJECT_PIDAMP_MAX );
	self->object->recalc |= OB_RECALC_OB;  
	Py_RETURN_NONE;
}

static PyObject *Object_getPIPerm( BPy_Object * self )
{
	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );
	return PyFloat_FromDouble ( (double) self->object->pd->pdef_perm );
}

static PyObject *Object_SetPIPerm( BPy_Object * self, PyObject * args )
{
	float value;

	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return NULL;

	self->object->pd->pdef_perm = EXPP_ClampFloat( value,
			EXPP_OBJECT_PIPERM_MIN, EXPP_OBJECT_PIPERM_MAX );
	self->object->recalc |= OB_RECALC_OB;  
	Py_RETURN_NONE;
}

static PyObject *Object_getPIStrength( BPy_Object * self )
{
	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	return PyFloat_FromDouble( ( double ) self->object->pd->f_strength );
}

static PyObject *Object_setPIStrength( BPy_Object * self, PyObject * args )
{
    float value;

	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected float argument" );

	self->object->pd->f_strength = EXPP_ClampFloat( value,
			EXPP_OBJECT_PISTRENGTH_MIN, EXPP_OBJECT_PISTRENGTH_MAX );
	self->object->recalc |= OB_RECALC_OB;  
	Py_RETURN_NONE;
}

static PyObject *Object_getPIFalloff( BPy_Object * self )
{
	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

    return PyFloat_FromDouble( ( double ) self->object->pd->f_power );
}

static PyObject *Object_setPIFalloff( BPy_Object * self, PyObject * args )
{
    float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected float argument" );

	self->object->pd->f_power = EXPP_ClampFloat( value,
			EXPP_OBJECT_PIPOWER_MIN, EXPP_OBJECT_PIPOWER_MAX );
	self->object->recalc |= OB_RECALC_OB;  
	Py_RETURN_NONE;
}

static PyObject *Object_getPIMaxDist( BPy_Object * self )
{
	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

    return PyFloat_FromDouble( ( double ) self->object->pd->maxdist );
}

static PyObject *Object_setPIMaxDist( BPy_Object * self, PyObject * args )
{
    float value;

	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected float argument" );

	self->object->pd->maxdist = EXPP_ClampFloat( value,
			EXPP_OBJECT_PIMAXDIST_MIN, EXPP_OBJECT_PIMAXDIST_MAX );
	self->object->recalc |= OB_RECALC_OB;  
	Py_RETURN_NONE;
}

static PyObject *Object_getPIRandomDamp( BPy_Object * self )
{
	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

    return PyFloat_FromDouble( ( double ) self->object->pd->pdef_rdamp );
}

static PyObject *Object_setPIRandomDamp( BPy_Object * self, PyObject * args )
{
    float value;

	if( !self->object->pd && !setupPI(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"particle deflection could not be accessed" );

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return NULL;

	self->object->pd->pdef_rdamp = EXPP_ClampFloat( value,
			EXPP_OBJECT_PIRDAMP_MIN, EXPP_OBJECT_PIRDAMP_MAX );
	self->object->recalc |= OB_RECALC_OB;  
	Py_RETURN_NONE;
}

/*************************************************************************/
/* softbody methods                                                      */
/*************************************************************************/

static PyObject *Object_getSBMass( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->nodemass );
}

static PyObject *Object_setSBMass( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->nodemass = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBNODEMASSMIN, EXPP_OBJECT_SBNODEMASSMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_getSBGravity( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->grav );
}

static PyObject *Object_setSBGravity( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->grav = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBGRAVMIN, EXPP_OBJECT_SBGRAVMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_getSBFriction( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->mediafrict );
}

static PyObject *Object_setSBFriction( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->mediafrict = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBMEDIAFRICTMIN, EXPP_OBJECT_SBMEDIAFRICTMAX );
	self->object->recalc |= OB_RECALC_OB;  
	Py_RETURN_NONE;
}

static PyObject *Object_getSBErrorLimit( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->rklimit );
}

static PyObject *Object_setSBErrorLimit( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->rklimit = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBRKLIMITMIN, EXPP_OBJECT_SBRKLIMITMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_getSBGoalSpring( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->goalspring );
}

static PyObject *Object_setSBGoalSpring( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->goalspring = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBGOALSPRINGMIN, EXPP_OBJECT_SBGOALSPRINGMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_getSBGoalFriction( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->goalfrict );
}

static PyObject *Object_setSBGoalFriction( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->goalfrict = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBGOALFRICTMIN, EXPP_OBJECT_SBGOALFRICTMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_getSBMinGoal( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->mingoal );
}

static PyObject *Object_setSBMinGoal( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->mingoal = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBMINGOALMIN, EXPP_OBJECT_SBMINGOALMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_getSBMaxGoal( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->maxgoal );
}

static PyObject *Object_setSBMaxGoal( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->maxgoal = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBMAXGOALMIN, EXPP_OBJECT_SBMAXGOALMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_getSBDefaultGoal( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->defgoal );
}

static PyObject *Object_setSBDefaultGoal( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->defgoal = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBDEFGOALMIN, EXPP_OBJECT_SBDEFGOALMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_getSBInnerSpring( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->inspring );
}

static PyObject *Object_setSBInnerSpring( BPy_Object * self, PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->inspring = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBINSPRINGMIN, EXPP_OBJECT_SBINSPRINGMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_getSBInnerSpringFriction( BPy_Object * self )
{
    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

    return PyFloat_FromDouble( ( double ) self->object->soft->infrict );
}

static PyObject *Object_setSBInnerSpringFriction( BPy_Object * self,
		PyObject * args )
{
    float value;

    if( !self->object->soft && !setupSB(self->object) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"softbody could not be accessed" );    

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected float argument" );

	self->object->soft->infrict = EXPP_ClampFloat( value,
			EXPP_OBJECT_SBINFRICTMIN, EXPP_OBJECT_SBINFRICTMAX );
	self->object->recalc |= OB_RECALC_OB;  

	Py_RETURN_NONE;
}

static PyObject *Object_SetSBUseGoal( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Object_setSBUseGoal );
}

static PyObject *Object_SetSBUseEdges( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Object_setSBUseEdges );
}

static PyObject *Object_SetSBStiffQuads( BPy_Object * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args,
			(setter)Object_setSBStiffQuads );
}

static PyObject *Object_trackAxis( BPy_Object * self )
{
	Object* ob;
	char ctr[3];

	memset( ctr, 0, sizeof(ctr));
	ob = self->object;

	switch(ob->trackflag){
		case(0):
			ctr[0] = 'X';
			break;
		case(1):
			ctr[0] = 'Y';
			break;
		case(2):
			ctr[0] = 'Z';
			break;
		case(3):
			ctr[0] = '-';
			ctr[1] = 'X';
			break;
		case(4):
			ctr[0] = '-';
			ctr[1] = 'Y';
			break;
		case(5):
			ctr[0] = '-';
			ctr[1] = 'Z';
			break;
		default:
			break;
	}

	return PyString_FromString(ctr);
}

static PyObject *Object_upAxis( BPy_Object * self )
{
	Object* ob;
	char cup[2];

	memset( cup, 0, sizeof(cup));
	ob = self->object;

	switch(ob->upflag){
		case(0):
			cup[0] = 'X';
			break;
		case(1):
			cup[0] = 'Y';
			break;
		case(2):
			cup[0] = 'Z';
			break;
		default:
			break;
	}

	return PyString_FromString(cup);
}
