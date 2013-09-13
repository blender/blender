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

#ifndef __GHOST_NDOFMANAGER3DCONNEXION_H__
#define __GHOST_NDOFMANAGER3DCONNEXION_H__

#ifdef WITH_INPUT_NDOF
#include <ConnexionClientAPI.h>


OSErr GHOST_NDOFManager3Dconnexion_available(void);
OSErr GHOST_NDOFManager3Dconnexion_oldDRV(void);
OSErr			GHOST_NDOFManager3Dconnexion_InstallConnexionHandlers(ConnexionMessageHandlerProc messageHandler, ConnexionAddedHandlerProc addedHandler, ConnexionRemovedHandlerProc removedHandler);
void			GHOST_NDOFManager3Dconnexion_CleanupConnexionHandlers(void);
UInt16			GHOST_NDOFManager3Dconnexion_RegisterConnexionClient(UInt32 signature, UInt8 *name, UInt16 mode, UInt32 mask);
void			GHOST_NDOFManager3Dconnexion_SetConnexionClientButtonMask(UInt16 clientID, UInt32 buttonMask);
void			GHOST_NDOFManager3Dconnexion_UnregisterConnexionClient(UInt16 clientID);
OSErr	GHOST_NDOFManager3Dconnexion_ConnexionControl(UInt32 message, SInt32 param, SInt32 *result);

extern OSErr InstallConnexionHandlers(ConnexionMessageHandlerProc messageHandler, ConnexionAddedHandlerProc addedHandler, ConnexionRemovedHandlerProc removedHandler) __attribute__((weak_import));
extern void		CleanupConnexionHandlers(void) __attribute__((weak_import));
extern UInt16 RegisterConnexionClient(UInt32 signature, UInt8 *name, UInt16 mode, UInt32 mask) __attribute__((weak_import));
extern void SetConnexionClientButtonMask(UInt16 clientID, UInt32 buttonMask) __attribute__((weak_import));
extern void UnregisterConnexionClient(UInt16 clientID) __attribute__((weak_import));
extern OSErr	ConnexionControl(UInt32 message, SInt32 param, SInt32 *result) __attribute__((weak_import));


#endif // WITH_INPUT_NDOF

#endif // #include guard
