/*
 * Set random/camera stuff
 *
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

/** \file gameengine/GameLogic/SCA_RandomActuator.cpp
 *  \ingroup gamelogic
 */


#include <stddef.h>

#include "BoolValue.h"
#include "IntValue.h"
#include "FloatValue.h"
#include "SCA_IActuator.h"
#include "SCA_RandomActuator.h"
#include "math.h"
#include "MT_Transform.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_RandomActuator::SCA_RandomActuator(SCA_IObject *gameobj, 
									 long seed,
									 SCA_RandomActuator::KX_RANDOMACT_MODE mode,
									 float para1,
									 float para2,
									 const STR_String &propName)
	: SCA_IActuator(gameobj, KX_ACT_RANDOM),
	  m_propname(propName),
	  m_parameter1(para1),
	  m_parameter2(para2),
	  m_distribution(mode)
{
	m_base = new SCA_RandomNumberGenerator(seed);
	m_counter = 0;
	enforceConstraints();
} 



SCA_RandomActuator::~SCA_RandomActuator()
{
	m_base->Release();
} 



CValue* SCA_RandomActuator::GetReplica()
{
	SCA_RandomActuator* replica = new SCA_RandomActuator(*this);
	// replication just copy the m_base pointer => common random generator
	replica->ProcessReplica();
	return replica;
}

void SCA_RandomActuator::ProcessReplica()
{
	SCA_IActuator::ProcessReplica();
	// increment reference count so that we can release the generator at the end
	m_base->AddRef();
}



bool SCA_RandomActuator::Update()
{
	//bool result = false;	/*unused*/
	bool bNegativeEvent = IsNegativeEvent();

	RemoveAllEvents();


	CValue *tmpval = NULL;

	if (bNegativeEvent)
		return false; // do nothing on negative events

	switch (m_distribution) {
	case KX_RANDOMACT_BOOL_CONST: {
		/* un petit peu filthy */
		bool res = !(m_parameter1 < 0.5);
		tmpval = new CBoolValue(res);
	}
	break;
	case KX_RANDOMACT_BOOL_UNIFORM: {
		/* flip a coin */
		bool res; 
		if (m_counter > 31) {
			m_previous = m_base->Draw();
			res = ((m_previous & 0x1) == 0);
			m_counter = 1;
		} else {
			res = (((m_previous >> m_counter) & 0x1) == 0);
			m_counter++;
		}
		tmpval = new CBoolValue(res);
	}
	break;
	case KX_RANDOMACT_BOOL_BERNOUILLI: {
		/* 'percentage' */
		bool res;
		res = (m_base->DrawFloat() < m_parameter1);
		tmpval = new CBoolValue(res);
	}
	break;
	case KX_RANDOMACT_INT_CONST: {
		/* constant */
		tmpval = new CIntValue((int) floor(m_parameter1));
	}
	break;
	case KX_RANDOMACT_INT_UNIFORM: {
		/* uniform (toss a die) */
		int res; 
		/* The [0, 1] interval is projected onto the [min, max+1] domain,    */
		/* and then rounded.                                                 */
		res = (int) floor( ((m_parameter2 - m_parameter1 + 1) * m_base->DrawFloat())
						   + m_parameter1);
		tmpval = new CIntValue(res);
	}
	break;
	case KX_RANDOMACT_INT_POISSON: {
		/* poisson (queues) */
		/* If x_1, x_2, ... is a sequence of random numbers with uniform     */
		/* distribution between zero and one, k is the first integer for     */
		/* which the product x_1*x_2*...*x_k < exp(-\lamba).                 */
		float a, b;
		int res = 0;
		/* The - sign is important here! The number to test for, a, must be  */
		/* between 0 and 1.                                                  */
		a = exp(-m_parameter1);
		/* a quickly reaches 0.... so we guard explicitly for that.          */
		if (a < FLT_MIN) a = FLT_MIN;
		b = m_base->DrawFloat();
		while (b >= a) {
			b = b * m_base->DrawFloat();
			res++;
		};	
		tmpval = new CIntValue(res);
	}
	break;
	case KX_RANDOMACT_FLOAT_CONST: {
		/* constant */
		tmpval = new CFloatValue(m_parameter1);
	}
	break;
	case KX_RANDOMACT_FLOAT_UNIFORM: {
		float res = ((m_parameter2 - m_parameter1) * m_base->DrawFloat())
			+ m_parameter1;
		tmpval = new CFloatValue(res);
	}
	break;
	case KX_RANDOMACT_FLOAT_NORMAL: {
		/* normal (big numbers): para1 = mean, para2 = std dev               */

		/* 

		   070301 - nzc - Changed the termination condition. I think I 
		   made a small mistake here, but it only affects distro's where
		   the seed equals 0. In that case, the algorithm locks. Let's
		   just guard that case separately.

		*/

		float x = 0.0, y = 0.0, s = 0.0, t = 0.0;
		if (m_base->GetSeed() == 0) {
			/*

			  070301 - nzc 
			  Just taking the mean here seems reasonable.

			 */
			tmpval = new CFloatValue(m_parameter1);
		} else {
			/*

			  070301 - nzc 
			  Now, with seed != 0, we will most assuredly get some
			  sensible values. The termination condition states two 
			  things: 
			  1. s >= 0 is not allowed: to prevent the distro from 
				 getting a bias towards high values. This is a small
				 correction, really, and might also be left out.
			  2. s == 0 is not allowed: to prevent a division by zero
				 when renormalising the drawn value to the desired
				 distribution shape. As a side effect, the distro will
				 never yield the exact mean. 
			  I am not sure whether this is consistent, since the error 
			  cause by #2 is of the same magnitude as the one 
			  prevented by #1. The error introduced into the SD will be 
			  improved, though. By how much? Hard to say... If you like
			  the maths, feel free to analyse. Be aware that this is 
			  one of the really old standard algorithms. I think the 
			  original came in Fortran, was translated to Pascal, and 
			  then someone came up with the C code. My guess it that
			  this will be quite sufficient here.

			 */
			do 
			{
				x = 2.0 * m_base->DrawFloat() - 1.0;
				y = 2.0 * m_base->DrawFloat() - 1.0;
				s = x*x + y*y;
			} while ( (s >= 1.0) || (s == 0.0) );
			t = x * sqrt( (-2.0 * log(s)) / s);
			tmpval = new CFloatValue(m_parameter1 + m_parameter2 * t);
		}
	}
	break;
	case KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL: {
		/* 1st order fall-off. I am very partial to using the half-life as    */
		/* controlling parameter. Using the 'normal' exponent is not very     */
		/* intuitive...                                                       */
		/* tmpval = new CFloatValue( (1.0 / m_parameter1)                     */
		tmpval = new CFloatValue( (m_parameter1) 
								  * (-log(1.0 - m_base->DrawFloat())) );

	}
	break;
	default:
	{
		/* unknown distribution... */
		static bool randomWarning = false;
		if (!randomWarning) {
			randomWarning = true;
			std::cout << "RandomActuator '" << GetName() << "' has an unknown distribution." << std::endl;
		}
		return false;
	}
	}

	/* Round up: assign it */
	CValue *prop = GetParent()->GetProperty(m_propname);
	if (prop) {
		prop->SetValue(tmpval);
	}
	tmpval->Release();

	return false;
}

void SCA_RandomActuator::enforceConstraints()
{
	/* The constraints that are checked here are the ones fundamental to     */
	/* the various distributions. Limitations of the algorithms are checked  */
	/* elsewhere (or they should be... ).                                    */
	switch (m_distribution) {
	case KX_RANDOMACT_BOOL_CONST:
	case KX_RANDOMACT_BOOL_UNIFORM:
	case KX_RANDOMACT_INT_CONST:
	case KX_RANDOMACT_INT_UNIFORM:
	case KX_RANDOMACT_FLOAT_UNIFORM:
	case KX_RANDOMACT_FLOAT_CONST:
		; /* Nothing to be done here. We allow uniform distro's to have      */
		/* 'funny' domains, i.e. max < min. This does not give problems.     */
		break;
	case KX_RANDOMACT_BOOL_BERNOUILLI: 
		/* clamp to [0, 1] */
		if (m_parameter1 < 0.0) {
			m_parameter1 = 0.0;
		} else if (m_parameter1 > 1.0) {
			m_parameter1 = 1.0;
		}
		break;
	case KX_RANDOMACT_INT_POISSON: 
		/* non-negative */
		if (m_parameter1 < 0.0) {
			m_parameter1 = 0.0;
		}
		break;
	case KX_RANDOMACT_FLOAT_NORMAL: 
		/* standard dev. is non-negative */
		if (m_parameter2 < 0.0) {
			m_parameter2 = 0.0;
		}
		break;
	case KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL: 
		/* halflife must be non-negative */
		if (m_parameter1 < 0.0) {
			m_parameter1 = 0.0;
		}
		break;
	default:
		; /* unknown distribution... */
	}
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_RandomActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_RandomActuator",
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

PyMethodDef SCA_RandomActuator::Methods[] = {
	KX_PYMETHODTABLE(SCA_RandomActuator, setBoolConst),
	KX_PYMETHODTABLE_NOARGS(SCA_RandomActuator, setBoolUniform),
	KX_PYMETHODTABLE(SCA_RandomActuator, setBoolBernouilli),

	KX_PYMETHODTABLE(SCA_RandomActuator, setIntConst),
	KX_PYMETHODTABLE(SCA_RandomActuator, setIntUniform),
	KX_PYMETHODTABLE(SCA_RandomActuator, setIntPoisson),

	KX_PYMETHODTABLE(SCA_RandomActuator, setFloatConst),
	KX_PYMETHODTABLE(SCA_RandomActuator, setFloatUniform),
	KX_PYMETHODTABLE(SCA_RandomActuator, setFloatNormal),
	KX_PYMETHODTABLE(SCA_RandomActuator, setFloatNegativeExponential),
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_RandomActuator::Attributes[] = {
	KX_PYATTRIBUTE_FLOAT_RO("para1",SCA_RandomActuator,m_parameter1),
	KX_PYATTRIBUTE_FLOAT_RO("para2",SCA_RandomActuator,m_parameter2),
	KX_PYATTRIBUTE_ENUM_RO("distribution",SCA_RandomActuator,m_distribution),
	KX_PYATTRIBUTE_STRING_RW_CHECK("propName",0,MAX_PROP_NAME,false,SCA_RandomActuator,m_propname,CheckProperty),
	KX_PYATTRIBUTE_RW_FUNCTION("seed",SCA_RandomActuator,pyattr_get_seed,pyattr_set_seed),
	{ NULL }	//Sentinel
};	

PyObject* SCA_RandomActuator::pyattr_get_seed(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_RandomActuator* act = static_cast<SCA_RandomActuator*>(self);
	return PyLong_FromSsize_t(act->m_base->GetSeed());
}

int SCA_RandomActuator::pyattr_set_seed(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	SCA_RandomActuator* act = static_cast<SCA_RandomActuator*>(self);
	if (PyLong_Check(value))	{
		int ival = PyLong_AsSsize_t(value);
		act->m_base->SetSeed(ival);
		return PY_SET_ATTR_SUCCESS;
	} else {
		PyErr_SetString(PyExc_TypeError, "actuator.seed = int: Random Actuator, expected an integer");
		return PY_SET_ATTR_FAIL;
	}
}

/* 11. setBoolConst */
KX_PYMETHODDEF_DOC_VARARGS(SCA_RandomActuator, setBoolConst,
"setBoolConst(value)\n"
"\t- value: 0 or 1\n"
"\tSet this generator to produce a constant boolean value.\n") 
{
	int paraArg;
	if(!PyArg_ParseTuple(args, "i:setBoolConst", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_BOOL_CONST;
	m_parameter1 = (paraArg) ? 1.0 : 0.0;
	
	Py_RETURN_NONE;
}
/* 12. setBoolUniform, */
KX_PYMETHODDEF_DOC_NOARGS(SCA_RandomActuator, setBoolUniform,
"setBoolUniform()\n"
"\tSet this generator to produce true and false, each with 50%% chance of occuring\n") 
{
	/* no args */
	m_distribution = KX_RANDOMACT_BOOL_UNIFORM;
	enforceConstraints();
	Py_RETURN_NONE;
}
/* 13. setBoolBernouilli,  */
KX_PYMETHODDEF_DOC_VARARGS(SCA_RandomActuator, setBoolBernouilli,
"setBoolBernouilli(value)\n"
"\t- value: a float between 0 and 1\n"
"\tReturn false value * 100%% of the time.\n")
{
	float paraArg;
	if(!PyArg_ParseTuple(args, "f:setBoolBernouilli", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_BOOL_BERNOUILLI;
	m_parameter1 = paraArg;	
	enforceConstraints();
	Py_RETURN_NONE;
}
/* 14. setIntConst,*/
KX_PYMETHODDEF_DOC_VARARGS(SCA_RandomActuator, setIntConst,
"setIntConst(value)\n"
"\t- value: integer\n"
"\tAlways return value\n") 
{
	int paraArg;
	if(!PyArg_ParseTuple(args, "i:setIntConst", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_INT_CONST;
	m_parameter1 = paraArg;
	enforceConstraints();
	Py_RETURN_NONE;
}
/* 15. setIntUniform,*/
KX_PYMETHODDEF_DOC_VARARGS(SCA_RandomActuator, setIntUniform,
"setIntUniform(lower_bound, upper_bound)\n"
"\t- lower_bound: integer\n"
"\t- upper_bound: integer\n"
"\tReturn a random integer between lower_bound and\n"
"\tupper_bound. The boundaries are included.\n")
{
	int paraArg1, paraArg2;
	if(!PyArg_ParseTuple(args, "ii:setIntUniform", &paraArg1, &paraArg2)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_INT_UNIFORM;
	m_parameter1 = paraArg1;
	m_parameter2 = paraArg2;
	enforceConstraints();
	Py_RETURN_NONE;
}
/* 16. setIntPoisson,		*/
KX_PYMETHODDEF_DOC_VARARGS(SCA_RandomActuator, setIntPoisson,
"setIntPoisson(value)\n"
"\t- value: float\n"
"\tReturn a Poisson-distributed number. This performs a series\n"
"\tof Bernouilli tests with parameter value. It returns the\n"
"\tnumber of tries needed to achieve succes.\n")
{
	float paraArg;
	if(!PyArg_ParseTuple(args, "f:setIntPoisson", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_INT_POISSON;
	m_parameter1 = paraArg;	
	enforceConstraints();
	Py_RETURN_NONE;
}
/* 17. setFloatConst */
KX_PYMETHODDEF_DOC_VARARGS(SCA_RandomActuator, setFloatConst,
"setFloatConst(value)\n"
"\t- value: float\n"
"\tAlways return value\n")
{
	float paraArg;
	if(!PyArg_ParseTuple(args, "f:setFloatConst", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_FLOAT_CONST;
	m_parameter1 = paraArg;	
	enforceConstraints();
	Py_RETURN_NONE;
}
/* 18. setFloatUniform, */
KX_PYMETHODDEF_DOC_VARARGS(SCA_RandomActuator, setFloatUniform,
"setFloatUniform(lower_bound, upper_bound)\n"
"\t- lower_bound: float\n"
"\t- upper_bound: float\n"
"\tReturn a random integer between lower_bound and\n"
"\tupper_bound.\n")
{
	float paraArg1, paraArg2;
	if(!PyArg_ParseTuple(args, "ff:setFloatUniform", &paraArg1, &paraArg2)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_FLOAT_UNIFORM;
	m_parameter1 = paraArg1;
	m_parameter2 = paraArg2;
	enforceConstraints();
	Py_RETURN_NONE;
}
/* 19. setFloatNormal, */
KX_PYMETHODDEF_DOC_VARARGS(SCA_RandomActuator, setFloatNormal,
"setFloatNormal(mean, standard_deviation)\n"
"\t- mean: float\n"
"\t- standard_deviation: float\n"
"\tReturn normal-distributed numbers. The average is mean, and the\n"
"\tdeviation from the mean is characterized by standard_deviation.\n")
{
	float paraArg1, paraArg2;
	if(!PyArg_ParseTuple(args, "ff:setFloatNormal", &paraArg1, &paraArg2)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_FLOAT_NORMAL;
	m_parameter1 = paraArg1;
	m_parameter2 = paraArg2;
	enforceConstraints();
	Py_RETURN_NONE;
}
/* 20. setFloatNegativeExponential, */
KX_PYMETHODDEF_DOC_VARARGS(SCA_RandomActuator, setFloatNegativeExponential, 
"setFloatNegativeExponential(half_life)\n"
"\t- half_life: float\n"
"\tReturn negative-exponentially distributed numbers. The half-life 'time'\n"
"\tis characterized by half_life.\n")
{
	float paraArg;
	if(!PyArg_ParseTuple(args, "f:setFloatNegativeExponential", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL;
	m_parameter1 = paraArg;	
	enforceConstraints();
	Py_RETURN_NONE;
}

#endif

/* eof */
