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
 * TerraplayNetworkDeviceInterface derived from NG_NetworkDeviceInterface
 */
#ifndef NG_TERRAPLAYNETWORKDEVICEINTERFACE_H
#define NG_TERRAPLAYNETWORKDEVICEINTERFACE_H

#include <deque>
#include "GASInterface.h"
#include "NG_NetworkDeviceInterface.h"

class NG_TerraplayNetworkDeviceInterface : public NG_NetworkDeviceInterface
{
	std::deque<NG_NetworkMessage*> m_messages;

	// Terraplay GAS stuff
	GASInterface *GAS;
	GASClientId group_id;
	GASRequestId group_id_request;
	int group_id_request_valid;

	void interface_error(char *str, GASResult error);
public:
	NG_TerraplayNetworkDeviceInterface();
	~NG_TerraplayNetworkDeviceInterface();

	bool Connect(char *GAS_address, unsigned int GAS_port,
		     char *GAS_password, unsigned int localport,
		     unsigned int timeout);
	bool Disconnect(void);

	void SendNetworkMessage(NG_NetworkMessage* nwmsg);
	vector<NG_NetworkMessage*> RetrieveNetworkMessages(void);

	int mytest(void);
};

#endif //NG_TERRAPLAYNETWORKDEVICEINTERFACE_H

