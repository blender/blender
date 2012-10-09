/*
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

/** \file KX_NetworkEventManager.h
 *  \ingroup ketsjinet
 *  \brief Ketsji Logic Extension: Network Event Manager class
 */

#ifndef __KX_NETWORKEVENTMANAGER_H__
#define __KX_NETWORKEVENTMANAGER_H__

#include "SCA_EventManager.h"

class KX_NetworkEventManager : public SCA_EventManager
{
	class NG_NetworkDeviceInterface* m_ndi;

public:
	KX_NetworkEventManager(class SCA_LogicManager* logicmgr,
	                       class NG_NetworkDeviceInterface *ndi);
	virtual ~KX_NetworkEventManager ();

	virtual void NextFrame();
	virtual void EndFrame();

	SCA_LogicManager* GetLogicManager() { return m_logicmgr; }
	class NG_NetworkDeviceInterface* GetNetworkDevice() {
		return m_ndi; }
};

#endif  /* __KX_NETWORKEVENTMANAGER_H__ */
