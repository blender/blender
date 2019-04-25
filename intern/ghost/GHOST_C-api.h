/*
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
 */
/** \file
 * \ingroup GHOST
 * \brief GHOST C-API function and type declarations.
 */

#ifndef __GHOST_C_API_H__
#define __GHOST_C_API_H__

#include "GHOST_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a "handle" for a C++ GHOST object.
 * A handle is just an opaque pointer to an empty struct.
 * In the API the pointer is casted to the actual C++ class.
 * The 'name' argument to the macro is the name of the handle to create.
 */

GHOST_DECLARE_HANDLE(GHOST_SystemHandle);
GHOST_DECLARE_HANDLE(GHOST_TimerTaskHandle);
GHOST_DECLARE_HANDLE(GHOST_WindowHandle);
GHOST_DECLARE_HANDLE(GHOST_EventHandle);
GHOST_DECLARE_HANDLE(GHOST_RectangleHandle);
GHOST_DECLARE_HANDLE(GHOST_EventConsumerHandle);
GHOST_DECLARE_HANDLE(GHOST_ContextHandle);

/**
 * Definition of a callback routine that receives events.
 * \param event The event received.
 * \param userdata The callback's user data, supplied to GHOST_CreateSystem.
 */
typedef int (*GHOST_EventCallbackProcPtr)(GHOST_EventHandle event, GHOST_TUserDataPtr userdata);

/**
 * Creates the one and only system.
 * \return a handle to the system.
 */
extern GHOST_SystemHandle GHOST_CreateSystem(void);

/**
 * Disposes the one and only system.
 * \param systemhandle The handle to the system
 * \return An indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeSystem(GHOST_SystemHandle systemhandle);

/**
 * Creates an event consumer object
 * \param eventCallback The event callback routine.
 * \param userdata Pointer to user data returned to the callback routine.
 */
extern GHOST_EventConsumerHandle GHOST_CreateEventConsumer(
    GHOST_EventCallbackProcPtr eventCallback, GHOST_TUserDataPtr userdata);

/**
 * Disposes an event consumer object
 * \param consumerhandle Handle to the event consumer.
 * \return An indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeEventConsumer(GHOST_EventConsumerHandle consumerhandle);

/**
 * Returns the system time.
 * Returns the number of milliseconds since the start of the system process.
 * Based on ANSI clock() routine.
 * \param systemhandle The handle to the system
 * \return The number of milliseconds.
 */
extern GHOST_TUns64 GHOST_GetMilliSeconds(GHOST_SystemHandle systemhandle);

/**
 * Installs a timer.
 * Note that, on most operating systems, messages need to be processed in order
 * for the timer callbacks to be invoked.
 * \param systemhandle The handle to the system
 * \param delay The time to wait for the first call to the timerProc (in milliseconds)
 * \param interval The interval between calls to the timerProc (in milliseconds)
 * \param timerProc The callback invoked when the interval expires,
 * \param userData Placeholder for user data.
 * \return A timer task (0 if timer task installation failed).
 */
extern GHOST_TimerTaskHandle GHOST_InstallTimer(GHOST_SystemHandle systemhandle,
                                                GHOST_TUns64 delay,
                                                GHOST_TUns64 interval,
                                                GHOST_TimerProcPtr timerProc,
                                                GHOST_TUserDataPtr userData);

/**
 * Removes a timer.
 * \param systemhandle The handle to the system
 * \param timertaskhandle Timer task to be removed.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_RemoveTimer(GHOST_SystemHandle systemhandle,
                                        GHOST_TimerTaskHandle timertaskhandle);

/***************************************************************************************
 * Display/window management functionality
 ***************************************************************************************/

/**
 * Returns the number of displays on this system.
 * \param systemhandle The handle to the system
 * \return The number of displays.
 */
extern GHOST_TUns8 GHOST_GetNumDisplays(GHOST_SystemHandle systemhandle);

/**
 * Returns the dimensions of the main display on this system.
 * \param systemhandle The handle to the system
 * \param width A pointer the width gets put in
 * \param height A pointer the height gets put in
 * \return void.
 */
extern void GHOST_GetMainDisplayDimensions(GHOST_SystemHandle systemhandle,
                                           GHOST_TUns32 *width,
                                           GHOST_TUns32 *height);

/**
 * Returns the dimensions of all displays combine
 * (the current workspace).
 * No need to worry about overlapping monitors.
 * \param systemhandle The handle to the system
 * \param width A pointer the width gets put in
 * \param height A pointer the height gets put in
 * \return void.
 */
extern void GHOST_GetAllDisplayDimensions(GHOST_SystemHandle systemhandle,
                                          GHOST_TUns32 *width,
                                          GHOST_TUns32 *height);

/**
 * Create a new window.
 * The new window is added to the list of windows managed.
 * Never explicitly delete the window, use disposeWindow() instead.
 * \param systemhandle The handle to the system
 * \param title The name of the window (displayed in the title bar of the window if the OS supports it).
 * \param left The coordinate of the left edge of the window.
 * \param top The coordinate of the top edge of the window.
 * \param width The width the window.
 * \param height The height the window.
 * \param state The state of the window when opened.
 * \param type The type of drawing context installed in this window.
 * \param glSettings: Misc OpenGL options.
 * \return A handle to the new window ( == NULL if creation failed).
 */
extern GHOST_WindowHandle GHOST_CreateWindow(GHOST_SystemHandle systemhandle,
                                             const char *title,
                                             GHOST_TInt32 left,
                                             GHOST_TInt32 top,
                                             GHOST_TUns32 width,
                                             GHOST_TUns32 height,
                                             GHOST_TWindowState state,
                                             GHOST_TDrawingContextType type,
                                             GHOST_GLSettings glSettings);

/**
 * Create a new offscreen context.
 * Never explicitly delete the context, use disposeContext() instead.
 * \param systemhandle The handle to the system
 * \return A handle to the new context ( == NULL if creation failed).
 */
extern GHOST_ContextHandle GHOST_CreateOpenGLContext(GHOST_SystemHandle systemhandle);

/**
 * Dispose of a context.
 * \param systemhandle The handle to the system
 * \param contexthandle Handle to the context to be disposed.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeOpenGLContext(GHOST_SystemHandle systemhandle,
                                                 GHOST_ContextHandle contexthandle);

/**
 * Returns the window user data.
 * \param windowhandle The handle to the window
 * \return The window user data.
 */
extern GHOST_TUserDataPtr GHOST_GetWindowUserData(GHOST_WindowHandle windowhandle);

/**
 * Changes the window user data.
 * \param windowhandle The handle to the window
 * \param userdata The window user data.
 */
extern void GHOST_SetWindowUserData(GHOST_WindowHandle windowhandle, GHOST_TUserDataPtr userdata);

/**
 * Dispose a window.
 * \param systemhandle The handle to the system
 * \param windowhandle Handle to the window to be disposed.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeWindow(GHOST_SystemHandle systemhandle,
                                          GHOST_WindowHandle windowhandle);

/**
 * Returns whether a window is valid.
 * \param systemhandle The handle to the system
 * \param windowhandle Handle to the window to be checked.
 * \return Indication of validity.
 */
extern int GHOST_ValidWindow(GHOST_SystemHandle systemhandle, GHOST_WindowHandle windowhandle);

/**
 * Begins full screen mode.
 * \param systemhandle The handle to the system
 * \param setting The new setting of the display.
 * \param stereoVisual Option for stereo display.
 * \return A handle to the window displayed in full screen.
 *         This window is invalid after full screen has been ended.
 */
extern GHOST_WindowHandle GHOST_BeginFullScreen(GHOST_SystemHandle systemhandle,
                                                GHOST_DisplaySetting *setting,
                                                const int stereoVisual);

/**
 * Ends full screen mode.
 * \param systemhandle The handle to the system
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_EndFullScreen(GHOST_SystemHandle systemhandle);

/**
 * Returns current full screen mode status.
 * \param systemhandle The handle to the system
 * \return The current status.
 */
extern int GHOST_GetFullScreen(GHOST_SystemHandle systemhandle);

/***************************************************************************************
 * Event management functionality
 ***************************************************************************************/

/**
 * Retrieves events from the system and stores them in the queue.
 * \param systemhandle The handle to the system
 * \param waitForEvent Boolean to indicate that ProcessEvents should
 * wait (block) until the next event before returning.
 * \return Indication of the presence of events.
 */
extern int GHOST_ProcessEvents(GHOST_SystemHandle systemhandle, int waitForEvent);

/**
 * Retrieves events from the queue and send them to the event consumers.
 * \param systemhandle The handle to the system
 */
extern void GHOST_DispatchEvents(GHOST_SystemHandle systemhandle);

/**
 * Adds the given event consumer to our list.
 * \param systemhandle The handle to the system
 * \param consumerhandle The event consumer to add.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_AddEventConsumer(GHOST_SystemHandle systemhandle,
                                             GHOST_EventConsumerHandle consumerhandle);

/**
 * Remove the given event consumer to our list.
 * \param systemhandle The handle to the system
 * \param consumerhandle The event consumer to remove.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_RemoveEventConsumer(GHOST_SystemHandle systemhandle,
                                                GHOST_EventConsumerHandle consumerhandle);

/***************************************************************************************
 * Progress bar functionality
 ***************************************************************************************/

/**
 * Sets the progress bar value displayed in the window/application icon
 * \param windowhandle The handle to the window
 * \param progress The progress % (0.0 to 1.0)
 */
extern GHOST_TSuccess GHOST_SetProgressBar(GHOST_WindowHandle windowhandle, float progress);

/**
 * Hides the progress bar in the icon
 * \param windowhandle The handle to the window
 */
extern GHOST_TSuccess GHOST_EndProgressBar(GHOST_WindowHandle windowhandle);

/***************************************************************************************
 * Cursor management functionality
 ***************************************************************************************/

/**
 * Returns the current cursor shape.
 * \param windowhandle The handle to the window
 * \return The current cursor shape.
 */
extern GHOST_TStandardCursor GHOST_GetCursorShape(GHOST_WindowHandle windowhandle);

/**
 * Set the shape of the cursor.
 * \param windowhandle The handle to the window
 * \param cursorshape The new cursor shape type id.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCursorShape(GHOST_WindowHandle windowhandle,
                                           GHOST_TStandardCursor cursorshape);

/**
 * Set the shape of the cursor to a custom cursor.
 * \param windowhandle The handle to the window
 * \param bitmap The bitmap data for the cursor.
 * \param  mask The mask data for the cursor.
 * \param hotX The X coordinate of the cursor hotspot.
 * \param hotY The Y coordinate of the cursor hotspot.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCustomCursorShape(GHOST_WindowHandle windowhandle,
                                                 GHOST_TUns8 bitmap[16][2],
                                                 GHOST_TUns8 mask[16][2],
                                                 int hotX,
                                                 int hotY);
/**
 * Set the shape of the cursor to a custom cursor of specified size.
 * \param windowhandle The handle to the window
 * \param bitmap The bitmap data for the cursor.
 * \param mask The mask data for the cursor.
 * \param sizex The width of the cursor
 * \param sizey The height of the cursor
 * \param hotX The X coordinate of the cursor hotspot.
 * \param hotY The Y coordinate of the cursor hotspot.
 * \param   fg_color, bg_color  Colors of the cursor
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCustomCursorShapeEx(GHOST_WindowHandle windowhandle,
                                                   GHOST_TUns8 *bitmap,
                                                   GHOST_TUns8 *mask,
                                                   int sizex,
                                                   int sizey,
                                                   int hotX,
                                                   int hotY,
                                                   int fg_color,
                                                   int bg_color);

/**
 * Returns the visibility state of the cursor.
 * \param windowhandle The handle to the window
 * \return The visibility state of the cursor.
 */
extern int GHOST_GetCursorVisibility(GHOST_WindowHandle windowhandle);

/**
 * Shows or hides the cursor.
 * \param windowhandle The handle to the window
 * \param visible The new visibility state of the cursor.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCursorVisibility(GHOST_WindowHandle windowhandle, int visible);

/**
 * Returns the current location of the cursor (location in screen coordinates)
 * \param systemhandle The handle to the system
 * \param x The x-coordinate of the cursor.
 * \param y The y-coordinate of the cursor.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_GetCursorPosition(GHOST_SystemHandle systemhandle,
                                              GHOST_TInt32 *x,
                                              GHOST_TInt32 *y);

/**
 * Updates the location of the cursor (location in screen coordinates).
 * Not all operating systems allow the cursor to be moved (without the input device being moved).
 * \param systemhandle The handle to the system
 * \param x The x-coordinate of the cursor.
 * \param y The y-coordinate of the cursor.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCursorPosition(GHOST_SystemHandle systemhandle,
                                              GHOST_TInt32 x,
                                              GHOST_TInt32 y);

/**
 * Grabs the cursor for a modal operation, to keep receiving
 * events when the mouse is outside the window. X11 only, others
 * do this automatically.
 * \param windowhandle The handle to the window
 * \param mode The new grab state of the cursor.
 * \param bounds The grab region (optional) - left,top,right,bottom
 * \param mouse_ungrab_xy XY for new mouse location (optional) - x,y
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCursorGrab(GHOST_WindowHandle windowhandle,
                                          GHOST_TGrabCursorMode mode,
                                          int bounds[4],
                                          const int mouse_ungrab_xy[2]);

/***************************************************************************************
 * Access to mouse button and keyboard states.
 ***************************************************************************************/

/**
 * Returns the state of a modifier key (ouside the message queue).
 * \param systemhandle The handle to the system
 * \param mask The modifier key state to retrieve.
 * \param isDown Pointer to return modifier state in.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_GetModifierKeyState(GHOST_SystemHandle systemhandle,
                                                GHOST_TModifierKeyMask mask,
                                                int *isDown);

/**
 * Returns the state of a mouse button (ouside the message queue).
 * \param systemhandle The handle to the system
 * \param mask The button state to retrieve.
 * \param isDown Pointer to return button state in.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_GetButtonState(GHOST_SystemHandle systemhandle,
                                           GHOST_TButtonMask mask,
                                           int *isDown);

#ifdef WITH_INPUT_NDOF
/***************************************************************************************
 * Access to 3D mouse.
 ***************************************************************************************/

/**
 * Sets 3D mouse deadzone
 * \param deadzone Deadzone of the 3D mouse (both for rotation and pan) relative to full range
 */
extern void GHOST_setNDOFDeadZone(float deadzone);
#endif

/***************************************************************************************
 * Drag'n'drop operations
 ***************************************************************************************/

/**
 * Tells if the ongoing drag'n'drop object can be accepted upon mouse drop
 */
extern void GHOST_setAcceptDragOperation(GHOST_WindowHandle windowhandle, GHOST_TInt8 canAccept);

/**
 * Returns the event type.
 * \param eventhandle The handle to the event
 * \return The event type.
 */
extern GHOST_TEventType GHOST_GetEventType(GHOST_EventHandle eventhandle);

/**
 * Returns the time this event was generated.
 * \param eventhandle The handle to the event
 * \return The event generation time.
 */
extern GHOST_TUns64 GHOST_GetEventTime(GHOST_EventHandle eventhandle);

/**
 * Returns the window this event was generated on,
 * or NULL if it is a 'system' event.
 * \param eventhandle The handle to the event
 * \return The generating window.
 */
extern GHOST_WindowHandle GHOST_GetEventWindow(GHOST_EventHandle eventhandle);

/**
 * Returns the event data.
 * \param eventhandle The handle to the event
 * \return The event data.
 */
extern GHOST_TEventDataPtr GHOST_GetEventData(GHOST_EventHandle eventhandle);

/**
 * Returns the timer callback.
 * \param timertaskhandle The handle to the timertask
 * \return The timer callback.
 */
extern GHOST_TimerProcPtr GHOST_GetTimerProc(GHOST_TimerTaskHandle timertaskhandle);

/**
 * Changes the timer callback.
 * \param timertaskhandle The handle to the timertask
 * \param timerProc The timer callback.
 */
extern void GHOST_SetTimerProc(GHOST_TimerTaskHandle timertaskhandle,
                               GHOST_TimerProcPtr timerProc);

/**
 * Returns the timer user data.
 * \param timertaskhandle The handle to the timertask
 * \return The timer user data.
 */
extern GHOST_TUserDataPtr GHOST_GetTimerTaskUserData(GHOST_TimerTaskHandle timertaskhandle);

/**
 * Changes the time user data.
 * \param timertaskhandle The handle to the timertask
 * \param userdata The timer user data.
 */
extern void GHOST_SetTimerTaskUserData(GHOST_TimerTaskHandle timertaskhandle,
                                       GHOST_TUserDataPtr userdata);

/**
 * Returns indication as to whether the window is valid.
 * \param windowhandle The handle to the window
 * \return The validity of the window.
 */
extern int GHOST_GetValid(GHOST_WindowHandle windowhandle);

/**
 * Returns the type of drawing context used in this window.
 * \param windowhandle The handle to the window
 * \return The current type of drawing context.
 */
extern GHOST_TDrawingContextType GHOST_GetDrawingContextType(GHOST_WindowHandle windowhandle);

/**
 * Tries to install a rendering context in this window.
 * \param windowhandle The handle to the window
 * \param type The type of rendering context installed.
 * \return Indication as to whether installation has succeeded.
 */
extern GHOST_TSuccess GHOST_SetDrawingContextType(GHOST_WindowHandle windowhandle,
                                                  GHOST_TDrawingContextType type);

/**
 * Sets the title displayed in the title bar.
 * \param windowhandle The handle to the window
 * \param title The title to display in the title bar.
 */
extern void GHOST_SetTitle(GHOST_WindowHandle windowhandle, const char *title);

/**
 * Returns the title displayed in the title bar. The title
 * should be free'd with free().
 *
 * \param windowhandle The handle to the window
 * \return The title, free with free().
 */
extern char *GHOST_GetTitle(GHOST_WindowHandle windowhandle);

/**
 * Returns the window rectangle dimensions.
 * These are screen coordinates.
 * \param windowhandle The handle to the window
 * \return A handle to the bounding rectangle of the window.
 */
extern GHOST_RectangleHandle GHOST_GetWindowBounds(GHOST_WindowHandle windowhandle);

/**
 * Returns the client rectangle dimensions.
 * The left and top members of the rectangle are always zero.
 * \param windowhandle The handle to the window
 * \return A handle to the bounding rectangle of the window.
 */
extern GHOST_RectangleHandle GHOST_GetClientBounds(GHOST_WindowHandle windowhandle);

/**
 * Disposes a rectangle object
 * \param rectanglehandle Handle to the rectangle.
 */
void GHOST_DisposeRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Resizes client rectangle width.
 * \param windowhandle The handle to the window
 * \param width The new width of the client area of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetClientWidth(GHOST_WindowHandle windowhandle, GHOST_TUns32 width);

/**
 * Resizes client rectangle height.
 * \param windowhandle The handle to the window
 * \param height The new height of the client area of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetClientHeight(GHOST_WindowHandle windowhandle, GHOST_TUns32 height);

/**
 * Resizes client rectangle.
 * \param windowhandle The handle to the window
 * \param width The new width of the client area of the window.
 * \param height The new height of the client area of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetClientSize(GHOST_WindowHandle windowhandle,
                                          GHOST_TUns32 width,
                                          GHOST_TUns32 height);

/**
 * Converts a point in screen coordinates to client rectangle coordinates
 * \param windowhandle The handle to the window
 * \param inX The x-coordinate on the screen.
 * \param inY The y-coordinate on the screen.
 * \param outX The x-coordinate in the client rectangle.
 * \param outY The y-coordinate in the client rectangle.
 */
extern void GHOST_ScreenToClient(GHOST_WindowHandle windowhandle,
                                 GHOST_TInt32 inX,
                                 GHOST_TInt32 inY,
                                 GHOST_TInt32 *outX,
                                 GHOST_TInt32 *outY);

/**
 * Converts a point in screen coordinates to client rectangle coordinates
 * \param windowhandle The handle to the window
 * \param inX The x-coordinate in the client rectangle.
 * \param inY The y-coordinate in the client rectangle.
 * \param outX The x-coordinate on the screen.
 * \param outY The y-coordinate on the screen.
 */
extern void GHOST_ClientToScreen(GHOST_WindowHandle windowhandle,
                                 GHOST_TInt32 inX,
                                 GHOST_TInt32 inY,
                                 GHOST_TInt32 *outX,
                                 GHOST_TInt32 *outY);

/**
 * Returns the state of the window (normal, minimized, maximized).
 * \param windowhandle The handle to the window
 * \return The state of the window.
 */
extern GHOST_TWindowState GHOST_GetWindowState(GHOST_WindowHandle windowhandle);

/**
 * Sets the state of the window (normal, minimized, maximized).
 * \param windowhandle The handle to the window
 * \param state The state of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetWindowState(GHOST_WindowHandle windowhandle,
                                           GHOST_TWindowState state);

/**
 * Sets the window "modified" status, indicating unsaved changes
 * \param windowhandle The handle to the window
 * \param isUnsavedChanges Unsaved changes or not
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetWindowModifiedState(GHOST_WindowHandle windowhandle,
                                                   GHOST_TUns8 isUnsavedChanges);

/**
 * Sets the order of the window (bottom, top).
 * \param windowhandle The handle to the window
 * \param order The order of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetWindowOrder(GHOST_WindowHandle windowhandle,
                                           GHOST_TWindowOrder order);

/**
 * Swaps front and back buffers of a window.
 * \param windowhandle The handle to the window
 * \return A success indicator.
 */
extern GHOST_TSuccess GHOST_SwapWindowBuffers(GHOST_WindowHandle windowhandle);

/**
 * Sets the swap interval for swapBuffers.
 * \param interval The swap interval to use.
 * \return A boolean success indicator.
 */
extern GHOST_TSuccess GHOST_SetSwapInterval(GHOST_WindowHandle windowhandle, int interval);

/**
 * Gets the current swap interval for swapBuffers.
 * \param windowhandle The handle to the window
 * \param intervalOut pointer to location to return swap interval (left untouched if there is an error)
 * \return A boolean success indicator of if swap interval was successfully read.
 */
extern GHOST_TSuccess GHOST_GetSwapInterval(GHOST_WindowHandle windowhandle, int *intervalOut);

/**
 * Activates the drawing context of this window.
 * \param windowhandle The handle to the window
 * \return A success indicator.
 */
extern GHOST_TSuccess GHOST_ActivateWindowDrawingContext(GHOST_WindowHandle windowhandle);

/**
 * Invalidates the contents of this window.
 * \param windowhandle The handle to the window
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_InvalidateWindow(GHOST_WindowHandle windowhandle);

/**
 * Activates the drawing context of this context.
 * \param contexthandle The handle to the context
 * \return A success indicator.
 */
extern GHOST_TSuccess GHOST_ActivateOpenGLContext(GHOST_ContextHandle contexthandle);

/**
 * Release the drawing context bound to this thread.
 * \param contexthandle The handle to the context
 * \return A success indicator.
 */
extern GHOST_TSuccess GHOST_ReleaseOpenGLContext(GHOST_ContextHandle contexthandle);

/**
 * Set which tablet API to use. Only affects Windows, other platforms have a single API.
 * \param systemhandle The handle to the system
 * \param api Enum indicating which API to use.
 */
extern void GHOST_SetTabletAPI(GHOST_SystemHandle systemhandle, GHOST_TTabletAPI api);

/**
 * Returns the status of the tablet
 * \param windowhandle The handle to the window
 * \return Status of tablet
 */
extern const GHOST_TabletData *GHOST_GetTabletData(GHOST_WindowHandle windowhandle);

/**
 * Access to rectangle width.
 * \param rectanglehandle The handle to the rectangle
 * \return width of the rectangle
 */
extern GHOST_TInt32 GHOST_GetWidthRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Access to rectangle height.
 * \param rectanglehandle The handle to the rectangle
 * \return height of the rectangle
 */
extern GHOST_TInt32 GHOST_GetHeightRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Gets all members of the rectangle.
 * \param rectanglehandle The handle to the rectangle
 * \param l Pointer to return left coordinate in.
 * \param t Pointer to return top coordinate in.
 * \param r Pointer to return right coordinate in.
 * \param b Pointer to return bottom coordinate in.
 */
extern void GHOST_GetRectangle(GHOST_RectangleHandle rectanglehandle,
                               GHOST_TInt32 *l,
                               GHOST_TInt32 *t,
                               GHOST_TInt32 *r,
                               GHOST_TInt32 *b);

/**
 * Sets all members of the rectangle.
 * \param rectanglehandle The handle to the rectangle
 * \param l requested left coordinate of the rectangle
 * \param t requested top coordinate of the rectangle
 * \param r requested right coordinate of the rectangle
 * \param b requested bottom coordinate of the rectangle
 */
extern void GHOST_SetRectangle(GHOST_RectangleHandle rectanglehandle,
                               GHOST_TInt32 l,
                               GHOST_TInt32 t,
                               GHOST_TInt32 r,
                               GHOST_TInt32 b);

/**
 * Returns whether this rectangle is empty.
 * Empty rectangles are rectangles that have width==0 and/or height==0.
 * \param rectanglehandle The handle to the rectangle
 * \return Success value (true == empty rectangle)
 */
extern GHOST_TSuccess GHOST_IsEmptyRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Returns whether this rectangle is valid.
 * Valid rectangles are rectangles that have m_l <= m_r and m_t <= m_b. Thus, empty rectangles are valid.
 * \param rectanglehandle The handle to the rectangle
 * \return Success value (true == valid rectangle)
 */
extern GHOST_TSuccess GHOST_IsValidRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Grows (or shrinks the rectangle).
 * The method avoids negative insets making the rectangle invalid
 * \param rectanglehandle The handle to the rectangle
 * \param i The amount of offset given to each extreme (negative values shrink the rectangle).
 */
extern void GHOST_InsetRectangle(GHOST_RectangleHandle rectanglehandle, GHOST_TInt32 i);

/**
 * Does a union of the rectangle given and this rectangle.
 * The result is stored in this rectangle.
 * \param rectanglehandle The handle to the rectangle
 * \param anotherrectanglehandle The rectangle that is input for the union operation.
 */
extern void GHOST_UnionRectangle(GHOST_RectangleHandle rectanglehandle,
                                 GHOST_RectangleHandle anotherrectanglehandle);

/**
 * Grows the rectangle to included a point.
 * \param rectanglehandle The handle to the rectangle
 * \param x The x-coordinate of the point.
 * \param y The y-coordinate of the point.
 */
extern void GHOST_UnionPointRectangle(GHOST_RectangleHandle rectanglehandle,
                                      GHOST_TInt32 x,
                                      GHOST_TInt32 y);

/**
 * Returns whether the point is inside this rectangle.
 * Point on the boundary is considered inside.
 * \param rectanglehandle The handle to the rectangle
 * \param x x-coordinate of point to test.
 * \param y y-coordinate of point to test.
 * \return Success value (true if point is inside).
 */
extern GHOST_TSuccess GHOST_IsInsideRectangle(GHOST_RectangleHandle rectanglehandle,
                                              GHOST_TInt32 x,
                                              GHOST_TInt32 y);

/**
 * Returns whether the rectangle is inside this rectangle.
 * \param rectanglehandle The handle to the rectangle
 * \param anotherrectanglehandle The rectangle to test.
 * \return visibility (not, partially or fully visible).
 */
extern GHOST_TVisibility GHOST_GetRectangleVisibility(
    GHOST_RectangleHandle rectanglehandle, GHOST_RectangleHandle anotherrectanglehandle);

/**
 * Sets rectangle members.
 * Sets rectangle members such that it is centered at the given location.
 * \param rectanglehandle The handle to the rectangle
 * \param cx Requested center x-coordinate of the rectangle
 * \param cy Requested center y-coordinate of the rectangle
 */
extern void GHOST_SetCenterRectangle(GHOST_RectangleHandle rectanglehandle,
                                     GHOST_TInt32 cx,
                                     GHOST_TInt32 cy);

/**
 * Sets rectangle members.
 * Sets rectangle members such that it is centered at the given location,
 * with the width requested.
 * \param rectanglehandle The handle to the rectangle
 * \param cx requested center x-coordinate of the rectangle
 * \param cy requested center y-coordinate of the rectangle
 * \param w requested width of the rectangle
 * \param h requested height of the rectangle
 */
extern void GHOST_SetRectangleCenter(GHOST_RectangleHandle rectanglehandle,
                                     GHOST_TInt32 cx,
                                     GHOST_TInt32 cy,
                                     GHOST_TInt32 w,
                                     GHOST_TInt32 h);

/**
 * Clips a rectangle.
 * Updates the rectangle given such that it will fit within this one.
 * This can result in an empty rectangle.
 * \param rectanglehandle The handle to the rectangle
 * \param anotherrectanglehandle The rectangle to clip
 * \return Whether clipping has occurred
 */
extern GHOST_TSuccess GHOST_ClipRectangle(GHOST_RectangleHandle rectanglehandle,
                                          GHOST_RectangleHandle anotherrectanglehandle);

/**
 * Return the data from the clipboard
 * \param selection Boolean to return the selection instead, X11 only feature.
 * \return clipboard data
 */
extern GHOST_TUns8 *GHOST_getClipboard(int selection);

/**
 * Put data to the Clipboard
 * \param buffer the string buffer to set.
 * \param selection Set the selection instead, X11 only feature.
 */
extern void GHOST_putClipboard(GHOST_TInt8 *buffer, int selection);

/**
 * Toggles console
 * \param action
 * - 0: Hides
 * - 1: Shows
 * - 2: Toggles
 * - 3: Hides if it runs not from  command line
 * - *: Does nothing
 * \return current status (1 -visible, 0 - hidden)
 */
extern int GHOST_toggleConsole(int action);

/**
 * Confirms quitting he program when there is just one window left open
 * in the application
 */
extern int GHOST_confirmQuit(GHOST_WindowHandle windowhandle);

/**
 * Informs if the system provides native dialogs (eg. confirm quit)
 */
extern int GHOST_SupportsNativeDialogs(void);

/**
 * Use native pixel size (MacBook pro 'retina'), if supported.
 */
extern int GHOST_UseNativePixels(void);

/**
 * Focus window after opening, or put them in the background.
 */
extern void GHOST_UseWindowFocus(int use_focus);

/**
 * If window was opened using native pixel size, it returns scaling factor.
 */
extern float GHOST_GetNativePixelSize(GHOST_WindowHandle windowhandle);

/**
 * Returns the suggested DPI for this window.
 */
extern GHOST_TUns16 GHOST_GetDPIHint(GHOST_WindowHandle windowhandle);

/**
 * Enable IME attached to the given window, i.e. allows user-input
 * events to be dispatched to the IME.
 * \param windowhandle Window handle of the caller
 * \param x Requested x-coordinate of the rectangle
 * \param y Requested y-coordinate of the rectangle
 * \param w Requested width of the rectangle
 * \param h Requested height of the rectangle
 * \param complete Whether or not to complete the ongoing composition
 * true:  Start a new composition
 * false: Move the IME windows to the given position without finishing it.
 */
extern void GHOST_BeginIME(GHOST_WindowHandle windowhandle,
                           GHOST_TInt32 x,
                           GHOST_TInt32 y,
                           GHOST_TInt32 w,
                           GHOST_TInt32 h,
                           int complete);
/**
 * Disable the IME attached to the given window, i.e. prohibits any user-input
 * events from being dispatched to the IME.
 * \param windowhandle The window handle of the caller
 */
extern void GHOST_EndIME(GHOST_WindowHandle windowhandle);

#ifdef __cplusplus
}
#endif

#endif
