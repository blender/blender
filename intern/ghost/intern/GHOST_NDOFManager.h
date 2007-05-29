
#ifndef _GHOST_NDOFMANAGER_H_
#define _GHOST_NDOFMANAGER_H_

#include "GHOST_System.h"
#include "GHOST_IWindow.h"



class GHOST_NDOFManager
{
public:
	/**
	 * Constructor.
	 */
	GHOST_NDOFManager();

	/**
	 * Destructor.
	 */
	virtual ~GHOST_NDOFManager();

    void deviceOpen(GHOST_IWindow* window,
        GHOST_NDOFLibraryInit_fp setNdofLibraryInit, 
        GHOST_NDOFLibraryShutdown_fp setNdofLibraryShutdown,
        GHOST_NDOFDeviceOpen_fp setNdofDeviceOpen,
        GHOST_NDOFEventHandler_fp setNdofEventHandler);

    bool available();

    /* to do: abstract for Linux, MacOS, etc. */
    GHOST_TEventNDOFData* handle(unsigned int message, unsigned int* wparam, unsigned long* lparam);

protected:
    void* m_DeviceHandle;
};


#endif
