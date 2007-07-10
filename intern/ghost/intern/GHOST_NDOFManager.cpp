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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include "GHOST_NDOFManager.h"


// the variable is outside the class because it must be accessed from plugin
static volatile GHOST_TEventNDOFData currentNdofValues = {0,0,0,0,0,0,0,0,0,0,0};

namespace
{
    GHOST_NDOFLibraryInit_fp ndofLibraryInit = 0;
    GHOST_NDOFLibraryShutdown_fp ndofLibraryShutdown = 0;
    GHOST_NDOFDeviceOpen_fp ndofDeviceOpen = 0;
}

GHOST_NDOFManager::GHOST_NDOFManager()
{
    m_DeviceHandle = 0;

    // discover the API from the plugin
    ndofLibraryInit = 0;
    ndofLibraryShutdown = 0;
    ndofDeviceOpen = 0;
}

GHOST_NDOFManager::~GHOST_NDOFManager()
{
    if (ndofLibraryShutdown)
        ndofLibraryShutdown(m_DeviceHandle);

    m_DeviceHandle = 0;
}


void
GHOST_NDOFManager::deviceOpen(GHOST_IWindow* window,
        GHOST_NDOFLibraryInit_fp setNdofLibraryInit, 
        GHOST_NDOFLibraryShutdown_fp setNdofLibraryShutdown,
        GHOST_NDOFDeviceOpen_fp setNdofDeviceOpen)
{
    ndofLibraryInit = setNdofLibraryInit;
    ndofLibraryShutdown = setNdofLibraryShutdown;
    ndofDeviceOpen = setNdofDeviceOpen;

    if (ndofLibraryInit  && ndofDeviceOpen)
    {
       printf("%i client \n", ndofLibraryInit());
		
		m_DeviceHandle = ndofDeviceOpen((void *)&currentNdofValues);    
		
		#if defined(_WIN32) || defined(__APPLE__)
			m_DeviceHandle = ndofDeviceOpen((void *)&currentNdofValues);    
		#else
			GHOST_SystemX11 *sys;
			sys = static_cast<GHOST_SystemX11*>(GHOST_ISystem::getSystem());
			void *ndofInfo = sys->prepareNdofInfo(&currentNdofValues);
			m_DeviceHandle = ndofDeviceOpen(ndofInfo);
		#endif
	}
}


/** original patch only */
/*  
GHOST_TEventNDOFData*
GHOST_NDOFManager::handle(unsigned int message, unsigned int* wParam, unsigned long* lParam)
{
    static GHOST_TEventNDOFData sbdata;
    int handled = 0;
    if (ndofEventHandler && m_DeviceHandle != 0)
    {
        handled = ndofEventHandler(&sbdata.tx, m_DeviceHandle, message, wParam, lParam);
    }
    printf("handled %i\n", handled);
    return handled ? &sbdata : 0;
}
*/

bool 
GHOST_NDOFManager::available() const
{ 
    return m_DeviceHandle != 0; 
}

bool 
GHOST_NDOFManager::event_present() const
{ 
    if( currentNdofValues.changed >0) {
		printf("time %llu but%u x%i y%i z%i rx%i ry%i rz%i \n"	, 			
				currentNdofValues.time,		currentNdofValues.buttons,
				currentNdofValues.tx,currentNdofValues.ty,currentNdofValues.tz,
				currentNdofValues.rx,currentNdofValues.ry,currentNdofValues.rz);
    	return true;
	}else
	return false;

}

void        GHOST_NDOFManager::GHOST_NDOFGetDatas(GHOST_TEventNDOFData &datas) const
{
	datas.tx = currentNdofValues.tx;
	datas.ty = currentNdofValues.ty;
	datas.tz = currentNdofValues.tz;
	datas.rx = currentNdofValues.rx;
	datas.ry = currentNdofValues.ry;
	datas.rz = currentNdofValues.rz;
	datas.buttons = currentNdofValues.buttons;
	datas.client = currentNdofValues.client;
	datas.address = currentNdofValues.address;
	datas.time = currentNdofValues.time;
	datas.delta = currentNdofValues.delta;
}
