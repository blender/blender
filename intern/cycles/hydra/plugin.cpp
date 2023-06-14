/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/plugin.h"
#include "hydra/render_delegate.h"
#include "util/log.h"
#include "util/path.h"

#include <pxr/base/arch/fileSystem.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/thisPlugin.h>
#include <pxr/base/tf/envSetting.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>

PXR_NAMESPACE_OPEN_SCOPE

#ifdef WITH_CYCLES_LOGGING
TF_DEFINE_ENV_SETTING(CYCLES_LOGGING, false, "Enable Cycles logging")
TF_DEFINE_ENV_SETTING(CYCLES_LOGGING_SEVERITY, 1, "Cycles logging verbosity")
#endif

HdCyclesPlugin::HdCyclesPlugin()
{
  const PlugPluginPtr plugin = PLUG_THIS_PLUGIN;
  // Initialize Cycles paths relative to the plugin resource path
  std::string rootPath = PXR_NS::ArchAbsPath(plugin->GetResourcePath());
  CCL_NS::path_init(std::move(rootPath));

#ifdef WITH_CYCLES_LOGGING
  if (TfGetEnvSetting(CYCLES_LOGGING)) {
    CCL_NS::util_logging_start();
    CCL_NS::util_logging_verbosity_set(TfGetEnvSetting(CYCLES_LOGGING_SEVERITY));
  }
#endif
}

HdCyclesPlugin::~HdCyclesPlugin() {}

#if PXR_VERSION < 2302
bool HdCyclesPlugin::IsSupported() const
{
  return true;
}
#else
bool HdCyclesPlugin::IsSupported(bool gpuEnabled) const
{
  return true;
}
#endif

HdRenderDelegate *HdCyclesPlugin::CreateRenderDelegate()
{
  return CreateRenderDelegate({});
}

HdRenderDelegate *HdCyclesPlugin::CreateRenderDelegate(const HdRenderSettingsMap &settingsMap)
{
  return new HD_CYCLES_NS::HdCyclesDelegate(settingsMap);
}

void HdCyclesPlugin::DeleteRenderDelegate(HdRenderDelegate *renderDelegate)
{
  delete renderDelegate;
}

// USD's type system accounts for namespace, so we'd have to register our name as
// HdCycles::HdCyclesPlugin in plugInfo.json, which isn't all that bad for JSON,
// but those colons may cause issues for any USD specific tooling. So just put our
// plugin class in the pxr namespace (which USD's type system will elide).
TF_REGISTRY_FUNCTION(TfType)
{
  HdRendererPluginRegistry::Define<PXR_NS::HdCyclesPlugin>();
}

PXR_NAMESPACE_CLOSE_SCOPE
