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
static PyObject *M_ParticleSys_New( PyObject * self, PyObject * args );
static PyObject *M_ParticleSys_Get( PyObject * self, PyObject * args );

/* Particle Methods */
static PyObject *Part_freeEdit( BPy_PartSys * self, PyObject * args );
static PyObject *Part_GetLoc( BPy_PartSys * self, PyObject * args );
static PyObject *Part_GetRot( BPy_PartSys * self, PyObject * args );
static PyObject *Part_GetMat( BPy_PartSys * self, PyObject * args );
static PyObject *Part_GetSize( BPy_PartSys * self, PyObject * args );
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
static int Part_setStep( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getStep( BPy_PartSys * self );
static int Part_setRenderStep( BPy_PartSys * self, PyObject * args );
static PyObject *Part_getRenderStep( BPy_PartSys * self );
static PyObject *Part_getDupOb( BPy_PartSys * self );
static PyObject *Part_getDrawAs( BPy_PartSys * self );
static PyObject *Part_GetAge( BPy_PartSys * self, PyObject * args );

/*****************************************************************************/
/* Python Effect_Type callback function prototypes:                           */
/*****************************************************************************/
static PyObject *ParticleSys_repr( void );

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
   	{"getMat", ( PyCFunction ) Part_GetMat,
	 METH_NOARGS, "() - Get particles material"},
   	{"getSize", ( PyCFunction ) Part_GetSize,
	 METH_VARARGS, "() - Get particles size in a list"},
    	{"getAge", ( PyCFunction ) Part_GetAge,
	 METH_VARARGS, "() - Get particles life in a list"},
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
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python method structure definition for Blender.Particle module:           */
/*****************************************************************************/
static struct PyMethodDef M_ParticleSys_methods[] = {
	{"New", ( PyCFunction ) M_ParticleSys_New, METH_VARARGS, M_ParticleSys_New_doc},
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
/* Description: This is a callback function for the BPy_Effect type. It      */
/*              builds a meaninful string to represent effcte objects.       */
/*****************************************************************************/

static PyObject *ParticleSys_repr( void )
{
	return PyString_FromString( "ParticleSys" );
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


PyObject *M_ParticleSys_New( PyObject * self, PyObject * args ){
	ParticleSystem *psys = 0;
	ParticleSystem *rpsys = 0;
	ModifierData *md;
	ParticleSystemModifierData *psmd;
	Object *ob = NULL;
	char *name = NULL;
	ID *id;
	int nr;

	if( !PyArg_ParseTuple( args, "s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected string argument" );

	for( ob = G.main->object.first; ob; ob = ob->id.next )
		if( !strcmp( name, ob->id.name + 2 ) )
			break;

	if( !ob )
		return EXPP_ReturnPyObjError( PyExc_AttributeError, 
			"object does not exist" );

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
	ParticleSettings *psys_iter;
	char *name = NULL;
#if 0

	ParticleSystem *blparticlesys = 0;
	Object *ob;

	PyObject *partsyslist,*current;
#endif
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
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
						   "Particle System '%s' not found", name);
			return EXPP_ReturnPyObjError( PyExc_NameError, error_msg );
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
#if 0
			pyobj = ParticleSystem_CreatePyObject( psys_iter);
			if( !pyobj){
				Py_DECREF( pylist );
				return EXPP_ReturnPyObjError(
					PyExc_MemoryError, 
					"could not create ParticleSystem PyObject");
			}
			PyList_SET_ITEM( pylist, index, pyobj);
#endif
			printf("name is %s\n", psys_iter->id.name+2);
			psys_iter = psys_iter->id.next;
			index++;
		}

		return pylist;
			
	}
			
		

#if 0

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

	if( PyType_Ready( &ParticleSys_Type ) < 0)
		return NULL;

	Types = Particle_TypeDict ();
	React = Particle_ReactOnDict();
	EmitFrom = Particle_EmitFrom();
	DrawAs = Particle_DrawAs();
	Dist = Particle_DistrDict();

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

static PyObject *Part_GetLoc( BPy_PartSys * self, PyObject * args ){
	ParticleSystem *psys = 0L;
	Object *ob = 0L;
	PyObject *partlist,*seglist;
	PyObject* loc = 0L;
	ParticleCacheKey **cache,*path;
	ParticleKey state;
	float cfra=bsystem_time(ob,(float)CFRA,0.0);
	int i,j,k;
	int	childexists = 0;
	int all = 0;
	int id = 0;

	if( !PyArg_ParseTuple( args, "|ii", &all,&id ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected one optional integer as argument" );

	psys = self->psys;
	ob = self->object;
	
	if (!ob || !psys)
		Py_RETURN_NONE;

	if (psys->part->type == 2){
		cache=psys->pathcache;

		/* little hack to calculate hair steps in render mode */
		psys->renderdata = (void*)(int)1;

		psys_cache_paths(ob, psys, cfra, 1);

		psys->renderdata = NULL;

		partlist = PyList_New( 0 );
		if( !partlist )
			return EXPP_ReturnPyObjError( PyExc_MemoryError, "PyList() failed" );

		for(i = 0; i < psys->totpart; i++){
			path=cache[i];
			seglist = PyList_New( 0 );
			k = path->steps+1;
			for( j = 0; j < k ; j++){
				loc = PyTuple_New(3);

				PyTuple_SetItem(loc,0,PyFloat_FromDouble((double)path->co[0]));
				PyTuple_SetItem(loc,1,PyFloat_FromDouble((double)path->co[1]));
				PyTuple_SetItem(loc,2,PyFloat_FromDouble((double)path->co[2]));

				if ( (PyList_Append(seglist,loc) < 0) ){
					Py_DECREF(seglist);
					Py_DECREF(partlist);
					Py_XDECREF(loc);
					return EXPP_ReturnPyObjError( PyExc_RuntimeError,
							"Couldn't append item to PyList" );
				}

				path++;
			}

			if ( PyList_Append(partlist,seglist) < 0 ){
				Py_DECREF(seglist);
				Py_DECREF(partlist);
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"Couldn't append item to PyList" );		
			}
		}

		cache=psys->childcache;

		for(i = 0; i < psys->totchild; i++){
			path=cache[i];
			seglist = PyList_New( 0 );
			k = path->steps+1;
			for( j = 0; j < k ; j++){
				loc = PyTuple_New(3);

				PyTuple_SetItem(loc,0,PyFloat_FromDouble((double)path->co[0]));
				PyTuple_SetItem(loc,1,PyFloat_FromDouble((double)path->co[1]));
				PyTuple_SetItem(loc,2,PyFloat_FromDouble((double)path->co[2]));

				if ( PyList_Append(seglist,loc) < 0){
					Py_DECREF(partlist);
					Py_XDECREF(loc);
					return EXPP_ReturnPyObjError( PyExc_RuntimeError,
							"Couldn't append item to PyList" );
				}

				path++;
			}

			if ( PyList_Append(partlist,seglist) < 0){
				Py_DECREF(partlist);
				Py_XDECREF(loc);
				return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"Couldn't append item to PyList" );	
			}
		}
		
	} else {
		int init;
		partlist = PyList_New( 0 );
		if( !partlist )
			return EXPP_ReturnPyObjError( PyExc_MemoryError, "PyList() failed" );

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
				if (!id)
					loc = PyTuple_New(3);
				else
					loc = PyTuple_New(4);
				PyTuple_SetItem(loc,0,PyFloat_FromDouble((double)state.co[0]));
				PyTuple_SetItem(loc,1,PyFloat_FromDouble((double)state.co[1]));
				PyTuple_SetItem(loc,2,PyFloat_FromDouble((double)state.co[2]));
				if (id)
					PyTuple_SetItem(loc,3,PyInt_FromLong(i));

				if ( PyList_Append(partlist,loc) < 0 ){
					Py_DECREF(partlist);
					Py_XDECREF(loc);
					return EXPP_ReturnPyObjError( PyExc_RuntimeError,
								"Couldn't append item to PyList" );
				}
			}
			else {
				if ( all ){
					if ( PyList_Append(partlist,Py_None) < 0 ){
						Py_DECREF(partlist);
						return EXPP_ReturnPyObjError( PyExc_RuntimeError,
									"Couldn't append item to PyList" );
					}
				}
			}
		}
	}
	return partlist;
}

static PyObject *Part_GetRot( BPy_PartSys * self, PyObject * args ){
	ParticleSystem *psys = 0L;
	Object *ob = 0L;
	PyObject *partlist = 0L;
	PyObject* loc = 0L;
	ParticleKey state;
	int i;
	int childexists = 0;
	int all = 0;
	int id = 0;

	float cfra=bsystem_time(ob,(float)CFRA,0.0);

	if( !PyArg_ParseTuple( args, "|ii", &all, &id ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected one optional integer as argument" );

	psys = self->psys;
	ob = self->object;
	
	if (!ob || !psys)
		Py_RETURN_NONE;

	if (psys->part->type != 2){
		partlist = PyList_New( 0 );

		if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
			childexists = 1;

		for (i = 0; i < psys->totpart + psys->totchild; i++){
			if (childexists && (i < psys->totpart))
				continue;

			state.time = cfra;
			if(psys_get_particle_state(ob,psys,i,&state,0)==0){
				if ( all ){
					PyList_Append(partlist,Py_None);
					continue;
				} else {
					continue;
				}
			}
			if (!id)
				loc = PyTuple_New(4);
			else
				loc = PyTuple_New(5);
			PyTuple_SetItem(loc,0,PyFloat_FromDouble((double)state.rot[0]));
			PyTuple_SetItem(loc,1,PyFloat_FromDouble((double)state.rot[1]));
			PyTuple_SetItem(loc,2,PyFloat_FromDouble((double)state.rot[2]));
			PyTuple_SetItem(loc,3,PyFloat_FromDouble((double)state.rot[3]));
			if (id)
				PyTuple_SetItem(loc,4,PyInt_FromLong(i));
			PyList_Append(partlist,loc);
		}
	}
	return partlist;
}

static PyObject *Part_GetSize( BPy_PartSys * self, PyObject * args ){
	ParticleKey state;
	ParticleSystem *psys = 0L;
	ParticleData *data;
	Object *ob = 0L;
	PyObject *partlist,*tuple;
	PyObject* siz = 0L;
	float size;
	int i;
	int childexists = 0;
	int all = 0;
	int id = 0;

	float cfra=bsystem_time(ob,(float)CFRA,0.0);

	if( !PyArg_ParseTuple( args, "|ii", &all, &id ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected one optional integer as argument" );

	data = self->psys->particles;

	psys = self->psys;
	ob = self->object;
	
	if (!ob || !psys)
		Py_RETURN_NONE;

		partlist = PyList_New( 0 );

		if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
			childexists = 1;

		for (i = 0; i < psys->totpart + psys->totchild; i++, data++){
		if (psys->part->type != 2){
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
			if (id){
				tuple = PyTuple_New(2);
				PyTuple_SetItem(tuple,0,PyFloat_FromDouble((double)size));
				PyTuple_SetItem(tuple,1,PyInt_FromLong(i));
				PyList_Append(partlist,tuple);
			} else {
				siz = PyFloat_FromDouble((double)size);
				PyList_Append(partlist,siz);
			}
		}
	}
	return partlist;
}


static PyObject *Part_GetAge( BPy_PartSys * self, PyObject * args ){
	ParticleKey state;
	ParticleSystem *psys = 0L;
	ParticleData *data;
	Object *ob = 0L;
	PyObject *partlist,*tuple;
	PyObject* lif = 0L;
	float life;
	int i;
	int childexists = 0;
	int all = 0;
	int id = 0;

	float cfra=bsystem_time(ob,(float)CFRA,0.0);

	if( !PyArg_ParseTuple( args, "|ii", &all, &id ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected one optional integer as argument" );

	data = self->psys->particles;

	psys = self->psys;
	ob = self->object;
	
	if (!ob || !psys)
		Py_RETURN_NONE;

		partlist = PyList_New( 0 );

		if (psys->totchild > 0 && !(psys->part->draw & PART_DRAW_PARENT))
			childexists = 1;

		for (i = 0; i < psys->totpart + psys->totchild; i++, data++){
		if (psys->part->type != 2){

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
			if (id){
				tuple = PyTuple_New(2);
				PyTuple_SetItem(tuple,0,PyFloat_FromDouble((double)life));
				PyTuple_SetItem(tuple,1,PyInt_FromLong(i));
				PyList_Append(partlist,tuple);
			} else {
				lif = PyFloat_FromDouble((double)life);
				PyList_Append(partlist,lif);
			}
		}
	}
	return partlist;
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

	if( !PyInt_Check( args ) ) {
		char errstr[128];
		sprintf ( errstr, "expected int argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

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

	if( !PyInt_Check( args ) ) {
		char errstr[128];
		sprintf ( errstr, "expected int argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

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

	if( !PyInt_Check( args ) ) {
		char errstr[128];
		sprintf ( errstr, "expected int argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

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
	if( !BPy_Object_Check( args ) ) {
		char errstr[128];
		sprintf ( errstr, "expected object argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

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

	if( !PyInt_Check( args ) ) {
		char errstr[128];
		sprintf ( errstr, "expected int argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

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
	return PyInt_FromLong( ((long)( self->psys->part->flag & PART_BOIDS_2D )) > 0 );
}

static int Part_setParticleDist( BPy_PartSys * self, PyObject * args )
{
	int number;
	char errstr[128];

	if( !PyInt_Check( args ) ) {
		sprintf ( errstr, "expected int argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

	number = PyInt_AS_LONG( args );

	if (number < 0 || number > 3){
		sprintf ( errstr, "expected int argument between 0 - 3" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

	self->psys->part->from = number;

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

	if( !PyInt_Check( args ) ) {
		char errstr[128];
		sprintf ( errstr, "expected int argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

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
	char errstr[128];

	if( !PyInt_Check( args ) ) {
		sprintf ( errstr, "expected int argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

	number = PyInt_AS_LONG( args );

	if (number < 0 || number > 2){
		sprintf ( errstr, "expected int argument between 0 - 2" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

	self->psys->part->distr = number;

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

	if( !PyInt_Check( args ) ) {
		char errstr[128];
		sprintf ( errstr, "expected int argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

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
	if( !BPy_Object_Check( args ) ) {
		char errstr[128];
		sprintf ( errstr, "expected object argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

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

	if((psys=psys_get_current(ob))){
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

	if( !PyInt_Check( args ) ) {
		char errstr[128];
		sprintf ( errstr, "expected int argument" );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

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
