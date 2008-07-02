/**
 * Generate random pulses
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
 */

#include "SCA_RandomSensor.h"
#include "SCA_EventManager.h"
#include "SCA_RandomEventManager.h"
#include "SCA_LogicManager.h"
#include "ConstExpr.h"
#include <iostream>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_RandomSensor::SCA_RandomSensor(SCA_EventManager* eventmgr, 
				 SCA_IObject* gameobj, 
				 int startseed,
  				 PyTypeObject* T)
    : SCA_ISensor(gameobj,eventmgr, T)
{
	// m_basegenerator is never deleted => memory leak
	m_basegenerator = new SCA_RandomNumberGenerator(startseed);
	Init();
}



SCA_RandomSensor::~SCA_RandomSensor() 
{
    /* Nothing to be done here. */
}

void SCA_RandomSensor::Init()
{
    m_iteration  = 0;
	m_interval = 0;
	m_lastdraw   = false;
    m_currentDraw = m_basegenerator->Draw();
}


CValue* SCA_RandomSensor::GetReplica()
{
	CValue* replica = new SCA_RandomSensor(*this);
	// replication copies m_basegenerator pointer => share same generator
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}



bool SCA_RandomSensor::IsPositiveTrigger()
{ 
	return (m_invert !=m_lastdraw);
}


bool SCA_RandomSensor::Evaluate(CValue* event)
{
    /* Random generator is the generator from Line 25 of Table 1 in          */
    /* [KNUTH 1981, The Art of Computer Programming Vol. 2                   */
    /* (2nd Ed.), pp102]                                                     */
    /* It's a very simple max. length sequence generator. We can             */
    /* draw 32 bool values before having to generate the next                */
    /* sequence value. There are some theorems that will tell you            */
    /* this is a reasonable way of generating bools. Check Knuth.            */
    /* Furthermore, we only draw each <delay>-eth frame.                     */

	bool evaluateResult = false;

	if (++m_interval > m_pulse_frequency) {
	    bool drawResult = false;
		m_interval = 0;
		if (m_iteration > 31) {
			m_currentDraw = m_basegenerator->Draw();
			drawResult = (m_currentDraw & 0x1) == 0;
			m_iteration = 1;
		} else {
			drawResult = ((m_currentDraw >> m_iteration) & 0x1) == 0;
			m_iteration++;
		}
		evaluateResult = drawResult != m_lastdraw;
		m_lastdraw = drawResult;
	}
    
    /* now pass this result to some controller */
	return evaluateResult;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_RandomSensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"SCA_RandomSensor",
	sizeof(SCA_RandomSensor),
	0,
	PyDestructor,
	0,
	__getattr,
	__setattr,
	0, //&MyPyCompare,
	__repr,
	0, //&cvalue_as_number,
	0,
	0,
	0,
	0
};

PyParentObject SCA_RandomSensor::Parents[] = {
	&SCA_RandomSensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef SCA_RandomSensor::Methods[] = {
	{"setSeed",     (PyCFunction) SCA_RandomSensor::sPySetSeed, METH_VARARGS, SetSeed_doc},
	{"getSeed",     (PyCFunction) SCA_RandomSensor::sPyGetSeed, METH_VARARGS, GetSeed_doc},
	{"getLastDraw", (PyCFunction) SCA_RandomSensor::sPyGetLastDraw, METH_VARARGS, GetLastDraw_doc},
	{NULL,NULL} //Sentinel
};

PyObject* SCA_RandomSensor::_getattr(const STR_String& attr) {
	_getattr_up(SCA_ISensor);
}

/* 1. setSeed                                                            */
char SCA_RandomSensor::SetSeed_doc[] = 
"setSeed(seed)\n"
"\t- seed: integer\n"
"\tSet the initial seed of the generator. Equal seeds produce\n"
"\tequal series. If the seed is 0, the generator will produce\n"
"\tthe same value on every call.\n";
PyObject* SCA_RandomSensor::PySetSeed(PyObject* self, PyObject* args, PyObject* kwds) {
	long seedArg;
	if(!PyArg_ParseTuple(args, "i", &seedArg)) {
		return NULL;
	}
	
	m_basegenerator->SetSeed(seedArg);
	
	Py_Return;
}

/* 2. getSeed                                                            */
char SCA_RandomSensor::GetSeed_doc[] = 
"getSeed()\n"
"\tReturns the initial seed of the generator. Equal seeds produce\n"
"\tequal series.\n";
PyObject* SCA_RandomSensor::PyGetSeed(PyObject* self, PyObject* args, PyObject* kwds) {
	return PyInt_FromLong(m_basegenerator->GetSeed());
}

/* 3. getLastDraw                                                            */
char SCA_RandomSensor::GetLastDraw_doc[] = 
"getLastDraw()\n"
"\tReturn the last value that was drawn.\n";
PyObject* SCA_RandomSensor::PyGetLastDraw(PyObject* self, PyObject* args, PyObject* kwds) {
	return PyInt_FromLong(m_lastdraw);
}

/* eof */
