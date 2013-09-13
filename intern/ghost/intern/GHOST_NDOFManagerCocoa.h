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
 * Contributor(s):
 *   Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __GHOST_NDOFMANAGERCOCOA_H__
#define __GHOST_NDOFMANAGERCOCOA_H__

#ifdef WITH_INPUT_NDOF


extern "C" {
#include <ConnexionClientAPI.h>
#include <stdio.h>
}


#include "GHOST_NDOFManager.h"
extern "C"  OSErr   GHOST_NDOFManager3Dconnexion_available(void);
extern "C"  OSErr   GHOST_NDOFManager3Dconnexion_oldDRV(void);
extern "C"  OSErr   GHOST_NDOFManager3Dconnexion_InstallConnexionHandlers(ConnexionMessageHandlerProc messageHandler, ConnexionAddedHandlerProc addedHandler, ConnexionRemovedHandlerProc removedHandler);
extern "C"  void GHOST_NDOFManager3Dconnexion_CleanupConnexionHandlers(void);
extern "C"  UInt16  GHOST_NDOFManager3Dconnexion_RegisterConnexionClient(UInt32 signature, UInt8 *name, UInt16 mode, UInt32 mask);
extern "C"  void    GHOST_NDOFManager3Dconnexion_SetConnexionClientButtonMask(UInt16 clientID, UInt32 buttonMask);
extern "C"  void	GHOST_NDOFManager3Dconnexion_UnregisterConnexionClient(UInt16 clientID);
extern "C"  OSErr	GHOST_NDOFManager3Dconnexion_ConnexionControl(UInt32 message, SInt32 param, SInt32 *result);

// Event capture is handled within the NDOF manager on Macintosh,
// so there's no need for SystemCocoa to look for them.

class GHOST_NDOFManagerCocoa : public GHOST_NDOFManager
{
public:
	GHOST_NDOFManagerCocoa(GHOST_System&);

	~GHOST_NDOFManagerCocoa();

	// whether multi-axis functionality is available (via the OS or driver)
	// does not imply that a device is plugged in or being used
    
private:
	unsigned short m_clientID;
};


#endif // WITH_INPUT_NDOF
#endif // #include guard
