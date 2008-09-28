/**
 * Set random/camera stuff
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

#include "BoolValue.h"
#include "IntValue.h"
#include "FloatValue.h"
#include "SCA_IActuator.h"
#include "SCA_RandomActuator.h"
#include "math.h"
#include "MT_Transform.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_RandomActuator::SCA_RandomActuator(SCA_IObject *gameobj, 
									 long seed,
									 SCA_RandomActuator::KX_RANDOMACT_MODE mode,
									 float para1,
									 float para2,
									 const STR_String &propName,
									 PyTypeObject* T)
	: SCA_IActuator(gameobj, T),
	  m_propname(propName),
	  m_parameter1(para1),
	  m_parameter2(para2),
	  m_distribution(mode)
{
	// m_base is never deleted, probably a memory leak!
	m_base = new SCA_RandomNumberGenerator(seed);
	m_counter = 0;
	enforceConstraints();
} 



SCA_RandomActuator::~SCA_RandomActuator()
{
	/* intentionally empty */ 
} 



CValue* SCA_RandomActuator::GetReplica()
{
	SCA_RandomActuator* replica = new SCA_RandomActuator(*this);
	// replication just copy the m_base pointer => common random generator
	replica->ProcessReplica();
	CValue::AddDataToReplica(replica);

	return replica;
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
		float a = 0.0, b = 0.0;
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

void SCA_RandomActuator::enforceConstraints() {
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

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_RandomActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"SCA_RandomActuator",
	sizeof(SCA_RandomActuator),
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

PyParentObject SCA_RandomActuator::Parents[] = {
	&SCA_RandomActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef SCA_RandomActuator::Methods[] = {
	{"setSeed",         (PyCFunction) SCA_RandomActuator::sPySetSeed, METH_VARARGS, SetSeed_doc},
	{"getSeed",         (PyCFunction) SCA_RandomActuator::sPyGetSeed, METH_VARARGS, GetSeed_doc},
	{"getPara1",        (PyCFunction) SCA_RandomActuator::sPyGetPara1, METH_VARARGS, GetPara1_doc},
	{"getPara2",        (PyCFunction) SCA_RandomActuator::sPyGetPara2, METH_VARARGS, GetPara2_doc},
	{"getDistribution", (PyCFunction) SCA_RandomActuator::sPyGetDistribution, METH_VARARGS, GetDistribution_doc},
	{"setProperty",     (PyCFunction) SCA_RandomActuator::sPySetProperty, METH_VARARGS, SetProperty_doc},
	{"getProperty",     (PyCFunction) SCA_RandomActuator::sPyGetProperty, METH_VARARGS, GetProperty_doc},
	{"setBoolConst",    (PyCFunction) SCA_RandomActuator::sPySetBoolConst, METH_VARARGS, SetBoolConst_doc},
	{"setBoolUniform",  (PyCFunction) SCA_RandomActuator::sPySetBoolUniform, METH_VARARGS, SetBoolUniform_doc},
	{"setBoolBernouilli",(PyCFunction) SCA_RandomActuator::sPySetBoolBernouilli, METH_VARARGS, SetBoolBernouilli_doc},
	{"setIntConst",     (PyCFunction) SCA_RandomActuator::sPySetIntConst, METH_VARARGS, SetIntConst_doc},
	{"setIntUniform",   (PyCFunction) SCA_RandomActuator::sPySetIntUniform, METH_VARARGS, SetIntUniform_doc},
	{"setIntPoisson",   (PyCFunction) SCA_RandomActuator::sPySetIntPoisson, METH_VARARGS, SetIntPoisson_doc},
	{"setFloatConst",   (PyCFunction) SCA_RandomActuator::sPySetFloatConst, METH_VARARGS, SetFloatConst_doc},
	{"setFloatUniform", (PyCFunction) SCA_RandomActuator::sPySetFloatUniform, METH_VARARGS, SetFloatUniform_doc},
	{"setFloatNormal",  (PyCFunction) SCA_RandomActuator::sPySetFloatNormal, METH_VARARGS, SetFloatNormal_doc},
	{"setFloatNegativeExponential", (PyCFunction) SCA_RandomActuator::sPySetFloatNegativeExponential, METH_VARARGS, SetFloatNegativeExponential_doc},
	{NULL,NULL} //Sentinel
};

PyObject* SCA_RandomActuator::_getattr(const STR_String& attr) {
	_getattr_up(SCA_IActuator);
}

/* 1. setSeed                                                            */
const char SCA_RandomActuator::SetSeed_doc[] = 
"setSeed(seed)\n"
"\t- seed: integer\n"
"\tSet the initial seed of the generator. Equal seeds produce\n"
"\tequal series. If the seed is 0, the generator will produce\n"
"\tthe same value on every call.\n";
PyObject* SCA_RandomActuator::PySetSeed(PyObject* self, PyObject* args, PyObject* kwds) {
	long seedArg;
	if(!PyArg_ParseTuple(args, "i", &seedArg)) {
		return NULL;
	}
	
	m_base->SetSeed(seedArg);
	
	Py_Return;
}
/* 2. getSeed                                                            */
const char SCA_RandomActuator::GetSeed_doc[] = 
"getSeed()\n"
"\tReturns the initial seed of the generator. Equal seeds produce\n"
"\tequal series.\n";
PyObject* SCA_RandomActuator::PyGetSeed(PyObject* self, PyObject* args, PyObject* kwds) {
	return PyInt_FromLong(m_base->GetSeed());
}

/* 4. getPara1                                                           */
const char SCA_RandomActuator::GetPara1_doc[] = 
"getPara1()\n"
"\tReturns the first parameter of the active distribution. Refer\n"
"\tto the documentation of the generator types for the meaning\n"
"\tof this value.";
PyObject* SCA_RandomActuator::PyGetPara1(PyObject* self, PyObject* args, PyObject* kwds) {
	return PyFloat_FromDouble(m_parameter1);
}

/* 6. getPara2                                                           */
const char SCA_RandomActuator::GetPara2_doc[] = 
"getPara2()\n"
"\tReturns the first parameter of the active distribution. Refer\n"
"\tto the documentation of the generator types for the meaning\n"
"\tof this value.";
PyObject* SCA_RandomActuator::PyGetPara2(PyObject* self, PyObject* args, PyObject* kwds) {
	return PyFloat_FromDouble(m_parameter2);
}

/* 8. getDistribution                                                    */
const char SCA_RandomActuator::GetDistribution_doc[] = 
"getDistribution()\n"
"\tReturns the type of the active distribution.\n";
PyObject* SCA_RandomActuator::PyGetDistribution(PyObject* self, PyObject* args, PyObject* kwds) {
	return PyInt_FromLong(m_distribution);
}

/* 9. setProperty                                                        */
const char SCA_RandomActuator::SetProperty_doc[] = 
"setProperty(name)\n"
"\t- name: string\n"
"\tSet the property to which the random value is assigned. If the \n"
"\tgenerator and property types do not match, the assignment is ignored.\n";
PyObject* SCA_RandomActuator::PySetProperty(PyObject* self, PyObject* args, PyObject* kwds) {
	char *nameArg;
	if (!PyArg_ParseTuple(args, "s", &nameArg)) {
		return NULL;
	}

	CValue* prop = GetParent()->FindIdentifier(nameArg);

	if (!prop->IsError()) {
		m_propname = nameArg;
	} else {
		; /* not found ... */
	}
	prop->Release();
	
	Py_Return;
}
/* 10. getProperty                                                       */
const char SCA_RandomActuator::GetProperty_doc[] = 
"getProperty(name)\n"
"\tReturn the property to which the random value is assigned. If the \n"
"\tgenerator and property types do not match, the assignment is ignored.\n";
PyObject* SCA_RandomActuator::PyGetProperty(PyObject* self, PyObject* args, PyObject* kwds) {
	return PyString_FromString(m_propname);
}

/* 11. setBoolConst */
const char SCA_RandomActuator::SetBoolConst_doc[] = 
"setBoolConst(value)\n"
"\t- value: 0 or 1\n"
"\tSet this generator to produce a constant boolean value.\n";
PyObject* SCA_RandomActuator::PySetBoolConst(PyObject* self, 
											PyObject* args, 
											PyObject* kwds) {
	int paraArg;
	if(!PyArg_ParseTuple(args, "i", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_BOOL_CONST;
	if (paraArg) {
		m_parameter1 = 1;
	}
	
	Py_Return;
}
/* 12. setBoolUniform, */
const char SCA_RandomActuator::SetBoolUniform_doc[] = 
"setBoolUniform()\n"
"\tSet this generator to produce true and false, each with 50%% chance of occuring\n";
PyObject* SCA_RandomActuator::PySetBoolUniform(PyObject* self, 
											  PyObject* args, 
											  PyObject* kwds) {
	/* no args */
	m_distribution = KX_RANDOMACT_BOOL_UNIFORM;
	enforceConstraints();
	Py_Return;
}
/* 13. setBoolBernouilli,  */
const char SCA_RandomActuator::SetBoolBernouilli_doc[] = 
"setBoolBernouilli(value)\n"
"\t- value: a float between 0 and 1\n"
"\tReturn false value * 100%% of the time.\n";
PyObject* SCA_RandomActuator::PySetBoolBernouilli(PyObject* self, 
												 PyObject* args, 
												 PyObject* kwds) {
	float paraArg;
	if(!PyArg_ParseTuple(args, "f", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_BOOL_BERNOUILLI;
	m_parameter1 = paraArg;	
	enforceConstraints();
	Py_Return;
}
/* 14. setIntConst,*/
const char SCA_RandomActuator::SetIntConst_doc[] = 
"setIntConst(value)\n"
"\t- value: integer\n"
"\tAlways return value\n";
PyObject* SCA_RandomActuator::PySetIntConst(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds) {
	int paraArg;
	if(!PyArg_ParseTuple(args, "i", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_INT_CONST;
	m_parameter1 = paraArg;
	enforceConstraints();
	Py_Return;
}
/* 15. setIntUniform,*/
const char SCA_RandomActuator::SetIntUniform_doc[] = 
"setIntUniform(lower_bound, upper_bound)\n"
"\t- lower_bound: integer\n"
"\t- upper_bound: integer\n"
"\tReturn a random integer between lower_bound and\n"
"\tupper_bound. The boundaries are included.\n";
PyObject* SCA_RandomActuator::PySetIntUniform(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds) {
	int paraArg1, paraArg2;
	if(!PyArg_ParseTuple(args, "ii", &paraArg1, &paraArg2)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_INT_UNIFORM;
	m_parameter1 = paraArg1;
	m_parameter2 = paraArg2;
	enforceConstraints();
	Py_Return;
}
/* 16. setIntPoisson,		*/
const char SCA_RandomActuator::SetIntPoisson_doc[] = 
"setIntPoisson(value)\n"
"\t- value: float\n"
"\tReturn a Poisson-distributed number. This performs a series\n"
"\tof Bernouilli tests with parameter value. It returns the\n"
"\tnumber of tries needed to achieve succes.\n";
PyObject* SCA_RandomActuator::PySetIntPoisson(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds) {
	float paraArg;
	if(!PyArg_ParseTuple(args, "f", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_INT_POISSON;
	m_parameter1 = paraArg;	
	enforceConstraints();
	Py_Return;
}
/* 17. setFloatConst,*/
const char SCA_RandomActuator::SetFloatConst_doc[] = 
"setFloatConst(value)\n"
"\t- value: float\n"
"\tAlways return value\n";
PyObject* SCA_RandomActuator::PySetFloatConst(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds) {
	float paraArg;
	if(!PyArg_ParseTuple(args, "f", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_FLOAT_CONST;
	m_parameter1 = paraArg;	
	enforceConstraints();
	Py_Return;
}
/* 18. setFloatUniform, */
const char SCA_RandomActuator::SetFloatUniform_doc[] = 
"setFloatUniform(lower_bound, upper_bound)\n"
"\t- lower_bound: float\n"
"\t- upper_bound: float\n"
"\tReturn a random integer between lower_bound and\n"
"\tupper_bound.\n";
PyObject* SCA_RandomActuator::PySetFloatUniform(PyObject* self, 
											   PyObject* args, 
											   PyObject* kwds) {
	float paraArg1, paraArg2;
	if(!PyArg_ParseTuple(args, "ff", &paraArg1, &paraArg2)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_FLOAT_UNIFORM;
	m_parameter1 = paraArg1;
	m_parameter2 = paraArg2;
	enforceConstraints();
	Py_Return;
}
/* 19. setFloatNormal, */
const char SCA_RandomActuator::SetFloatNormal_doc[] = 
"setFloatNormal(mean, standard_deviation)\n"
"\t- mean: float\n"
"\t- standard_deviation: float\n"
"\tReturn normal-distributed numbers. The average is mean, and the\n"
"\tdeviation from the mean is characterized by standard_deviation.\n";
PyObject* SCA_RandomActuator::PySetFloatNormal(PyObject* self, 
											  PyObject* args, 
											  PyObject* kwds) {
	float paraArg1, paraArg2;
	if(!PyArg_ParseTuple(args, "ff", &paraArg1, &paraArg2)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_FLOAT_NORMAL;
	m_parameter1 = paraArg1;
	m_parameter2 = paraArg2;
	enforceConstraints();
	Py_Return;
}
/* 20. setFloatNegativeExponential, */
const char SCA_RandomActuator::SetFloatNegativeExponential_doc[] = 
"setFloatNegativeExponential(half_life)\n"
"\t- half_life: float\n"
"\tReturn negative-exponentially distributed numbers. The half-life 'time'\n"
"\tis characterized by half_life.\n";
PyObject* SCA_RandomActuator::PySetFloatNegativeExponential(PyObject* self, 
														   PyObject* args, 
														   PyObject* kwds) {
	float paraArg;
	if(!PyArg_ParseTuple(args, "f", &paraArg)) {
		return NULL;
	}
	
	m_distribution = KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL;
	m_parameter1 = paraArg;	
	enforceConstraints();
	Py_Return;
}
	
/* eof */
