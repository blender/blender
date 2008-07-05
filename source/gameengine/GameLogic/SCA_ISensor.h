/**
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
 * Interface Class for all logic Sensors. Implements
 * pulsemode and pulsefrequency, and event suppression.
 */

#ifndef __SCA_ISENSOR
#define __SCA_ISENSOR

#include "SCA_ILogicBrick.h"

/**
 * Interface Class for all logic Sensors. Implements
 * pulsemode,pulsefrequency */
class SCA_ISensor : public SCA_ILogicBrick
{
	Py_Header;
	class SCA_EventManager* m_eventmgr;
	bool	m_triggered;

	/** Pulse positive  pulses? */
	bool m_pos_pulsemode;

	/** Pulse negative pulses? */
	bool m_neg_pulsemode;

	/** Repeat frequency in pulse mode. */
	int m_pulse_frequency;

	/** Number of ticks since the last positive pulse. */
	int m_pos_ticks;

	/** Number of ticks since the last negative pulse. */
	int m_neg_ticks;

	/** invert the output signal*/
	bool m_invert;

	/** detect level instead of edge*/
	bool m_level;

	/** sensor has been reset */
	bool m_reset;

	/** Sensor must ignore updates? */
	bool m_suspended;

	/** number of connections to controller */
	int m_links;

	/** Pass the activation on to the logic manager.*/
	void SignalActivation(class SCA_LogicManager* logicmgr);
	
public:
	SCA_ISensor(SCA_IObject* gameobj,
				class SCA_EventManager* eventmgr,
				PyTypeObject* T );;
	~SCA_ISensor();
	virtual void	ReParent(SCA_IObject* parent);

	/** Because we want sensors to share some behaviour, the Activate has     */
	/* an implementation on this level. It requires an evaluate on the lower */
	/* level of individual sensors. Mapping the old activate()s is easy.     */
	/* The IsPosTrig() also has to change, to keep things consistent.        */
	void Activate(class SCA_LogicManager* logicmgr,CValue* event);
	virtual bool Evaluate(CValue* event) = 0;
	virtual bool IsPositiveTrigger();
	virtual void Init();
	
	virtual PyObject* _getattr(const STR_String& attr);
	virtual CValue* GetReplica()=0;

	/** Set parameters for the pulsing behaviour.
	 * @param posmode Trigger positive pulses?
	 * @param negmode Trigger negative pulses?
	 * @param freq    Frequency to use when doing pulsing.
	 */
	void SetPulseMode(bool posmode,
					  bool negmode,
					  int freq);
	
	/** Release sensor
	 *  For property sensor, it is used to release the pre-calculated expression
	 *  so that self references are removed before the sensor itself is released
	 */
	virtual void Delete() { Release(); }
	/** Set inversion of pulses on or off. */
	void SetInvert(bool inv);
	/** set the level detection on or off */
	void SetLevel(bool lvl);

	void RegisterToManager();
	virtual float GetNumber();

	/** Stop sensing for a while. */
	void Suspend();

	/** Is this sensor switched off? */
	bool IsSuspended();
	
	/** Resume sensing. */
	void Resume();

	void IncLink()
		{ m_links++; }
	void DecLink();
	bool IsNoLink() const 
		{ return !m_links; }

	/* Python functions: */
	KX_PYMETHOD_DOC(SCA_ISensor,IsPositive);
	KX_PYMETHOD_DOC(SCA_ISensor,GetUsePosPulseMode);
	KX_PYMETHOD_DOC(SCA_ISensor,SetUsePosPulseMode);
	KX_PYMETHOD_DOC(SCA_ISensor,GetFrequency);
	KX_PYMETHOD_DOC(SCA_ISensor,SetFrequency);
	KX_PYMETHOD_DOC(SCA_ISensor,GetUseNegPulseMode);
	KX_PYMETHOD_DOC(SCA_ISensor,SetUseNegPulseMode);
	KX_PYMETHOD_DOC(SCA_ISensor,GetInvert);
	KX_PYMETHOD_DOC(SCA_ISensor,SetInvert);
	KX_PYMETHOD_DOC(SCA_ISensor,GetLevel);
	KX_PYMETHOD_DOC(SCA_ISensor,SetLevel);

};

#endif //__SCA_ISENSOR

