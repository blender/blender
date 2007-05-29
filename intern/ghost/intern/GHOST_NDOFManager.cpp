
// Insert Blender compatible license here :-)

// note: an implementation is currently only provided for Windows, but designed to be easy to move to Linux, etc.

/**

    To use this implemenation, you must specify the #define WITH_SPACEBALL for the ghost library.
    Only this cpp file is affected by the macro, the header file and everything else are independent
    of the spaceball libraries.

    The 3dXWare SDK is available from the tab on the left side of -
    http://www.3dconnexion.com/support/4a.php

    The SDK is necessary to build this file with WITH_SPACEBALL defined.

    For this stuff to work, siappdll.dll and spwini.dll must be in the executable path of blender

 */


#include "GHOST_NDOFManager.h"
//#include "GHOST_WindowWin32.h"





namespace
{
    GHOST_NDOFLibraryInit_fp ndofLibraryInit = 0;
    GHOST_NDOFLibraryShutdown_fp ndofLibraryShutdown = 0;
    GHOST_NDOFDeviceOpen_fp ndofDeviceOpen = 0;
    GHOST_NDOFEventHandler_fp ndofEventHandler = 0;
}


//typedef enum SpwRetVal (WINAPI *PFNSI_INIT) (void);

GHOST_NDOFManager::GHOST_NDOFManager()
{
    m_DeviceHandle = 0;

    // discover the API from the plugin
    ndofLibraryInit = 0;
    ndofLibraryShutdown = 0;
    ndofDeviceOpen = 0;
    ndofEventHandler = 0;
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
        GHOST_NDOFDeviceOpen_fp setNdofDeviceOpen,
        GHOST_NDOFEventHandler_fp setNdofEventHandler)
{
    ndofLibraryInit = setNdofLibraryInit;
    ndofLibraryShutdown = setNdofLibraryShutdown;
    ndofDeviceOpen = setNdofDeviceOpen;
    ndofEventHandler = setNdofEventHandler;

    if (ndofLibraryInit)
    {
        ndofLibraryInit();
    }
/*
    if (ndofDeviceOpen)
    {
        GHOST_WindowWin32* win32 = (GHOST_WindowWin32*) window; // GHOST_IWindow doesn't have RTTI...
        if (win32 != 0)
        {
            m_DeviceHandle = ndofDeviceOpen(win32->getHWND());
        }
    }
    */
}



GHOST_TEventNDOFData*
GHOST_NDOFManager::handle(unsigned int message, unsigned int* wParam, unsigned long* lParam)
{
    static GHOST_TEventNDOFData sbdata;
    int handled = 0;
    if (ndofEventHandler && m_DeviceHandle != 0)
    {
        handled = ndofEventHandler(&sbdata.tx, m_DeviceHandle, message, wParam, lParam);
    }
    return handled ? &sbdata : 0;
}


bool 
GHOST_NDOFManager::available() 
{ 
    return m_DeviceHandle != 0; 
}
