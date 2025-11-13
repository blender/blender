/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Abstraction for XR (VR, AR, MR, ..) access via OpenXR.
 */

#include <algorithm>
#include <cassert>
#include <sstream>
#include <string>
#include <string_view>

#include "GHOST_Types.h"
#include "GHOST_XrException.hh"
#include "GHOST_XrSession.hh"
#include "GHOST_Xr_intern.hh"

#include "GHOST_XrContext.hh"

struct OpenXRInstanceData {
  XrInstance instance = XR_NULL_HANDLE;
  XrInstanceProperties instance_properties = {};

  std::vector<XrExtensionProperties> extensions;
  std::vector<XrApiLayerProperties> layers;

  static PFN_xrCreateDebugUtilsMessengerEXT s_xrCreateDebugUtilsMessengerEXT_fn;
  static PFN_xrDestroyDebugUtilsMessengerEXT s_xrDestroyDebugUtilsMessengerEXT_fn;

  XrDebugUtilsMessengerEXT debug_messenger = XR_NULL_HANDLE;
};

PFN_xrCreateDebugUtilsMessengerEXT OpenXRInstanceData::s_xrCreateDebugUtilsMessengerEXT_fn =
    nullptr;
PFN_xrDestroyDebugUtilsMessengerEXT OpenXRInstanceData::s_xrDestroyDebugUtilsMessengerEXT_fn =
    nullptr;

GHOST_XrErrorHandlerFn GHOST_XrContext::s_error_handler = nullptr;
void *GHOST_XrContext::s_error_handler_customdata = nullptr;

/* -------------------------------------------------------------------- */
/** \name Create, Initialize and Destruct
 * \{ */

GHOST_XrContext::GHOST_XrContext(const GHOST_XrContextCreateInfo *create_info)
    : oxr_(std::make_unique<OpenXRInstanceData>()),
      debug_(create_info->context_flag & GHOST_kXrContextDebug),
      debug_time_(create_info->context_flag & GHOST_kXrContextDebugTime)
{
}

GHOST_XrContext::~GHOST_XrContext()
{
  /* Destroy session data first. Otherwise xrDestroyInstance will implicitly do it, before the
   * session had a chance to do so explicitly. */
  session_ = nullptr;

  if (oxr_->debug_messenger != XR_NULL_HANDLE) {
    assert(oxr_->s_xrDestroyDebugUtilsMessengerEXT_fn != nullptr);
    oxr_->s_xrDestroyDebugUtilsMessengerEXT_fn(oxr_->debug_messenger);
  }
  if (oxr_->instance != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroyInstance(oxr_->instance));
    oxr_->instance = XR_NULL_HANDLE;
  }
}

void GHOST_XrContext::initialize(const GHOST_XrContextCreateInfo *create_info)
{
  initApiLayers();
  initExtensions();
  if (isDebugMode()) {
    printSDKVersion();
    printAvailableAPILayersAndExtensionsInfo();
  }

  /* Multiple graphics binding extensions can be enabled, but only one will actually be used
   * (determined later on). */
  const std::vector<GHOST_TXrGraphicsBinding> graphics_binding_types =
      determineGraphicsBindingTypesToEnable(create_info);

  assert(oxr_->instance == XR_NULL_HANDLE);
  createOpenXRInstance(graphics_binding_types);
  storeInstanceProperties();

  /* Multiple bindings may be enabled. Now that we know the runtime in use, settle for one. */
  gpu_binding_type_ = determineGraphicsBindingTypeToUse(graphics_binding_types, create_info);

  printInstanceInfo();
  if (isDebugMode()) {
    initDebugMessenger();
  }
}

void GHOST_XrContext::createOpenXRInstance(
    const std::vector<GHOST_TXrGraphicsBinding> &graphics_binding_types)
{
  XrInstanceCreateInfo create_info = {XR_TYPE_INSTANCE_CREATE_INFO};

  std::string("Blender").copy(create_info.applicationInfo.applicationName,
                              XR_MAX_APPLICATION_NAME_SIZE);
  create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

  getAPILayersToEnable(enabled_layers_);
  getExtensionsToEnable(graphics_binding_types, enabled_extensions_);
  create_info.enabledApiLayerCount = enabled_layers_.size();
  create_info.enabledApiLayerNames = enabled_layers_.data();
  create_info.enabledExtensionCount = enabled_extensions_.size();
  create_info.enabledExtensionNames = enabled_extensions_.data();
  if (isDebugMode()) {
    printExtensionsAndAPILayersToEnable();
  }

  CHECK_XR(xrCreateInstance(&create_info, &oxr_->instance),
           "Failed to connect to an OpenXR runtime.");
}

void GHOST_XrContext::storeInstanceProperties()
{
  const std::map<std::string, GHOST_TXrOpenXRRuntimeID> runtime_map = {
      {"Monado(XRT) by Collabora et al", OPENXR_RUNTIME_MONADO},
      {"Oculus", OPENXR_RUNTIME_OCULUS},
      {"SteamVR/OpenXR", OPENXR_RUNTIME_STEAMVR},
      {"Windows Mixed Reality Runtime", OPENXR_RUNTIME_WMR},
      {"Varjo OpenXR Runtime", OPENXR_RUNTIME_VARJO}};
  decltype(runtime_map)::const_iterator runtime_map_iter;

  oxr_->instance_properties.type = XR_TYPE_INSTANCE_PROPERTIES;
  CHECK_XR(xrGetInstanceProperties(oxr_->instance, &oxr_->instance_properties),
           "Failed to get OpenXR runtime information. Do you have an active runtime set up?");

  runtime_map_iter = runtime_map.find(oxr_->instance_properties.runtimeName);
  if (runtime_map_iter != runtime_map.end()) {
    runtime_id_ = runtime_map_iter->second;
  }
}

/** \} */ /* Create, Initialize and Destruct */

/* -------------------------------------------------------------------- */
/** \name Debug Printing
 * \{ */

void GHOST_XrContext::printSDKVersion()
{
  const XrVersion sdk_version = XR_CURRENT_API_VERSION;

  printf("OpenXR SDK Version: %u.%u.%u\n",
         XR_VERSION_MAJOR(sdk_version),
         XR_VERSION_MINOR(sdk_version),
         XR_VERSION_PATCH(sdk_version));
}

void GHOST_XrContext::printInstanceInfo()
{
  assert(oxr_->instance != XR_NULL_HANDLE);

  printf("Connected to OpenXR runtime: %s (Version %u.%u.%u)\n",
         oxr_->instance_properties.runtimeName,
         XR_VERSION_MAJOR(oxr_->instance_properties.runtimeVersion),
         XR_VERSION_MINOR(oxr_->instance_properties.runtimeVersion),
         XR_VERSION_PATCH(oxr_->instance_properties.runtimeVersion));
}

void GHOST_XrContext::printAvailableAPILayersAndExtensionsInfo()
{
  puts("Available OpenXR API-layers/extensions:");
  for (const XrApiLayerProperties &layer_info : oxr_->layers) {
    printf("Layer: %s\n", layer_info.layerName);
  }
  for (const XrExtensionProperties &ext_info : oxr_->extensions) {
    printf("Extension: %s\n", ext_info.extensionName);
  }
}

void GHOST_XrContext::printExtensionsAndAPILayersToEnable()
{
  for (const char *layer_name : enabled_layers_) {
    printf("Enabling OpenXR API-Layer: %s\n", layer_name);
  }
  for (const char *ext_name : enabled_extensions_) {
    printf("Enabling OpenXR Extension: %s\n", ext_name);
  }
}

static XrBool32 debug_messenger_func(XrDebugUtilsMessageSeverityFlagsEXT /*messageSeverity*/,
                                     XrDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
                                     const XrDebugUtilsMessengerCallbackDataEXT *callbackData,
                                     void * /*user_data*/)
{
  puts("OpenXR Debug Message:");
  puts(callbackData->message);
  return XR_FALSE; /* OpenXR spec suggests always returning false. */
}

void GHOST_XrContext::initDebugMessenger()
{
  XrDebugUtilsMessengerCreateInfoEXT create_info = {XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};

  /* Extension functions need to be obtained through xrGetInstanceProcAddr(). */
  if (XR_FAILED(xrGetInstanceProcAddr(
          oxr_->instance,
          "xrCreateDebugUtilsMessengerEXT",
          (PFN_xrVoidFunction *)&oxr_->s_xrCreateDebugUtilsMessengerEXT_fn)) ||
      XR_FAILED(xrGetInstanceProcAddr(
          oxr_->instance,
          "xrDestroyDebugUtilsMessengerEXT",
          (PFN_xrVoidFunction *)&oxr_->s_xrDestroyDebugUtilsMessengerEXT_fn)))
  {
    oxr_->s_xrCreateDebugUtilsMessengerEXT_fn = nullptr;
    oxr_->s_xrDestroyDebugUtilsMessengerEXT_fn = nullptr;

    fprintf(stderr,
            "Could not use XR_EXT_debug_utils to enable debug prints. Not a fatal error, "
            "continuing without the messenger.\n");
    return;
  }

  create_info.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                  XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                  XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.userCallback = debug_messenger_func;

  if (XR_FAILED(oxr_->s_xrCreateDebugUtilsMessengerEXT_fn(
          oxr_->instance, &create_info, &oxr_->debug_messenger)))
  {
    fprintf(stderr,
            "Failed to create OpenXR debug messenger. Not a fatal error, continuing without the "
            "messenger.\n");
    return;
  }
}

/** \} */ /* Debug Printing */

/* -------------------------------------------------------------------- */
/** \name Error handling
 * \{ */

void GHOST_XrContext::dispatchErrorMessage(const GHOST_XrException *exception) const
{
  GHOST_XrError error;

  error.user_message = exception->msg_.data();
  error.customdata = s_error_handler_customdata;

  char error_string_buf[XR_MAX_RESULT_STRING_SIZE];
  xrResultToString(getInstance(), static_cast<XrResult>(exception->result_), error_string_buf);

  if (isDebugMode()) {
    fprintf(stderr,
            "Error: \t%s\n\tOpenXR error: %s (error value: %i)\n",
            error.user_message,
            error_string_buf,
            exception->result_);
  }

  /* Potentially destroys GHOST_XrContext */
  s_error_handler(&error);
}

void GHOST_XrContext::setErrorHandler(GHOST_XrErrorHandlerFn handler_fn, void *customdata)
{
  s_error_handler = handler_fn;
  s_error_handler_customdata = customdata;
}

/** \} */ /* Error handling */

/* -------------------------------------------------------------------- */
/** \name OpenXR API-Layers and Extensions
 * \{ */

/**
 * \param layer_name: May be nullptr for extensions not belonging to a specific layer.
 */
void GHOST_XrContext::initExtensionsEx(std::vector<XrExtensionProperties> &extensions,
                                       const char *layer_name)
{
  uint32_t extension_count = 0;

  /* Get count for array creation/init first. */
  CHECK_XR(xrEnumerateInstanceExtensionProperties(layer_name, 0, &extension_count, nullptr),
           "Failed to query OpenXR runtime information. Do you have an active runtime set up?");

  if (extension_count == 0) {
    /* Extensions are optional, can successfully exit. */
    return;
  }

  for (uint32_t i = 0; i < extension_count; i++) {
    XrExtensionProperties ext = {XR_TYPE_EXTENSION_PROPERTIES};
    extensions.push_back(ext);
  }

  /* Actually get the extensions. */
  CHECK_XR(xrEnumerateInstanceExtensionProperties(
               layer_name, extension_count, &extension_count, extensions.data()),
           "Failed to query OpenXR runtime information. Do you have an active runtime set up?");
}

void GHOST_XrContext::initExtensions()
{
  initExtensionsEx(oxr_->extensions, nullptr);
}

void GHOST_XrContext::initApiLayers()
{
  uint32_t layer_count = 0;

  /* Get count for array creation/init first. */
  CHECK_XR(xrEnumerateApiLayerProperties(0, &layer_count, nullptr),
           "Failed to query OpenXR runtime information. Do you have an active runtime set up?");

  if (layer_count == 0) {
    /* Layers are optional, can safely exit. */
    return;
  }

  oxr_->layers = std::vector<XrApiLayerProperties>(layer_count);
  for (XrApiLayerProperties &layer : oxr_->layers) {
    layer.type = XR_TYPE_API_LAYER_PROPERTIES;
  }

  /* Actually get the layers. */
  CHECK_XR(xrEnumerateApiLayerProperties(layer_count, &layer_count, oxr_->layers.data()),
           "Failed to query OpenXR runtime information. Do you have an active runtime set up?");
  for (const XrApiLayerProperties &layer : oxr_->layers) {
    /* Each layer may have own extensions. */
    initExtensionsEx(oxr_->extensions, layer.layerName);
  }
}

static bool openxr_layer_is_available(const std::vector<XrApiLayerProperties> &layers_info,
                                      const std::string &layer_name)
{
  for (const XrApiLayerProperties &layer_info : layers_info) {
    if (layer_info.layerName == layer_name) {
      return true;
    }
  }

  return false;
}

static bool openxr_extension_is_available(
    const std::vector<XrExtensionProperties> &extensions_info,
    const std::string_view &extension_name)
{
  for (const XrExtensionProperties &ext_info : extensions_info) {
    if (ext_info.extensionName == extension_name) {
      return true;
    }
  }

  return false;
}

/**
 * Gather an array of names for the API-layers to enable.
 */
void GHOST_XrContext::getAPILayersToEnable(std::vector<const char *> &r_ext_names)
{
  static std::vector<std::string> try_layers;

  try_layers.clear();

  if (isDebugMode()) {
    try_layers.push_back("XR_APILAYER_LUNARG_core_validation");
  }

  r_ext_names.reserve(try_layers.size());

  for (const std::string &layer : try_layers) {
    if (openxr_layer_is_available(oxr_->layers, layer)) {
      r_ext_names.push_back(layer.data());
    }
  }
}

static const char *openxr_ext_name_from_wm_gpu_binding(GHOST_TXrGraphicsBinding binding)
{
  switch (binding) {
    case GHOST_kXrGraphicsOpenGL:
#ifdef WITH_OPENGL_BACKEND
      return XR_KHR_OPENGL_ENABLE_EXTENSION_NAME;
#else
      return nullptr;
#endif

    case GHOST_kXrGraphicsVulkan:
#ifdef WITH_VULKAN_BACKEND
      return XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
#else
      return nullptr;
#endif

    case GHOST_kXrGraphicsMetal:
#ifdef WITH_METAL_BACKEND
      return XR_KHR_METAL_ENABLE_EXTENSION_NAME;
#else
      return nullptr;
#endif

#ifdef WIN32
#  ifdef WITH_OPENGL_BACKEND
    case GHOST_kXrGraphicsOpenGLD3D11:
#  endif
#  ifdef WITH_VULKAN_BACKEND
    case GHOST_kXrGraphicsVulkanD3D11:
#  endif
      return XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
#endif
    case GHOST_kXrGraphicsUnknown:
      assert(!"Could not identify graphics binding to choose.");
      return nullptr;
  }

  return nullptr;
}

/**
 * Gather an array of names for the extensions to enable.
 */
void GHOST_XrContext::getExtensionsToEnable(
    const std::vector<GHOST_TXrGraphicsBinding> &graphics_binding_types,
    std::vector<const char *> &r_ext_names)
{
  std::vector<std::string_view> try_ext;

  /* Try enabling debug extension. */
  if (isDebugMode()) {
    try_ext.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  /* Interaction profile extensions. */
  try_ext.push_back(XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME);
  try_ext.push_back(XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME);
#ifdef XR_HTC_VIVE_FOCUS3_CONTROLLER_INTERACTION_EXTENSION_NAME
  try_ext.push_back(XR_HTC_VIVE_FOCUS3_CONTROLLER_INTERACTION_EXTENSION_NAME);
#endif
  try_ext.push_back(XR_HUAWEI_CONTROLLER_INTERACTION_EXTENSION_NAME);

  /* Controller model extension. */
  try_ext.push_back(XR_MSFT_CONTROLLER_MODEL_EXTENSION_NAME);

  /* Varjo quad view extension. */
  try_ext.push_back(XR_VARJO_QUAD_VIEWS_EXTENSION_NAME);

  /* Varjo foveated extension. */
  try_ext.push_back(XR_VARJO_FOVEATED_RENDERING_EXTENSION_NAME);

  /* Meta/Facebook passthrough extension. */
  try_ext.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);

  r_ext_names.reserve(try_ext.size() + graphics_binding_types.size());

  /* Add graphics binding extensions (may be multiple ones, we'll settle for one to use later, once
   * we have more info about the runtime). */
  for (GHOST_TXrGraphicsBinding type : graphics_binding_types) {
    const char *gpu_binding = openxr_ext_name_from_wm_gpu_binding(type);
    assert(openxr_extension_is_available(oxr_->extensions, gpu_binding));
    r_ext_names.push_back(gpu_binding);
  }

#if defined(WITH_GHOST_X11)
  if (openxr_extension_is_available(oxr_->extensions, XR_MNDX_EGL_ENABLE_EXTENSION_NAME)) {
    /* Use EGL if that backend is available. */
    r_ext_names.push_back(XR_MNDX_EGL_ENABLE_EXTENSION_NAME);
  }
#endif

  for (const std::string_view &ext : try_ext) {
    if (openxr_extension_is_available(oxr_->extensions, ext)) {
      r_ext_names.push_back(ext.data());
    }
  }
}

/**
 * Decide which graphics binding extension to use based on
 * #GHOST_XrContextCreateInfo.gpu_binding_candidates and available extensions.
 */
std::vector<GHOST_TXrGraphicsBinding> GHOST_XrContext::determineGraphicsBindingTypesToEnable(
    const GHOST_XrContextCreateInfo *create_info)
{
  std::vector<GHOST_TXrGraphicsBinding> result;
  assert(create_info->gpu_binding_candidates != nullptr);
  assert(create_info->gpu_binding_candidates_count > 0);

  for (uint32_t i = 0; i < create_info->gpu_binding_candidates_count; i++) {
    assert(create_info->gpu_binding_candidates[i] != GHOST_kXrGraphicsUnknown);
    const char *ext_name = openxr_ext_name_from_wm_gpu_binding(
        create_info->gpu_binding_candidates[i]);
    if (openxr_extension_is_available(oxr_->extensions, ext_name)) {
      result.push_back(create_info->gpu_binding_candidates[i]);
    }
  }

  if (result.empty()) {
    throw GHOST_XrException("No supported graphics binding found.");
  }

  return result;
}

GHOST_TXrGraphicsBinding GHOST_XrContext::determineGraphicsBindingTypeToUse(
    const std::vector<GHOST_TXrGraphicsBinding> &enabled_types,
    const GHOST_XrContextCreateInfo *create_info)
{
  /* Return the first working type. */
  for (GHOST_TXrGraphicsBinding type : enabled_types) {
#ifdef WIN32
    /* The SteamVR OpenGL backend currently fails for NVIDIA GPU's. Disable it and allow falling
     * back to the DirectX one. */
    if ((runtime_id_ == OPENXR_RUNTIME_STEAMVR) && (type == GHOST_kXrGraphicsOpenGL) &&
        ((create_info->context_flag & GHOST_kXrContextGpuNVIDIA) != 0))
    {
      continue;
    }
#else
    ((void)create_info);
#endif

    assert(type != GHOST_kXrGraphicsUnknown);
    return type;
  }

  throw GHOST_XrException("Failed to determine a graphics binding to use.");
}

/** \} */ /* OpenXR API-Layers and Extensions */

/* -------------------------------------------------------------------- */
/** \name Session management
 *
 * Manage session lifetime and delegate public calls to #GHOST_XrSession.
 * \{ */

void GHOST_XrContext::startSession(const GHOST_XrSessionBeginInfo *begin_info)
{
  custom_funcs_.session_create_fn = begin_info->create_fn;
  custom_funcs_.session_exit_fn = begin_info->exit_fn;
  custom_funcs_.session_exit_customdata = begin_info->exit_customdata;

  if (session_ == nullptr) {
    session_ = std::make_unique<GHOST_XrSession>(*this);
  }
  session_->start(begin_info);
}

void GHOST_XrContext::endSession()
{
  if (session_) {
    if (session_->isRunning()) {
      session_->requestEnd();
    }
    else {
      session_ = nullptr;
    }
  }
}

bool GHOST_XrContext::isSessionRunning() const
{
  return session_ && session_->isRunning();
}

void GHOST_XrContext::drawSessionViews(void *draw_customdata)
{
  session_->draw(draw_customdata);
}

/**
 * Delegates event to session, allowing context to destruct the session if needed.
 */
void GHOST_XrContext::handleSessionStateChange(const XrEventDataSessionStateChanged &lifecycle)
{
  if (session_ && session_->handleStateChangeEvent(lifecycle) == GHOST_XrSession::SESSION_DESTROY)
  {
    session_ = nullptr;
  }
}

/** \} */ /* Session Management */

/* -------------------------------------------------------------------- */
/** \name Public Accessors and Mutators
 *
 * Public as in, exposed in the Ghost API.
 * \{ */

GHOST_XrSession *GHOST_XrContext::getSession()
{
  return session_.get();
}

const GHOST_XrSession *GHOST_XrContext::getSession() const
{
  return session_.get();
}

void GHOST_XrContext::setGraphicsContextBindFuncs(GHOST_XrGraphicsContextBindFn bind_fn,
                                                  GHOST_XrGraphicsContextUnbindFn unbind_fn)
{
  if (session_) {
    session_->unbindGraphicsContext();
  }
  custom_funcs_.gpu_ctx_bind_fn = bind_fn;
  custom_funcs_.gpu_ctx_unbind_fn = unbind_fn;
}

void GHOST_XrContext::setDrawViewFunc(GHOST_XrDrawViewFn draw_view_fn)
{
  custom_funcs_.draw_view_fn = draw_view_fn;
}

void GHOST_XrContext::setPassthroughEnabledFunc(
    GHOST_XrPassthroughEnabledFn passthrough_enabled_fn)
{
  custom_funcs_.passthrough_enabled_fn = passthrough_enabled_fn;
}

void GHOST_XrContext::setDisablePassthroughFunc(
    GHOST_XrDisablePassthroughFn disable_passthrough_fn)
{
  custom_funcs_.disable_passthrough_fn = disable_passthrough_fn;
}

bool GHOST_XrContext::needsUpsideDownDrawing() const
{
  /* Must only be called after the session was started */
  assert(session_);
  return session_->needsUpsideDownDrawing();
}

/** \} */ /* Public Accessors and Mutators */

/* -------------------------------------------------------------------- */
/** \name Ghost Internal Accessors and Mutators
 * \{ */

GHOST_TXrOpenXRRuntimeID GHOST_XrContext::getOpenXRRuntimeID() const
{
  return runtime_id_;
}

const GHOST_XrCustomFuncs &GHOST_XrContext::getCustomFuncs() const
{
  return custom_funcs_;
}

GHOST_TXrGraphicsBinding GHOST_XrContext::getGraphicsBindingType() const
{
  return gpu_binding_type_;
}

XrInstance GHOST_XrContext::getInstance() const
{
  return oxr_->instance;
}

bool GHOST_XrContext::isDebugMode() const
{
  return debug_;
}

bool GHOST_XrContext::isDebugTimeMode() const
{
  return debug_time_;
}

bool GHOST_XrContext::isExtensionEnabled(const char *ext) const
{
  bool contains = std::find(enabled_extensions_.begin(), enabled_extensions_.end(), ext) !=
                  enabled_extensions_.end();
  return contains;
}

/** \} */ /* Ghost Internal Accessors and Mutators */
