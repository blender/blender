/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Abstraction for XR (VR, AR, MR, ..) access via OpenXR.
 */

#include "GHOST_XrContext.hh"
#include "GHOST_XrException.hh"
#include "GHOST_XrSession.hh"

#include "GHOST_Xr-api.hh"

#define GHOST_XR_CAPI_CALL(call, ctx) \
  try { \
    call; \
  } \
  catch (GHOST_XrException & e) { \
    (ctx)->dispatchErrorMessage(&e); \
  }

#define GHOST_XR_CAPI_CALL_RET(call, ctx) \
  try { \
    return call; \
  } \
  catch (GHOST_XrException & e) { \
    (ctx)->dispatchErrorMessage(&e); \
  }

void GHOST_XrErrorHandler(GHOST_XrErrorHandlerFn handler_fn, void *customdata)
{
  GHOST_XrContext::setErrorHandler(handler_fn, customdata);
}

GHOST_IXrContext *GHOST_XrContextCreate(const GHOST_XrContextCreateInfo *create_info)
{
  auto xr_context = std::make_unique<GHOST_XrContext>(create_info);

  /* TODO GHOST_XrContext's should probably be owned by the GHOST_System, which will handle context
   * creation and destruction. Try-catch logic can be moved to C-API then. */
  try {
    xr_context->initialize(create_info);
  }
  catch (GHOST_XrException &e) {
    xr_context->dispatchErrorMessage(&e);
    return nullptr;
  }

  /* Give ownership to the caller. */
  return xr_context.release();
}

void GHOST_XrContextDestroy(GHOST_IXrContext *xr_context)
{
  delete xr_context;
}

void GHOST_XrSessionStart(GHOST_IXrContext *xr_context, const GHOST_XrSessionBeginInfo *begin_info)
{
  GHOST_XR_CAPI_CALL(xr_context->startSession(begin_info), xr_context);
}

void GHOST_XrSessionEnd(GHOST_IXrContext *xr_context)
{
  GHOST_XR_CAPI_CALL(xr_context->endSession(), xr_context);
}

void GHOST_XrSessionDrawViews(GHOST_IXrContext *xr_context, void *draw_customdata)
{
  GHOST_XR_CAPI_CALL(xr_context->drawSessionViews(draw_customdata), xr_context);
}

int GHOST_XrSessionIsRunning(const GHOST_IXrContext *xr_context)
{
  GHOST_XR_CAPI_CALL_RET(xr_context->isSessionRunning(), xr_context);
  return 0; /* Only reached if exception is thrown. */
}

void GHOST_XrGraphicsContextBindFuncs(GHOST_IXrContext *xr_context,
                                      GHOST_XrGraphicsContextBindFn bind_fn,
                                      GHOST_XrGraphicsContextUnbindFn unbind_fn)
{
  GHOST_XR_CAPI_CALL(xr_context->setGraphicsContextBindFuncs(bind_fn, unbind_fn), xr_context);
}

void GHOST_XrDrawViewFunc(GHOST_IXrContext *xr_context, GHOST_XrDrawViewFn draw_view_fn)
{
  GHOST_XR_CAPI_CALL(xr_context->setDrawViewFunc(draw_view_fn), xr_context);
}

void GHOST_XrPassthroughEnabledFunc(GHOST_IXrContext *xr_context,
                                    GHOST_XrPassthroughEnabledFn passthrough_enabled_fn)
{
  GHOST_XR_CAPI_CALL(xr_context->setPassthroughEnabledFunc(passthrough_enabled_fn), xr_context);
}

void GHOST_XrDisablePassthroughFunc(GHOST_IXrContext *xr_context,
                                    GHOST_XrDisablePassthroughFn disable_passthrough_fn)
{
  GHOST_XR_CAPI_CALL(xr_context->setDisablePassthroughFunc(disable_passthrough_fn), xr_context);
}

int GHOST_XrSessionNeedsUpsideDownDrawing(const GHOST_IXrContext *xr_context)
{
  GHOST_XR_CAPI_CALL_RET(xr_context->needsUpsideDownDrawing(), xr_context);
  return 0; /* Only reached if exception is thrown. */
}

int GHOST_XrCreateActionSet(GHOST_IXrContext *xr_context, const GHOST_XrActionSetInfo *info)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->createActionSet(*info), xr_context);
  return 0;
}

void GHOST_XrDestroyActionSet(GHOST_IXrContext *xr_context, const char *action_set_name)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->destroyActionSet(action_set_name), xr_context);
}

int GHOST_XrCreateActions(GHOST_IXrContext *xr_context,
                          const char *action_set_name,
                          uint32_t count,
                          const GHOST_XrActionInfo *infos)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->createActions(action_set_name, count, infos), xr_context);
  return 0;
}

void GHOST_XrDestroyActions(GHOST_IXrContext *xr_context,
                            const char *action_set_name,
                            uint32_t count,
                            const char *const *action_names)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->destroyActions(action_set_name, count, action_names), xr_context);
}

int GHOST_XrCreateActionBindings(GHOST_IXrContext *xr_context,
                                 const char *action_set_name,
                                 uint32_t count,
                                 const GHOST_XrActionProfileInfo *infos)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->createActionBindings(action_set_name, count, infos),
                         xr_context);
  return 0;
}

void GHOST_XrDestroyActionBindings(GHOST_IXrContext *xr_context,
                                   const char *action_set_name,
                                   uint32_t count,
                                   const char *const *action_names,
                                   const char *const *profile_paths)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(
      xr_session->destroyActionBindings(action_set_name, count, action_names, profile_paths),
      xr_context);
}

int GHOST_XrAttachActionSets(GHOST_IXrContext *xr_context)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->attachActionSets(), xr_context);
  return 0;
}

int GHOST_XrSyncActions(GHOST_IXrContext *xr_context, const char *action_set_name)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->syncActions(action_set_name), xr_context);
  return 0;
}

int GHOST_XrApplyHapticAction(GHOST_IXrContext *xr_context,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path,
                              const int64_t *duration,
                              const float *frequency,
                              const float *amplitude)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(
      xr_session->applyHapticAction(
          action_set_name, action_name, subaction_path, *duration, *frequency, *amplitude),
      xr_context);
  return 0;
}

void GHOST_XrStopHapticAction(GHOST_IXrContext *xr_context,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->stopHapticAction(action_set_name, action_name, subaction_path),
                     xr_context);
}

void *GHOST_XrGetActionSetCustomdata(GHOST_IXrContext *xr_context, const char *action_set_name)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->getActionSetCustomdata(action_set_name), xr_context);
  return nullptr;
}

void *GHOST_XrGetActionCustomdata(GHOST_IXrContext *xr_context,
                                  const char *action_set_name,
                                  const char *action_name)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->getActionCustomdata(action_set_name, action_name),
                         xr_context);
  return nullptr;
}

uint GHOST_XrGetActionCount(GHOST_IXrContext *xr_context, const char *action_set_name)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->getActionCount(action_set_name), xr_context);
  return 0;
}

void GHOST_XrGetActionCustomdataArray(GHOST_IXrContext *xr_context,
                                      const char *action_set_name,
                                      void **r_customdata_array)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->getActionCustomdataArray(action_set_name, r_customdata_array),
                     xr_context);
}

int GHOST_XrLoadControllerModel(GHOST_IXrContext *xr_context, const char *subaction_path)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->loadControllerModel(subaction_path), xr_context);
  return 0;
}

void GHOST_XrUnloadControllerModel(GHOST_IXrContext *xr_context, const char *subaction_path)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL(xr_session->unloadControllerModel(subaction_path), xr_context);
}

int GHOST_XrUpdateControllerModelComponents(GHOST_IXrContext *xr_context,
                                            const char *subaction_path)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->updateControllerModelComponents(subaction_path), xr_context);
  return 0;
}

int GHOST_XrGetControllerModelData(GHOST_IXrContext *xr_context,
                                   const char *subaction_path,
                                   GHOST_XrControllerModelData *r_data)
{
  GHOST_XrSession *xr_session = xr_context->getSession();
  GHOST_XR_CAPI_CALL_RET(xr_session->getControllerModelData(subaction_path, *r_data), xr_context);
  return 0;
}
