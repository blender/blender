/**
 * Do an object ipo
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

#ifndef __KX_IPOACTUATOR
#define __KX_IPOACTUATOR

#include "SCA_IActuator.h"

class KX_IpoActuator : public SCA_IActuator
{
	Py_Header;
private:
	/** Computes the IPO start time from the current time
	    and the current frame. */
	void SetStartTime(float curtime);
	/** Computes the current frame from the current time
	    and the IPO start time. */
	void SetLocalTime(float curtime);
	/** Ensures the current frame is between the start and
	    end frames. */
	bool ClampLocalTime();

protected:
	bool	m_bNegativeEvent;

	/** Begin frame of the ipo. */
	float	m_startframe;
	
	/** End frame of the ipo. */
	float   m_endframe;

	/** Include children in the transforms? */
	bool	m_recurse;

	/** Current active frame of the ipo. */
	float   m_localtime;
	
	/** The time this ipo started at. */
	float	m_starttime;

	/** play backwards or forwards? (positive means forward). */
	float	m_direction;

	/** Name of the property (only used in from_prop mode). */
	STR_String	m_propname;

	/** Interpret the ipo as a force? */
	bool    m_ipo_as_force;
	
	/** Add Ipo curve to current loc/rot/scale */
	bool    m_ipo_add;
	
	/** The Ipo curve is applied in local coordinates */
	bool    m_ipo_local;

	bool	m_bIpoPlaying;

public:
	enum IpoActType
	{
		KX_ACT_IPO_NODEF = 0,
		KX_ACT_IPO_PLAY,
		KX_ACT_IPO_PINGPONG,
		KX_ACT_IPO_FLIPPER,
		KX_ACT_IPO_LOOPSTOP,
		KX_ACT_IPO_LOOPEND,
		KX_ACT_IPO_KEY2KEY,
		KX_ACT_IPO_FROM_PROP,
		KX_ACT_IPO_MAX
	};

	static STR_String S_KX_ACT_IPO_PLAY_STRING;
	static STR_String S_KX_ACT_IPO_PINGPONG_STRING;
	static STR_String S_KX_ACT_IPO_FLIPPER_STRING;
	static STR_String S_KX_ACT_IPO_LOOPSTOP_STRING;
	static STR_String S_KX_ACT_IPO_LOOPEND_STRING;
	static STR_String S_KX_ACT_IPO_KEY2KEY_STRING;
	static STR_String S_KX_ACT_IPO_FROM_PROP_STRING;

	IpoActType string2mode(char* modename);
	
	IpoActType	m_type;

	KX_IpoActuator(SCA_IObject* gameobj,
				   const STR_String& propname,
				   float starttime,
				   float endtime,
				   bool recurse,
				   int acttype,
				   bool ipo_as_force, 
				   bool ipo_add,
				   bool ipo_local,
				   PyTypeObject* T=&Type);
	virtual ~KX_IpoActuator() {};

	virtual CValue* GetReplica() {
		KX_IpoActuator* replica = new KX_IpoActuator(*this);//m_float,GetName());
		replica->ProcessReplica();
		// this will copy properties and so on...
		CValue::AddDataToReplica(replica);
		return replica;
	};

	void		SetStart(float starttime);
	void		SetEnd(float endtime);
	virtual		bool Update(double curtime, bool frame);

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	virtual PyObject* _getattr(const STR_String& attr);
	//KX_PYMETHOD_DOC
	KX_PYMETHOD_DOC(KX_IpoActuator,Set);
	KX_PYMETHOD_DOC(KX_IpoActuator,SetProperty);
/*  	KX_PYMETHOD_DOC(KX_IpoActuator,SetKey2Key); */
	KX_PYMETHOD_DOC(KX_IpoActuator,SetStart);
	KX_PYMETHOD_DOC_NOARGS(KX_IpoActuator,GetStart);
	KX_PYMETHOD_DOC(KX_IpoActuator,SetEnd);
	KX_PYMETHOD_DOC_NOARGS(KX_IpoActuator,GetEnd);
	KX_PYMETHOD_DOC(KX_IpoActuator,SetIpoAsForce);
	KX_PYMETHOD_DOC_NOARGS(KX_IpoActuator,GetIpoAsForce);
	KX_PYMETHOD_DOC(KX_IpoActuator,SetIpoAdd);
	KX_PYMETHOD_DOC_NOARGS(KX_IpoActuator,GetIpoAdd);
	KX_PYMETHOD_DOC(KX_IpoActuator,SetType);
	KX_PYMETHOD_DOC_NOARGS(KX_IpoActuator,GetType);
	KX_PYMETHOD_DOC(KX_IpoActuator,SetForceIpoActsLocal);
	KX_PYMETHOD_DOC_NOARGS(KX_IpoActuator,GetForceIpoActsLocal);
	
};

#endif //__KX_IPOACTUATOR

