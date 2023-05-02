/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 NVIDIA Corporation
 * Copyright 2022 Blender Foundation */

#include "hydra/render_pass.h"
#include "hydra/camera.h"
#include "hydra/output_driver.h"
#include "hydra/render_buffer.h"
#include "hydra/render_delegate.h"
#include "hydra/session.h"

#ifdef WITH_HYDRA_DISPLAY_DRIVER
#  include "hydra/display_driver.h"
#endif

#include "scene/camera.h"
#include "scene/integrator.h"
#include "scene/scene.h"

#include "session/session.h"

#include <pxr/imaging/hd/renderPassState.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

HdCyclesRenderPass::HdCyclesRenderPass(HdRenderIndex *index,
                                       HdRprimCollection const &collection,
                                       HdCyclesSession *renderParam)
    : HdRenderPass(index, collection), _renderParam(renderParam)
{
  Session *const session = _renderParam->session;
  // Reset cancel state so session thread can continue rendering
  session->progress.reset();

  session->set_output_driver(make_unique<HdCyclesOutputDriver>(renderParam));

  const auto renderDelegate = static_cast<const HdCyclesDelegate *>(
      GetRenderIndex()->GetRenderDelegate());
  if (renderDelegate->IsDisplaySupported()) {
#ifdef WITH_HYDRA_DISPLAY_DRIVER
    session->set_display_driver(
        make_unique<HdCyclesDisplayDriver>(renderParam, renderDelegate->GetHgi()));
#endif
  }
}

HdCyclesRenderPass::~HdCyclesRenderPass()
{
  Session *const session = _renderParam->session;
  session->cancel(true);
}

bool HdCyclesRenderPass::IsConverged() const
{
  for (const HdRenderPassAovBinding &aovBinding : _renderParam->GetAovBindings()) {
    if (aovBinding.renderBuffer && !aovBinding.renderBuffer->IsConverged()) {
      return false;
    }
  }

  return true;
}

void HdCyclesRenderPass::ResetConverged()
{
  for (const HdRenderPassAovBinding &aovBinding : _renderParam->GetAovBindings()) {
    if (const auto renderBuffer = static_cast<HdCyclesRenderBuffer *>(aovBinding.renderBuffer)) {
      renderBuffer->SetConverged(false);
    }
  }
}

void HdCyclesRenderPass::_Execute(const HdRenderPassStateSharedPtr &renderPassState,
                                  const TfTokenVector &renderTags)
{
  Scene *const scene = _renderParam->session->scene;
  Session *const session = _renderParam->session;

  if (session->progress.get_cancel()) {
    return;  // Something went wrong and cannot continue without recreating the session
  }

  if (scene->mutex.try_lock()) {
    const auto renderDelegate = static_cast<HdCyclesDelegate *>(
        GetRenderIndex()->GetRenderDelegate());

    const unsigned int settingsVersion = renderDelegate->GetRenderSettingsVersion();

    // Update requested AOV bindings
    const HdRenderPassAovBindingVector &aovBindings = renderPassState->GetAovBindings();
    if (_renderParam->GetAovBindings() != aovBindings ||
        // Need to resync passes when denoising is enabled or disabled to update the pass mode
        (settingsVersion != _lastSettingsVersion && scene->integrator->use_denoise_is_modified()))
    {
      _renderParam->SyncAovBindings(aovBindings);

      if (renderDelegate->IsDisplaySupported()) {
        // Update display pass to the first requested color AOV
        HdRenderPassAovBinding displayAovBinding = !aovBindings.empty() ? aovBindings.front() :
                                                                          HdRenderPassAovBinding();
        if (displayAovBinding.aovName == HdAovTokens->color && displayAovBinding.renderBuffer) {
          _renderParam->SetDisplayAovBinding(displayAovBinding);
        }
        else {
          _renderParam->SetDisplayAovBinding(HdRenderPassAovBinding());
        }
      }
    }

    // Update camera dimensions to the viewport size
#if PXR_VERSION >= 2102
    CameraUtilFraming framing = renderPassState->GetFraming();
    if (!framing.IsValid()) {
      const GfVec4f vp = renderPassState->GetViewport();
      framing = CameraUtilFraming(GfRect2i(GfVec2i(0), int(vp[2]), int(vp[3])));
    }

    scene->camera->set_full_width(framing.dataWindow.GetWidth());
    scene->camera->set_full_height(framing.dataWindow.GetHeight());
#else
    const GfVec4f vp = renderPassState->GetViewport();
    scene->camera->set_full_width(int(vp[2]));
    scene->camera->set_full_height(int(vp[3]));
#endif

    if (const auto camera = static_cast<const HdCyclesCamera *>(renderPassState->GetCamera())) {
      camera->ApplyCameraSettings(_renderParam, scene->camera);
    }
    else {
      HdCyclesCamera::ApplyCameraSettings(_renderParam,
                                          renderPassState->GetWorldToViewMatrix(),
                                          renderPassState->GetProjectionMatrix(),
                                          renderPassState->GetClipPlanes(),
                                          scene->camera);
    }

    // Reset session if the session, scene, camera or AOV bindings changed
    if (scene->need_reset() || settingsVersion != _lastSettingsVersion) {
      _lastSettingsVersion = settingsVersion;

      // Reset convergence state of all render buffers
      ResetConverged();

      BufferParams buffer_params;
#if PXR_VERSION >= 2102
      buffer_params.full_x = static_cast<int>(framing.displayWindow.GetMin()[0]);
      buffer_params.full_y = static_cast<int>(framing.displayWindow.GetMin()[1]);
      buffer_params.full_width = static_cast<int>(framing.displayWindow.GetSize()[0]);
      buffer_params.full_height = static_cast<int>(framing.displayWindow.GetSize()[1]);

      buffer_params.window_x = framing.dataWindow.GetMinX() - buffer_params.full_x;
      buffer_params.window_y = framing.dataWindow.GetMinY() - buffer_params.full_y;
      buffer_params.window_width = framing.dataWindow.GetWidth();
      buffer_params.window_height = framing.dataWindow.GetHeight();

      buffer_params.width = buffer_params.window_width;
      buffer_params.height = buffer_params.window_height;
#else
      buffer_params.width = static_cast<int>(vp[2]);
      buffer_params.height = static_cast<int>(vp[3]);
      buffer_params.full_width = buffer_params.width;
      buffer_params.full_height = buffer_params.height;
      buffer_params.window_width = buffer_params.width;
      buffer_params.window_height = buffer_params.height;
#endif

      session->reset(session->params, buffer_params);
    }

    scene->mutex.unlock();

    // Start Cycles render thread if not already running
    session->start();
  }

  session->draw();
}

void HdCyclesRenderPass::_MarkCollectionDirty() {}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
