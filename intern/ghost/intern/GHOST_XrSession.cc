/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <list>
#include <sstream>

#include "GHOST_C-api.h"

#include "GHOST_IXrGraphicsBinding.hh"
#include "GHOST_XrAction.hh"
#include "GHOST_XrContext.hh"
#include "GHOST_XrControllerModel.hh"
#include "GHOST_XrException.hh"
#include "GHOST_XrSwapchain.hh"
#include "GHOST_Xr_intern.hh"

#include "GHOST_XrSession.hh"

struct OpenXRSessionData {
  XrSystemId system_id = XR_NULL_SYSTEM_ID;
  XrSession session = XR_NULL_HANDLE;
  XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;

  /* Use stereo rendering by default. */
  XrViewConfigurationType view_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  bool foveation_supported = false;

  XrSpace reference_space;
  XrSpace view_space;
  XrSpace combined_eye_space;
  std::vector<XrView> views;
  std::vector<GHOST_XrSwapchain> swapchains;

  std::map<std::string, GHOST_XrActionSet> action_sets;
  /* Controller models identified by subaction path. */
  std::map<std::string, GHOST_XrControllerModel> controller_models;
};

struct GHOST_XrDrawInfo {
  XrFrameState frame_state;

  /** Time at frame start to benchmark frame render durations. */
  std::chrono::high_resolution_clock::time_point frame_begin_time;
  /* Time previous frames took for rendering (in ms). */
  std::list<double> last_frame_times;

  /* Whether foveation is active for the frame. */
  bool foveation_active;
};

/* -------------------------------------------------------------------- */
/** \name Create, Initialize and Destruct
 * \{ */

GHOST_XrSession::GHOST_XrSession(GHOST_XrContext &xr_context)
    : m_context(&xr_context), m_oxr(std::make_unique<OpenXRSessionData>())
{
}

GHOST_XrSession::~GHOST_XrSession()
{
  unbindGraphicsContext();

  m_oxr->swapchains.clear();
  m_oxr->action_sets.clear();

  if (m_oxr->reference_space != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySpace(m_oxr->reference_space));
  }
  if (m_oxr->view_space != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySpace(m_oxr->view_space));
  }
  if (m_oxr->combined_eye_space != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySpace(m_oxr->combined_eye_space));
  }
  if (m_oxr->session != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySession(m_oxr->session));
  }

  m_oxr->session = XR_NULL_HANDLE;
  m_oxr->session_state = XR_SESSION_STATE_UNKNOWN;

  m_context->getCustomFuncs().session_exit_fn(m_context->getCustomFuncs().session_exit_customdata);
}

/**
 * A system in OpenXR the combination of some sort of HMD plus controllers and whatever other
 * devices are managed through OpenXR. So this attempts to init the HMD and the other devices.
 */
void GHOST_XrSession::initSystem()
{
  assert(m_context->getInstance() != XR_NULL_HANDLE);
  assert(m_oxr->system_id == XR_NULL_SYSTEM_ID);

  XrSystemGetInfo system_info = {};
  system_info.type = XR_TYPE_SYSTEM_GET_INFO;
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  CHECK_XR(xrGetSystem(m_context->getInstance(), &system_info, &m_oxr->system_id),
           "Failed to get device information. Is a device plugged in?");
}

/** \} */ /* Create, Initialize and Destruct */

/* -------------------------------------------------------------------- */
/** \name State Management
 * \{ */

static void create_reference_spaces(OpenXRSessionData &oxr,
                                    const GHOST_XrPose &base_pose,
                                    bool isDebugMode)
{
  XrReferenceSpaceCreateInfo create_info = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  create_info.poseInReferenceSpace.orientation.w = 1.0f;

  create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
#if 0
/* TODO
 *
 * Proper reference space set up is not supported yet. We simply hand OpenXR
 * the global space as reference space and apply its pose onto the active
 * camera matrix to get a basic viewing experience going. If there's no active
 * camera with stick to the world origin.
 *
 * Once we have proper reference space set up (i.e. a way to define origin, up-
 * direction and an initial view rotation perpendicular to the up-direction),
 * we can hand OpenXR a proper reference pose/space.
 */
  create_info.poseInReferenceSpace.position.x = base_pose->position[0];
  create_info.poseInReferenceSpace.position.y = base_pose->position[1];
  create_info.poseInReferenceSpace.position.z = base_pose->position[2];
  create_info.poseInReferenceSpace.orientation.x = base_pose->orientation_quat[1];
  create_info.poseInReferenceSpace.orientation.y = base_pose->orientation_quat[2];
  create_info.poseInReferenceSpace.orientation.z = base_pose->orientation_quat[3];
  create_info.poseInReferenceSpace.orientation.w = base_pose->orientation_quat[0];
#else
  (void)base_pose;
#endif

  XrResult result = xrCreateReferenceSpace(oxr.session, &create_info, &oxr.reference_space);

  if (XR_FAILED(result)) {
    /* One of the rare cases where we don't want to immediately throw an exception on failure,
     * since runtimes are not required to support the stage reference space. If the runtime
     * doesn't support it then just fall back to the local space. */
    if (result == XR_ERROR_REFERENCE_SPACE_UNSUPPORTED) {
      if (isDebugMode) {
        printf(
            "Warning: XR runtime does not support stage reference space, falling back to local "
            "reference space.\n");
      }
      create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
      CHECK_XR(xrCreateReferenceSpace(oxr.session, &create_info, &oxr.reference_space),
               "Failed to create local reference space.");
    }
    else {
      throw GHOST_XrException("Failed to create stage reference space.", result);
    }
  }
  else {
    /* Check if tracking bounds are valid. Tracking bounds may be invalid if the user did not
     * define a tracking space via the XR runtime. */
    XrExtent2Df extents;
    CHECK_XR(xrGetReferenceSpaceBoundsRect(oxr.session, XR_REFERENCE_SPACE_TYPE_STAGE, &extents),
             "Failed to get stage reference space bounds.");
    if (extents.width == 0.0f || extents.height == 0.0f) {
      if (isDebugMode) {
        printf(
            "Warning: Invalid stage reference space bounds, falling back to local reference "
            "space. To use the stage reference space, please define a tracking space via the XR "
            "runtime.\n");
      }
      /* Fallback to local space. */
      if (oxr.reference_space != XR_NULL_HANDLE) {
        CHECK_XR(xrDestroySpace(oxr.reference_space), "Failed to destroy stage reference space.");
      }

      create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
      CHECK_XR(xrCreateReferenceSpace(oxr.session, &create_info, &oxr.reference_space),
               "Failed to create local reference space.");
    }
  }

  create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  CHECK_XR(xrCreateReferenceSpace(oxr.session, &create_info, &oxr.view_space),
           "Failed to create view reference space.");

  /* Foveation reference spaces. */
  if (oxr.foveation_supported) {
    create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_COMBINED_EYE_VARJO;
    CHECK_XR(xrCreateReferenceSpace(oxr.session, &create_info, &oxr.combined_eye_space),
             "Failed to create combined eye reference space.");
  }
}

void GHOST_XrSession::start(const GHOST_XrSessionBeginInfo *begin_info)
{
  assert(m_context->getInstance() != XR_NULL_HANDLE);
  assert(m_oxr->session == XR_NULL_HANDLE);
  if (m_context->getCustomFuncs().gpu_ctx_bind_fn == nullptr) {
    throw GHOST_XrException(
        "Invalid API usage: No way to bind graphics context to the XR session. Call "
        "GHOST_XrGraphicsContextBindFuncs() with valid parameters before starting the "
        "session (through GHOST_XrSessionStart()).");
  }

  initSystem();

  bindGraphicsContext();
  if (m_gpu_ctx == nullptr) {
    throw GHOST_XrException(
        "Invalid API usage: No graphics context returned through the callback set with "
        "GHOST_XrGraphicsContextBindFuncs(). This is required for session starting (through "
        "GHOST_XrSessionStart()).");
  }

  std::string requirement_str;
  m_gpu_binding = GHOST_XrGraphicsBindingCreateFromType(m_context->getGraphicsBindingType(),
                                                        *m_gpu_ctx);
  if (!m_gpu_binding->checkVersionRequirements(
          *m_gpu_ctx, m_context->getInstance(), m_oxr->system_id, &requirement_str))
  {
    std::ostringstream strstream;
    strstream << "Available graphics context version does not meet the following requirements: "
              << requirement_str;
    throw GHOST_XrException(strstream.str().data());
  }
  m_gpu_binding->initFromGhostContext(*m_gpu_ctx);

  XrSessionCreateInfo create_info = {};
  create_info.type = XR_TYPE_SESSION_CREATE_INFO;
  create_info.systemId = m_oxr->system_id;
  create_info.next = &m_gpu_binding->oxr_binding;

  CHECK_XR(xrCreateSession(m_context->getInstance(), &create_info, &m_oxr->session),
           "Failed to create VR session. The OpenXR runtime may have additional requirements for "
           "the graphics driver that are not met. Other causes are possible too however.\nTip: "
           "The --debug-xr command line option for Blender might allow the runtime to output "
           "detailed error information to the command line.");

  prepareDrawing();
  create_reference_spaces(*m_oxr, begin_info->base_pose, m_context->isDebugMode());

  /* Create and bind actions here. */
  m_context->getCustomFuncs().session_create_fn();
}

void GHOST_XrSession::requestEnd()
{
  xrRequestExitSession(m_oxr->session);
}

void GHOST_XrSession::beginSession()
{
  XrSessionBeginInfo begin_info = {XR_TYPE_SESSION_BEGIN_INFO};
  begin_info.primaryViewConfigurationType = m_oxr->view_type;
  CHECK_XR(xrBeginSession(m_oxr->session, &begin_info), "Failed to cleanly begin the VR session.");
}

void GHOST_XrSession::endSession()
{
  assert(m_oxr->session != XR_NULL_HANDLE);
  CHECK_XR(xrEndSession(m_oxr->session), "Failed to cleanly end the VR session.");
}

GHOST_XrSession::LifeExpectancy GHOST_XrSession::handleStateChangeEvent(
    const XrEventDataSessionStateChanged &lifecycle)
{
  m_oxr->session_state = lifecycle.state;

  /* Runtime may send events for apparently destroyed session. Our handle should be NULL then. */
  assert(m_oxr->session == XR_NULL_HANDLE || m_oxr->session == lifecycle.session);

  switch (lifecycle.state) {
    case XR_SESSION_STATE_READY:
      beginSession();
      break;
    case XR_SESSION_STATE_STOPPING:
      endSession();
      break;
    case XR_SESSION_STATE_EXITING:
    case XR_SESSION_STATE_LOSS_PENDING:
      return SESSION_DESTROY;
    default:
      break;
  }

  return SESSION_KEEP_ALIVE;
}

/** \} */ /* State Management */

/* -------------------------------------------------------------------- */
/** \name Drawing
 * \{ */

void GHOST_XrSession::prepareDrawing()
{
  assert(m_context->getInstance() != XR_NULL_HANDLE);

  std::vector<XrViewConfigurationView> view_configs;
  uint32_t view_count;

  /* Attempt to use quad view if supported. */
  if (m_context->isExtensionEnabled(XR_VARJO_QUAD_VIEWS_EXTENSION_NAME)) {
    m_oxr->view_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO;
  }

  m_oxr->foveation_supported = m_context->isExtensionEnabled(
      XR_VARJO_FOVEATED_RENDERING_EXTENSION_NAME);

  CHECK_XR(
      xrEnumerateViewConfigurationViews(
          m_context->getInstance(), m_oxr->system_id, m_oxr->view_type, 0, &view_count, nullptr),
      "Failed to get count of view configurations.");
  view_configs.resize(view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  CHECK_XR(xrEnumerateViewConfigurationViews(m_context->getInstance(),
                                             m_oxr->system_id,
                                             m_oxr->view_type,
                                             view_configs.size(),
                                             &view_count,
                                             view_configs.data()),
           "Failed to get view configurations.");

  /* If foveated rendering is used, query the foveated views. */
  if (m_oxr->foveation_supported) {
    std::vector<XrFoveatedViewConfigurationViewVARJO> request_foveated_config{
        view_count, {XR_TYPE_FOVEATED_VIEW_CONFIGURATION_VIEW_VARJO, nullptr, XR_TRUE}};

    auto foveated_views = std::vector<XrViewConfigurationView>(view_count,
                                                               {XR_TYPE_VIEW_CONFIGURATION_VIEW});

    for (uint32_t i = 0; i < view_count; i++) {
      foveated_views[i].next = &request_foveated_config[i];
    }
    CHECK_XR(xrEnumerateViewConfigurationViews(m_context->getInstance(),
                                               m_oxr->system_id,
                                               m_oxr->view_type,
                                               view_configs.size(),
                                               &view_count,
                                               foveated_views.data()),
             "Failed to get foveated view configurations.");

    /* Ensure swapchains have correct size even when foveation is being used. */
    for (uint32_t i = 0; i < view_count; i++) {
      view_configs[i].recommendedImageRectWidth = std::max(
          view_configs[i].recommendedImageRectWidth, foveated_views[i].recommendedImageRectWidth);
      view_configs[i].recommendedImageRectHeight = std::max(
          view_configs[i].recommendedImageRectHeight,
          foveated_views[i].recommendedImageRectHeight);
    }
  }

  for (const XrViewConfigurationView &view_config : view_configs) {
    m_oxr->swapchains.emplace_back(*m_gpu_binding, m_oxr->session, view_config);
  }

  m_oxr->views.resize(view_count, {XR_TYPE_VIEW});

  m_draw_info = std::make_unique<GHOST_XrDrawInfo>();
}

void GHOST_XrSession::beginFrameDrawing()
{
  XrFrameWaitInfo wait_info = {XR_TYPE_FRAME_WAIT_INFO};
  XrFrameBeginInfo begin_info = {XR_TYPE_FRAME_BEGIN_INFO};
  XrFrameState frame_state = {XR_TYPE_FRAME_STATE};

  /* TODO Blocking call. Drawing should run on a separate thread to avoid interferences. */
  CHECK_XR(xrWaitFrame(m_oxr->session, &wait_info, &frame_state),
           "Failed to synchronize frame rates between Blender and the device.");

  /* Check if we have foveation available for the current frame. */
  m_draw_info->foveation_active = false;
  if (m_oxr->foveation_supported) {
    XrSpaceLocation render_gaze_location{XR_TYPE_SPACE_LOCATION};
    CHECK_XR(xrLocateSpace(m_oxr->combined_eye_space,
                           m_oxr->view_space,
                           frame_state.predictedDisplayTime,
                           &render_gaze_location),
             "Failed to locate combined eye space.");

    m_draw_info->foveation_active = (render_gaze_location.locationFlags &
                                     XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) != 0;
  }

  CHECK_XR(xrBeginFrame(m_oxr->session, &begin_info),
           "Failed to submit frame rendering start state.");

  m_draw_info->frame_state = frame_state;

  if (m_context->isDebugTimeMode()) {
    m_draw_info->frame_begin_time = std::chrono::high_resolution_clock::now();
  }
}

static void print_debug_timings(GHOST_XrDrawInfo &draw_info)
{
  /** Render time of last 8 frames (in ms) to calculate an average. */
  std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() -
                                                       draw_info.frame_begin_time;
  const double duration_ms = duration.count();
  const int avg_frame_count = 8;
  double avg_ms_tot = 0.0;

  if (draw_info.last_frame_times.size() >= avg_frame_count) {
    draw_info.last_frame_times.pop_front();
    assert(draw_info.last_frame_times.size() == avg_frame_count - 1);
  }
  draw_info.last_frame_times.push_back(duration_ms);
  for (double ms_iter : draw_info.last_frame_times) {
    avg_ms_tot += ms_iter;
  }

  printf("VR frame render time: %.0fms - %.2f FPS (%.2f FPS 8 frames average)\n",
         duration_ms,
         1000.0 / duration_ms,
         1000.0 / (avg_ms_tot / draw_info.last_frame_times.size()));
}

void GHOST_XrSession::endFrameDrawing(std::vector<XrCompositionLayerBaseHeader *> &layers)
{
  XrFrameEndInfo end_info = {XR_TYPE_FRAME_END_INFO};

  end_info.displayTime = m_draw_info->frame_state.predictedDisplayTime;
  end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  end_info.layerCount = layers.size();
  end_info.layers = layers.data();

  CHECK_XR(xrEndFrame(m_oxr->session, &end_info), "Failed to submit rendered frame.");

  if (m_context->isDebugTimeMode()) {
    print_debug_timings(*m_draw_info);
  }
}

void GHOST_XrSession::draw(void *draw_customdata)
{
  std::vector<XrCompositionLayerProjectionView>
      projection_layer_views; /* Keep alive until #xrEndFrame() call! */
  XrCompositionLayerProjection proj_layer;
  std::vector<XrCompositionLayerBaseHeader *> layers;

  beginFrameDrawing();

  if (m_draw_info->frame_state.shouldRender) {
    proj_layer = drawLayer(projection_layer_views, draw_customdata);
    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&proj_layer));
  }

  endFrameDrawing(layers);
}

static void ghost_xr_draw_view_info_from_view(const XrView &view, GHOST_XrDrawViewInfo &r_info)
{
  /* Set and convert to Blender coordinate space. */
  copy_openxr_pose_to_ghost_pose(view.pose, r_info.eye_pose);

  r_info.fov.angle_left = view.fov.angleLeft;
  r_info.fov.angle_right = view.fov.angleRight;
  r_info.fov.angle_up = view.fov.angleUp;
  r_info.fov.angle_down = view.fov.angleDown;
}

void GHOST_XrSession::drawView(GHOST_XrSwapchain &swapchain,
                               XrCompositionLayerProjectionView &r_proj_layer_view,
                               XrSpaceLocation &view_location,
                               XrView &view,
                               uint32_t view_idx,
                               void *draw_customdata)
{
  XrSwapchainImageBaseHeader *swapchain_image = swapchain.acquireDrawableSwapchainImage();
  GHOST_XrDrawViewInfo draw_view_info = {};

  r_proj_layer_view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
  r_proj_layer_view.pose = view.pose;
  r_proj_layer_view.fov = view.fov;
  swapchain.updateCompositionLayerProjectViewSubImage(r_proj_layer_view.subImage);

  assert(view_idx < 256);
  draw_view_info.view_idx = char(view_idx);
  draw_view_info.swapchain_format = swapchain.getFormat();
  draw_view_info.expects_srgb_buffer = swapchain.isBufferSRGB();
  draw_view_info.ofsx = r_proj_layer_view.subImage.imageRect.offset.x;
  draw_view_info.ofsy = r_proj_layer_view.subImage.imageRect.offset.y;
  draw_view_info.width = r_proj_layer_view.subImage.imageRect.extent.width;
  draw_view_info.height = r_proj_layer_view.subImage.imageRect.extent.height;
  copy_openxr_pose_to_ghost_pose(view_location.pose, draw_view_info.local_pose);
  ghost_xr_draw_view_info_from_view(view, draw_view_info);

  /* Draw! */
  m_context->getCustomFuncs().draw_view_fn(&draw_view_info, draw_customdata);
  m_gpu_binding->submitToSwapchainImage(*swapchain_image, draw_view_info);

  swapchain.releaseImage();
}

XrCompositionLayerProjection GHOST_XrSession::drawLayer(
    std::vector<XrCompositionLayerProjectionView> &r_proj_layer_views, void *draw_customdata)
{
  XrViewLocateInfo viewloc_info = {XR_TYPE_VIEW_LOCATE_INFO};
  XrViewLocateFoveatedRenderingVARJO foveated_info{
      XR_TYPE_VIEW_LOCATE_FOVEATED_RENDERING_VARJO, nullptr, true};
  XrViewState view_state = {XR_TYPE_VIEW_STATE};
  XrCompositionLayerProjection layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  XrSpaceLocation view_location{XR_TYPE_SPACE_LOCATION};
  uint32_t view_count;

  viewloc_info.viewConfigurationType = m_oxr->view_type;
  viewloc_info.displayTime = m_draw_info->frame_state.predictedDisplayTime;
  viewloc_info.space = m_oxr->reference_space;

  if (m_draw_info->foveation_active) {
    viewloc_info.next = &foveated_info;
  }

  CHECK_XR(xrLocateViews(m_oxr->session,
                         &viewloc_info,
                         &view_state,
                         m_oxr->views.size(),
                         &view_count,
                         m_oxr->views.data()),
           "Failed to query frame view and projection state.");

  assert(m_oxr->swapchains.size() == view_count);

  CHECK_XR(
      xrLocateSpace(
          m_oxr->view_space, m_oxr->reference_space, viewloc_info.displayTime, &view_location),
      "Failed to query frame view space");

  r_proj_layer_views.resize(view_count);

  for (uint32_t view_idx = 0; view_idx < view_count; view_idx++) {
    drawView(m_oxr->swapchains[view_idx],
             r_proj_layer_views[view_idx],
             view_location,
             m_oxr->views[view_idx],
             view_idx,
             draw_customdata);
  }

  layer.space = m_oxr->reference_space;
  layer.viewCount = r_proj_layer_views.size();
  layer.views = r_proj_layer_views.data();

  return layer;
}

bool GHOST_XrSession::needsUpsideDownDrawing() const
{
  return m_gpu_binding && m_gpu_binding->needsUpsideDownDrawing(*m_gpu_ctx);
}

/** \} */ /* Drawing */

/* -------------------------------------------------------------------- */
/** \name State Queries
 * \{ */

bool GHOST_XrSession::isRunning() const
{
  if (m_oxr->session == XR_NULL_HANDLE) {
    return false;
  }
  switch (m_oxr->session_state) {
    case XR_SESSION_STATE_READY:
    case XR_SESSION_STATE_SYNCHRONIZED:
    case XR_SESSION_STATE_VISIBLE:
    case XR_SESSION_STATE_FOCUSED:
      return true;
    default:
      return false;
  }
}

/** \} */ /* State Queries */

/* -------------------------------------------------------------------- */
/** \name Graphics Context Injection
 *
 * Sessions need access to Ghost graphics context information. Additionally, this API allows
 * creating contexts on the fly (created on start, destructed on end). For this, callbacks to bind
 * (potentially create) and unbind (potentially destruct) a Ghost graphics context have to be set,
 * which will be called on session start and end respectively.
 *
 * \{ */

void GHOST_XrSession::bindGraphicsContext()
{
  const GHOST_XrCustomFuncs &custom_funcs = m_context->getCustomFuncs();
  assert(custom_funcs.gpu_ctx_bind_fn);
  m_gpu_ctx = static_cast<GHOST_Context *>(custom_funcs.gpu_ctx_bind_fn());
}

void GHOST_XrSession::unbindGraphicsContext()
{
  const GHOST_XrCustomFuncs &custom_funcs = m_context->getCustomFuncs();
  if (custom_funcs.gpu_ctx_unbind_fn) {
    custom_funcs.gpu_ctx_unbind_fn((GHOST_ContextHandle)m_gpu_ctx);
  }
  m_gpu_ctx = nullptr;
}

/** \} */ /* Graphics Context Injection */

/* -------------------------------------------------------------------- */
/** \name Actions
 *
 * \{ */

static GHOST_XrActionSet *find_action_set(OpenXRSessionData *oxr, const char *action_set_name)
{
  std::map<std::string, GHOST_XrActionSet>::iterator it = oxr->action_sets.find(action_set_name);
  if (it == oxr->action_sets.end()) {
    return nullptr;
  }
  return &it->second;
}

bool GHOST_XrSession::createActionSet(const GHOST_XrActionSetInfo &info)
{
  std::map<std::string, GHOST_XrActionSet> &action_sets = m_oxr->action_sets;
  if (action_sets.find(info.name) != action_sets.end()) {
    return false;
  }

  XrInstance instance = m_context->getInstance();

  action_sets.emplace(
      std::piecewise_construct, std::make_tuple(info.name), std::make_tuple(instance, info));

  return true;
}

void GHOST_XrSession::destroyActionSet(const char *action_set_name)
{
  std::map<std::string, GHOST_XrActionSet> &action_sets = m_oxr->action_sets;
  if (action_sets.find(action_set_name) != action_sets.end()) {
    action_sets.erase(action_set_name);
  }
}

bool GHOST_XrSession::createActions(const char *action_set_name,
                                    uint32_t count,
                                    const GHOST_XrActionInfo *infos)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return false;
  }

  XrInstance instance = m_context->getInstance();

  for (uint32_t i = 0; i < count; ++i) {
    if (!action_set->createAction(instance, infos[i])) {
      return false;
    }
  }

  return true;
}

void GHOST_XrSession::destroyActions(const char *action_set_name,
                                     uint32_t count,
                                     const char *const *action_names)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return;
  }

  for (uint32_t i = 0; i < count; ++i) {
    action_set->destroyAction(action_names[i]);
  }
}

bool GHOST_XrSession::createActionBindings(const char *action_set_name,
                                           uint32_t count,
                                           const GHOST_XrActionProfileInfo *infos)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return false;
  }

  XrInstance instance = m_context->getInstance();
  XrSession session = m_oxr->session;

  for (uint32_t profile_idx = 0; profile_idx < count; ++profile_idx) {
    const GHOST_XrActionProfileInfo &info = infos[profile_idx];

    GHOST_XrAction *action = action_set->findAction(info.action_name);
    if (action == nullptr) {
      continue;
    }

    action->createBinding(instance, session, info);
  }

  return true;
}

void GHOST_XrSession::destroyActionBindings(const char *action_set_name,
                                            uint32_t count,
                                            const char *const *action_names,
                                            const char *const *profile_paths)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return;
  }

  for (uint32_t i = 0; i < count; ++i) {
    GHOST_XrAction *action = action_set->findAction(action_names[i]);
    if (action == nullptr) {
      continue;
    }

    action->destroyBinding(profile_paths[i]);
  }
}

bool GHOST_XrSession::attachActionSets()
{
  /* Suggest action bindings for all action sets. */
  std::map<XrPath, std::vector<XrActionSuggestedBinding>> profile_bindings;
  for (auto &[name, action_set] : m_oxr->action_sets) {
    action_set.getBindings(profile_bindings);
  }

  if (profile_bindings.size() < 1) {
    return false;
  }

  XrInteractionProfileSuggestedBinding bindings_info{
      XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
  XrInstance instance = m_context->getInstance();

  for (auto &[profile, bindings] : profile_bindings) {
    bindings_info.interactionProfile = profile;
    bindings_info.countSuggestedBindings = uint32_t(bindings.size());
    bindings_info.suggestedBindings = bindings.data();

    CHECK_XR(xrSuggestInteractionProfileBindings(instance, &bindings_info),
             "Failed to suggest interaction profile bindings.");
  }

  /* Attach action sets. */
  XrSessionActionSetsAttachInfo attach_info{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach_info.countActionSets = uint32_t(m_oxr->action_sets.size());

  /* Create an aligned copy of the action sets to pass to xrAttachSessionActionSets(). */
  std::vector<XrActionSet> action_sets(attach_info.countActionSets);
  uint32_t i = 0;
  for (auto &[name, action_set] : m_oxr->action_sets) {
    action_sets[i++] = action_set.getActionSet();
  }
  attach_info.actionSets = action_sets.data();

  CHECK_XR(xrAttachSessionActionSets(m_oxr->session, &attach_info),
           "Failed to attach XR action sets.");

  return true;
}

bool GHOST_XrSession::syncActions(const char *action_set_name)
{
  std::map<std::string, GHOST_XrActionSet> &action_sets = m_oxr->action_sets;

  XrActionsSyncInfo sync_info{XR_TYPE_ACTIONS_SYNC_INFO};
  sync_info.countActiveActionSets = (action_set_name != nullptr) ? 1 :
                                                                   uint32_t(action_sets.size());
  if (sync_info.countActiveActionSets < 1) {
    return false;
  }

  std::vector<XrActiveActionSet> active_action_sets(sync_info.countActiveActionSets);
  GHOST_XrActionSet *action_set = nullptr;

  if (action_set_name != nullptr) {
    action_set = find_action_set(m_oxr.get(), action_set_name);
    if (action_set == nullptr) {
      return false;
    }

    XrActiveActionSet &active_action_set = active_action_sets[0];
    active_action_set.actionSet = action_set->getActionSet();
    active_action_set.subactionPath = XR_NULL_PATH;
  }
  else {
    uint32_t i = 0;
    for (auto &[name, action_set] : action_sets) {
      XrActiveActionSet &active_action_set = active_action_sets[i++];
      active_action_set.actionSet = action_set.getActionSet();
      active_action_set.subactionPath = XR_NULL_PATH;
    }
  }
  sync_info.activeActionSets = active_action_sets.data();

  CHECK_XR(xrSyncActions(m_oxr->session, &sync_info), "Failed to synchronize XR actions.");

  /* Update action states (i.e. Blender custom data). */
  XrSession session = m_oxr->session;
  XrSpace reference_space = m_oxr->reference_space;
  const XrTime &predicted_display_time = m_draw_info->frame_state.predictedDisplayTime;

  if (action_set != nullptr) {
    action_set->updateStates(session, reference_space, predicted_display_time);
  }
  else {
    for (auto &[name, action_set] : action_sets) {
      action_set.updateStates(session, reference_space, predicted_display_time);
    }
  }

  return true;
}

bool GHOST_XrSession::applyHapticAction(const char *action_set_name,
                                        const char *action_name,
                                        const char *subaction_path,
                                        const int64_t &duration,
                                        const float &frequency,
                                        const float &amplitude)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return false;
  }

  GHOST_XrAction *action = action_set->findAction(action_name);
  if (action == nullptr) {
    return false;
  }

  action->applyHapticFeedback(
      m_oxr->session, action_name, subaction_path, duration, frequency, amplitude);

  return true;
}

void GHOST_XrSession::stopHapticAction(const char *action_set_name,
                                       const char *action_name,
                                       const char *subaction_path)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return;
  }

  GHOST_XrAction *action = action_set->findAction(action_name);
  if (action == nullptr) {
    return;
  }

  action->stopHapticFeedback(m_oxr->session, action_name, subaction_path);
}

void *GHOST_XrSession::getActionSetCustomdata(const char *action_set_name)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return nullptr;
  }

  return action_set->getCustomdata();
}

void *GHOST_XrSession::getActionCustomdata(const char *action_set_name, const char *action_name)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return nullptr;
  }

  GHOST_XrAction *action = action_set->findAction(action_name);
  if (action == nullptr) {
    return nullptr;
  }

  return action->getCustomdata();
}

uint32_t GHOST_XrSession::getActionCount(const char *action_set_name)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return 0;
  }

  return action_set->getActionCount();
}

void GHOST_XrSession::getActionCustomdataArray(const char *action_set_name,
                                               void **r_customdata_array)
{
  GHOST_XrActionSet *action_set = find_action_set(m_oxr.get(), action_set_name);
  if (action_set == nullptr) {
    return;
  }

  action_set->getActionCustomdataArray(r_customdata_array);
}

/** \} */ /* Actions */

/* -------------------------------------------------------------------- */
/** \name Controller Model
 *
 * \{ */

bool GHOST_XrSession::loadControllerModel(const char *subaction_path)
{
  if (!m_context->isExtensionEnabled(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME)) {
    return false;
  }

  XrSession session = m_oxr->session;
  std::map<std::string, GHOST_XrControllerModel> &controller_models = m_oxr->controller_models;
  std::map<std::string, GHOST_XrControllerModel>::iterator it = controller_models.find(
      subaction_path);

  if (it == controller_models.end()) {
    XrInstance instance = m_context->getInstance();
    it = controller_models
             .emplace(std::piecewise_construct,
                      std::make_tuple(subaction_path),
                      std::make_tuple(instance, subaction_path))
             .first;
  }

  it->second.load(session);

  return true;
}

void GHOST_XrSession::unloadControllerModel(const char *subaction_path)
{
  std::map<std::string, GHOST_XrControllerModel> &controller_models = m_oxr->controller_models;
  if (controller_models.find(subaction_path) != controller_models.end()) {
    controller_models.erase(subaction_path);
  }
}

bool GHOST_XrSession::updateControllerModelComponents(const char *subaction_path)
{
  XrSession session = m_oxr->session;
  std::map<std::string, GHOST_XrControllerModel>::iterator it = m_oxr->controller_models.find(
      subaction_path);
  if (it == m_oxr->controller_models.end()) {
    return false;
  }

  it->second.updateComponents(session);

  return true;
}

bool GHOST_XrSession::getControllerModelData(const char *subaction_path,
                                             GHOST_XrControllerModelData &r_data)
{
  std::map<std::string, GHOST_XrControllerModel>::iterator it = m_oxr->controller_models.find(
      subaction_path);
  if (it == m_oxr->controller_models.end()) {
    return false;
  }

  it->second.getData(r_data);

  return true;
}

/** \} */ /* Controller Model */
