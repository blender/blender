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

#include "NG_TerraplayNetworkDeviceInterface.h"
#include "NG_NetworkMessage.h"

//---- relocate these
void NG_TerraplayNetworkDeviceInterface::interface_error(char *str, GASResult error) {
	GASRString err_str = GAS->ErrorTranslate(error);
	if (err_str.result == GASOK)
		printf("%s: %s\n",str,err_str.ptr);
	else
		printf("%s: UNKNOWN (Error code %d)", error);
}
//---- END relocate these

NG_TerraplayNetworkDeviceInterface::NG_TerraplayNetworkDeviceInterface()
{
	group_id = GASCLIENTIDNULL;
	group_id_request_valid = false;
	this->Offline();

	if ((GAS = new GASInterface()) == NULL) {
		// terror
		printf("ERROR GAS Common Network Interface NOT created\n");
		// do something useful
	} else {
		printf("GAS Common Network Interface created\n");
	}
}

NG_TerraplayNetworkDeviceInterface::~NG_TerraplayNetworkDeviceInterface()
{
	if (GAS != NULL) {
		delete GAS;
		printf("GAS Common Network Interface deleted\n");
	}
}

bool NG_TerraplayNetworkDeviceInterface::Connect(char *GAS_address,
	unsigned int GAS_port, char *GAS_password, unsigned int localport,
	unsigned int timeout)
{
	GASResult result;
	printf("Establishing connection to GAS...\n");
	result = GAS->ConnectionRequest(GAS_address, GAS_port,
		 GAS_password,localport, timeout);
	if (result == GASOK) {
		this->Online();
		GASRClientId client_id = GAS->Connected();
		if (client_id.result != GASOK) {
			printf("... connected, but no client ID\n");
			return false;
		} else {
			printf("Connected with client ID %d\n",
				client_id.clientid);
			return true;
		}
	} else {
		interface_error("Connection", result);
		return false;
	}
}

bool NG_TerraplayNetworkDeviceInterface::Disconnect(void)
{
	int i = 0;
	printf("Disconnecting...\n");
	if (! this->IsOnline()) {
		printf("ehh... /me was not connected\n");
		return false;
	}

	GASRRequestId   req = GAS->ConnectionClose();
	if (req.result != GASWAITING) {
		interface_error("ConnectionClose",req.result);
		this->~NG_TerraplayNetworkDeviceInterface();
	}
	this->Offline();
// dit is erg fout  :( ik wil helemaal geen ~NG_ hier

	while (true) {
		GASRMessage gas_message;
		GASResult result = GAS->GasActivity(GASBLOCK, 100);
		if (++i>5000) {
			printf("\nGiving up on waiting for connection close\n");
			this->~NG_TerraplayNetworkDeviceInterface();
		}
		switch (result) {
		case GASCONNECTIONOK:
			break;
		case GASGASMESSAGE:
			gas_message = GAS->GasMessageGetNext();
			if (gas_message.type == GASRCONNECTIONCLOSE) {
				if (gas_message.result == GASOK ||
					gas_message.result == GASALREADYDONE) {
					return true;
				} else {
					interface_error("GasMessageGetNext",
						gas_message.result);
					return false;
				}
			}
		// no break ...
		default:
			interface_error("GasActivity",result);
		}
	}
	return true;
}

STR_String NG_TerraplayNetworkDeviceInterface::GetNetworkVersion()
{
	GASRString version = GAS->Version();
	if (version.result != GASOK) {
		interface_error("GetNetworkVersion", version.result);
		return NULL;
	} else {
		return version.ptr;
	}
}

int NG_TerraplayNetworkDeviceInterface::mytest() {
    return (3);
}

void NG_TerraplayNetworkDeviceInterface::SendNetworkMessage(NG_NetworkMessage* nwmsg)
{
	GASPayload payload;
	GASResult result;
	STR_String mystring;

	if (group_id == GASCLIENTIDNULL) {
		printf("Oops, no group to send to yet\n");
		return;
	}

	mystring = nwmsg->GetMessageText().ReadPtr();
	payload.ptr = (void *) mystring.Ptr();
	payload.size = mystring.Length() + 1;

	result = GAS->ClientMessageSend(group_id, payload, GASBESTEFFORT);

	switch (result) {
	case GASOK:
		break;
	default:
		interface_error("ClientMessageSend",result);
	}
	// NOTE. You shall NOT free the payload with PayloadFree().
	// This is your own payload, allocated and freed by yourself
	// anyway you want.
}

vector <NG_NetworkMessage*> NG_TerraplayNetworkDeviceInterface::RetrieveNetworkMessages()
{

	vector <NG_NetworkMessage*> messages;
	//todo: spend your expensive time here!

	return messages;
}
