/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * \brief Abstraction for XR (VR, AR, MR, ..) access via OpenXR.
 */

#pragma once

#include "GHOST_IXrContext.hh"
#include "GHOST_Types.hh"

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
GHOST_IXrContext *GHOST_XrContextCreate(const GHOST_XrContextCreateInfo *create_info);
/**
 * Free a XR-context involving OpenXR runtime link destruction and freeing of all internal data.
 */
void GHOST_XrContextDestroy(GHOST_IXrContext *xr_context);

/**
 * Set callbacks for binding and unbinding a graphics context for a session. The binding callback
 * may create a new graphics context thereby. In fact that's the sole reason for this callback
 * approach to binding. Just make sure to have an unbind function set that properly destructs.
 *
 * \param bind_fn: Function to retrieve (possibly create) a graphics context.
 * \param unbind_fn: Function to release (possibly free) a graphics context.
 */
void GHOST_XrGraphicsContextBindFuncs(GHOST_IXrContext *xr_context,
                                      GHOST_XrGraphicsContextBindFn bind_fn,
                                      GHOST_XrGraphicsContextUnbindFn unbind_fn);

/**
 * Set the drawing callback for views. A view would typically be either the left or the right eye,
 * although other configurations are possible. When #GHOST_XrSessionDrawViews() is called to draw
 * an XR frame, \a draw_view_fn is executed for each view.
 *
 * \param draw_view_fn: The callback to draw a single view for an XR frame.
 */
void GHOST_XrDrawViewFunc(GHOST_IXrContext *xr_context, GHOST_XrDrawViewFn draw_view_fn);

/**
 * Set the callback to check if passthrough is enabled.
 * If enabled, the passthrough composition layer is added in GHOST_XrSession::draw().
 *
 * \param passthrough_enabled_fn: The callback to check if passthrough is enabled.
 */
void GHOST_XrPassthroughEnabledFunc(GHOST_IXrContext *xr_context,
                                    GHOST_XrPassthroughEnabledFn passthrough_enabled_fn);

/**
 * Set the callback to force disable passthrough in case it is not supported.
 * Called in GHOST_XrSession::draw().
 *
 * \param disable_passthrough_fn: The callback to disable passthrough.
 */
void GHOST_XrDisablePassthroughFunc(GHOST_IXrContext *xr_context,
                                    GHOST_XrDisablePassthroughFn disable_passthrough_fn);

/* sessions */
/**
 * Create internal session data for \a xr_context and ask the OpenXR runtime to invoke a session.
 *
 * \param begin_info: Options for the session creation.
 */
void GHOST_XrSessionStart(GHOST_IXrContext *xr_context,
                          const GHOST_XrSessionBeginInfo *begin_info);
/**
 * Destruct internal session data for \a xr_context and ask the OpenXR runtime to stop a session.
 */
void GHOST_XrSessionEnd(GHOST_IXrContext *xr_context);
/**
 * Draw a single frame by calling the view drawing callback defined by #GHOST_XrDrawViewFunc() for
 * each view and submit it to the OpenXR runtime.
 *
 * \param customdata: Handle to some data that will get passed to the view drawing callback.
 */
void GHOST_XrSessionDrawViews(GHOST_IXrContext *xr_context, void *customdata);
/**
 * Check if a \a xr_context has a session that, according to the OpenXR definition would be
 * considered to be 'running'
 * (https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#session_running).
 */
int GHOST_XrSessionIsRunning(const GHOST_IXrContext *xr_context);

/**
 * Check if \a xr_context has a session that requires an upside-down frame-buffer (compared to
 * GPU). If true, the render result should be flipped vertically for correct output.
 * \note Only to be called after session start, may otherwise result in a false negative.
 */
int GHOST_XrSessionNeedsUpsideDownDrawing(const GHOST_IXrContext *xr_context);

/* events */
/**
 * Invoke handling of all OpenXR events for \a xr_context. Should be called on every main-loop
 * iteration and will early-exit if \a xr_context is nullptr (so caller doesn't have to check).
 *
 * \returns GHOST_kSuccess if any event was handled, otherwise GHOST_kFailure.
 */
GHOST_TSuccess GHOST_XrEventsHandle(GHOST_IXrContext *xr_context);

/* actions */
/**
 * Create an OpenXR action set for input/output.
 */
int GHOST_XrCreateActionSet(GHOST_IXrContext *xr_context, const GHOST_XrActionSetInfo *info);

/**
 * Destroy a previously created OpenXR action set.
 */
void GHOST_XrDestroyActionSet(GHOST_IXrContext *xr_context, const char *action_set_name);

/**
 * Create OpenXR input/output actions.
 */
int GHOST_XrCreateActions(GHOST_IXrContext *xr_context,
                          const char *action_set_name,
                          uint32_t count,
                          const GHOST_XrActionInfo *infos);

/**
 * Destroy previously created OpenXR actions.
 */
void GHOST_XrDestroyActions(GHOST_IXrContext *xr_context,
                            const char *action_set_name,
                            uint32_t count,
                            const char *const *action_names);

/**
 * Create input/output path bindings for OpenXR actions.
 */
int GHOST_XrCreateActionBindings(GHOST_IXrContext *xr_context,
                                 const char *action_set_name,
                                 uint32_t count,
                                 const GHOST_XrActionProfileInfo *infos);

/**
 * Destroy previously created bindings for OpenXR actions.
 */
void GHOST_XrDestroyActionBindings(GHOST_IXrContext *xr_context,
                                   const char *action_set_name,
                                   uint32_t count,
                                   const char *const *action_names,
                                   const char *const *profile_paths);

/**
 * Attach all created action sets to the current OpenXR session.
 */
int GHOST_XrAttachActionSets(GHOST_IXrContext *xr_context);

/**
 * Update button/tracking states for OpenXR actions.
 *
 * \param action_set_name: The name of the action set to sync. If nullptr, all action sets
 * attached to the session will be synced.
 */
int GHOST_XrSyncActions(GHOST_IXrContext *xr_context, const char *action_set_name);

/**
 * Apply an OpenXR haptic output action.
 */
int GHOST_XrApplyHapticAction(GHOST_IXrContext *xr_context_handle,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path,
                              const int64_t *duration,
                              const float *frequency,
                              const float *amplitude);

/**
 * Stop a previously applied OpenXR haptic output action.
 */
void GHOST_XrStopHapticAction(GHOST_IXrContext *xr_context_handle,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path);

/**
 * Get action set custom data (owned by Blender, not GHOST).
 */
void *GHOST_XrGetActionSetCustomdata(GHOST_IXrContext *xr_context, const char *action_set_name);

/**
 * Get action custom data (owned by Blender, not GHOST).
 */
void *GHOST_XrGetActionCustomdata(GHOST_IXrContext *xr_context,
                                  const char *action_set_name,
                                  const char *action_name);

/**
 * Get the number of actions in an action set.
 */
unsigned int GHOST_XrGetActionCount(GHOST_IXrContext *xr_context, const char *action_set_name);

/**
 * Get custom data for all actions in an action set.
 */
void GHOST_XrGetActionCustomdataArray(GHOST_IXrContext *xr_context,
                                      const char *action_set_name,
                                      void **r_customdata_array);

/* controller model */
/**
 * Load the OpenXR controller model.
 */
int GHOST_XrLoadControllerModel(GHOST_IXrContext *xr_context, const char *subaction_path);

/**
 * Unload the OpenXR controller model.
 */
void GHOST_XrUnloadControllerModel(GHOST_IXrContext *xr_context, const char *subaction_path);

/**
 * Update component transforms for the OpenXR controller model.
 */
int GHOST_XrUpdateControllerModelComponents(GHOST_IXrContext *xr_context,
                                            const char *subaction_path);

/**
 * Get vertex data for the OpenXR controller model.
 */
int GHOST_XrGetControllerModelData(GHOST_IXrContext *xr_context,
                                   const char *subaction_path,
                                   GHOST_XrControllerModelData *r_data);
