/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/render_delegate.h"
#include "hydra/camera.h"
#include "hydra/curves.h"
#include "hydra/field.h"
#include "hydra/instancer.h"
#include "hydra/light.h"
#include "hydra/material.h"
#include "hydra/mesh.h"
#include "hydra/node_util.h"
#include "hydra/pointcloud.h"
#include "hydra/render_buffer.h"
#include "hydra/render_pass.h"
#include "hydra/session.h"
#include "hydra/volume.h"
#include "scene/integrator.h"
#include "scene/scene.h"
#include "session/session.h"

#include <pxr/base/tf/getenv.h>
#include <pxr/imaging/hd/extComputation.h>
#include <pxr/imaging/hgi/tokens.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(HdCyclesRenderSettingsTokens, HD_CYCLES_RENDER_SETTINGS_TOKENS);

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(_tokens,
    (cycles)
    (openvdbAsset)
);
// clang-format on

namespace {

const TfTokenVector kSupportedRPrimTypes = {
    HdPrimTypeTokens->basisCurves,
    HdPrimTypeTokens->mesh,
    HdPrimTypeTokens->points,
#ifdef WITH_OPENVDB
    HdPrimTypeTokens->volume,
#endif
};

const TfTokenVector kSupportedSPrimTypes = {
    HdPrimTypeTokens->camera,
    HdPrimTypeTokens->material,
    HdPrimTypeTokens->diskLight,
    HdPrimTypeTokens->distantLight,
    HdPrimTypeTokens->domeLight,
    HdPrimTypeTokens->rectLight,
    HdPrimTypeTokens->sphereLight,
    HdPrimTypeTokens->extComputation,
};

const TfTokenVector kSupportedBPrimTypes = {
    HdPrimTypeTokens->renderBuffer,
#ifdef WITH_OPENVDB
    _tokens->openvdbAsset,
#endif
};

SessionParams GetSessionParams(const HdRenderSettingsMap &settings)
{
  SessionParams params;
  params.threads = 0;
  params.background = false;
  params.use_resolution_divider = false;

  HdRenderSettingsMap::const_iterator it;

  // Pull all setting that contribute to device creation first
  it = settings.find(HdCyclesRenderSettingsTokens->threads);
  if (it != settings.end()) {
    params.threads = VtValue::Cast<int>(it->second).GetWithDefault(params.threads);
  }

  // Get the Cycles device from settings or environment, falling back to CPU
  std::string deviceType = Device::string_from_type(DEVICE_CPU);
  it = settings.find(HdCyclesRenderSettingsTokens->device);
  if (it != settings.end()) {
    deviceType = VtValue::Cast<std::string>(it->second).GetWithDefault(deviceType);
  }
  else {
    const std::string deviceTypeEnv = TfGetenv("CYCLES_DEVICE");
    if (!deviceTypeEnv.empty()) {
      deviceType = deviceTypeEnv;
    }
  }

  // Move to all uppercase for Device::type_from_string
  std::transform(deviceType.begin(), deviceType.end(), deviceType.begin(), ::toupper);

  vector<DeviceInfo> devices = Device::available_devices(
      DEVICE_MASK(Device::type_from_string(deviceType.c_str())));
  if (devices.empty()) {
    devices = Device::available_devices(DEVICE_MASK_CPU);
    if (!devices.empty()) {
      params.device = devices.front();
    }
  }
  else {
    params.device = Device::get_multi_device(devices, params.threads, params.background);
  }

  return params;
}

}  // namespace

HdCyclesDelegate::HdCyclesDelegate(const HdRenderSettingsMap &settingsMap,
                                   Session *session_,
                                   const bool keep_nodes)
    : HdRenderDelegate()
{
  _renderParam = session_ ? std::make_unique<HdCyclesSession>(session_, keep_nodes) :
                            std::make_unique<HdCyclesSession>(GetSessionParams(settingsMap));

  for (const auto &setting : settingsMap) {
    // Skip over the settings known to be used for initialization only
    if (setting.first == HdCyclesRenderSettingsTokens->device ||
        setting.first == HdCyclesRenderSettingsTokens->threads)
    {
      continue;
    }

    SetRenderSetting(setting.first, setting.second);
  }
}

HdCyclesDelegate::~HdCyclesDelegate() {}

void HdCyclesDelegate::SetDrivers(const HdDriverVector &drivers)
{
  for (HdDriver *hdDriver : drivers) {
    if (hdDriver->name == HgiTokens->renderDriver && hdDriver->driver.IsHolding<Hgi *>()) {
      _hgi = hdDriver->driver.UncheckedGet<Hgi *>();
      break;
    }
  }
}

bool HdCyclesDelegate::IsDisplaySupported() const
{
#if defined(_WIN32) && defined(WITH_HYDRA_DISPLAY_DRIVER)
  return _hgi && _hgi->GetAPIName() == HgiTokens->OpenGL;
#else
  return false;
#endif
}

const TfTokenVector &HdCyclesDelegate::GetSupportedRprimTypes() const
{
  return kSupportedRPrimTypes;
}

const TfTokenVector &HdCyclesDelegate::GetSupportedSprimTypes() const
{
  return kSupportedSPrimTypes;
}

const TfTokenVector &HdCyclesDelegate::GetSupportedBprimTypes() const
{
  return kSupportedBPrimTypes;
}

HdRenderParam *HdCyclesDelegate::GetRenderParam() const
{
  return _renderParam.get();
}

HdResourceRegistrySharedPtr HdCyclesDelegate::GetResourceRegistry() const
{
  return HdResourceRegistrySharedPtr();
}

bool HdCyclesDelegate::IsPauseSupported() const
{
  return true;
}

bool HdCyclesDelegate::Pause()
{
  _renderParam->session->set_pause(true);
  return true;
}

bool HdCyclesDelegate::Resume()
{
  _renderParam->session->set_pause(false);
  return true;
}

HdRenderPassSharedPtr HdCyclesDelegate::CreateRenderPass(HdRenderIndex *index,
                                                         const HdRprimCollection &collection)
{
  return HdRenderPassSharedPtr(new HdCyclesRenderPass(index, collection, _renderParam.get()));
}

HdInstancer *HdCyclesDelegate::CreateInstancer(HdSceneDelegate *delegate,
                                               const SdfPath &instancerId
#if PXR_VERSION < 2102
                                               ,
                                               const SdfPath &parentId
#endif
)
{
  return new HdCyclesInstancer(delegate,
                               instancerId
#if PXR_VERSION < 2102
                               ,
                               parentId
#endif
  );
}

void HdCyclesDelegate::DestroyInstancer(HdInstancer *instancer)
{
  delete instancer;
}

HdRprim *HdCyclesDelegate::CreateRprim(const TfToken &typeId,
                                       const SdfPath &rprimId
#if PXR_VERSION < 2102
                                       ,
                                       const SdfPath &instancerId
#endif
)
{
  if (typeId == HdPrimTypeTokens->mesh) {
    return new HdCyclesMesh(rprimId
#if PXR_VERSION < 2102
                            ,
                            instancerId
#endif
    );
  }
  if (typeId == HdPrimTypeTokens->basisCurves) {
    return new HdCyclesCurves(rprimId
#if PXR_VERSION < 2102
                              ,
                              instancerId
#endif
    );
  }
  if (typeId == HdPrimTypeTokens->points) {
    return new HdCyclesPoints(rprimId
#if PXR_VERSION < 2102
                              ,
                              instancerId
#endif
    );
  }
#ifdef WITH_OPENVDB
  if (typeId == HdPrimTypeTokens->volume) {
    return new HdCyclesVolume(rprimId
#  if PXR_VERSION < 2102
                              ,
                              instancerId
#  endif
    );
  }
#endif

  TF_CODING_ERROR("Unknown Rprim type %s", typeId.GetText());
  return nullptr;
}

void HdCyclesDelegate::DestroyRprim(HdRprim *rPrim)
{
  delete rPrim;
}

HdSprim *HdCyclesDelegate::CreateSprim(const TfToken &typeId, const SdfPath &sprimId)
{
  if (typeId == HdPrimTypeTokens->camera) {
    return new HdCyclesCamera(sprimId);
  }
  if (typeId == HdPrimTypeTokens->material) {
    return new HdCyclesMaterial(sprimId);
  }
  if (typeId == HdPrimTypeTokens->diskLight || typeId == HdPrimTypeTokens->distantLight ||
      typeId == HdPrimTypeTokens->domeLight || typeId == HdPrimTypeTokens->rectLight ||
      typeId == HdPrimTypeTokens->sphereLight)
  {
    return new HdCyclesLight(sprimId, typeId);
  }
  if (typeId == HdPrimTypeTokens->extComputation) {
    return new HdExtComputation(sprimId);
  }

  TF_CODING_ERROR("Unknown Sprim type %s", typeId.GetText());
  return nullptr;
}

HdSprim *HdCyclesDelegate::CreateFallbackSprim(const TfToken &typeId)
{
  return CreateSprim(typeId, SdfPath::EmptyPath());
}

void HdCyclesDelegate::DestroySprim(HdSprim *sPrim)
{
  delete sPrim;
}

HdBprim *HdCyclesDelegate::CreateBprim(const TfToken &typeId, const SdfPath &bprimId)
{
  if (typeId == HdPrimTypeTokens->renderBuffer) {
    return new HdCyclesRenderBuffer(bprimId);
  }
#ifdef WITH_OPENVDB
  if (typeId == _tokens->openvdbAsset) {
    return new HdCyclesField(bprimId, typeId);
  }
#endif

  TF_CODING_ERROR("Unknown Bprim type %s", typeId.GetText());
  return nullptr;
}

HdBprim *HdCyclesDelegate::CreateFallbackBprim(const TfToken &typeId)
{
  return CreateBprim(typeId, SdfPath::EmptyPath());
}

void HdCyclesDelegate::DestroyBprim(HdBprim *bPrim)
{
  delete bPrim;
}

void HdCyclesDelegate::CommitResources(HdChangeTracker *tracker)
{
  TF_UNUSED(tracker);

  const SceneLock lock(_renderParam.get());

  _renderParam->UpdateScene();
}

TfToken HdCyclesDelegate::GetMaterialBindingPurpose() const
{
  return HdTokens->full;
}

#if HD_API_VERSION < 41
TfToken HdCyclesDelegate::GetMaterialNetworkSelector() const
{
  return _tokens->cycles;
}
#else
TfTokenVector HdCyclesDelegate::GetMaterialRenderContexts() const
{
  return {_tokens->cycles};
}
#endif

VtDictionary HdCyclesDelegate::GetRenderStats() const
{
  const Stats &stats = _renderParam->session->stats;
  const Progress &progress = _renderParam->session->progress;

  double totalTime, renderTime;
  progress.get_time(totalTime, renderTime);
  double fractionDone = progress.get_progress();

  std::string status, substatus;
  progress.get_status(status, substatus);
  if (!substatus.empty()) {
    status += " | " + substatus;
  }

  return {{"rendererName", VtValue("Cycles")},
          {"rendererVersion", VtValue(GfVec3i(0, 0, 0))},
          {"percentDone", VtValue(floor_to_int(fractionDone * 100))},
          {"fractionDone", VtValue(fractionDone)},
          {"loadClockTime", VtValue(totalTime - renderTime)},
          {"peakMemory", VtValue(stats.mem_peak)},
          {"totalClockTime", VtValue(totalTime)},
          {"totalMemory", VtValue(stats.mem_used)},
          {"renderProgressAnnotation", VtValue(status)}};
}

HdAovDescriptor HdCyclesDelegate::GetDefaultAovDescriptor(const TfToken &name) const
{
  if (name == HdAovTokens->color) {
    HdFormat colorFormat = HdFormatFloat32Vec4;
    if (IsDisplaySupported()) {
      // Can use Cycles 'DisplayDriver' in OpenGL, but it only supports 'half4' format
      colorFormat = HdFormatFloat16Vec4;
    }

    return HdAovDescriptor(colorFormat, false, VtValue(GfVec4f(0.0f)));
  }
  if (name == HdAovTokens->depth) {
    return HdAovDescriptor(HdFormatFloat32, false, VtValue(1.0f));
  }
  if (name == HdAovTokens->normal) {
    return HdAovDescriptor(HdFormatFloat32Vec3, false, VtValue(GfVec3f(0.0f)));
  }
  if (name == HdAovTokens->primId || name == HdAovTokens->instanceId ||
      name == HdAovTokens->elementId)
  {
    return HdAovDescriptor(HdFormatInt32, false, VtValue(-1));
  }

  return HdAovDescriptor();
}

HdRenderSettingDescriptorList HdCyclesDelegate::GetRenderSettingDescriptors() const
{
  Scene *const scene = _renderParam->session->scene;

  HdRenderSettingDescriptorList descriptors;

  descriptors.push_back({
      "Time Limit",
      HdCyclesRenderSettingsTokens->timeLimit,
      VtValue(0.0),
  });
  descriptors.push_back({
      "Sample Count",
      HdCyclesRenderSettingsTokens->samples,
      VtValue(1024),
  });
  descriptors.push_back({
      "Sample Offset",
      HdCyclesRenderSettingsTokens->sampleOffset,
      VtValue(0),
  });

  for (const SocketType &socket : scene->integrator->type->inputs) {
    descriptors.push_back({socket.ui_name.string(),
                           TfToken("cycles:integrator:" + socket.name.string()),
                           GetNodeValue(scene->integrator, socket)});
  }

  return descriptors;
}

void HdCyclesDelegate::SetRenderSetting(const PXR_NS::TfToken &key, const PXR_NS::VtValue &value)
{
  Scene *const scene = _renderParam->session->scene;
  Session *const session = _renderParam->session;

  if (key == HdCyclesRenderSettingsTokens->stageMetersPerUnit) {
    _renderParam->SetStageMetersPerUnit(
        VtValue::Cast<double>(value).GetWithDefault(_renderParam->GetStageMetersPerUnit()));
  }
  else if (key == HdCyclesRenderSettingsTokens->timeLimit) {
    session->set_time_limit(
        VtValue::Cast<double>(value).GetWithDefault(session->params.time_limit));
  }
  else if (key == HdCyclesRenderSettingsTokens->samples) {
    static const int max_samples = Integrator::MAX_SAMPLES;
    int samples = VtValue::Cast<int>(value).GetWithDefault(session->params.samples);
    samples = std::min(std::max(1, samples), max_samples);
    session->set_samples(samples);
  }
  else if (key == HdCyclesRenderSettingsTokens->sampleOffset) {
    session->params.sample_offset = VtValue::Cast<int>(value).GetWithDefault(
        session->params.sample_offset);
    ++_settingsVersion;
  }
  else {
    const std::string &keyString = key.GetString();
    if (keyString.rfind("cycles:integrator:", 0) == 0) {
      ustring socketName(keyString, sizeof("cycles:integrator:") - 1);
      if (const SocketType *socket = scene->integrator->type->find_input(socketName)) {
        SetNodeValue(scene->integrator, *socket, value);
        ++_settingsVersion;
      }
    }
  }
}

VtValue HdCyclesDelegate::GetRenderSetting(const TfToken &key) const
{
  Scene *const scene = _renderParam->session->scene;
  Session *const session = _renderParam->session;

  if (key == HdCyclesRenderSettingsTokens->stageMetersPerUnit) {
    return VtValue(_renderParam->GetStageMetersPerUnit());
  }
  else if (key == HdCyclesRenderSettingsTokens->device) {
    return VtValue(TfToken(Device::string_from_type(session->params.device.type)));
  }
  else if (key == HdCyclesRenderSettingsTokens->threads) {
    return VtValue(session->params.threads);
  }
  else if (key == HdCyclesRenderSettingsTokens->timeLimit) {
    return VtValue(session->params.time_limit);
  }
  else if (key == HdCyclesRenderSettingsTokens->samples) {
    return VtValue(session->params.samples);
  }
  else if (key == HdCyclesRenderSettingsTokens->sampleOffset) {
    return VtValue(session->params.sample_offset);
  }
  else {
    const std::string &keyString = key.GetString();
    if (keyString.rfind("cycles:integrator:", 0) == 0) {
      ustring socketName(keyString, sizeof("cycles:integrator:") - 1);
      if (const SocketType *socket = scene->integrator->type->find_input(socketName)) {
        return GetNodeValue(scene->integrator, *socket);
      }
    }
  }

  return VtValue();
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
