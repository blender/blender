/**
 * KX_CDActuator.cpp
 *
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "KX_CDActuator.h"
#include "KX_GameObject.h"
#include <iostream>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */
KX_CDActuator::KX_CDActuator(SCA_IObject* gameobject,
							 KX_CDACT_TYPE type,
							 int track,
							 short start,
							 short end)
							 : SCA_IActuator(gameobject)
{
	m_type = type;
	m_track = track;
	m_lastEvent = true;
	m_isplaying = false;
	m_startFrame = start;
	m_endFrame = end;
}



KX_CDActuator::~KX_CDActuator()
{
}


/* hmmm, do we want this? */
CValue* KX_CDActuator::GetReplica()
{
	KX_CDActuator* replica = new KX_CDActuator(*this);
	replica->ProcessReplica();
	return replica;
};



bool KX_CDActuator::Update()
{
	bool result = false;
	bool bNegativeEvent = IsNegativeEvent();

	RemoveAllEvents();

	if (!bNegativeEvent)
	{
		switch (m_type)
		{
		default:
			// implement me !!
			break;
		}
	}
	return result;
}



/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_CDActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_SoundActuator",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_CDActuator::Methods[] = {
	// Deprecated ----->
	{"setGain",(PyCFunction) KX_CDActuator::sPySetGain,METH_VARARGS,NULL},
	{"getGain",(PyCFunction) KX_CDActuator::sPyGetGain,METH_VARARGS,NULL},
	// <-----
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, startCD),
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, pauseCD),
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, resumeCD),
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, stopCD),
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, playAll),
	KX_PYMETHODTABLE_O(KX_CDActuator, playTrack),
	{NULL,NULL,NULL,NULL} //Sentinel
};

PyAttributeDef KX_CDActuator::Attributes[] = {
	KX_PYATTRIBUTE_INT_RW("track", 1, 99, false, KX_CDActuator, m_track),
	{ NULL }	//Sentinel
};

int KX_CDActuator::pyattr_setGain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CDActuator* act = static_cast<KX_CDActuator*>(self);
	return PY_SET_ATTR_SUCCESS;
}

KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, startCD,
"startCD()\n"
"\tStarts the CD playing.\n")
{
	Py_RETURN_NONE;
}


KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, pauseCD,
"pauseCD()\n"
"\tPauses the CD playing.\n")
{
	Py_RETURN_NONE;
}


KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, resumeCD,
"resumeCD()\n"
"\tResumes the CD playing.\n")
{
	Py_RETURN_NONE;
}


KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, stopCD,
"stopCD()\n"
"\tStops the CD playing.\n")
{
	Py_RETURN_NONE;
}


KX_PYMETHODDEF_DOC_O(KX_CDActuator, playTrack,
"playTrack(trackNumber)\n"
"\tPlays the track selected.\n")
{
	if (PyLong_Check(value)) {
		int track = PyLong_AsSsize_t(value);
	}
	Py_RETURN_NONE;
}



KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, playAll,
"playAll()\n"
"\tPlays the CD from the beginning.\n")
{
	Py_RETURN_NONE;
}

// Deprecated ----->
PyObject* KX_CDActuator::PySetGain(PyObject* args)
{
	float gain = 1.0;
	ShowDeprecationWarning("setGain()", "the volume property");
	if (!PyArg_ParseTuple(args, "f:setGain", &gain))
		return NULL;

	Py_RETURN_NONE;
}



PyObject* KX_CDActuator::PyGetGain(PyObject* args)
{
	float gain = 1.0;
	ShowDeprecationWarning("getGain()", "the volume property");
	PyObject* result = PyFloat_FromDouble(gain);

	return result;
}
// <-----
