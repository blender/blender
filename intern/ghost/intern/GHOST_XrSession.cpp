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
 */

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

#include "GHOST_IXrGraphicsBinding.h"
#include "GHOST_XrContext.h"
#include "GHOST_XrException.h"
#include "GHOST_XrSwapchain.h"
#include "GHOST_Xr_intern.h"

#include "GHOST_XrSession.h"

struct OpenXRSessionData {
  XrSystemId system_id = XR_NULL_SYSTEM_ID;
  XrSession session = XR_NULL_HANDLE;
  XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;

  /* Only stereo rendering supported now. */
  const XrViewConfigurationType view_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  XrSpace reference_space;
  XrSpace view_space;
  std::vector<XrView> views;
  std::vector<GHOST_XrSwapchain> swapchains;
};

struct GHOST_XrDrawInfo {
  XrFrameState frame_state;

  /** Time at frame start to benchmark frame render durations. */
  std::chrono::high_resolution_clock::time_point frame_begin_time;
  /* Time previous frames took for rendering (in ms). */
  std::list<double> last_frame_times;
};

/* -------------------------------------------------------------------- */
/** \name Create, Initialize and Destruct
 *
 * \{ */

GHOST_XrSession::GHOST_XrSession(GHOST_XrContext *xr_context)
    : m_context(xr_context), m_oxr(new OpenXRSessionData())
{
}

GHOST_XrSession::~GHOST_XrSession()
{
  unbindGraphicsContext();

  m_oxr->swapchains.clear();

  if (m_oxr->reference_space != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySpace(m_oxr->reference_space));
  }
  if (m_oxr->view_space != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySpace(m_oxr->view_space));
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
 *
 * \{ */

static void create_reference_spaces(OpenXRSessionData *oxr, const GHOST_XrPose *base_pose)
{
  XrReferenceSpaceCreateInfo create_info = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  create_info.poseInReferenceSpace.orientation.w = 1.0f;

  create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
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

  CHECK_XR(xrCreateReferenceSpace(oxr->session, &create_info, &oxr->reference_space),
           "Failed to create reference space.");

  create_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  CHECK_XR(xrCreateReferenceSpace(oxr->session, &create_info, &oxr->view_space),
           "Failed to create view reference space.");
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
                                                        m_gpu_ctx);
  if (!m_gpu_binding->checkVersionRequirements(
          m_gpu_ctx, m_context->getInstance(), m_oxr->system_id, &requirement_str)) {
    std::ostringstream strstream;
    strstream << "Available graphics context version does not meet the following requirements: "
              << requirement_str;
    throw GHOST_XrException(strstream.str().c_str());
  }
  m_gpu_binding->initFromGhostContext(m_gpu_ctx);

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
  create_reference_spaces(m_oxr.get(), &begin_info->base_pose);
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
    const XrEventDataSessionStateChanged *lifecycle)
{
  m_oxr->session_state = lifecycle->state;

  /* Runtime may send events for apparently destroyed session. Our handle should be NULL then. */
  assert((m_oxr->session == XR_NULL_HANDLE) || (m_oxr->session == lifecycle->session));

  switch (lifecycle->state) {
    case XR_SESSION_STATE_READY: {
      beginSession();
      break;
    }
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
 *
 * \{ */

void GHOST_XrSession::prepareDrawing()
{
  std::vector<XrViewConfigurationView> view_configs;
  uint32_t view_count;

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
           "Failed to get count of view configurations.");

  for (const XrViewConfigurationView &view_config : view_configs) {
    m_oxr->swapchains.emplace_back(*m_gpu_binding, m_oxr->session, view_config);
  }

  m_oxr->views.resize(view_count, {XR_TYPE_VIEW});

  m_draw_info = std::unique_ptr<GHOST_XrDrawInfo>(new GHOST_XrDrawInfo());
}

void GHOST_XrSession::beginFrameDrawing()
{
  XrFrameWaitInfo wait_info = {XR_TYPE_FRAME_WAIT_INFO};
  XrFrameBeginInfo begin_info = {XR_TYPE_FRAME_BEGIN_INFO};
  XrFrameState frame_state = {XR_TYPE_FRAME_STATE};

  /* TODO Blocking call. Drawing should run on a separate thread to avoid interferences. */
  CHECK_XR(xrWaitFrame(m_oxr->session, &wait_info, &frame_state),
           "Failed to synchronize frame rates between Blender and the device.");

  CHECK_XR(xrBeginFrame(m_oxr->session, &begin_info),
           "Failed to submit frame rendering start state.");

  m_draw_info->frame_state = frame_state;

  if (m_context->isDebugTimeMode()) {
    m_draw_info->frame_begin_time = std::chrono::high_resolution_clock::now();
  }
}

static void print_debug_timings(GHOST_XrDrawInfo *draw_info)
{
  /** Render time of last 8 frames (in ms) to calculate an average. */
  std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() -
                                                       draw_info->frame_begin_time;
  const double duration_ms = duration.count();
  const int avg_frame_count = 8;
  double avg_ms_tot = 0.0;

  if (draw_info->last_frame_times.size() >= avg_frame_count) {
    draw_info->last_frame_times.pop_front();
    assert(draw_info->last_frame_times.size() == avg_frame_count - 1);
  }
  draw_info->last_frame_times.push_back(duration_ms);
  for (double ms_iter : draw_info->last_frame_times) {
    avg_ms_tot += ms_iter;
  }

  printf("VR frame render time: %.0fms - %.2f FPS (%.2f FPS 8 frames average)\n",
         duration_ms,
         1000.0 / duration_ms,
         1000.0 / (avg_ms_tot / draw_info->last_frame_times.size()));
}

void GHOST_XrSession::endFrameDrawing(std::vector<XrCompositionLayerBaseHeader *> *layers)
{
  XrFrameEndInfo end_info = {XR_TYPE_FRAME_END_INFO};

  end_info.displayTime = m_draw_info->frame_state.predictedDisplayTime;
  end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  end_info.layerCount = layers->size();
  end_info.layers = layers->data();

  CHECK_XR(xrEndFrame(m_oxr->session, &end_info), "Failed to submit rendered frame.");

  if (m_context->isDebugTimeMode()) {
    print_debug_timings(m_draw_info.get());
  }
}

void GHOST_XrSession::draw(void *draw_customdata)
{
  std::vector<XrCompositionLayerProjectionView>
      projection_layer_views; /* Keep alive until xrEndFrame() call! */
  XrCompositionLayerProjection proj_layer;
  std::vector<XrCompositionLayerBaseHeader *> layers;

  beginFrameDrawing();

  if (m_draw_info->frame_state.shouldRender) {
    proj_layer = drawLayer(projection_layer_views, draw_customdata);
    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&proj_layer));
  }

  endFrameDrawing(&layers);
}

static void copy_openxr_pose_to_ghost_pose(const XrPosef &oxr_pose, GHOST_XrPose &r_ghost_pose)
{
  /* Set and convert to Blender coodinate space. */
  r_ghost_pose.position[0] = oxr_pose.position.x;
  r_ghost_pose.position[1] = oxr_pose.position.y;
  r_ghost_pose.position[2] = oxr_pose.position.z;
  r_ghost_pose.orientation_quat[0] = oxr_pose.orientation.w;
  r_ghost_pose.orientation_quat[1] = oxr_pose.orientation.x;
  r_ghost_pose.orientation_quat[2] = oxr_pose.orientation.y;
  r_ghost_pose.orientation_quat[3] = oxr_pose.orientation.z;
}

static void ghost_xr_draw_view_info_from_view(const XrView &view, GHOST_XrDrawViewInfo &r_info)
{
  /* Set and convert to Blender coodinate space. */
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
                               void *draw_customdata)
{
  XrSwapchainImageBaseHeader *swapchain_image = swapchain.acquireDrawableSwapchainImage();
  GHOST_XrDrawViewInfo draw_view_info = {};

  r_proj_layer_view.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
  r_proj_layer_view.pose = view.pose;
  r_proj_layer_view.fov = view.fov;
  swapchain.updateCompositionLayerProjectViewSubImage(r_proj_layer_view.subImage);

  draw_view_info.expects_srgb_buffer = swapchain.isBufferSRGB();
  draw_view_info.ofsx = r_proj_layer_view.subImage.imageRect.offset.x;
  draw_view_info.ofsy = r_proj_layer_view.subImage.imageRect.offset.y;
  draw_view_info.width = r_proj_layer_view.subImage.imageRect.extent.width;
  draw_view_info.height = r_proj_layer_view.subImage.imageRect.extent.height;
  copy_openxr_pose_to_ghost_pose(view_location.pose, draw_view_info.local_pose);
  ghost_xr_draw_view_info_from_view(view, draw_view_info);

  /* Draw! */
  m_context->getCustomFuncs().draw_view_fn(&draw_view_info, draw_customdata);
  m_gpu_binding->submitToSwapchainImage(swapchain_image, &draw_view_info);

  swapchain.releaseImage();
}

XrCompositionLayerProjection GHOST_XrSession::drawLayer(
    std::vector<XrCompositionLayerProjectionView> &r_proj_layer_views, void *draw_customdata)
{
  XrViewLocateInfo viewloc_info = {XR_TYPE_VIEW_LOCATE_INFO};
  XrViewState view_state = {XR_TYPE_VIEW_STATE};
  XrCompositionLayerProjection layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  XrSpaceLocation view_location{XR_TYPE_SPACE_LOCATION};
  uint32_t view_count;

  viewloc_info.viewConfigurationType = m_oxr->view_type;
  viewloc_info.displayTime = m_draw_info->frame_state.predictedDisplayTime;
  viewloc_info.space = m_oxr->reference_space;

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
 *
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
