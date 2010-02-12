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

	/** Name of the property where we write the current frame number */
	STR_String	m_framepropname;

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

	static const char *S_KX_ACT_IPO_PLAY_STRING;
	static const char *S_KX_ACT_IPO_PINGPONG_STRING;
	static const char *S_KX_ACT_IPO_FLIPPER_STRING;
	static const char *S_KX_ACT_IPO_LOOPSTOP_STRING;
	static const char *S_KX_ACT_IPO_LOOPEND_STRING;
	static const char *S_KX_ACT_IPO_KEY2KEY_STRING;
	static const char *S_KX_ACT_IPO_FROM_PROP_STRING;

	int string2mode(char* modename);
	
	int m_type;

	KX_IpoActuator(SCA_IObject* gameobj,
				   const STR_String& propname,
				   const STR_String& framePropname,
				   float starttime,
				   float endtime,
				   bool recurse,
				   int acttype,
				   bool ipo_as_force, 
				   bool ipo_add,
				   bool ipo_local);
	virtual ~KX_IpoActuator() {};

	virtual CValue* GetReplica() {
		KX_IpoActuator* replica = new KX_IpoActuator(*this);//m_float,GetName());
		replica->ProcessReplica();
		return replica;
	};

	void		SetStart(float starttime);
	void		SetEnd(float endtime);
	virtual		bool Update(double curtime, bool frame);

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */
	
};

#endif //__KX_IPOACTUATOR

