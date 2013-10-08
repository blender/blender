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
 *   Jake Kauth on 9/12/13.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef WITH_INPUT_NDOF

#include <ConnexionClientAPI.h>
#include <stdio.h>

#include "GHOST_NDOFManager3Dconnexion.h"

/* It is to be noted that these implementations are linked in as
 * 'extern "C"' calls from GHOST_NDOFManagerCocoa.

 * This is done in order to
 * preserve weak linking capability (which as of clang-3.3 and xcode5
 * breaks weak linking when there is name mangling of c++ libraries.)
 *
 * We need to have the weak linked file as pure C.  Therefore we build a 
 * compiled bridge from the real weak linked calls and the calls within C++

 */

OSErr GHOST_NDOFManager3Dconnexion_available(void)
{
	// extern unsigned int InstallConnexionHandlers() __attribute__((weak_import));
	// Make the linker happy for the framework check (see link below for more info)
	// http://developer.apple.com/documentation/MacOSX/Conceptual/BPFrameworks/Concepts/WeakLinking.html
	return InstallConnexionHandlers != 0;
	// this means that the driver is installed and dynamically linked to blender
}

OSErr GHOST_NDOFManager3Dconnexion_oldDRV()
{
	//extern unsigned int SetConnexionClientButtonMask() __attribute__((weak_import));
	// Make the linker happy for the framework check (see link below for more info)
	// http://developer.apple.com/documentation/MacOSX/Conceptual/BPFrameworks/Concepts/WeakLinking.html
	return SetConnexionClientButtonMask != 0;
	// this means that the driver has this symbol
}

UInt16 GHOST_NDOFManager3Dconnexion_RegisterConnexionClient(UInt32 signature, UInt8 *name, UInt16 mode, UInt32 mask)
{
	return RegisterConnexionClient(signature,  name,  mode,  mask);
}

void GHOST_NDOFManager3Dconnexion_SetConnexionClientButtonMask(UInt16 clientID, UInt32 buttonMask)
{
	return SetConnexionClientButtonMask( clientID,  buttonMask);
}

void GHOST_NDOFManager3Dconnexion_UnregisterConnexionClient(UInt16 clientID)
{
	return UnregisterConnexionClient( clientID);
}

OSErr GHOST_NDOFManager3Dconnexion_InstallConnexionHandlers(
        ConnexionMessageHandlerProc messageHandler,
        ConnexionAddedHandlerProc addedHandler,
        ConnexionRemovedHandlerProc removedHandler)
{
	return InstallConnexionHandlers( messageHandler,  addedHandler,  removedHandler);
}

void GHOST_NDOFManager3Dconnexion_CleanupConnexionHandlers(void)
{
	return CleanupConnexionHandlers();
}

OSErr GHOST_NDOFManager3Dconnexion_ConnexionControl(UInt32 message, SInt32 param, SInt32 *result)
{
	return ConnexionControl( message,  param, result);
}

#endif // WITH_INPUT_NDOF
