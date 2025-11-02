/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup GHOST
 * \brief GHOST C-API function and type declarations.
 */

#pragma once

#include "GHOST_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Definition of a callback routine that receives events.
 * \param event: The event received.
 * \param user_data: The callback's user data, supplied to #GHOST_CreateSystem.
 */
using GHOST_EventCallbackProcPtr = bool (*)(GHOST_EventHandle event, GHOST_TUserDataPtr user_data);

/**
 * Creates the one and only system.
 * \return a handle to the system.
 */
extern GHOST_SystemHandle GHOST_CreateSystem(void);
extern GHOST_SystemHandle GHOST_CreateSystemBackground(void);

/**
 * Specifies whether debug messages are to be enabled for the specific system handle.
 * \param systemhandle: The handle to the system.
 * \param debug: Flag for systems to debug.
 */
extern void GHOST_SystemInitDebug(GHOST_SystemHandle systemhandle, GHOST_Debug debug);

#if !(defined(WIN32) || defined(__APPLE__))
extern const char *GHOST_SystemBackend(void);
#endif

/**
 * Disposes the one and only system.
 * \param systemhandle: The handle to the system.
 * \return An indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeSystem(GHOST_SystemHandle systemhandle);

/**
 * Show a system message box to the user
 * \param systemhandle: The handle to the system.
 * \param title: Title of the message box.
 * \param message: Message of the message box.
 * \param help_label: Text to show on the help button that opens a link.
 * \param continue_label: Text to show on the ok button that continues.
 * \param link: Optional (hyper)link to a web-page to show when pressing help.
 * \param dialog_options: Options to configure the message box.
 */
extern void GHOST_ShowMessageBox(GHOST_SystemHandle systemhandle,
                                 const char *title,
                                 const char *message,
                                 const char *help_label,
                                 const char *continue_label,
                                 const char *link,
                                 GHOST_DialogOptions dialog_options);

/**
 * Creates an event consumer object
 * \param eventCallback: The event callback routine.
 * \param user_data: Pointer to user data returned to the callback routine.
 */
extern GHOST_EventConsumerHandle GHOST_CreateEventConsumer(
    GHOST_EventCallbackProcPtr eventCallback, GHOST_TUserDataPtr user_data);

/**
 * Disposes an event consumer object
 * \param consumerhandle: Handle to the event consumer.
 * \return An indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeEventConsumer(GHOST_EventConsumerHandle consumerhandle);

/**
 * Returns the system time.
 * Returns the number of milliseconds since the start of the system.
 *
 * \param systemhandle: The handle to the system.
 * \return The number of milliseconds.
 */
extern uint64_t GHOST_GetMilliSeconds(GHOST_SystemHandle systemhandle);

/**
 * Installs a timer.
 * Note that, on most operating systems, messages need to be processed in order
 * for the timer callbacks to be invoked.
 * \param systemhandle: The handle to the system.
 * \param delay: The time to wait for the first call to the timer_proc (in milliseconds).
 * \param interval: The interval between calls to the timer_proc (in milliseconds).
 * \param timer_proc: The callback invoked when the interval expires.
 * \param user_data: Placeholder for user data.
 * \return A timer task (0 if timer task installation failed).
 */
extern GHOST_TimerTaskHandle GHOST_InstallTimer(GHOST_SystemHandle systemhandle,
                                                uint64_t delay,
                                                uint64_t interval,
                                                GHOST_TimerProcPtr timer_proc,
                                                GHOST_TUserDataPtr user_data);

/**
 * Removes a timer.
 * \param systemhandle: The handle to the system.
 * \param timertaskhandle: Timer task to be removed.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_RemoveTimer(GHOST_SystemHandle systemhandle,
                                        GHOST_TimerTaskHandle timertaskhandle);

/***************************************************************************************
 * Display/window management functionality
 ***************************************************************************************/

/**
 * Returns the number of displays on this system.
 * \param systemhandle: The handle to the system.
 * \return The number of displays.
 */
extern uint8_t GHOST_GetNumDisplays(GHOST_SystemHandle systemhandle);

/**
 * Returns the dimensions of the main display on this system.
 * \param systemhandle: The handle to the system.
 * \param r_width: A pointer the width gets put in.
 * \param r_height: A pointer the height gets put in.
 * \return success.
 */
extern GHOST_TSuccess GHOST_GetMainDisplayDimensions(GHOST_SystemHandle systemhandle,
                                                     uint32_t *r_width,
                                                     uint32_t *r_height);

/**
 * Returns the dimensions of all displays combine
 * (the current workspace).
 * No need to worry about overlapping monitors.
 * \param systemhandle: The handle to the system.
 * \param r_width: A pointer the width gets put in.
 * \param r_height: A pointer the height gets put in.
 * \return success.
 */
extern GHOST_TSuccess GHOST_GetAllDisplayDimensions(GHOST_SystemHandle systemhandle,
                                                    uint32_t *r_width,
                                                    uint32_t *r_height);
/**
 * Create a new window.
 * The new window is added to the list of windows managed.
 * Never explicitly delete the window, use disposeWindow() instead.
 * \param systemhandle: The handle to the system.
 * \param parent_windowhandle: Handle of parent (or owner) window, or nullptr
 * \param title: The name of the window.
 * (displayed in the title bar of the window if the OS supports it).
 * \param left: The coordinate of the left edge of the window.
 * \param top: The coordinate of the top edge of the window.
 * \param width: The width the window.
 * \param height: The height the window.
 * \param state: The state of the window when opened.
 * \param is_dialog: Stay on top of parent window, no icon in taskbar, can't be minimized.
 * \param gpu_settings: Misc GPU options.
 * \return A handle to the new window ( == nullptr if creation failed).
 */
extern GHOST_WindowHandle GHOST_CreateWindow(GHOST_SystemHandle systemhandle,
                                             GHOST_WindowHandle parent_windowhandle,
                                             const char *title,
                                             int32_t left,
                                             int32_t top,
                                             uint32_t width,
                                             uint32_t height,
                                             GHOST_TWindowState state,
                                             bool is_dialog,
                                             GHOST_GPUSettings gpu_settings);

/**
 * Create a new off-screen context.
 * Never explicitly delete the context, use #disposeContext() instead.
 * \param systemhandle: The handle to the system.
 * \param gpu_settings: Misc GPU options.
 * \return A handle to the new context ( == nullptr if creation failed).
 */
extern GHOST_ContextHandle GHOST_CreateGPUContext(GHOST_SystemHandle systemhandle,
                                                  GHOST_GPUSettings gpu_settings);

/**
 * Dispose of a context.
 * \param systemhandle: The handle to the system.
 * \param contexthandle: Handle to the context to be disposed.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeGPUContext(GHOST_SystemHandle systemhandle,
                                              GHOST_ContextHandle contexthandle);

/**
 * Returns the window user data.
 * \param windowhandle: The handle to the window.
 * \return The window user data.
 */
extern GHOST_TUserDataPtr GHOST_GetWindowUserData(GHOST_WindowHandle windowhandle);

/**
 * Changes the window user data.
 * \param windowhandle: The handle to the window.
 * \param user_data: The window user data.
 */
extern void GHOST_SetWindowUserData(GHOST_WindowHandle windowhandle, GHOST_TUserDataPtr user_data);

extern bool GHOST_IsDialogWindow(GHOST_WindowHandle windowhandle);

/**
 * Dispose a window.
 * \param systemhandle: The handle to the system.
 * \param windowhandle: Handle to the window to be disposed.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeWindow(GHOST_SystemHandle systemhandle,
                                          GHOST_WindowHandle windowhandle);

/**
 * Returns whether a window is valid.
 * \param systemhandle: The handle to the system.
 * \param windowhandle: Handle to the window to be checked.
 * \return Indication of validity.
 */
extern bool GHOST_ValidWindow(GHOST_SystemHandle systemhandle, GHOST_WindowHandle windowhandle);

/**
 * Returns high dynamic range color information about this window.
 * \return HDR info.
 */
extern GHOST_WindowHDRInfo GHOST_WindowGetHDRInfo(GHOST_WindowHandle windowhandle);

/**
 * Get the Window under the cursor. Although coordinates of the mouse are supplied, platform-
 * specific implementations are free to ignore these and query the mouse location themselves, due
 * to them possibly being incorrect under certain conditions, for example when using multiple
 * monitors that vary in scale and/or DPI.
 * \param x: The x-coordinate of the cursor.
 * \param y: The y-coordinate of the cursor.
 * \return The window under the cursor or nullptr in none.
 */
extern GHOST_WindowHandle GHOST_GetWindowUnderCursor(GHOST_SystemHandle systemhandle,
                                                     int32_t x,
                                                     int32_t y);

/***************************************************************************************
 * Event management functionality
 ***************************************************************************************/

/**
 * Retrieves events from the system and stores them in the queue.
 * \param systemhandle: The handle to the system.
 * \param waitForEvent: Boolean to indicate that #ProcessEvents should.
 * wait (block) until the next event before returning.
 * \return Indication of the presence of events.
 */
extern bool GHOST_ProcessEvents(GHOST_SystemHandle systemhandle, bool waitForEvent);

/**
 * Retrieves events from the queue and send them to the event consumers.
 * \param systemhandle: The handle to the system.
 */
extern void GHOST_DispatchEvents(GHOST_SystemHandle systemhandle);

/**
 * Adds the given event consumer to our list.
 * \param systemhandle: The handle to the system.
 * \param consumerhandle: The event consumer to add.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_AddEventConsumer(GHOST_SystemHandle systemhandle,
                                             GHOST_EventConsumerHandle consumerhandle);

/**
 * Remove the given event consumer to our list.
 * \param systemhandle: The handle to the system.
 * \param consumerhandle: The event consumer to remove.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_RemoveEventConsumer(GHOST_SystemHandle systemhandle,
                                                GHOST_EventConsumerHandle consumerhandle);

/***************************************************************************************
 * Progress bar functionality
 ***************************************************************************************/

/**
 * Sets the progress bar value displayed in the window/application icon
 * \param windowhandle: The handle to the window.
 * \param progress: The progress percentage (0.0 to 1.0).
 */
extern GHOST_TSuccess GHOST_SetProgressBar(GHOST_WindowHandle windowhandle, float progress);

/**
 * Hides the progress bar in the icon
 * \param windowhandle: The handle to the window.
 */
extern GHOST_TSuccess GHOST_EndProgressBar(GHOST_WindowHandle windowhandle);

/***************************************************************************************
 * Cursor management functionality
 ***************************************************************************************/

/**
 * Returns the current cursor shape.
 * \param windowhandle: The handle to the window.
 * \return The current cursor shape.
 */
extern GHOST_TStandardCursor GHOST_GetCursorShape(GHOST_WindowHandle windowhandle);

/**
 * Set the shape of the cursor. If the shape is not supported by the platform,
 * it will use the default cursor instead.
 * \param windowhandle: The handle to the window.
 * \param cursorshape: The new cursor shape type id.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCursorShape(GHOST_WindowHandle windowhandle,
                                           GHOST_TStandardCursor cursorshape);

/**
 * Test if the standard cursor shape is supported by current platform.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_HasCursorShape(GHOST_WindowHandle windowhandle,
                                           GHOST_TStandardCursor cursorshape);

/**
 * Set the shape of the cursor to a custom cursor of specified size. Two
 * formats are supported. XBitMap will always be a 1bpp 32x32 bitmap and mask.
 * If mask is nullptr the bitmap should be assumed to be 32-bit RGBA bitmap of
 * any size and dimension up to 128x128. RGBA data will only supplied if
 * WM_CAPABILITY_RGBA_CURSORS capability flag is set.
 * \param windowhandle: The handle to the window.
 * \param bitmap: The bitmap data for the cursor.
 * \param mask: The mask for 1bpp cursor, nullptr if RGBA cursor.
 * \param size: The width & height of the cursor.
 * \param hot_spot: The X,Y coordinates of the cursor hot-spot.
 * \param can_invert_color: Let the cursor colors be inverted to match platform convention.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCustomCursorShape(GHOST_WindowHandle windowhandle,
                                                 const uint8_t *bitmap,
                                                 const uint8_t *mask,
                                                 const int size[2],
                                                 const int hot_spot[2],
                                                 bool can_invert_color);
/**
 * Set a cursor "generator", allowing the GHOST back-end to dynamically
 * generate cursors at different sizes as needed, depending on the monitor DPI.
 *
 * \param cursor_generator: An object which generates cursors.
 * Ownership is transferred to GHOST which is responsible for calling it's free method.
 *
 * The capability flag: #GHOST_kCapabilityCursorGenerator should be checked,
 * otherwise this call is a no-op.
 */
extern GHOST_TSuccess GHOST_SetCustomCursorGenerator(GHOST_WindowHandle windowhandle,
                                                     GHOST_CursorGenerator *cursor_generator);

extern GHOST_TSuccess GHOST_GetCursorBitmap(GHOST_WindowHandle windowhandle,
                                            GHOST_CursorBitmapRef *bitmap);

/**
 * \return the size of the cursor in logical pixels (before Hi-DPI scaling is applied).
 */
extern uint32_t GHOST_GetCursorPreferredLogicalSize(const GHOST_SystemHandle systemhandle);

/**
 * Returns the visibility state of the cursor.
 * \param windowhandle: The handle to the window.
 * \return The visibility state of the cursor.
 */
extern bool GHOST_GetCursorVisibility(GHOST_WindowHandle windowhandle);

/**
 * Shows or hides the cursor.
 * \param windowhandle: The handle to the window.
 * \param visible: The new visibility state of the cursor.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCursorVisibility(GHOST_WindowHandle windowhandle, bool visible);

/**
 * Returns the current location of the cursor (location in client relative coordinates)
 * \param systemhandle: The handle to the system.
 * \param x: The x-coordinate of the cursor.
 * \param y: The y-coordinate of the cursor.
 * \return Indication of success.
 */
GHOST_TSuccess GHOST_GetCursorPosition(const GHOST_SystemHandle systemhandle,
                                       const GHOST_WindowHandle windowhandle,
                                       int32_t *x,
                                       int32_t *y);
/**
 * Updates the location of the cursor (location in client relative coordinates).
 * Not all operating systems allow the cursor to be moved (without the input device being moved).
 * \param systemhandle: The handle to the system.
 * \param x: The x-coordinate of the cursor.
 * \param y: The y-coordinate of the cursor.
 * \return Indication of success.
 */
GHOST_TSuccess GHOST_SetCursorPosition(GHOST_SystemHandle systemhandle,
                                       GHOST_WindowHandle windowhandle,
                                       int32_t x,
                                       int32_t y);

void GHOST_GetCursorGrabState(GHOST_WindowHandle windowhandle,
                              GHOST_TGrabCursorMode *r_mode,
                              GHOST_TAxisFlag *r_axis_flag,
                              int r_bounds[4],
                              bool *r_use_software_cursor);

/**
 * Grabs the cursor for a modal operation, to keep receiving
 * events when the mouse is outside the window. X11 only, others
 * do this automatically.
 * \param windowhandle: The handle to the window.
 * \param mode: The new grab state of the cursor.
 * \param bounds: The grab region (optional) - left,top,right,bottom.
 * \param mouse_ungrab_xy: XY for new mouse location (optional).
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetCursorGrab(GHOST_WindowHandle windowhandle,
                                          GHOST_TGrabCursorMode mode,
                                          GHOST_TAxisFlag wrap_axis,
                                          const int bounds[4],
                                          const int mouse_ungrab_xy[2]);

/***************************************************************************************
 * Access to mouse button and keyboard states.
 ***************************************************************************************/

/**
 * Returns the state of a modifier key (outside the message queue).
 * \param systemhandle: The handle to the system.
 * \param mask: The modifier key state to retrieve.
 * \param is_down: Pointer to return modifier state in.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_GetModifierKeyState(GHOST_SystemHandle systemhandle,
                                                GHOST_TModifierKey mask,
                                                bool *r_is_down);

/**
 * Returns the state of a mouse button (outside the message queue).
 * \param systemhandle: The handle to the system.
 * \param mask: The button state to retrieve.
 * \param is_down: Pointer to return button state in.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_GetButtonState(GHOST_SystemHandle systemhandle,
                                           GHOST_TButton mask,
                                           bool *r_is_down);

#ifdef WITH_INPUT_NDOF
/***************************************************************************************
 * Access to 3D mouse.
 ***************************************************************************************/

/**
 * Sets 3D mouse dead-zone.
 * \param deadzone: Dead-zone of the 3D mouse (both for rotation and pan) relative to full range.
 */
extern void GHOST_setNDOFDeadZone(float deadzone);
#endif

/***************************************************************************************
 * Drag & drop operations
 ***************************************************************************************/

/**
 * Tells if the ongoing drag & drop object can be accepted upon mouse drop.
 */
extern void GHOST_setAcceptDragOperation(GHOST_WindowHandle windowhandle, bool can_accept);

/**
 * Returns the event type.
 * \param eventhandle: The handle to the event.
 * \return The event type.
 */
extern GHOST_TEventType GHOST_GetEventType(GHOST_EventHandle eventhandle);

/**
 * Returns the time this event was generated.
 * \param eventhandle: The handle to the event.
 * \return The event generation time.
 */
extern uint64_t GHOST_GetEventTime(GHOST_EventHandle eventhandle);

/**
 * Returns the window this event was generated on,
 * or nullptr if it is a 'system' event.
 * \param eventhandle: The handle to the event.
 * \return The generating window.
 */
extern GHOST_WindowHandle GHOST_GetEventWindow(GHOST_EventHandle eventhandle);

/**
 * Returns the event data.
 * \param eventhandle: The handle to the event.
 * \return The event data.
 */
extern GHOST_TEventDataPtr GHOST_GetEventData(GHOST_EventHandle eventhandle);

/**
 * Returns the timer callback.
 * \param timertaskhandle: The handle to the timer-task.
 * \return The timer callback.
 */
extern GHOST_TimerProcPtr GHOST_GetTimerProc(GHOST_TimerTaskHandle timertaskhandle);

/**
 * Changes the timer callback.
 * \param timertaskhandle: The handle to the timer-task.
 * \param timer_proc: The timer callback.
 */
extern void GHOST_SetTimerProc(GHOST_TimerTaskHandle timertaskhandle,
                               GHOST_TimerProcPtr timer_proc);

/**
 * Returns the timer user data.
 * \param timertaskhandle: The handle to the timer-task.
 * \return The timer user data.
 */
extern GHOST_TUserDataPtr GHOST_GetTimerTaskUserData(GHOST_TimerTaskHandle timertaskhandle);

/**
 * Changes the time user data.
 * \param timertaskhandle: The handle to the timer-task.
 * \param user_data: The timer user data.
 */
extern void GHOST_SetTimerTaskUserData(GHOST_TimerTaskHandle timertaskhandle,
                                       GHOST_TUserDataPtr user_data);

/**
 * Returns indication as to whether the window is valid.
 * \param windowhandle: The handle to the window.
 * \return The validity of the window.
 */
extern bool GHOST_GetValid(GHOST_WindowHandle windowhandle);

/**
 * Returns the type of drawing context used in this window.
 * \param windowhandle: The handle to the window.
 * \return The current type of drawing context.
 */
extern GHOST_TDrawingContextType GHOST_GetDrawingContextType(GHOST_WindowHandle windowhandle);

/**
 * Tries to install a rendering context in this window.
 * \param windowhandle: The handle to the window.
 * \param type: The type of rendering context installed.
 * \return Indication as to whether installation has succeeded.
 */
extern GHOST_TSuccess GHOST_SetDrawingContextType(GHOST_WindowHandle windowhandle,
                                                  GHOST_TDrawingContextType type);

/**
 * Returns the drawing context used by this window.
 * \param windowhandle: The handle to the window.
 * \return The window drawing context.
 */
extern GHOST_ContextHandle GHOST_GetDrawingContext(GHOST_WindowHandle windowhandle);

/**
 * Sets the title displayed in the title bar.
 * \param windowhandle: The handle to the window.
 * \param title: The title to display in the title bar.
 */
extern void GHOST_SetTitle(GHOST_WindowHandle windowhandle, const char *title);

/**
 * Returns the title displayed in the title bar.
 * The title must be freed with free().
 *
 * \param windowhandle: The handle to the window.
 * \return The title, free with free().
 */
extern char *GHOST_GetTitle(GHOST_WindowHandle windowhandle);

/**
 * Sets the file name represented by this window.
 * \param filepath: The file directory.
 * \return Indication if the backend implements file associated with window.
 */
extern void GHOST_SetPath(GHOST_WindowHandle windowhandle, const char *filepath);

/**
 * Return the current window decoration style flags.
 */
extern GHOST_TWindowDecorationStyleFlags GHOST_GetWindowDecorationStyleFlags(
    GHOST_WindowHandle windowhandle);

/**
 * Set the window decoration style flags.
 * \param style_flags: Window decoration style flags.
 */
extern void GHOST_SetWindowDecorationStyleFlags(GHOST_WindowHandle windowhandle,
                                                GHOST_TWindowDecorationStyleFlags style_flags);

/**
 * Set the window decoration style settings.
 * \param decoration_settings: Window decoration style settings.
 */
extern void GHOST_SetWindowDecorationStyleSettings(
    GHOST_WindowHandle windowhandle, GHOST_WindowDecorationStyleSettings decoration_settings);

/**
 * Apply the window decoration style using the current flags and settings.
 */
extern GHOST_TSuccess GHOST_ApplyWindowDecorationStyle(GHOST_WindowHandle windowhandle);

/**
 * Returns the window rectangle dimensions.
 * These are screen coordinates.
 * \param windowhandle: The handle to the window.
 * \return A handle to the bounding rectangle of the window.
 */
extern GHOST_RectangleHandle GHOST_GetWindowBounds(GHOST_WindowHandle windowhandle);

/**
 * Returns the client rectangle dimensions.
 * The left and top members of the rectangle are always zero.
 * \param windowhandle: The handle to the window.
 * \return A handle to the bounding rectangle of the window.
 */
extern GHOST_RectangleHandle GHOST_GetClientBounds(GHOST_WindowHandle windowhandle);

/**
 * Disposes a rectangle object.
 * \param rectanglehandle: Handle to the rectangle.
 */
void GHOST_DisposeRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Resizes client rectangle width.
 * \param windowhandle: The handle to the window.
 * \param width: The new width of the client area of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetClientWidth(GHOST_WindowHandle windowhandle, uint32_t width);

/**
 * Resizes client rectangle height.
 * \param windowhandle: The handle to the window.
 * \param height: The new height of the client area of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetClientHeight(GHOST_WindowHandle windowhandle, uint32_t height);

/**
 * Resizes client rectangle.
 * \param windowhandle: The handle to the window.
 * \param width: The new width of the client area of the window.
 * \param height: The new height of the client area of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetClientSize(GHOST_WindowHandle windowhandle,
                                          uint32_t width,
                                          uint32_t height);

/**
 * Converts a point in screen coordinates to client rectangle coordinates
 * \param windowhandle: The handle to the window.
 * \param inX: The x-coordinate on the screen.
 * \param inY: The y-coordinate on the screen.
 * \param outX: The x-coordinate in the client rectangle.
 * \param outY: The y-coordinate in the client rectangle.
 */
extern void GHOST_ScreenToClient(
    GHOST_WindowHandle windowhandle, int32_t inX, int32_t inY, int32_t *outX, int32_t *outY);

/**
 * Converts a point in client rectangle coordinates to screen coordinates.
 * \param windowhandle: The handle to the window.
 * \param inX: The x-coordinate in the client rectangle.
 * \param inY: The y-coordinate in the client rectangle.
 * \param outX: The x-coordinate on the screen.
 * \param outY: The y-coordinate on the screen.
 */
extern void GHOST_ClientToScreen(
    GHOST_WindowHandle windowhandle, int32_t inX, int32_t inY, int32_t *outX, int32_t *outY);

/**
 * Returns the state of the window (normal, minimized, maximized).
 * \param windowhandle: The handle to the window.
 * \return The state of the window.
 */
extern GHOST_TWindowState GHOST_GetWindowState(GHOST_WindowHandle windowhandle);

/**
 * Sets the state of the window (normal, minimized, maximized).
 * \param windowhandle: The handle to the window.
 * \param state: The state of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetWindowState(GHOST_WindowHandle windowhandle,
                                           GHOST_TWindowState state);

/**
 * Sets the window "modified" status, indicating unsaved changes.
 * \param windowhandle: The handle to the window.
 * \param is_unsaved_changes: Unsaved changes or not.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetWindowModifiedState(GHOST_WindowHandle windowhandle,
                                                   bool is_unsaved_changes);

/**
 * Sets the order of the window (bottom, top).
 * \param windowhandle: The handle to the window.
 * \param order: The order of the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_SetWindowOrder(GHOST_WindowHandle windowhandle,
                                           GHOST_TWindowOrder order);

/**
 * Acquire a swap chain buffer.
 * \param windowhandle: The handle to the window.
 * \return A success indicator.
 */
extern GHOST_TSuccess GHOST_SwapWindowBufferAcquire(GHOST_WindowHandle windowhandle);

/**
 * Swaps front and back buffers of a window.
 * \param windowhandle: The handle to the window.
 * \return A success indicator.
 */
extern GHOST_TSuccess GHOST_SwapWindowBufferRelease(GHOST_WindowHandle windowhandle);

/**
 * Sets the swap interval for #swapBuffers.
 * \param interval: The swap interval to use.
 * \return A boolean success indicator.
 */
extern GHOST_TSuccess GHOST_SetSwapInterval(GHOST_WindowHandle windowhandle, int interval);

/**
 * Gets the current swap interval for #swapBuffers.
 * \param windowhandle: The handle to the window
 * \param r_interval: pointer to location to return swap interval
 * (left untouched if there is an error)
 * \return A boolean success indicator of if swap interval was successfully read.
 */
extern GHOST_TSuccess GHOST_GetSwapInterval(GHOST_WindowHandle windowhandle, int *r_interval);

/**
 * Activates the drawing context of this window.
 * \param windowhandle: The handle to the window.
 * \return A success indicator.
 */
extern GHOST_TSuccess GHOST_ActivateWindowDrawingContext(GHOST_WindowHandle windowhandle);

/**
 * Invalidates the contents of this window.
 * \param windowhandle: The handle to the window.
 * \return Indication of success.
 */
extern GHOST_TSuccess GHOST_InvalidateWindow(GHOST_WindowHandle windowhandle);

/**
 * Activates the drawing context of this context.
 * \param contexthandle: The handle to the context.
 * \return A success indicator.
 */
extern GHOST_TSuccess GHOST_ActivateGPUContext(GHOST_ContextHandle contexthandle);

/**
 * Release the drawing context bound to this thread.
 * \param contexthandle: The handle to the context.
 * \return A success indicator.
 */
extern GHOST_TSuccess GHOST_ReleaseGPUContext(GHOST_ContextHandle contexthandle);

/**
 * Return the thread's currently active drawing context.
 */
extern GHOST_ContextHandle GHOST_GetActiveGPUContext();

/**
 * Get the GPU frame-buffer handle that serves as a default frame-buffer.
 */
extern unsigned int GHOST_GetContextDefaultGPUFramebuffer(GHOST_ContextHandle contexthandle);

/**
 * Get the GPU frame-buffer handle that serves as a default frame-buffer.
 */
extern unsigned int GHOST_GetDefaultGPUFramebuffer(GHOST_WindowHandle windowhandle);

/**
 * Use multi-touch gestures if supported.
 * \param systemhandle: The handle to the system.
 * \param use: Enable or disable.
 */
extern void GHOST_SetMultitouchGestures(GHOST_SystemHandle systemhandle, const bool use);

/**
 * Set which tablet API to use. Only affects Windows, other platforms have a single API.
 * \param systemhandle: The handle to the system.
 * \param api: Enum indicating which API to use.
 */
extern void GHOST_SetTabletAPI(GHOST_SystemHandle systemhandle, GHOST_TTabletAPI api);

/**
 * Get the color of the pixel at the current mouse cursor location
 * \param r_color: returned sRGB float colors
 * \return Success value (true == successful and supported by platform)
 */
extern GHOST_TSuccess GHOST_GetPixelAtCursor(float r_color[3]);

/**
 * Access to rectangle width.
 * \param rectanglehandle: The handle to the rectangle.
 * \return width of the rectangle
 */
extern int32_t GHOST_GetWidthRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Access to rectangle height.
 * \param rectanglehandle: The handle to the rectangle.
 * \return height of the rectangle
 */
extern int32_t GHOST_GetHeightRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Gets all members of the rectangle.
 * \param rectanglehandle: The handle to the rectangle.
 * \param l: Pointer to return left coordinate in.
 * \param t: Pointer to return top coordinate in.
 * \param r: Pointer to return right coordinate in.
 * \param b: Pointer to return bottom coordinate in.
 */
extern void GHOST_GetRectangle(
    GHOST_RectangleHandle rectanglehandle, int32_t *l, int32_t *t, int32_t *r, int32_t *b);

/**
 * Sets all members of the rectangle.
 * \param rectanglehandle: The handle to the rectangle.
 * \param l: requested left coordinate of the rectangle.
 * \param t: requested top coordinate of the rectangle.
 * \param r: requested right coordinate of the rectangle.
 * \param b: requested bottom coordinate of the rectangle.
 */
extern void GHOST_SetRectangle(
    GHOST_RectangleHandle rectanglehandle, int32_t l, int32_t t, int32_t r, int32_t b);

/**
 * Returns whether this rectangle is empty.
 * Empty rectangles are rectangles that have width==0 and/or height==0.
 * \param rectanglehandle: The handle to the rectangle.
 * \return Success value (true == empty rectangle)
 */
extern GHOST_TSuccess GHOST_IsEmptyRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Returns whether this rectangle is valid.
 * Valid rectangles are rectangles that have l_ <= r_ and t_ <= b_.
 * Thus, empty rectangles are valid.
 * \param rectanglehandle: The handle to the rectangle.
 * \return Success value (true == valid rectangle)
 */
extern GHOST_TSuccess GHOST_IsValidRectangle(GHOST_RectangleHandle rectanglehandle);

/**
 * Grows (or shrinks the rectangle).
 * The method avoids negative insets making the rectangle invalid
 * \param rectanglehandle: The handle to the rectangle.
 * \param i: The amount of offset given to each extreme (negative values shrink the rectangle).
 */
extern void GHOST_InsetRectangle(GHOST_RectangleHandle rectanglehandle, int32_t i);

/**
 * Does a union of the rectangle given and this rectangle.
 * The result is stored in this rectangle.
 * \param rectanglehandle: The handle to the rectangle.
 * \param anotherrectanglehandle: The rectangle that is input for the union operation.
 */
extern void GHOST_UnionRectangle(GHOST_RectangleHandle rectanglehandle,
                                 GHOST_RectangleHandle anotherrectanglehandle);

/**
 * Grows the rectangle to included a point.
 * \param rectanglehandle: The handle to the rectangle.
 * \param x: The x-coordinate of the point.
 * \param y: The y-coordinate of the point.
 */
extern void GHOST_UnionPointRectangle(GHOST_RectangleHandle rectanglehandle, int32_t x, int32_t y);

/**
 * Returns whether the point is inside this rectangle.
 * Point on the boundary is considered inside.
 * \param rectanglehandle: The handle to the rectangle.
 * \param x: x-coordinate of point to test.
 * \param y: y-coordinate of point to test.
 * \return Success value (true if point is inside).
 */
extern GHOST_TSuccess GHOST_IsInsideRectangle(GHOST_RectangleHandle rectanglehandle,
                                              int32_t x,
                                              int32_t y);

/**
 * Returns whether the rectangle is inside this rectangle.
 * \param rectanglehandle: The handle to the rectangle.
 * \param anotherrectanglehandle: The rectangle to test.
 * \return visibility (not, partially or fully visible).
 */
extern GHOST_TVisibility GHOST_GetRectangleVisibility(
    GHOST_RectangleHandle rectanglehandle, GHOST_RectangleHandle anotherrectanglehandle);

/**
 * Sets rectangle members.
 * Sets rectangle members such that it is centered at the given location.
 * \param rectanglehandle: The handle to the rectangle.
 * \param cx: Requested center x-coordinate of the rectangle.
 * \param cy: Requested center y-coordinate of the rectangle.
 */
extern void GHOST_SetCenterRectangle(GHOST_RectangleHandle rectanglehandle,
                                     int32_t cx,
                                     int32_t cy);

/**
 * Sets rectangle members.
 * Sets rectangle members such that it is centered at the given location,
 * with the width requested.
 * \param rectanglehandle: The handle to the rectangle.
 * \param cx: requested center x-coordinate of the rectangle.
 * \param cy: requested center y-coordinate of the rectangle.
 * \param w: requested width of the rectangle.
 * \param h: requested height of the rectangle.
 */
extern void GHOST_SetRectangleCenter(
    GHOST_RectangleHandle rectanglehandle, int32_t cx, int32_t cy, int32_t w, int32_t h);

/**
 * Clips a rectangle.
 * Updates the rectangle given such that it will fit within this one.
 * This can result in an empty rectangle.
 * \param rectanglehandle: The handle to the rectangle.
 * \param anotherrectanglehandle: The rectangle to clip.
 * \return Whether clipping has occurred
 */
extern GHOST_TSuccess GHOST_ClipRectangle(GHOST_RectangleHandle rectanglehandle,
                                          GHOST_RectangleHandle anotherrectanglehandle);

/**
 * Return the data from the clipboard
 * \param selection: Boolean to return the selection instead.
 * The capability flag: #GHOST_kCapabilityClipboardPrimary can be used to check for supported.
 * \return clipboard data
 */
extern char *GHOST_getClipboard(bool selection);

/**
 * Put data to the Clipboard
 * \param buffer: the string buffer to set.
 * \param selection: Set the selection instead, X11 only feature.
 */
extern void GHOST_putClipboard(const char *buffer, bool selection);

/**
 * Returns GHOST_kSuccess if the clipboard contains an image.
 */
extern GHOST_TSuccess GHOST_hasClipboardImage(void);

/**
 * Get image data from the Clipboard
 * \param r_width: the returned image width in pixels.
 * \param r_height: the returned image height in pixels.
 * \return pointer uint array in RGBA byte order. Caller must free.
 */
extern uint *GHOST_getClipboardImage(int *r_width, int *r_height);

/**
 * Put image data to the Clipboard
 * \param rgba: uint array in RGBA byte order.
 * \param width: the image width in pixels.
 * \param height: the image height in pixels.
 */
extern GHOST_TSuccess GHOST_putClipboardImage(uint *rgba, int width, int height);

/**
 * Set the Console State
 * \param action: console state
 * \return current status (1 -visible, 0 - hidden)
 */
extern bool GHOST_setConsoleWindowState(GHOST_TConsoleWindowState action);

/**
 * Use native pixel size (MacBook pro 'retina'), if supported.
 */
extern bool GHOST_UseNativePixels(void);

/**
 * Return features which are supported by the GHOST back-end.
 */
extern GHOST_TCapabilityFlag GHOST_GetCapabilities(void);

/**
 * Assign the callback which generates a back-trace (may be nullptr).
 */
extern void GHOST_SetBacktraceHandler(GHOST_TBacktraceFn backtrace_fn);

/**
 * When `use_window_frame` is false, don't show window frames.
 *
 * \note This must run before the system is created.
 */
extern void GHOST_UseWindowFrame(bool use_window_frame);

/**
 * Focus window after opening, or put them in the background.
 */
extern void GHOST_UseWindowFocus(bool use_focus);

/**
 * Focus and raise windows on mouse hover.
 */
extern void GHOST_SetAutoFocus(bool auto_focus);

/**
 * If window was opened using native pixel size, it returns scaling factor.
 */
extern float GHOST_GetNativePixelSize(GHOST_WindowHandle windowhandle);

/**
 * Returns the suggested DPI for this window.
 */
extern uint16_t GHOST_GetDPIHint(GHOST_WindowHandle windowhandle);

/**
 * Enable IME attached to the given window, i.e. allows user-input
 * events to be dispatched to the IME.
 * \param windowhandle: Window handle of the caller.
 * \param x: Requested x-coordinate of the rectangle.
 * \param y: Requested y-coordinate of the rectangle.
 * \param w: Requested width of the rectangle.
 * \param h: Requested height of the rectangle.
 * \param complete: Whether or not to complete the ongoing composition.
 * - true:  Start a new composition.
 * - false: Move the IME windows to the given position without finishing it.
 */
extern void GHOST_BeginIME(
    GHOST_WindowHandle windowhandle, int32_t x, int32_t y, int32_t w, int32_t h, bool complete);
/**
 * Disable the IME attached to the given window, i.e. prohibits any user-input
 * events from being dispatched to the IME.
 * \param windowhandle: The window handle of the caller.
 */
extern void GHOST_EndIME(GHOST_WindowHandle windowhandle);

#ifdef WITH_XR_OPENXR

/* XR-context */

/**
 * Set a custom callback to be executed whenever an error occurs. Should be set before calling
 * #GHOST_XrContextCreate() to get error handling during context creation too.
 *
 * \param customdata: Handle to some data that will get passed to \a handler_fn should an error be
 *                    thrown.
 */
void GHOST_XrErrorHandler(GHOST_XrErrorHandlerFn handler_fn, void *customdata);

/**
 * \brief Initialize the Ghost XR-context.
 *
 * Includes setting up the OpenXR runtime link, querying available extensions and API layers,
 * enabling extensions and API layers.
 *
 * \param create_info: Options for creating the XR-context, e.g. debug-flags and ordered array of
 *                     graphics bindings to try enabling.
 */
GHOST_XrContextHandle GHOST_XrContextCreate(const GHOST_XrContextCreateInfo *create_info);
/**
 * Free a XR-context involving OpenXR runtime link destruction and freeing of all internal data.
 */
void GHOST_XrContextDestroy(GHOST_XrContextHandle xr_context);

/**
 * Set callbacks for binding and unbinding a graphics context for a session. The binding callback
 * may create a new graphics context thereby. In fact that's the sole reason for this callback
 * approach to binding. Just make sure to have an unbind function set that properly destructs.
 *
 * \param bind_fn: Function to retrieve (possibly create) a graphics context.
 * \param unbind_fn: Function to release (possibly free) a graphics context.
 */
void GHOST_XrGraphicsContextBindFuncs(GHOST_XrContextHandle xr_context,
                                      GHOST_XrGraphicsContextBindFn bind_fn,
                                      GHOST_XrGraphicsContextUnbindFn unbind_fn);

/**
 * Set the drawing callback for views. A view would typically be either the left or the right eye,
 * although other configurations are possible. When #GHOST_XrSessionDrawViews() is called to draw
 * an XR frame, \a draw_view_fn is executed for each view.
 *
 * \param draw_view_fn: The callback to draw a single view for an XR frame.
 */
void GHOST_XrDrawViewFunc(GHOST_XrContextHandle xr_context, GHOST_XrDrawViewFn draw_view_fn);

/**
 * Set the callback to check if passthrough is enabled.
 * If enabled, the passthrough composition layer is added in GHOST_XrSession::draw().
 *
 * \param passthrough_enabled_fn: The callback to check if passthrough is enabled.
 */
void GHOST_XrPassthroughEnabledFunc(GHOST_XrContextHandle xr_context,
                                    GHOST_XrPassthroughEnabledFn passthrough_enabled_fn);

/**
 * Set the callback to force disable passthrough in case is not supported.
 * Called in GHOST_XrSession::draw().
 *
 * \param disable_passthrough_fn: The callback to disable passthrough.
 */
void GHOST_XrDisablePassthroughFunc(GHOST_XrContextHandle xr_context,
                                    GHOST_XrDisablePassthroughFn disable_passthrough_fn);

/* sessions */
/**
 * Create internal session data for \a xr_context and ask the OpenXR runtime to invoke a session.
 *
 * \param begin_info: Options for the session creation.
 */
void GHOST_XrSessionStart(GHOST_XrContextHandle xr_context,
                          const GHOST_XrSessionBeginInfo *begin_info);
/**
 * Destruct internal session data for \a xr_context and ask the OpenXR runtime to stop a session.
 */
void GHOST_XrSessionEnd(GHOST_XrContextHandle xr_context);
/**
 * Draw a single frame by calling the view drawing callback defined by #GHOST_XrDrawViewFunc() for
 * each view and submit it to the OpenXR runtime.
 *
 * \param customdata: Handle to some data that will get passed to the view drawing callback.
 */
void GHOST_XrSessionDrawViews(GHOST_XrContextHandle xr_context, void *customdata);
/**
 * Check if a \a xr_context has a session that, according to the OpenXR definition would be
 * considered to be 'running'
 * (https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#session_running).
 */
int GHOST_XrSessionIsRunning(const GHOST_XrContextHandle xr_context);

/**
 * Check if \a xr_context has a session that requires an upside-down frame-buffer (compared to
 * GPU). If true, the render result should be flipped vertically for correct output.
 * \note Only to be called after session start, may otherwise result in a false negative.
 */
int GHOST_XrSessionNeedsUpsideDownDrawing(const GHOST_XrContextHandle xr_context);

/* events */
/**
 * Invoke handling of all OpenXR events for \a xr_context. Should be called on every main-loop
 * iteration and will early-exit if \a xr_context is nullptr (so caller doesn't have to check).
 *
 * \returns GHOST_kSuccess if any event was handled, otherwise GHOST_kFailure.
 */
GHOST_TSuccess GHOST_XrEventsHandle(GHOST_XrContextHandle xr_context);

/* actions */
/**
 * Create an OpenXR action set for input/output.
 */
int GHOST_XrCreateActionSet(GHOST_XrContextHandle xr_context, const GHOST_XrActionSetInfo *info);

/**
 * Destroy a previously created OpenXR action set.
 */
void GHOST_XrDestroyActionSet(GHOST_XrContextHandle xr_context, const char *action_set_name);

/**
 * Create OpenXR input/output actions.
 */
int GHOST_XrCreateActions(GHOST_XrContextHandle xr_context,
                          const char *action_set_name,
                          uint32_t count,
                          const GHOST_XrActionInfo *infos);

/**
 * Destroy previously created OpenXR actions.
 */
void GHOST_XrDestroyActions(GHOST_XrContextHandle xr_context,
                            const char *action_set_name,
                            uint32_t count,
                            const char *const *action_names);

/**
 * Create input/output path bindings for OpenXR actions.
 */
int GHOST_XrCreateActionBindings(GHOST_XrContextHandle xr_context,
                                 const char *action_set_name,
                                 uint32_t count,
                                 const GHOST_XrActionProfileInfo *infos);

/**
 * Destroy previously created bindings for OpenXR actions.
 */
void GHOST_XrDestroyActionBindings(GHOST_XrContextHandle xr_context,
                                   const char *action_set_name,
                                   uint32_t count,
                                   const char *const *action_names,
                                   const char *const *profile_paths);

/**
 * Attach all created action sets to the current OpenXR session.
 */
int GHOST_XrAttachActionSets(GHOST_XrContextHandle xr_context);

/**
 * Update button/tracking states for OpenXR actions.
 *
 * \param action_set_name: The name of the action set to sync. If nullptr, all action sets
 * attached to the session will be synced.
 */
int GHOST_XrSyncActions(GHOST_XrContextHandle xr_context, const char *action_set_name);

/**
 * Apply an OpenXR haptic output action.
 */
int GHOST_XrApplyHapticAction(GHOST_XrContextHandle xr_context_handle,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path,
                              const int64_t *duration,
                              const float *frequency,
                              const float *amplitude);

/**
 * Stop a previously applied OpenXR haptic output action.
 */
void GHOST_XrStopHapticAction(GHOST_XrContextHandle xr_context_handle,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path);

/**
 * Get action set custom data (owned by Blender, not GHOST).
 */
void *GHOST_XrGetActionSetCustomdata(GHOST_XrContextHandle xr_context,
                                     const char *action_set_name);

/**
 * Get action custom data (owned by Blender, not GHOST).
 */
void *GHOST_XrGetActionCustomdata(GHOST_XrContextHandle xr_context,
                                  const char *action_set_name,
                                  const char *action_name);

/**
 * Get the number of actions in an action set.
 */
unsigned int GHOST_XrGetActionCount(GHOST_XrContextHandle xr_context, const char *action_set_name);

/**
 * Get custom data for all actions in an action set.
 */
void GHOST_XrGetActionCustomdataArray(GHOST_XrContextHandle xr_context,
                                      const char *action_set_name,
                                      void **r_customdata_array);

/* controller model */
/**
 * Load the OpenXR controller model.
 */
int GHOST_XrLoadControllerModel(GHOST_XrContextHandle xr_context, const char *subaction_path);

/**
 * Unload the OpenXR controller model.
 */
void GHOST_XrUnloadControllerModel(GHOST_XrContextHandle xr_context, const char *subaction_path);

/**
 * Update component transforms for the OpenXR controller model.
 */
int GHOST_XrUpdateControllerModelComponents(GHOST_XrContextHandle xr_context,
                                            const char *subaction_path);

/**
 * Get vertex data for the OpenXR controller model.
 */
int GHOST_XrGetControllerModelData(GHOST_XrContextHandle xr_context,
                                   const char *subaction_path,
                                   GHOST_XrControllerModelData *r_data);

#endif /* WITH_XR_OPENXR */

#ifdef WITH_VULKAN_BACKEND

/**
 * Get Vulkan handles for the given context.
 *
 * These handles are the same for a given context.
 * Should only be called when using a Vulkan context.
 * Other contexts will not return any handles and leave the
 * handles where the parameters are referring to unmodified.
 *
 * \param context: GHOST context handle of a vulkan context to
 *     get the Vulkan handles from.
 * \param r_handles: After calling this structure is filled with
 *     the vulkan handles of the context.
 */
void GHOST_GetVulkanHandles(GHOST_ContextHandle context, GHOST_VulkanHandles *r_handles);

/**
 * Set the pre and post callbacks for vulkan swap-chain in the given context.
 *
 * \param context: GHOST context handle of a vulkan context to
 *     get the Vulkan handles from.
 * \param swap_buffer_draw_callback: Function pointer to be called when acquired swap buffer is
 *     released, allowing Vulkan backend to update the swap chain.
 * \param swap_buffer_acquired_callback: Function to be called at when swap buffer is acquired.
 *     Allowing Vulkan backend to update the framebuffer. It is also called when no swap chain
 *     exists indicating that the window was minimuzed.
 * \param openxr_acquire_image_callback: Function to be called when an image needs to be acquired
 *     to be drawn to an OpenXR swap-chain.
 * \param openxr_release_image_callback: Function to be called after an image has been drawn to
 *     the OpenXR swap-chain.
 */
void GHOST_SetVulkanSwapBuffersCallbacks(
    GHOST_ContextHandle context,
    void (*swap_buffer_draw_callback)(const GHOST_VulkanSwapChainData *),
    void (*swap_buffer_acquired_callback)(void),
    void (*openxr_acquire_image_callback)(GHOST_VulkanOpenXRData *),
    void (*openxr_release_image_callback)(GHOST_VulkanOpenXRData *));

/**
 * Acquire the current swap-chain format.
 *
 * \param windowhandle:  GHOST window handle to a window to get the resource from.
 * \param r_surface_format: After calling this function the VkSurfaceFormatKHR
 *     referenced by this parameter will contain the surface format of the
 *     surface. The format is the same as the image returned in the r_image
 *     parameter.
 * \param r_extent: After calling this function the VkExtent2D
 *     referenced by this parameter will contain the size of the
 *     frame buffer and image in pixels.
 */
void GHOST_GetVulkanSwapChainFormat(GHOST_WindowHandle windowhandle,
                                    GHOST_VulkanSwapChainData *r_swap_chain_data);

#endif

#ifdef __cplusplus
}

#endif
