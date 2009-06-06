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
 * This is a new part of Blender.
 *
 * Contributor(s): 
 *    Original version: Jacques Guignot, Jean-Michel Soler
 *    Rewrite :        Cedric Paille, Stephen Swaney, Joilnen Leite
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Particle.h"
#include "gen_utils.h"
#include "BKE_object.h"
#include "BKE_main.h"
#include "BKE_particle.h"
#include "BKE_global.h"
#include "BKE_depsgraph.h"
#include "BKE_modifier.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"
#include "BKE_pointcache.h"
#include "BKE_DerivedMesh.h"
#include "BIF_editparticle.h"
#include "BIF_space.h"
#include "blendef.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_material_types.h"
#include "BLI_blenlib.h"
#include "mydevice.h"
#include "Object.h"
#include "Material.h"

#include "MEM_guardedalloc.h"



/* Type Methods */
static PyObject *M_ParticleSys_New( PyObject * self, PyObject * value );
static PyObject *M_ParticleSys_Get( PyObject * self, PyObject * args );

/* Particle Methods */
static PyObject *Part_freeEdit( BPy_PartSys * self, PyObject * args );
static PyObject *Part_GetLoc( BPy_PartSys * self, PyObject * args );
static PyObject *Part_GetRot( BPy_PartSys * self, PyObject * args );
static PyObject *Part_SetMat( BPy_PartSys * self, PyObject * args );
static PyObject *Part_GetMat( BPy_PartSys * self, PyObject * args );
static PyObject *Part_GetSize( BPy_PartSys * self, PyObject * args );
static PyObject *Part_GetVertGroup( BPy_PartSys * self, PyObject * args );
static PyObject *Part_SetVertGroup( BPy_PartSys * self, PyObject * args );
static int Part_setSeed( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getSeed( BPy_PartSys * self );
static int Part_setType( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getType( BPy_PartSys * self );
static int Part_setResol( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getResol( BPy_PartSys * self );
static int Part_setStart( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getStart( BPy_PartSys * self );
static int Part_setEnd( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getEnd( BPy_PartSys * self );
static int Part_setEditable( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getEditable( BPy_PartSys * self );
static int Part_setAmount( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getAmount( BPy_PartSys * self );
static int Part_setMultiReact( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getMultiReact( BPy_PartSys * self );
static int Part_setReactShape( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getReactShape( BPy_PartSys * self );
static int Part_setSegments( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getSegments( BPy_PartSys * self );
static int Part_setLife( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getLife( BPy_PartSys * self );
static int Part_setRandLife( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRandLife( BPy_PartSys * self );
static int Part_set2d( BPy_PartSys * self, PyObject * args );
static PyObject *Part_get2d( BPy_PartSys * self );
static int Part_setMaxVel( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getMaxVel( BPy_PartSys * self );
static int Part_setAvVel( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getAvVel( BPy_PartSys * self );
static int Part_setLatAcc( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getLatAcc( BPy_PartSys * self );
static int Part_setMaxTan( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getMaxTan( BPy_PartSys * self );
static int Part_setGroundZ( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getGroundZ( BPy_PartSys * self );
static int Part_setOb( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getOb( BPy_PartSys * self );
static PyObject *Part_getRandEmission( BPy_PartSys * self );
static int Part_setRandEmission( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRandEmission( BPy_PartSys * self );
static int Part_setParticleDist( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getParticleDist( BPy_PartSys * self );
static int Part_setEvenDist( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getEvenDist( BPy_PartSys * self );
static int Part_setDist( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getDist( BPy_PartSys * self );
static int Part_setParticleDisp( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getParticleDisp( BPy_PartSys * self );
static int Part_setJitterAmount( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getJitterAmount( BPy_PartSys * self );
static int Part_setPF( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getPF( BPy_PartSys * self );
static int Part_setInvert( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getInvert( BPy_PartSys * self );
static int Part_setTargetOb( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getTargetOb( BPy_PartSys * self );
static int Part_setTargetPsys( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getTargetPsys( BPy_PartSys * self );
static int Part_setRenderObject( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRenderObject( BPy_PartSys * self );
static int Part_setRenderMaterialColor( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRenderMaterialColor( BPy_PartSys * self );
static int Part_setRenderParents( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRenderParents( BPy_PartSys * self );
static int Part_setRenderUnborn( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRenderUnborn( BPy_PartSys * self );
static int Part_setRenderDied( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRenderDied( BPy_PartSys * self );
static int Part_setRenderMaterialIndex( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRenderMaterialIndex( BPy_PartSys * self );
static int Part_setStep( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getStep( BPy_PartSys * self );
static int Part_setRenderStep( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRenderStep( BPy_PartSys * self );
static PyObject *Part_getDupOb( BPy_PartSys * self );
static PyObject *Part_getDrawAs( BPy_PartSys * self );
static int Part_setPhysType( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getPhysType( BPy_PartSys * self );
static int Part_setIntegrator( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getIntegrator( BPy_PartSys * self );
static int Part_setIniVelObject( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getIniVelObject( BPy_PartSys * self );
static int Part_setIniVelNormal( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getIniVelNormal( BPy_PartSys * self );
static int Part_setIniVelRandom( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getIniVelRandom( BPy_PartSys * self );
static int Part_setIniVelTan( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getIniVelTan( BPy_PartSys * self );
static int Part_setIniVelRot( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getIniVelRot( BPy_PartSys * self );
static int Part_setIniVelPart( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getIniVelPart( BPy_PartSys * self );
static int Part_setIniVelReact( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getIniVelReact( BPy_PartSys * self );
static int Part_setRotDynamic( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRotDynamic( BPy_PartSys * self );
static int Part_setRotation( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRotation( BPy_PartSys * self );
static int Part_setRotRandom( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRotRandom( BPy_PartSys * self );
static int Part_setRotPhase( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRotPhase( BPy_PartSys * self );
static int Part_setRotPhaseR( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRotPhaseR( BPy_PartSys * self );
static int Part_setRotAngularV( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRotAngularV( BPy_PartSys * self );
static int Part_setRotAngularVAm( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRotAngularVAm( BPy_PartSys * self );
static int Part_setGlobAccX( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getGlobAccX( BPy_PartSys * self );
static int Part_setGlobAccY( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getGlobAccY( BPy_PartSys * self );
static int Part_setGlobAccZ( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getGlobAccZ( BPy_PartSys * self );
static int Part_setGlobDrag( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getGlobDrag( BPy_PartSys * self );
static int Part_setGlobBrown( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getGlobBrown( BPy_PartSys * self );
static int Part_setGlobDamp( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getGlobDamp( BPy_PartSys * self );
static PyObject *Part_GetAge( BPy_PartSys * self, PyObject * args );
static int Part_setChildAmount( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildAmount( BPy_PartSys * self );
static int Part_setChildType( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildType( BPy_PartSys * self );
static int Part_setChildRenderAmount( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRenderAmount( BPy_PartSys * self );
static int Part_setChildRadius( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRadius( BPy_PartSys * self );
static int Part_setChildRoundness( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRoundness( BPy_PartSys * self );
static int Part_setChildClumping( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildClumping( BPy_PartSys * self );
static int Part_setChildShape( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildShape( BPy_PartSys * self );
static int Part_setChildSize( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildSize( BPy_PartSys * self );
static int Part_setChildRandom( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRandom( BPy_PartSys * self );
static int Part_setChildRough1( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRough1( BPy_PartSys * self );
static int Part_setChildRough1Size( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRough1Size( BPy_PartSys * self );
static int Part_setChildRough2( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRough2( BPy_PartSys * self );
static int Part_setChildRough2Size( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRough2Size( BPy_PartSys * self );
static int Part_setChildRough2Thres( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRough2Thres( BPy_PartSys * self );
static int Part_setChildRoughE( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRoughE( BPy_PartSys * self );
static int Part_setChildRoughEShape( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildRoughEShape( BPy_PartSys * self );
static int Part_setChildKink( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildKink( BPy_PartSys * self );
static int Part_setChildKinkAxis( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildKinkAxis( BPy_PartSys * self );
static int Part_setChildKinkFreq( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildKinkFreq( BPy_PartSys * self );
static int Part_setChildKinkShape( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildKinkShape( BPy_PartSys * self );
static int Part_setChildKinkAmp( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildKinkAmp( BPy_PartSys * self );
static int Part_setChildBranch( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildBranch( BPy_PartSys * self );
static int Part_setChildBranchAnim( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildBranchAnim( BPy_PartSys * self );
static int Part_setChildBranchSymm( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildBranchSymm( BPy_PartSys * self );
static int Part_setChildBranchThre( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getChildBranchThre( BPy_PartSys * self );

/*****************************************************************************/
/* Python Effect_Type callback function prototypes:                           */
/*****************************************************************************/
static PyObject *ParticleSys_repr( BPy_PartSys * self );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Particle.__doc__                                                  */
/*****************************************************************************/
static char M_ParticleSys_doc[] = "The Blender Effect module\n\n\
This module provides access to **Object Data** in Blender.\n\
Functions :\n\
	Get(name) : retreives particle system (as list)  with the given name\n";
static char M_ParticleSys_Get_doc[] = "xxx";
static char M_ParticleSys_New_doc[] = "xxx";

/*****************************************************************************/
/* Python BPy_ParticleSys methods table:                                     */
/*****************************************************************************/

static PyMethodDef BPy_ParticleSys_methods[] = {
	{"freeEdit", ( PyCFunction ) Part_freeEdit,
	 METH_NOARGS, "() - Free from edit mode"},
  	{"getLoc", ( PyCFunction ) Part_GetLoc,
	 METH_VARARGS, "() - Get particles location"},
   	{"getRot", ( PyCFunction ) Part_GetRot,
	 METH_VARARGS, "() - Get particles rotations (list of 4 floats quaternion)"},
    {"setMat", ( PyCFunction ) Part_SetMat,
	 METH_VARARGS, "() - Set particles material"},
   	{"getMat", ( PyCFunction ) Part_GetMat,
	 METH_NOARGS, "() - Get particles material"},
   	{"getSize", ( PyCFunction ) Part_GetSize,
	 METH_VARARGS, "() - Get particles size in a list"},
    	{"getAge", ( PyCFunction ) Part_GetAge,
	 METH_VARARGS, "() - Get particles life in a list"},
	{"getVertGroup", ( PyCFunction ) Part_GetVertGroup,
	 METH_VARARGS, "() - Get the vertex group which affects a particles attribute"},
	{"setVertGroup", ( PyCFunction ) Part_SetVertGroup,
	 METH_VARARGS, "() - Set the vertex group to affect a particles attribute"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_ParticleSys attributes get/set structure:                           */
/*****************************************************************************/
static PyGetSetDef BPy_ParticleSys_getseters[] = {
/* Extras */
	{"seed",
	 (getter)Part_getSeed, (setter)Part_setSeed,
	 "Set an offset in the random table",
	 NULL},
 /* basics */
 	{"type",
	 (getter)Part_getType, (setter)Part_setType,
	 "Type of particle system ( Particle.TYPE[ 'HAIR' | 'REACTOR' | 'EMITTER' ] )",
	 NULL},
  	{"resolutionGrid",
	 (getter)Part_getResol, (setter)Part_setResol,
	 "The resolution of the particle grid",
	 NULL},
   	{"startFrame",
	 (getter)Part_getStart, (setter)Part_setStart,
	 "Frame # to start emitting particles",
	 NULL},
   	{"endFrame",
	 (getter)Part_getEnd, (setter)Part_setEnd,
	 "Frame # to stop emitting particles",
	 NULL},
   	{"editable",
	 (getter)Part_getEditable, (setter)Part_setEditable,
	 "Finalize hair to enable editing in particle mode",
	 NULL},
    {"amount",
	 (getter)Part_getAmount, (setter)Part_setAmount,
	 "The total number of particles",
	 NULL},
    {"multireact",
	 (getter)Part_getMultiReact, (setter)Part_setMultiReact,
	 "React multiple times ( Paricle.REACTON[ 'NEAR' | 'COLLISION' | 'DEATH' ] )",
	 NULL},
    {"reactshape",
	 (getter)Part_getReactShape, (setter)Part_setReactShape,
	 "Power of reaction strength dependence on distance to target",
	 NULL},
    {"hairSegments",
	 (getter)Part_getSegments, (setter)Part_setSegments,
	 "Amount of hair segments",
	 NULL},
    {"lifetime",
	 (getter)Part_getLife, (setter)Part_setLife,
	 "Specify the life span of the particles",
	 NULL},
    {"randlife",
	 (getter)Part_getRandLife, (setter)Part_setRandLife,
	 "Give the particle life a random variation",
	 NULL},
     {"randemission",
	 (getter)Part_getRandEmission, (setter)Part_setRandEmission,
	 "Give the particle life a random variation",
	 NULL},
     {"particleDistribution",
	 (getter)Part_getParticleDist, (setter)Part_setParticleDist,
	 "Where to emit particles from  Paricle.EMITFROM[ 'PARTICLE' | 'VOLUME' | 'FACES' | 'VERTS' ] )",
	 NULL},
     {"evenDistribution",
	 (getter)Part_getEvenDist, (setter)Part_setEvenDist,
	 "Use even distribution from faces based on face areas or edge lengths",
	 NULL},
     {"distribution",
	 (getter)Part_getDist, (setter)Part_setDist,
	 "How to distribute particles on selected element Paricle.DISTRIBUTION[ 'GRID' | 'RANDOM' | 'JITTERED' ] )",
	 NULL},
     {"jitterAmount",
	 (getter)Part_getJitterAmount, (setter)Part_setJitterAmount,
	 "Amount of jitter applied to the sampling",
	 NULL},
     {"pf",
	 (getter)Part_getPF, (setter)Part_setPF,
	 "Emission locations / face (0 = automatic)",
	 NULL},
     {"invert",
	 (getter)Part_getInvert, (setter)Part_setInvert,
	 "Invert what is considered object and what is not.",
	 NULL},
     {"targetObject",
	 (getter)Part_getTargetOb, (setter)Part_setTargetOb,
	 "The object that has the target particle system (empty if same object)",
	 NULL},
     {"targetpsys",
	 (getter)Part_getTargetPsys, (setter)Part_setTargetPsys,
	 "The target particle system number in the object",
	 NULL},
/* Physics */
    {"2d",
	 (getter)Part_get2d, (setter)Part_set2d,
	 "Constrain boids to a surface",
	 NULL},
    {"maxvel",
	 (getter)Part_getMaxVel, (setter)Part_setMaxVel,
	 "Maximum velocity",
	 NULL},
    {"avvel",
	 (getter)Part_getAvVel, (setter)Part_setAvVel,
	 "The usual speed % of max velocity",
	 NULL},
    {"latacc",
	 (getter)Part_getLatAcc, (setter)Part_setLatAcc,
	 "Lateral acceleration % of max velocity",
	 NULL},
    {"tanacc",
	 (getter)Part_getMaxTan, (setter)Part_setMaxTan,
	 "Tangential acceleration % of max velocity",
	 NULL},
    {"groundz",
	 (getter)Part_getGroundZ, (setter)Part_setGroundZ,
	 "Default Z value",
	 NULL},
     {"object",
	 (getter)Part_getOb, (setter)Part_setOb,
	 "Constrain boids to object's surface",
	 NULL},
/* Visualisation */
     {"renderEmitter",
	 (getter)Part_getRenderObject, (setter)Part_setRenderObject,
	 "Render emitter object",
	 NULL},
	 {"renderMatCol",
	 (getter)Part_getRenderMaterialColor, (setter)Part_setRenderMaterialColor,
	 "Draw particles using material's diffuse color",
	 NULL},
	 {"renderParents",
	 (getter)Part_getRenderParents, (setter)Part_setRenderParents,
	 "Render parent particles",
	 NULL},
	 {"renderUnborn",
	 (getter)Part_getRenderUnborn, (setter)Part_setRenderUnborn,
	 "Show particles before they are emitted",
	 NULL},
	 {"renderDied",
	 (getter)Part_getRenderDied, (setter)Part_setRenderDied,
	 "Show particles after they have died",
	 NULL},
	 {"renderMaterial",
	 (getter)Part_getRenderMaterialIndex, (setter)Part_setRenderMaterialIndex,
	 "Specify material index used for the particles",
	 NULL},
     {"displayPercentage",
	 (getter)Part_getParticleDisp, (setter)Part_setParticleDisp,
	 "Particle display percentage",
	 NULL},
     {"hairDisplayStep",
	 (getter)Part_getStep, (setter)Part_setStep,
	 "How many steps paths are drawn with (power of 2)",
	 NULL},
     {"hairRenderStep",
	 (getter)Part_getRenderStep, (setter)Part_setRenderStep,
	 "How many steps paths are rendered with (power of 2)",
	 NULL},
     {"duplicateObject",
	 (getter)Part_getDupOb, NULL,
	 "Get the duplicate ob",
	 NULL},
     {"drawAs",
	 (getter)Part_getDrawAs, NULL,
	 "Get draw type Particle.DRAWAS([ 'NONE' | 'OBJECT' | 'POINT' | ... ] )",
	 NULL},
/* Newtonian Physics */
	{"physics",
	 (getter)Part_getPhysType, (setter)Part_setPhysType,
	 "Select particle physics type Particle.PHYSICS([ 'BOIDS' | 'KEYED' | 'NEWTONIAN' | 'NONE' ])",
	 NULL},
	{"integration",
	 (getter)Part_getIntegrator, (setter)Part_setIntegrator,
	 "Select physics integrator type Particle.INTEGRATOR([ 'RK4' | 'MIDPOINT' | 'EULER' ])",
	 NULL},
	 {"inVelObj",
	 (getter)Part_getIniVelObject, (setter)Part_setIniVelObject,
	 "Let the object give the particle a starting speed",
	 NULL},
	 {"inVelNor",
	 (getter)Part_getIniVelNormal, (setter)Part_setIniVelNormal,
	 "Let the surface normal give the particle a starting speed",
	 NULL},
	 {"inVelRan",
	 (getter)Part_getIniVelRandom, (setter)Part_setIniVelRandom,
	 "Give the starting speed a random variation",
	 NULL},
	 {"inVelTan",
	 (getter)Part_getIniVelTan, (setter)Part_setIniVelTan,
	 "Let the surface tangent give the particle a starting speed",
	 NULL},
	 {"inVelRot",
	 (getter)Part_getIniVelRot, (setter)Part_setIniVelRot,
	 "Rotate the surface tangent",
	 NULL},
	 {"inVelPart",
	 (getter)Part_getIniVelPart, (setter)Part_setIniVelPart,
	 "Let the target particle give the particle a starting speed",
	 NULL},
	 {"inVelReact",
	 (getter)Part_getIniVelReact, (setter)Part_setIniVelReact,
	 "Let the vector away from the target particles location give the particle a starting speed",
	 NULL},
	 {"rotation",
	 (getter)Part_getRotation, (setter)Part_setRotation,
	 "Particles initial rotation Particle.ROTATION([ 'OBZ' | 'OBY' | 'OBX' | 'GLZ' | 'GLY' | 'GLX' | 'VEL' | 'NOR' | 'NONE' ])",
	 NULL},
	 {"rotDyn",
	 (getter)Part_getRotDynamic, (setter)Part_setRotDynamic,
	 "Sets rotation to dynamic/constant",
	 NULL},
	 {"rotRand",
	 (getter)Part_getRotRandom, (setter)Part_setRotRandom,
	 "Randomize rotation",
	 NULL},
	 {"rotPhase",
	 (getter)Part_getRotPhase, (setter)Part_setRotPhase,
	 "Initial rotation phase",
	 NULL},
	 {"rotPhaseR",
	 (getter)Part_getRotPhaseR, (setter)Part_setRotPhaseR,
	 "Randomize rotation phase",
	 NULL},
	 {"rotAnV",
	 (getter)Part_getRotAngularV, (setter)Part_setRotAngularV,
	 "Select particle angular velocity mode Particle.ANGULARV([ 'RANDOM' | 'SPIN' | 'NONE' ])",
	 NULL},
	 {"rotAnVAm",
	 (getter)Part_getRotAngularVAm, (setter)Part_setRotAngularVAm,
	 "Angular velocity amount",
	 NULL},
	 {"glAccX",
	 (getter)Part_getGlobAccX, (setter)Part_setGlobAccX,
	 "Specify a constant acceleration along the X-axis",
	 NULL},
	 {"glAccY",
	 (getter)Part_getGlobAccY, (setter)Part_setGlobAccY,
	 "Specify a constant acceleration along the Y-axis",
	 NULL},
	 {"glAccZ",
	 (getter)Part_getGlobAccZ, (setter)Part_setGlobAccZ,
	 "Specify a constant acceleration along the Z-axis",
	 NULL},
	 {"glDrag",
	 (getter)Part_getGlobDrag, (setter)Part_setGlobDrag,
	 "Specify the amount of air-drag",
	 NULL},
	 {"glBrown",
	 (getter)Part_getGlobBrown, (setter)Part_setGlobBrown,
	 "Specify the amount of brownian motion",
	 NULL},
	 {"glDamp",
	 (getter)Part_getGlobDamp, (setter)Part_setGlobDamp,
	 "Specify the amount of damping",
	 NULL},
/* Children */
	{"childAmount",
	 (getter)Part_getChildAmount, (setter)Part_setChildAmount,
	 "The total number of children",
	 NULL},
	 {"childType",
	 (getter)Part_getChildType, (setter)Part_setChildType,
	 "Type of childrens ( Particle.CHILDTYPE[ 'FACES' | 'PARTICLES' | 'NONE' ] )",
	 NULL},
	 {"childRenderAmount",
	 (getter)Part_getChildRenderAmount, (setter)Part_setChildRenderAmount,
	 "Amount of children/parent for rendering",
	 NULL},
	 {"childRadius",
	 (getter)Part_getChildRadius, (setter)Part_setChildRadius,
	 "Radius of children around parent",
	 NULL},
	 {"childRound",
	 (getter)Part_getChildRoundness, (setter)Part_setChildRoundness,
	 "Roundness of children around parent",
	 NULL},
	 {"childClump",
	 (getter)Part_getChildClumping, (setter)Part_setChildClumping,
	 "Amount of clumpimg",
	 NULL},
	 {"childShape",
	 (getter)Part_getChildShape, (setter)Part_setChildShape,
	 "Shape of clumpimg",
	 NULL},
	 {"childSize",
	 (getter)Part_getChildSize, (setter)Part_setChildSize,
	 "A multiplier for the child particle size",
	 NULL},
	 {"childRand",
	 (getter)Part_getChildRandom, (setter)Part_setChildRandom,
	 "Random variation to the size of the child particles",
	 NULL},
	 {"childRough1",
	 (getter)Part_getChildRough1, (setter)Part_setChildRough1,
	 "Amount of location dependant rough",
	 NULL},
	 {"childRough1Size",
	 (getter)Part_getChildRough1Size, (setter)Part_setChildRough1Size,
	 "Size of location dependant rough",
	 NULL},
	 {"childRough2",
	 (getter)Part_getChildRough2, (setter)Part_setChildRough2,
	 "Amount of random rough",
	 NULL},
	 {"childRough2Size",
	 (getter)Part_getChildRough2Size, (setter)Part_setChildRough2Size,
	 "Size of random rough",
	 NULL},
	 {"childRough2Thresh",
	 (getter)Part_getChildRough2Thres, (setter)Part_setChildRough2Thres,
	 "Amount of particles left untouched by random rough",
	 NULL},
	 {"childRoughE",
	 (getter)Part_getChildRoughE, (setter)Part_setChildRoughE,
	 "Amount of end point rough",
	 NULL},
	 {"childRoughEShape",
	 (getter)Part_getChildRoughEShape, (setter)Part_setChildRoughEShape,
	 "Shape of end point rough",
	 NULL},
	 {"childKink",
	 (getter)Part_getChildKink, (setter)Part_setChildKink,
	 "Type of periodic offset on the path (Particle.CHILDKINK[ 'BRAID' | 'WAVE' | 'RADIAL' | 'CURL' | 'NOTHING' ])",
	 NULL},
	 {"childKinkAxis",
	 (getter)Part_getChildKinkAxis, (setter)Part_setChildKinkAxis,
	 "Which axis to use for offset (Particle.CHILDKINKAXIS[ 'Z' | 'Y' | 'X' ])",
	 NULL},
	 {"childKinkFreq",
	 (getter)Part_getChildKinkFreq, (setter)Part_setChildKinkFreq,
	 "The frequency of the offset (1/total length)",
	 NULL},
	 {"childKinkShape",
	 (getter)Part_getChildKinkShape, (setter)Part_setChildKinkShape,
	 "Adjust the offset to the beginning/end",
	 NULL},
	 {"childKinkAmp",
	 (getter)Part_getChildKinkAmp, (setter)Part_setChildKinkAmp,
	 "The amplitude of the offset",
	 NULL},
	 {"childBranch",
	 (getter)Part_getChildBranch, (setter)Part_setChildBranch,
	 "Branch child paths from eachother",
	 NULL},
	 {"childBranchAnim",
	 (getter)Part_getChildBranchAnim, (setter)Part_setChildBranchAnim,
	 "Animate branching",
	 NULL},
	 {"childBranchSymm",
	 (getter)Part_getChildBranchSymm, (setter)Part_setChildBranchSymm,
	 "Start and end points are the same",
	 NULL},
	 {"childBranchThre",
	 (getter)Part_getChildBranchThre, (setter)Part_setChildBranchThre,
	 "Threshold of branching",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python method structure definition for Blender.Particle module:           */
/*****************************************************************************/
static struct PyMethodDef M_ParticleSys_methods[] = {
	{"New", ( PyCFunction ) M_ParticleSys_New, METH_O, M_ParticleSys_New_doc},
	{"Get", M_ParticleSys_Get, METH_VARARGS, M_ParticleSys_Get_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python ParticleSys_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject ParticleSys_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender ParticleSys",           /* char *tp_name; */
	sizeof( BPy_PartSys ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) ParticleSys_repr,/* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
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
	BPy_ParticleSys_methods,      /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_ParticleSys_getseters,  /* struct PyGetSetDef *tp_getset; */
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

/*****************************************************************************/
/* Function:    PARTICLESYS_repr                                             */
/* Description: This is a callback function for the BPy_PartSys type. It     */
/*              builds a meaningful string to represent effect objects.      */
/*****************************************************************************/

static PyObject *ParticleSys_repr( BPy_PartSys * self )
{
	return PyString_FromFormat( "ParticleSys \"%s\"",
			self->psys->part->id.name+2 );
}

/*****************************************************************************/
/* Function : P_sys_FromPyObject                                           */
/*****************************************************************************/

struct ParticleSystem *P_sys_FromPyObject( BPy_PartSys * py_obj )
{
	BPy_PartSys *blen_obj;

	blen_obj = ( BPy_PartSys * ) py_obj;
	return ( blen_obj->psys );
}

/*****************************************************************************/
/* Function : ParticleSysCreatePyObject                                            */
/*****************************************************************************/
PyObject *ParticleSys_CreatePyObject( ParticleSystem * psystem, Object *ob )
{
	BPy_PartSys *blen_object;

	blen_object =
		( BPy_PartSys * ) PyObject_NEW( BPy_PartSys, &ParticleSys_Type );

	if( blen_object )
		blen_object->psys = (ParticleSystem *)psystem;

	blen_object->object = ob;

	return ( PyObject * ) blen_object;
}


PyObject *M_ParticleSys_New( PyObject * self, PyObject * value)
{
	ParticleSystem *psys = 0;
	ParticleSystem *rpsys = 0;
	ModifierData *md;
	ParticleSystemModifierData *psmd;
	Object *ob = NULL;
	ID *id;
	int nr;
	
	if ( PyString_Check( value ) ) {
		char *name;
		name = PyString_AsString ( value );
		ob = ( Object * ) GetIdFromList( &( G.main->object ), name );
		if( !ob )
			return EXPP_ReturnPyObjError( PyExc_AttributeError, name );
	} else if ( BPy_Object_Check(value) ) {
		ob = (( BPy_Object * ) value)->object;
	} else {
		return EXPP_ReturnPyObjError( PyExc_TypeError, "expected object or string" );
	}

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


/* 

Get( name ) returns named particle sys or list of all
throws NameError if name not found

*/

PyObject *M_ParticleSys_Get( PyObject * self, PyObject * args ) 
{
#if 1
	return EXPP_ReturnPyObjError( PyExc_NotImplementedError,
		"Particle.Get() not implemented" );
#else
	ParticleSettings *psys_iter;
	char *name = NULL;

	ParticleSystem *blparticlesys = 0;
	Object *ob;

	PyObject *partsyslist,*current;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected string argument" );

	psys_iter = G.main->particle.first; /* initialize our iterator */

	if( name ) {   /* find psys by name */

		PyObject *wanted_obj = NULL;
	
		while( psys_iter && ! wanted_obj ){
			if( !strcmp( name, psys_iter->id.name + 2)){
				printf("** found %s\n", psys_iter->id.name+2);
				//wanted_obj = ParticleSys_CreatePyObject( psys_iter );
				break;
			}
			psys_iter = psys_iter->id.next;
		}

		if( !wanted_obj){  /* requested object not found */
			PyErr_Format(PyExc_NameError, "Particle System '%s' not found", name);
			return NULL;
		}

		return wanted_obj;

	}else {  /* no arg - return a list of bpy objs all P. systems */

		PyObject *pylist;
		int index = 0;

		pylist = PyList_New( BLI_countlist( &G.main->particle ));
		printf("** list is %d long\n", PyList_Size( pylist));
		if( ! pylist ){
			return EXPP_ReturnPyObjError( 
				PyExc_MemoryError,
				"could not create ParticleSystem list");
		}
		
		while( psys_iter ){
			pyobj = ParticleSystem_CreatePyObject( psys_iter);
			if( !pyobj){
				Py_DECREF( pylist );
				return EXPP_ReturnPyObjError(
					PyExc_MemoryError, 
					"could not create ParticleSystem PyObject");
			}
			PyList_SET_ITEM( pylist, index, pyobj);
			printf("name is %s\n", psys_iter->id.name+2);
			psys_iter = psys_iter->id.next;
			index++;
		}

		return pylist;
			
	}
			
	for( ob = G.main->particlesystem.first; ob; ob = ob->id.next )
		if( !strcmp( name, ob->id.name + 2 ) )
			break;

	if( !ob )
		return EXPP_ReturnPyObjError( PyExc_AttributeError, 
				"object does not exist" );

	blparticlesys = ob->particlesystem.first;
	

	partsyslist = PyList_New( 0 );

	if (!blparticlesys)
		return partsyslist;

	current = ParticleSys_CreatePyObject( blparticlesys, ob );
	PyList_Append(partsyslist,current);


	while((blparticlesys = blparticlesys->next)){
		current = ParticleSys_CreatePyObject( blparticlesys, ob );
		PyList_Append(partsyslist,current);
	}

	return partsyslist;
#endif
}


/*****************************************************************************/
/* Function:              ParticleSys_Init                                   */
/*****************************************************************************/

/* create the Blender.Particle.Type constant dict */

static PyObject *Particle_TypeDict( void )
{
	PyObject *Types = PyConstant_New(  );

	if( Types ) {
		BPy_constant *c = ( BPy_constant * ) Types;

		PyConstant_Insert( c, "HAIR",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "REACTOR",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "EMITTER",
				 PyInt_FromLong( 0 ) );
	}
	return Types;
}

/* create the Blender.Particle.Distribution constant dict */

static PyObject *Particle_DistrDict( void )
{
	PyObject *Distr = PyConstant_New(  );

	if( Distr ) {
		BPy_constant *c = ( BPy_constant * ) Distr;

		PyConstant_Insert( c, "GRID",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "RANDOM",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "JITTERED",
				 PyInt_FromLong( 0 ) );
	}
	return Distr;
}

/* create the Blender.Particle.EmitFrom constant dict */

static PyObject *Particle_EmitFrom( void )
{
	PyObject *EmitFrom = PyConstant_New(  );

	if( EmitFrom ) {
		BPy_constant *c = ( BPy_constant * ) EmitFrom;

		PyConstant_Insert( c, "VERTS",
				 PyInt_FromLong( 0 ) );
		PyConstant_Insert( c, "FACES",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "VOLUME",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "PARTICLE",
				 PyInt_FromLong( 3 ) );
	}
	return EmitFrom;
}

/* create the Blender.Particle.Collision constant dict */

static PyObject *Particle_ReactOnDict( void )
{
	PyObject *ReactOn = PyConstant_New(  );

	if( ReactOn ) {
		BPy_constant *c = ( BPy_constant * ) ReactOn;

		PyConstant_Insert( c, "NEAR",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "COLLISION",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "DEATH",
				 PyInt_FromLong( 0 ) );
	}
	return ReactOn;
}

/* create the Blender.Particle.Physics constant dict */

static PyObject *Particle_PhysicsDict( void )
{
	PyObject *Physics = PyConstant_New(  );

	if( Physics ) {
		BPy_constant *c = ( BPy_constant * ) Physics;

		PyConstant_Insert( c, "BOIDS",
				 PyInt_FromLong( 3 ) );
		PyConstant_Insert( c, "KEYED",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "NEWTONIAN",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "NONE",
				 PyInt_FromLong( 0 ) );
	}
	return Physics;
}

/* create the Blender.Particle.Integrator constant dict */

static PyObject *Particle_IntegratorDict( void )
{
	PyObject *Integrator = PyConstant_New(  );

	if( Integrator ) {
		BPy_constant *c = ( BPy_constant * ) Integrator;

		PyConstant_Insert( c, "EULER",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "MIDPOINT",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "EULER",
				 PyInt_FromLong( 0 ) );
	}
	return Integrator;
}

/* create the Blender.Particle.Rotation constant dict */

static PyObject *Particle_RotationDict( void )
{
	PyObject *Rotation = PyConstant_New(  );

	if( Rotation ) {
		BPy_constant *c = ( BPy_constant * ) Rotation;

		PyConstant_Insert( c, "OBZ",
				 PyInt_FromLong( 8 ) );
		PyConstant_Insert( c, "OBY",
				 PyInt_FromLong( 7 ) );
		PyConstant_Insert( c, "OBX",
				 PyInt_FromLong( 6 ) );
		PyConstant_Insert( c, "GLZ",
				 PyInt_FromLong( 5 ) );
		PyConstant_Insert( c, "GLY",
				 PyInt_FromLong( 4 ) );
		PyConstant_Insert( c, "GLX",
				 PyInt_FromLong( 3 ) );
		PyConstant_Insert( c, "VEL",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "NOR",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "NONE",
				 PyInt_FromLong( 0 ) );
	}
	return Rotation;
}

/* create the Blender.Particle.AngularV constant dict */

static PyObject *Particle_AngularVDict( void )
{
	PyObject *AngularV = PyConstant_New(  );

	if( AngularV ) {
		BPy_constant *c = ( BPy_constant * ) AngularV;

		PyConstant_Insert( c, "RANDOM",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "SPIN",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "NONE",
				 PyInt_FromLong( 0 ) );
	}
	return AngularV;
}

/* create the Blender.Particle.ChildType constant dict */

static PyObject *Particle_ChildTypeDict( void )
{
	PyObject *ChildTypes = PyConstant_New(  );

	if( ChildTypes ) {
		BPy_constant *c = ( BPy_constant * ) ChildTypes;

		PyConstant_Insert( c, "FACES",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "PARTICLES",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "NONE",
				 PyInt_FromLong( 0 ) );
	}
	return ChildTypes;
}

/* create the Blender.Particle.VertexGroups constant dict */

static PyObject *Particle_VertexGroupsDict( void )
{
	PyObject *VertexGroups = PyConstant_New(  );

	if( VertexGroups ) {
		BPy_constant *c = ( BPy_constant * ) VertexGroups;

		PyConstant_Insert( c, "EFFECTOR",
				 PyInt_FromLong( 11 ) );
		PyConstant_Insert( c, "TANROT",
				 PyInt_FromLong( 10 ) );
		PyConstant_Insert( c, "TANVEL",
				 PyInt_FromLong( 9 ) );
		PyConstant_Insert( c, "SIZE",
				 PyInt_FromLong( 8 ) );
		PyConstant_Insert( c, "ROUGHE",
				 PyInt_FromLong( 7 ) );
		PyConstant_Insert( c, "ROUGH2",
				 PyInt_FromLong( 6 ) );
		PyConstant_Insert( c, "ROUGH1",
				 PyInt_FromLong( 5 ) );
		PyConstant_Insert( c, "KINK",
				 PyInt_FromLong( 4 ) );
		PyConstant_Insert( c, "CLUMP",
				 PyInt_FromLong( 3 ) );
		PyConstant_Insert( c, "LENGHT",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "VELOCITY",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "DENSITY",
				 PyInt_FromLong( 0 ) );
	}
	return VertexGroups;
}


/* create the Blender.Particle.ChildKink constant dict */

static PyObject *Particle_ChildKinkDict( void )
{
	PyObject *ChildKinks = PyConstant_New(  );

	if( ChildKinks ) {
		BPy_constant *c = ( BPy_constant * ) ChildKinks;

		PyConstant_Insert( c, "BRAID",
				 PyInt_FromLong( 4 ) );
		PyConstant_Insert( c, "WAVE",
				 PyInt_FromLong( 3 ) );
		PyConstant_Insert( c, "RADIAL",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "CURL",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "NOTHING",
				 PyInt_FromLong( 0 ) );
	}
	return ChildKinks;
}

/* create the Blender.Particle.ChildKinkAxis constant dict */

static PyObject *Particle_ChildKinkAxisDict( void )
{
	PyObject *ChildKinkAxes = PyConstant_New(  );

	if( ChildKinkAxes ) {
		BPy_constant *c = ( BPy_constant * ) ChildKinkAxes;

		PyConstant_Insert( c, "Z",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "Y",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "X",
				 PyInt_FromLong( 0 ) );
	}
	return ChildKinkAxes;
}

static PyObject *Particle_DrawAs( void )
{
	PyObject *DrawAs = PyConstant_New(  );

	if( DrawAs ) {
		BPy_constant *c = ( BPy_constant * ) DrawAs;

		PyConstant_Insert( c, "NONE",
				 PyInt_FromLong( 0 ) );
		PyConstant_Insert( c, "POINT",
				 PyInt_FromLong( 1 ) );
		PyConstant_Insert( c, "CIRCLE",
				 PyInt_FromLong( 2 ) );
		PyConstant_Insert( c, "CROSS",
				 PyInt_FromLong( 3 ) );
		PyConstant_Insert( c, "AXIS",
				 PyInt_FromLong( 4 ) );
		PyConstant_Insert( c, "LINE",
				 PyInt_FromLong( 5 ) );
		PyConstant_Insert( c, "PATH",
				 PyInt_FromLong( 6 ) );
		PyConstant_Insert( c, "OBJECT",
				 PyInt_FromLong( 7 ) );
		PyConstant_Insert( c, "GROUP",
				 PyInt_FromLong( 8 ) );
		PyConstant_Insert( c, "BILLBOARD",
				 PyInt_FromLong( 9 ) );
	}
	return DrawAs;
}

void Particle_Recalc(BPy_PartSys* self,int child){
	psys_flush_settings(self->psys->part,0,child );
}

void Particle_RecalcPsys_distr(BPy_PartSys* self,int child){
	psys_flush_settings(self->psys->part,PSYS_DISTR,child);
}

PyObject *ParticleSys_Init( void ){
	PyObject *submodule;
	PyObject *Types;
	PyObject *React;
	PyObject *EmitFrom;
	PyObject *Dist;
	PyObject *DrawAs;
	PyObject *Physics;
	PyObject *Integrator;
	PyObject *Rotation;
	PyObject *AngularV;
	PyObject *ChildTypes;
	PyObject *VertexGroups;
	PyObject *ChildKinks;
	PyObject *ChildKinkAxes;


	if( PyType_Ready( &ParticleSys_Type ) < 0)
		return NULL;

	Types = Particle_TypeDict ();
	React = Particle_ReactOnDict();
	EmitFrom = Particle_EmitFrom();
	DrawAs = Particle_DrawAs();
	Dist = Particle_DistrDict();
	Physics = Particle_PhysicsDict();
	Integrator = Particle_IntegratorDict();
	Rotation = Particle_RotationDict();
	AngularV = Particle_AngularVDict();
	VertexGroups = Particle_VertexGroupsDict();
	ChildTypes = Particle_ChildTypeDict();
	ChildKinks = Particle_ChildKinkDict();
	ChildKinkAxes = Particle_ChildKinkAxisDict();

	submodule = Py_InitModule3( "Blender.Particle", 
								M_ParticleSys_methods, M_ParticleSys_doc );

	if( Types )
		PyModule_AddObject( submodule, "TYPE", Types );
	if( React )
		PyModule_AddObject( submodule, "REACTON", React );
	if( EmitFrom )
		PyModule_AddObject( submodule, "EMITFROM", EmitFrom );
	if( Dist )
		PyModule_AddObject( submodule, "DISTRIBUTION", Dist );
	if( DrawAs )
		PyModule_AddObject( submodule, "DRAWAS", DrawAs );
	if( Physics )
		PyModule_AddObject( submodule, "PHYSICS", Physics );
	if( Integrator )
		PyModule_AddObject( submodule, "INTEGRATOR", Integrator );
	if( Rotation )
		PyModule_AddObject( submodule, "ROTATION", Rotation );
	if( AngularV )
		PyModule_AddObject( submodule, "ANGULARV", AngularV );
	if( VertexGroups )
		PyModule_AddObject( submodule, "VERTEXGROUPS", VertexGroups );
	if( ChildTypes )
		PyModule_AddObject( submodule, "CHILDTYPE", ChildTypes );
	if( ChildKinks )
		PyModule_AddObject( submodule, "CHILDKINK", ChildKinks );
	if( ChildKinkAxes )
		PyModule_AddObject( submodule, "CHILDKINKAXIS", ChildKinkAxes );

	return ( submodule );
}

static PyObject *Part_freeEdit( BPy_PartSys * self, PyObject * args ){

	if(self->psys->flag & PSYS_EDITED){
		if(self->psys->edit)
			PE_free_particle_edit(self->psys);

		self->psys->flag &= ~PSYS_EDITED;
		self->psys->recalc |= PSYS_RECALC_HAIR;

		DAG_object_flush_update(G.scene, self->object, OB_RECALC_DATA);
	}
	Py_RETURN_NONE;
}

static PyObject *Part_GetLoc( BPy_PartSys * self, PyObject * args )
{
	ParticleSystem *psys = 0L;
	Object *ob = 0L;
	PyObject *partlist,*seglist=0L;
	ParticleCacheKey **cache,*path;
	PyObject* loc = 0L;
	ParticleKey state;
	DerivedMesh* dm;
	float cfra;
	int i,j,k;
	float vm[4][4],wm[4][4];
	int	childexists = 0;
	int all = 0;
	int id = 0;

	cfra = bsystem_time(ob,(float)CFRA,0.0);

	if( !PyArg_ParseTuple( args, "|ii", &all,&id ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected two optional integers as arguments" );

	psys = self->psys;
	ob = self->object;
	
	if (!ob || !psys)
		Py_RETURN_NONE;

	G.rendering = 1;

	/* Just to create a valid rendering context */
	psys_render_set(ob,psys,vm,wm,0,0,0);

	dm = mesh_create_derived_render(ob,CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
	dm->release(dm);

	if ( !psys_check_enabled(ob,psys) ){
		G.rendering = 0;
		psys_render_restore(ob,psys);
		Particle_Recalc(self,1);
		Py_RETURN_NONE;
	}

	partlist = PyList_New( 0 );
	if( !partlist ){
		PyErr_SetString( PyExc_MemoryError, "PyList_New() failed" );
		goto error;
	}

	if (psys->part->type == PART_HAIR){
		cache = psys->pathcache;

		if ( ((self->psys->part->draw & PART_DRAW_PARENT) && (self->psys->part->childtype != 0)) || (self->psys->part->childtype == 0) ){

			for(i = 0; i < psys->totpart; i++){
				seglist = PyList_New( 0 );
				if (!seglist){
					PyErr_SetString( PyExc_MemoryError,
							"PyList_New() failed" );
					goto error;
				}

				path=cache[i];
				k = path->steps+1;
				for( j = 0; j < k ; j++, path++){
					loc = Py_BuildValue("(fff)",(double)path->co[0],
							(double)path->co[1], (double)path->co[2]);

					if (!loc){
						PyErr_SetString( PyExc_RuntimeError,
								"Couldn't build tuple" );
						goto error;
					}

					if ( (PyList_Append(seglist,loc) < 0) ){
						PyErr_SetString( PyExc_RuntimeError,
								"Couldn't append item to PyList" );
						goto error;
					}
					Py_DECREF(loc); /* PyList_Append increfs */
					loc = NULL;
				}

				if ( PyList_Append(partlist,seglist) < 0 ){
					PyErr_SetString( PyExc_RuntimeError,
							"Couldn't append item to PyList" );		
					goto error;
				}
				Py_DECREF(seglist); /* PyList_Append increfs */
				seglist = NULL;
			}
		}

		cache=psys->childcache;

		for(i = 0; i < psys->totchild; i++){
			seglist = PyList_New( 0 );
			if (!seglist){
				PyErr_SetString( PyExc_MemoryError,
						"PyList_New() failed" );
				goto error;
			}

			path=cache[i];
			k = path->steps+1;
			for( j = 0; j < k ; j++, path++ ){
				loc = Py_BuildValue("(fff)",(double)path->co[0],
						(double)path->co[1], (double)path->co[2]);

				if (!loc){
					PyErr_SetString( PyExc_RuntimeError,
							"Couldn't build tuple" );
					goto error;
				}

				if ( PyList_Append(seglist,loc) < 0){
					PyErr_SetString( PyExc_RuntimeError,
							"Couldn't append item to PyList" );
					goto error;
				}
				Py_DECREF(loc);/* PyList_Append increfs */
				loc = NULL;
			}

			if ( PyList_Append(partlist,seglist) < 0){
				PyErr_SetString( PyExc_RuntimeError,
						"Couldn't append item to PyList" );	
				goto error;
			}
			Py_DECREF(seglist); /* PyList_Append increfs */
			seglist = NULL;
		}
	} else {
		int init;
		char *fmt = NULL;

		if(id)
			fmt = "(fffi)";
		else
			fmt = "(fff)";

		if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
			childexists = 1;

		for (i = 0; i < psys->totpart + psys->totchild; i++){
			if (childexists && (i < psys->totpart))
				continue;

			state.time = cfra;
			if(psys_get_particle_state(ob,psys,i,&state,0)==0)
				init = 0;
			else
				init = 1;

			if (init){
				loc = Py_BuildValue(fmt,(double)state.co[0],
						(double)state.co[1], (double)state.co[2],i);
				
				if (!loc){
					PyErr_SetString( PyExc_RuntimeError,
							"Couldn't build tuple" );
					goto error;
				}

				if ( PyList_Append(partlist,loc) < 0 ){
					PyErr_SetString( PyExc_RuntimeError,
							"Couldn't append item to PyList" );
					goto error;
				}
				Py_DECREF(loc);
				loc = NULL;
			} else {
				if ( all && PyList_Append(partlist,Py_None) < 0 ){
					PyErr_SetString( PyExc_RuntimeError,
							"Couldn't append item to PyList" );
					goto error;
				}
			}
		}
	}

	psys_render_restore(ob,psys);
	G.rendering = 0;
	Particle_Recalc(self,1);
	return partlist;

error:
	Py_XDECREF(partlist);
	Py_XDECREF(seglist);
	Py_XDECREF(loc);
	psys_render_restore(ob,psys);
	G.rendering = 0;
	Particle_Recalc(self,1);
	return NULL;
}

static PyObject *Part_GetRot( BPy_PartSys * self, PyObject * args )
{
	ParticleSystem *psys = 0L;
	Object *ob = 0L;
	PyObject *partlist = 0L;
	PyObject* loc = 0L;
	ParticleKey state;
	DerivedMesh* dm;
	float vm[4][4],wm[4][4];
	int i;
	int childexists = 0;
	int all = 0;
	int id = 0;
    char *fmt = NULL;

	float cfra=bsystem_time(ob,(float)CFRA,0.0);

	if( !PyArg_ParseTuple( args, "|ii", &all, &id ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected two optional integers as arguments" );

	psys = self->psys;
	ob = self->object;
	
	if (!ob || !psys)
		Py_RETURN_NONE;

	G.rendering = 1;

	/* Just to create a valid rendering context */
	psys_render_set(ob,psys,vm,wm,0,0,0);

	dm = mesh_create_derived_render(ob,CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
	dm->release(dm);

	if ( !psys_check_enabled(ob,psys) ){
		G.rendering = 0;
		psys_render_restore(ob,psys);
		Particle_Recalc(self,1);
		Py_RETURN_NONE;
	}

	if (psys->part->type != PART_HAIR){
		partlist = PyList_New( 0 );

		if( !partlist ){
			PyErr_SetString( PyExc_MemoryError, "PyList_New() failed" );
			goto error;
		}

		if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
			childexists = 1;

		if(id)
			fmt = "(ffffi)";
		else
			fmt = "(ffff)";

		for (i = 0; i < psys->totpart + psys->totchild; i++){
			if (childexists && (i < psys->totpart))
				continue;

			state.time = cfra;
			if(psys_get_particle_state(ob,psys,i,&state,0)==0){
				if ( all && PyList_Append(partlist,Py_None) < 0){
					PyErr_SetString( PyExc_RuntimeError,
						"Couldn't append item to PyList" );
					goto error;
				}
			} else {
				loc = Py_BuildValue(fmt,(double)state.rot[0], (double)state.rot[1],
						(double)state.rot[2], (double)state.rot[3], i);

				if (!loc){
					PyErr_SetString( PyExc_RuntimeError,
							"Couldn't build tuple" );
					goto error;
				}
				if (PyList_Append(partlist,loc) < 0){
					PyErr_SetString ( PyExc_RuntimeError,
							"Couldn't append item to PyList" );
					goto error;
				}
				Py_DECREF(loc); /* PyList_Append increfs */
				loc = NULL;
			}
		}
	} else {
		partlist = EXPP_incr_ret( Py_None );
	}

	psys_render_restore(ob,psys);
	G.rendering = 0;
	Particle_Recalc(self,1);
	return partlist;

error:
	Py_XDECREF(partlist);
	Py_XDECREF(loc);
	psys_render_restore(ob,psys);
	G.rendering = 0;
	Particle_Recalc(self,1);
	return NULL;
}

static PyObject *Part_GetSize( BPy_PartSys * self, PyObject * args )
{
	ParticleKey state;
	ParticleSystem *psys = 0L;
	ParticleData *data;
	Object *ob = 0L;
	PyObject *partlist,*tuple=0L;
	DerivedMesh* dm;
	float vm[4][4],wm[4][4];
	float size;
	int i;
	int childexists = 0;
	int all = 0;
	int id = 0;
    char *fmt = NULL;

	float cfra=bsystem_time(ob,(float)CFRA,0.0);

	if( !PyArg_ParseTuple( args, "|ii", &all, &id ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected two optional integers as arguments" );

	psys = self->psys;
	ob = self->object;
	
	if (!ob || !psys)
		Py_RETURN_NONE;

	G.rendering = 1;

	/* Just to create a valid rendering context */
	psys_render_set(ob,psys,vm,wm,0,0,0);

	dm = mesh_create_derived_render(ob,CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
	dm->release(dm);
	data = self->psys->particles;

	if ( !psys_check_enabled(ob,psys) ){
		psys_render_restore(ob,psys);
		G.rendering = 0;
		Particle_Recalc(self,1);
		Py_RETURN_NONE;
	}

	partlist = PyList_New( 0 );

	if( !partlist ){
		PyErr_SetString( PyExc_MemoryError, "PyList_New() failed" );
		goto error;
	}

	if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
		childexists = 1;

	if(id)
		fmt = "(fi)";
	else
		fmt = "f";

	for (i = 0; i < psys->totpart + psys->totchild; i++, data++){
		if (psys->part->type != PART_HAIR){
			if (childexists && (i < psys->totpart))
				continue;

			if ( !all ){
				state.time = cfra;
				if(psys_get_particle_state(ob,psys,i,&state,0)==0)
					continue;
			}

			if (i < psys->totpart){
				size = data->size;
			} else {
				ChildParticle *cpa= &psys->child[i-psys->totpart];
				size = psys_get_child_size(psys,cpa,cfra,0);
			}

			tuple = Py_BuildValue(fmt,(double)size,i);

			if (!tuple){
				PyErr_SetString( PyExc_RuntimeError,
						"Couldn't build tuple" );
				goto error;
			}

			if (PyList_Append(partlist,tuple) < 0){
				PyErr_SetString( PyExc_RuntimeError,
						"Couldn't append item to PyList" );
				goto error;
			}
			Py_DECREF(tuple);
			tuple = NULL;
		}
	}

	psys_render_restore(ob,psys);
	G.rendering = 0;
	Particle_Recalc(self,1);
	return partlist;

error:
	Py_XDECREF(partlist);
	Py_XDECREF(tuple);
	psys_render_restore(ob,psys);
	G.rendering = 0;
	Particle_Recalc(self,1);
	return NULL;
}


static PyObject *Part_GetAge( BPy_PartSys * self, PyObject * args )
{
	ParticleKey state;
	ParticleSystem *psys = 0L;
	ParticleData *data;
	Object *ob = 0L;
	PyObject *partlist,*tuple=0L;
	DerivedMesh* dm;
	float vm[4][4],wm[4][4];
	float life;
	int i;
	int childexists = 0;
	int all = 0;
	int id = 0;
	char *fmt = NULL;

	float cfra=bsystem_time(ob,(float)CFRA,0.0);

	if( !PyArg_ParseTuple( args, "|ii", &all, &id ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected two optional integers as arguments" );

	psys = self->psys;
	ob = self->object;
	
	if (!ob || !psys)
		Py_RETURN_NONE;

	G.rendering = 1;

	/* Just to create a valid rendering context */
	psys_render_set(ob,psys,vm,wm,0,0,0);

	dm = mesh_create_derived_render(ob,CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
	dm->release(dm);
	data = self->psys->particles;

	if ( !psys_check_enabled(ob,psys) ){
		psys_render_restore(ob,psys);
		G.rendering = 0;
		Py_RETURN_NONE;
	}

	partlist = PyList_New( 0 );
	if( !partlist ){
		PyErr_SetString( PyExc_MemoryError, "PyList_New() failed" );
		goto error;
	}

	if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
		childexists = 1;

	if(id)
		fmt = "(fi)";
	else
		fmt = "f";

	for (i = 0; i < psys->totpart + psys->totchild; i++, data++){
		if (psys->part->type != PART_HAIR){

			if (childexists && (i < psys->totpart))
				continue;

			if ( !all ){
				state.time = cfra;
				if(psys_get_particle_state(ob,psys,i,&state,0)==0)
					continue;
			}

			if (i < psys->totpart){
				life = (cfra-data->time)/data->lifetime;
			} else {
				ChildParticle *cpa= &psys->child[i-psys->totpart];
				life = psys_get_child_time(psys,cpa,cfra);
			}

			tuple = Py_BuildValue(fmt,(double)life,i);

			if (!tuple){
				PyErr_SetString( PyExc_RuntimeError,
						"Couldn't build tuple" );
				goto error;
			}

			if (PyList_Append(partlist,tuple) < 0){
				PyErr_SetString( PyExc_RuntimeError,
						"Couldn't append item to PyList" );
				goto error;
			}
			Py_DECREF(tuple);
			tuple = NULL;
		}
	}

	psys_render_restore(ob,psys);
	G.rendering = 0;
	Particle_Recalc(self,1);
	return partlist;

error:
	Py_XDECREF(partlist);
	Py_XDECREF(tuple);
	psys_render_restore(ob,psys);
	G.rendering = 0;
	Particle_Recalc(self,1);
	return NULL;
}

static PyObject *Part_SetMat( BPy_PartSys * self, PyObject * args )
{
	Object *ob = self->object;
	BPy_Material *pymat;
	Material *mat;
	short index;

	if( !PyArg_ParseTuple( args, "O!", &Material_Type, &pymat ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Blender Material PyObject" );

	mat = pymat->material;

	if( ob->totcol >= MAXMAT )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "object data material lists can't have more than 16 materials" );

	index = find_material_index(ob,mat);
	if (index == 0){	/*Not found*/
		assign_material(ob,mat,ob->totcol+1);
		index = find_material_index(ob,mat);
	}

	if (index>0 && index<MAXMAT)
		self->psys->part->omat = index;

	/* since we have messed with object, we need to flag for DAG recalc */
	self->object->recalc |= OB_RECALC_OB;

	Py_RETURN_NONE;
}

static PyObject *Part_GetMat( BPy_PartSys * self, PyObject * args ){
	Material *ma;
	PyObject* mat = 0L;
	ma = give_current_material(self->object,self->psys->part->omat);
	if(!ma)
		Py_RETURN_NONE;

	mat = Material_CreatePyObject(ma);
	return mat;
}

static PyObject *Part_GetVertGroup( BPy_PartSys * self, PyObject * args ){
	PyObject *list;
	char errstr[128];
	bDeformGroup *defGroup = NULL;
	Object *obj = self->object;
	int vg_attribute = 0;
	int vg_number = 0;
	int count;
	PyObject *vg_neg;
	PyObject *vg_name;

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "particle system must be linked to an object first" );
	
	if( obj->type != OB_MESH )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "linked object is not a mesh" );
	
	if( !PyArg_ParseTuple( args, "i", &vg_attribute ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected integer argument" );
	
	if( vg_attribute < 0 || vg_attribute > PSYS_TOT_VG-1 ){
		sprintf ( errstr, "expected int argument in [0,%d]", PSYS_TOT_VG-1 );
		return EXPP_ReturnPyObjError( PyExc_TypeError, errstr );
	}

	/*search*/
	vg_number = self->psys->vgroup[vg_attribute];
	count = 1;
	defGroup = obj->defbase.first;
	while(count<vg_number && defGroup){
		defGroup = defGroup->next;
		count++;
	}

	/*vg_name*/
	if (defGroup && vg_number>0)
		vg_name = PyString_FromString( defGroup->name );
	else
		vg_name = PyString_FromString( "" );
	
	/*vg_neg*/
	vg_neg = PyInt_FromLong( ((long)( self->psys->vg_neg & (1<<vg_attribute) )) > 0 );

	list = PyList_New( 2 );
	PyList_SET_ITEM( list, 0, vg_name );
	PyList_SET_ITEM( list, 1, vg_neg );

	return list;
}

static PyObject *Part_SetVertGroup( BPy_PartSys * self, PyObject * args ){
	char errstr[128];
	bDeformGroup *defGroup;
	Object *obj = self->object;
	char *vg_name = NULL;
	int vg_attribute = 0;
	int vg_neg = 0;
	int vg_number = 0;
	int count;

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "particle system must be linked to an object first" );
	
	if( obj->type != OB_MESH )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "linked object is not a mesh" );
	
	if( !PyArg_ParseTuple( args, "sii", &vg_name, &vg_attribute, &vg_neg ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected one string and two integers arguments" );
	
	if( vg_attribute < 0 || vg_attribute > PSYS_TOT_VG-1 ){
		sprintf ( errstr, "expected int argument in [0,%d]", PSYS_TOT_VG-1 );
		return EXPP_ReturnPyObjError( PyExc_TypeError, errstr );
	}

	/*search*/
	count = 1;
	defGroup = obj->defbase.first;
	while (defGroup){
		if (strcmp(vg_name,defGroup->name)==0)
			vg_number = count;
		defGroup = defGroup->next;
		count++;
	}

	/*vgroup*/
	self->psys->vgroup[vg_attribute] = vg_number;

	/*vg_neg*/
	if (vg_neg){
		self->psys->vg_neg |= (1<<vg_attribute);
	}else{
		self->psys->vg_neg &= ~(1<<vg_attribute);
	}

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	Py_RETURN_NONE;
}


/*****************************************************************************/
/* Function:              Set/Get Seed                                       */
/*****************************************************************************/

static int Part_setSeed( BPy_PartSys * self, PyObject * args )
{
	return EXPP_setIValueRange( args, &self->psys->seed,
			0, 255, 'i' );
}

static PyObject *Part_getSeed( BPy_PartSys * self )
{
	return PyInt_FromLong( (long)( self->psys->seed ) );
}

static int Part_setType( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->type,
			0, 2, 'h' );

	psys_flush_settings( self->psys->part, PSYS_TYPE, 1 );

	return res;
}

static PyObject *Part_getType( BPy_PartSys * self )
{
	return PyInt_FromLong( (short)( self->psys->part->type ) );
}

static int Part_setResol( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->grid_res,
			0, 100, 'i' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getResol( BPy_PartSys * self )
{
	return PyInt_FromLong( ((int)( self->psys->part->grid_res )) );
}

static int Part_setStart( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->sta,
			0.0f, 100000.0f );

	psys_flush_settings(self->psys->part,PSYS_INIT,1);

	return res;
}

static PyObject *Part_getStart( BPy_PartSys * self )
{
	return PyFloat_FromDouble( (float)( self->psys->part->sta ) );
}

static int Part_setEnd( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->end,
			0.0f, 100000.0f );

	psys_flush_settings(self->psys->part,PSYS_INIT,1);

	return res;
}

static PyObject *Part_getEnd( BPy_PartSys * self )
{
	return PyFloat_FromDouble( (long)( self->psys->part->end ) );
}

static int Part_setEditable( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if(!number){
		if(self->psys->edit)
			PE_free_particle_edit(self->psys);

		self->psys->flag &= ~PSYS_EDITED;
		self->psys->recalc |= PSYS_RECALC_HAIR;

		DAG_object_flush_update(G.scene, self->object, OB_RECALC_DATA);
	}
	else
	{
		self->psys->flag |= PSYS_EDITED;
		if(G.f & G_PARTICLEEDIT)
			PE_create_particle_edit(self->object, self->psys);
	}

	return 0;
}

static PyObject *Part_getEditable( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->flag & PSYS_EDITED )) > 0  );
}

static int Part_setAmount( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->totpart,
			0, 100000, 'i' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getAmount( BPy_PartSys * self )
{
	return PyInt_FromLong( ((int)( self->psys->part->totpart )) );
}

static int Part_setMultiReact( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );


	if (number){
		self->psys->part->flag |= PART_REACT_MULTIPLE;
	}else{
		self->psys->part->flag &= ~PART_REACT_MULTIPLE;
	}

	Particle_Recalc(self,1);

	return 0;
}

static PyObject *Part_getMultiReact( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_REACT_MULTIPLE )) > 0 );
}

static int Part_setReactShape( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->reactshape,
			0.0f, 10.0f );

	Particle_Recalc(self,1);

	return res;
}

static PyObject *Part_getReactShape( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((double)( self->psys->part->reactshape )) );
}

static int Part_setSegments( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->hair_step,
			2, 50, 'h' );

	Particle_Recalc(self,1);

	return res;
}

static PyObject *Part_getSegments( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->hair_step )) );
}

static int Part_setLife( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->lifetime,
			1.0f, MAXFRAMEF );

	Particle_Recalc(self,1);

	return res;
}

static PyObject *Part_getLife( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((double)( self->psys->part->lifetime )) );
}

static int Part_setRandLife( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->randlife,
			0.0f, 2.0f );

	Particle_Recalc(self,1);

	return res;
}

static PyObject *Part_getRandLife( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((double)( self->psys->part->randlife )) );
}

static int Part_set2d( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_BOIDS_2D;
	}else{
		self->psys->part->flag &= ~PART_BOIDS_2D;
	}

	Particle_Recalc(self,1);

	return 0;
}

static PyObject *Part_get2d( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_BOIDS_2D )) > 0 );
}

static int Part_setMaxVel( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->max_vel,
			0.0f, 200.0f );

	Particle_Recalc(self,1);

	return res;
}

static PyObject *Part_getMaxVel( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((double)( self->psys->part->max_vel )) );
}

static int Part_setAvVel( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->average_vel,
			0.0f, 1.0f );

	Particle_Recalc(self,1);

	return res;
}

static PyObject *Part_getAvVel( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((double)( self->psys->part->average_vel )) );
}

static int Part_setLatAcc( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->max_lat_acc,
			0.0f, 1.0f );

	Particle_Recalc(self,1);

	return res;
}

static PyObject *Part_getLatAcc( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((double)( self->psys->part->max_lat_acc )) );
}

static int Part_setMaxTan( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->max_tan_acc,
			0.0f, 1.0f );

	Particle_Recalc(self,1);

	return res;
}

static PyObject *Part_getMaxTan( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((double)( self->psys->part->max_tan_acc )) );
}

static int Part_setGroundZ( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->groundz,
			-100.0f, 100.0f );

	Particle_Recalc(self,1);

	return res;
}

static PyObject *Part_getGroundZ( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((double)( self->psys->part->groundz )) );
}

static int Part_setOb( BPy_PartSys * self, PyObject * args )
{
	Object *obj;
	if( !BPy_Object_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected object argument" );

	obj = Object_FromPyObject(args);

	self->psys->keyed_ob = obj;

	return 0;
}

static PyObject *Part_getOb( BPy_PartSys * self )
{
	Object * obj;
	obj = self->psys->keyed_ob;
	if (!obj)
		Py_RETURN_NONE;

	return Object_CreatePyObject( obj );
}

static int Part_setRandEmission( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_TRAND;
	}else{
		self->psys->part->flag &= ~PART_TRAND;
	}

	Particle_RecalcPsys_distr(self,1);

	return 0;
}

static PyObject *Part_getRandEmission( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_TRAND )) > 0 );
}

static int Part_setParticleDist( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number < 0 || number > 3)
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument between 0 - 3" );

	self->psys->part->from = (short)number;

	Particle_RecalcPsys_distr(self,1);

	return 0;
}

static PyObject *Part_getParticleDist( BPy_PartSys * self )
{
	return PyInt_FromLong( (long)( self->psys->part->from ) );
}

static int Part_setEvenDist( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_EDISTR;
	}else{
		self->psys->part->flag &= ~PART_EDISTR;
	}

	Particle_RecalcPsys_distr(self,1);

	return 0;
}

static PyObject *Part_getEvenDist( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_EDISTR )) > 0 );
}

static int Part_setDist( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number < 0 || number > 2)
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument between 0 - 2" );

	self->psys->part->distr = (short)number;

	Particle_RecalcPsys_distr(self,1);

	return 0;
}

static PyObject *Part_getDist( BPy_PartSys * self )
{
	return PyInt_FromLong( (long)( self->psys->part->distr ) );
}

static int Part_setJitterAmount( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->jitfac,
			0.0f, 2.0f );

	Particle_RecalcPsys_distr(self,1);

	return res;
}

static PyObject *Part_getJitterAmount( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((double)( self->psys->part->jitfac )) );
}



static int Part_setPF( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->userjit,
			0, 1000, 'i' );

	Particle_RecalcPsys_distr(self,1);

	return res;
}

static PyObject *Part_getPF( BPy_PartSys * self )
{
	return PyInt_FromLong( ((short)( self->psys->part->userjit )) );
}

static int Part_setInvert( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_GRID_INVERT;
	}else{
		self->psys->part->flag &= ~PART_GRID_INVERT;
	}

	Particle_RecalcPsys_distr(self,1);

	return 0;
}

static PyObject *Part_getInvert( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_GRID_INVERT )) > 0 );
}

static int Part_setTargetOb( BPy_PartSys * self, PyObject * args )
{
	Object *obj;
	
	if( !BPy_Object_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected object argument" );

	obj = Object_FromPyObject(args);

	self->psys->target_ob = obj;

	return 0;
}

static PyObject *Part_getTargetOb( BPy_PartSys * self )
{
	Object * obj;
	obj = self->psys->target_ob;
	if (!obj)
		Py_RETURN_NONE;

	return Object_CreatePyObject( obj );
}



PyObject *Part_getDupOb( BPy_PartSys * self )
{
	Object * obj;
	obj = self->psys->part->dup_ob;
	if (!obj)
		Py_RETURN_NONE;

	return Object_CreatePyObject( obj );
}

static int Part_setTargetPsys( BPy_PartSys * self, PyObject * args ){
	int tottpsys;
	int res;
	Object *tob=0;
	ParticleSystem *psys = self->psys;
	Object *ob;

	ob = self->object;

	if(psys->target_ob)
		tob=psys->target_ob;
	else
		tob=ob;

	tottpsys = BLI_countlist(&tob->particlesystem);

	res = EXPP_setIValueRange( args, &self->psys->target_psys, 0, tottpsys, 'h' );

	if( ( psys = psys_get_current(ob) ) ){
		if(psys->keyed_ob==ob || psys->target_ob==ob){
			if(psys->keyed_ob==ob)
				psys->keyed_ob=NULL;
			else
				psys->target_ob=NULL;
		}
		else{
			DAG_scene_sort(G.scene);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		}
	}

	return res;
}

static PyObject *Part_getTargetPsys( BPy_PartSys * self ){
	return PyInt_FromLong( (short)( self->psys->target_psys ) );
}

static int Part_setRenderObject( BPy_PartSys * self, PyObject * args )
{
	int number,nr;
	ParticleSystem *psys = 0L;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->draw |= PART_DRAW_EMITTER;
	}else{
		self->psys->part->draw &= ~PART_DRAW_EMITTER;
	}

	/* check need for dupliobjects */
	nr=0;
	for(psys=self->object->particlesystem.first; psys; psys=psys->next){
		if(ELEM(psys->part->draw_as,PART_DRAW_OB,PART_DRAW_GR))
			nr++;
	}
	if(nr)
		self->object->transflag |= OB_DUPLIPARTS;
	else
		self->object->transflag &= ~OB_DUPLIPARTS;

	return 0;
}

static PyObject *Part_getRenderObject( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->draw & PART_DRAW_EMITTER )) > 0 );
}

static int Part_setRenderMaterialColor( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->draw |= PART_DRAW_MAT_COL;
	}else{
		self->psys->part->draw &= ~PART_DRAW_MAT_COL;
	}

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return 0;
}

static PyObject *Part_getRenderMaterialColor( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->draw & PART_DRAW_MAT_COL )) > 0 );
}

static int Part_setRenderParents( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->draw |= PART_DRAW_PARENT;
	}else{
		self->psys->part->draw &= ~PART_DRAW_PARENT;
	}

	return 0;
}

static PyObject *Part_getRenderParents( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->draw & PART_DRAW_PARENT )) > 0 );
}

static int Part_setRenderUnborn( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_UNBORN;
	}else{
		self->psys->part->flag &= ~PART_UNBORN;
	}

	return 0;
}

static PyObject *Part_getRenderUnborn( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_UNBORN )) > 0 );
}

static int Part_setRenderDied( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_DIED;
	}else{
		self->psys->part->flag &= ~PART_DIED;
	}

	return 0;
}

static int Part_setRenderMaterialIndex( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->omat,
			1, 16, 'i' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getRenderMaterialIndex( BPy_PartSys * self )
{
	return PyInt_FromLong( ((int)( self->psys->part->omat )) );
}

static PyObject *Part_getRenderDied( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_DIED )) > 0 );
}

static int Part_setParticleDisp( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->disp,
			0, 100, 'i' );

	Particle_Recalc(self,0);


	return res;
}

static PyObject *Part_getParticleDisp( BPy_PartSys * self )
{
	return PyInt_FromLong( ((short)( self->psys->part->disp )) );
}

static int Part_setStep( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->draw_step,
			0, 7, 'i' );

	Particle_Recalc(self,1);


	return res;
}

static PyObject *Part_getStep( BPy_PartSys * self )
{
	return PyInt_FromLong( ((short)( self->psys->part->draw_step )) );
}

static int Part_setRenderStep( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->ren_step,
			0, 7, 'i' );

	/*Particle_Recalc(self,1);*/


	return res;
}

static PyObject *Part_getRenderStep( BPy_PartSys * self )
{
	return PyInt_FromLong( ((short)( self->psys->part->ren_step )) );
}

static PyObject *Part_getDrawAs( BPy_PartSys * self )
{
	return PyInt_FromLong( (long)( self->psys->part->draw_as ) );
}

static int Part_setPhysType( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->phystype,
			0, 3, 'h' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getPhysType( BPy_PartSys * self )
{
	return PyInt_FromLong( (short)( self->psys->part->phystype ) );
}

static int Part_setIntegrator( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->integrator,
			0, 2, 'h' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getIntegrator( BPy_PartSys * self )
{
	return PyInt_FromLong( (short)( self->psys->part->integrator ) );
}

static int Part_setIniVelObject( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->obfac,
			-1.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getIniVelObject( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->obfac )) );
}

static int Part_setIniVelNormal( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->normfac,
			-200.0, 200.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getIniVelNormal( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->normfac )) );
}

static int Part_setIniVelRandom( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->randfac,
			0.0, 200.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getIniVelRandom( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->randfac )) );
}

static int Part_setIniVelTan( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->tanfac,
			-200.0, 200.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getIniVelTan( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->tanfac )) );
}

static int Part_setIniVelRot( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->tanphase,
			-1.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getIniVelRot( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->tanphase )) );
}

static int Part_setIniVelPart( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->partfac,
			-10.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getIniVelPart( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->partfac )) );
}

static int Part_setIniVelReact( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->reactfac,
			-10.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getIniVelReact( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->reactfac )) );
}

static int Part_setRotation( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->rotmode,
			0, 8, 'h' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getRotation( BPy_PartSys * self )
{
	return PyInt_FromLong( (short)( self->psys->part->rotmode ) );
}

static int Part_setRotDynamic( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_ROT_DYN;
	}else{
		self->psys->part->flag &= ~PART_ROT_DYN;
	}

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return 0;
}

static PyObject *Part_getRotDynamic( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_ROT_DYN )) > 0 );
}

static int Part_setRotRandom( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->randrotfac,
			0.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getRotRandom( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->randrotfac )) );
}

static int Part_setRotPhase( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->phasefac,
			-1.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getRotPhase( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->phasefac )) );
}

static int Part_setRotPhaseR( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->randphasefac,
			0.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getRotPhaseR( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->randphasefac )) );
}

static int Part_setRotAngularV( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->avemode,
			0, 2, 'h' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getRotAngularV( BPy_PartSys * self )
{
	return PyInt_FromLong( ((int)( self->psys->part->avemode )) );
}

static int Part_setRotAngularVAm( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->avefac,
			-200.0, 200.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getRotAngularVAm( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->avefac )) );
}

static int Part_setGlobAccX( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->acc[0],
			-200.0, 200.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getGlobAccX( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->acc[0] )) );
}

static int Part_setGlobAccY( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->acc[1],
			-200.0, 200.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getGlobAccY( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->acc[1] )) );
}

static int Part_setGlobAccZ( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->acc[2],
			-200.0, 200.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getGlobAccZ( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->acc[2] )) );
}

static int Part_setGlobDrag( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->dragfac,
			0.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getGlobDrag( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->dragfac )) );
}

static int Part_setGlobBrown( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->brownfac,
			0.0, 200.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getGlobBrown( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->brownfac )) );
}

static int Part_setGlobDamp( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->dampfac,
			0.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getGlobDamp( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->dampfac )) );
}

static int Part_setChildAmount( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->child_nbr,
			0, MAX_PART_CHILDREN, 'i' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildAmount( BPy_PartSys * self )
{
	return PyInt_FromLong( ((int)( self->psys->part->child_nbr )) );
}

static int Part_setChildType( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->childtype,
			0, 2, 'h' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildType( BPy_PartSys * self )
{
	return PyInt_FromLong( (short)( self->psys->part->childtype ) );
}

static int Part_setChildRenderAmount( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->ren_child_nbr,
			0, MAX_PART_CHILDREN, 'i' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRenderAmount( BPy_PartSys * self )
{
	return PyInt_FromLong( ((int)( self->psys->part->ren_child_nbr )) );
}

static int Part_setChildRadius( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->childrad,
			0.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRadius( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->childrad )) );
}

static int Part_setChildRoundness( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->childflat,
			0.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRoundness( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->childflat )) );
}

static int Part_setChildClumping( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->clumpfac,
			-1.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildClumping( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->clumpfac )) );
}

static int Part_setChildShape( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->clumppow,
			-0.999, 0.999 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildShape( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->clumppow )) );
}

static int Part_setChildSize( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->childsize,
			0.01, 100.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildSize( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->childsize )) );
}

static int Part_setChildRandom( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->childrandsize,
			0.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRandom( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->childrandsize )) );
}

static int Part_setChildRough1( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->rough1,
			0.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRough1( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->rough1 )) );
}

static int Part_setChildRough1Size( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->rough1_size,
			0.01, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRough1Size( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->rough1_size )) );
}

static int Part_setChildRough2( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->rough2,
			0.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRough2( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->rough2 )) );
}

static int Part_setChildRough2Size( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->rough2_size,
			0.01, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRough2Size( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->rough2_size )) );
}

static int Part_setChildRough2Thres( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->rough2_thres,
			0.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRough2Thres( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->rough2_thres )) );
}

static int Part_setChildRoughE( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->rough_end,
			0.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRoughE( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->rough_end )) );
}

static int Part_setChildRoughEShape( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->rough_end_shape,
			0.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildRoughEShape( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->rough_end_shape )) );
}

static int Part_setChildKink( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->kink,
			0, 4, 'h' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildKink( BPy_PartSys * self )
{
	return PyInt_FromLong( (short)( self->psys->part->kink ) );
}

static int Part_setChildKinkAxis( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setIValueRange( args, &self->psys->part->kink_axis,
			0, 2, 'h' );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildKinkAxis( BPy_PartSys * self )
{
	return PyInt_FromLong( (short)( self->psys->part->kink_axis ) );
}

static int Part_setChildKinkFreq( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->kink_freq,
			0.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildKinkFreq( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->kink_freq )) );
}

static int Part_setChildKinkShape( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->kink_shape,
			-0.999, 0.999 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildKinkShape( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->kink_shape )) );
}

static int Part_setChildKinkAmp( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->kink_amp,
			0.0, 10.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildKinkAmp( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->kink_amp )) );
}

static int Part_setChildBranch( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_BRANCHING;
	}else{
		self->psys->part->flag &= ~PART_BRANCHING;
	}

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return 0;
}

static PyObject *Part_getChildBranch( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_BRANCHING )) > 0 );
}

static int Part_setChildBranchAnim( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_ANIM_BRANCHING;
	}else{
		self->psys->part->flag &= ~PART_ANIM_BRANCHING;
	}

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return 0;
}

static PyObject *Part_getChildBranchAnim( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_ANIM_BRANCHING )) > 0 );
}

static int Part_setChildBranchSymm( BPy_PartSys * self, PyObject * args )
{
	int number;

	if( !PyInt_Check( args ) )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected int argument" );

	number = PyInt_AS_LONG( args );

	if (number){
		self->psys->part->flag |= PART_SYMM_BRANCHING;
	}else{
		self->psys->part->flag &= ~PART_SYMM_BRANCHING;
	}

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return 0;
}

static PyObject *Part_getChildBranchSymm( BPy_PartSys * self )
{
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_SYMM_BRANCHING )) > 0 );
}

static int Part_setChildBranchThre( BPy_PartSys * self, PyObject * args )
{
	int res = EXPP_setFloatRange( args, &self->psys->part->branch_thres,
			0.0, 1.0 );

	psys_flush_settings( self->psys->part, PSYS_ALLOC, 1 );

	return res;
}

static PyObject *Part_getChildBranchThre( BPy_PartSys * self )
{
	return PyFloat_FromDouble( ((float)( self->psys->part->branch_thres )) );
}
