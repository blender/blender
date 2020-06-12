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
 *
 * Abstraction for XR (VR, AR, MR, ..) access via OpenXR.
 */

#include <cassert>
#include <sstream>
#include <string>

#include "GHOST_Types.h"
#include "GHOST_XrException.h"
#include "GHOST_XrSession.h"
#include "GHOST_Xr_intern.h"

#include "GHOST_XrContext.h"

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
 *
 * \{ */

GHOST_XrContext::GHOST_XrContext(const GHOST_XrContextCreateInfo *create_info)
    : m_oxr(new OpenXRInstanceData()),
      m_debug(create_info->context_flag & GHOST_kXrContextDebug),
      m_debug_time(create_info->context_flag & GHOST_kXrContextDebugTime)
{
}

GHOST_XrContext::~GHOST_XrContext()
{
  /* Destroy session data first. Otherwise xrDestroyInstance will implicitly do it, before the
   * session had a chance to do so explicitly. */
  m_session = nullptr;

  if (m_oxr->debug_messenger != XR_NULL_HANDLE) {
    assert(m_oxr->s_xrDestroyDebugUtilsMessengerEXT_fn != nullptr);
    m_oxr->s_xrDestroyDebugUtilsMessengerEXT_fn(m_oxr->debug_messenger);
  }
  if (m_oxr->instance != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroyInstance(m_oxr->instance));
    m_oxr->instance = XR_NULL_HANDLE;
  }
}

void GHOST_XrContext::initialize(const GHOST_XrContextCreateInfo *create_info)
{
  initApiLayers();
  initExtensions();
  if (isDebugMode()) {
    printAvailableAPILayersAndExtensionsInfo();
  }

  m_gpu_binding_type = determineGraphicsBindingTypeToEnable(create_info);

  assert(m_oxr->instance == XR_NULL_HANDLE);
  createOpenXRInstance();
  storeInstanceProperties();
  printInstanceInfo();
  if (isDebugMode()) {
    initDebugMessenger();
  }
}

void GHOST_XrContext::createOpenXRInstance()
{
  XrInstanceCreateInfo create_info = {XR_TYPE_INSTANCE_CREATE_INFO};

  std::string("Blender").copy(create_info.applicationInfo.applicationName,
                              XR_MAX_APPLICATION_NAME_SIZE);
  create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

  getAPILayersToEnable(m_enabled_layers);
  getExtensionsToEnable(m_enabled_extensions);
  create_info.enabledApiLayerCount = m_enabled_layers.size();
  create_info.enabledApiLayerNames = m_enabled_layers.data();
  create_info.enabledExtensionCount = m_enabled_extensions.size();
  create_info.enabledExtensionNames = m_enabled_extensions.data();
  if (isDebugMode()) {
    printExtensionsAndAPILayersToEnable();
  }

  CHECK_XR(xrCreateInstance(&create_info, &m_oxr->instance),
           "Failed to connect to an OpenXR runtime.");
}

void GHOST_XrContext::storeInstanceProperties()
{
  const std::map<std::string, GHOST_TXrOpenXRRuntimeID> runtime_map = {
      {"Monado(XRT) by Collabora et al", OPENXR_RUNTIME_MONADO},
      {"Oculus", OPENXR_RUNTIME_OCULUS},
      {"SteamVR/OpenXR", OPENXR_RUNTIME_STEAMVR},
      {"Windows Mixed Reality Runtime", OPENXR_RUNTIME_WMR}};
  decltype(runtime_map)::const_iterator runtime_map_iter;

  m_oxr->instance_properties.type = XR_TYPE_INSTANCE_PROPERTIES;
  CHECK_XR(xrGetInstanceProperties(m_oxr->instance, &m_oxr->instance_properties),
           "Failed to get OpenXR runtime information. Do you have an active runtime set up?");

  runtime_map_iter = runtime_map.find(m_oxr->instance_properties.runtimeName);
  if (runtime_map_iter != runtime_map.end()) {
    m_runtime_id = runtime_map_iter->second;
  }
}

/** \} */ /* Create, Initialize and Destruct */

/* -------------------------------------------------------------------- */
/** \name Debug Printing
 *
 * \{ */

void GHOST_XrContext::printInstanceInfo()
{
  assert(m_oxr->instance != XR_NULL_HANDLE);

  printf("Connected to OpenXR runtime: %s (Version %u.%u.%u)\n",
         m_oxr->instance_properties.runtimeName,
         XR_VERSION_MAJOR(m_oxr->instance_properties.runtimeVersion),
         XR_VERSION_MINOR(m_oxr->instance_properties.runtimeVersion),
         XR_VERSION_PATCH(m_oxr->instance_properties.runtimeVersion));
}

void GHOST_XrContext::printAvailableAPILayersAndExtensionsInfo()
{
  puts("Available OpenXR API-layers/extensions:");
  for (XrApiLayerProperties &layer_info : m_oxr->layers) {
    printf("Layer: %s\n", layer_info.layerName);
  }
  for (XrExtensionProperties &ext_info : m_oxr->extensions) {
    printf("Extension: %s\n", ext_info.extensionName);
  }
}

void GHOST_XrContext::printExtensionsAndAPILayersToEnable()
{
  for (const char *layer_name : m_enabled_layers) {
    printf("Enabling OpenXR API-Layer: %s\n", layer_name);
  }
  for (const char *ext_name : m_enabled_extensions) {
    printf("Enabling OpenXR Extension: %s\n", ext_name);
  }
}

static XrBool32 debug_messenger_func(XrDebugUtilsMessageSeverityFlagsEXT /*messageSeverity*/,
                                     XrDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
                                     const XrDebugUtilsMessengerCallbackDataEXT *callbackData,
                                     void * /*userData*/)
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
          m_oxr->instance,
          "xrCreateDebugUtilsMessengerEXT",
          (PFN_xrVoidFunction *)&m_oxr->s_xrCreateDebugUtilsMessengerEXT_fn)) ||
      XR_FAILED(xrGetInstanceProcAddr(
          m_oxr->instance,
          "xrDestroyDebugUtilsMessengerEXT",
          (PFN_xrVoidFunction *)&m_oxr->s_xrDestroyDebugUtilsMessengerEXT_fn))) {
    m_oxr->s_xrCreateDebugUtilsMessengerEXT_fn = nullptr;
    m_oxr->s_xrDestroyDebugUtilsMessengerEXT_fn = nullptr;

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

  if (XR_FAILED(m_oxr->s_xrCreateDebugUtilsMessengerEXT_fn(
          m_oxr->instance, &create_info, &m_oxr->debug_messenger))) {
    fprintf(stderr,
            "Failed to create OpenXR debug messenger. Not a fatal error, continuing without the "
            "messenger.\n");
    return;
  }
}

/** \} */ /* Debug Printing */

/* -------------------------------------------------------------------- */
/** \name Error handling
 *
 * \{ */

void GHOST_XrContext::dispatchErrorMessage(const GHOST_XrException *exception) const
{
  GHOST_XrError error;

  error.user_message = exception->m_msg;
  error.customdata = s_error_handler_customdata;

  if (isDebugMode()) {
    fprintf(stderr,
            "Error: \t%s\n\tOpenXR error value: %i\n",
            error.user_message,
            exception->m_result);
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
 *
 * \{ */

/**
 * \param layer_name May be NULL for extensions not belonging to a specific layer.
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
  initExtensionsEx(m_oxr->extensions, nullptr);
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

  m_oxr->layers = std::vector<XrApiLayerProperties>(layer_count);
  for (XrApiLayerProperties &layer : m_oxr->layers) {
    layer.type = XR_TYPE_API_LAYER_PROPERTIES;
  }

  /* Actually get the layers. */
  CHECK_XR(xrEnumerateApiLayerProperties(layer_count, &layer_count, m_oxr->layers.data()),
           "Failed to query OpenXR runtime information. Do you have an active runtime set up?");
  for (XrApiLayerProperties &layer : m_oxr->layers) {
    /* Each layer may have own extensions. */
    initExtensionsEx(m_oxr->extensions, layer.layerName);
  }
}

static bool openxr_layer_is_available(const std::vector<XrApiLayerProperties> layers_info,
                                      const std::string &layer_name)
{
  for (const XrApiLayerProperties &layer_info : layers_info) {
    if (layer_info.layerName == layer_name) {
      return true;
    }
  }

  return false;
}

static bool openxr_extension_is_available(const std::vector<XrExtensionProperties> extensions_info,
                                          const std::string &extension_name)
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
    if (openxr_layer_is_available(m_oxr->layers, layer)) {
      r_ext_names.push_back(layer.c_str());
    }
  }
}

static const char *openxr_ext_name_from_wm_gpu_binding(GHOST_TXrGraphicsBinding binding)
{
  switch (binding) {
    case GHOST_kXrGraphicsOpenGL:
      return XR_KHR_OPENGL_ENABLE_EXTENSION_NAME;
#ifdef WIN32
    case GHOST_kXrGraphicsD3D11:
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
void GHOST_XrContext::getExtensionsToEnable(std::vector<const char *> &r_ext_names)
{
  assert(m_gpu_binding_type != GHOST_kXrGraphicsUnknown);

  const char *gpu_binding = openxr_ext_name_from_wm_gpu_binding(m_gpu_binding_type);
  static std::vector<std::string> try_ext;

  try_ext.clear();

  /* Try enabling debug extension. */
#ifndef WIN32
  if (isDebugMode()) {
    try_ext.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
#endif

  r_ext_names.reserve(try_ext.size() + 1); /* + 1 for graphics binding extension. */

  /* Add graphics binding extension. */
  assert(gpu_binding);
  assert(openxr_extension_is_available(m_oxr->extensions, gpu_binding));
  r_ext_names.push_back(gpu_binding);

  for (const std::string &ext : try_ext) {
    if (openxr_extension_is_available(m_oxr->extensions, ext)) {
      r_ext_names.push_back(ext.c_str());
    }
  }
}

/**
 * Decide which graphics binding extension to use based on
 * #GHOST_XrContextCreateInfo.gpu_binding_candidates and available extensions.
 */
GHOST_TXrGraphicsBinding GHOST_XrContext::determineGraphicsBindingTypeToEnable(
    const GHOST_XrContextCreateInfo *create_info)
{
  assert(create_info->gpu_binding_candidates != NULL);
  assert(create_info->gpu_binding_candidates_count > 0);

  for (uint32_t i = 0; i < create_info->gpu_binding_candidates_count; i++) {
    assert(create_info->gpu_binding_candidates[i] != GHOST_kXrGraphicsUnknown);
    const char *ext_name = openxr_ext_name_from_wm_gpu_binding(
        create_info->gpu_binding_candidates[i]);
    if (openxr_extension_is_available(m_oxr->extensions, ext_name)) {
      return create_info->gpu_binding_candidates[i];
    }
  }

  return GHOST_kXrGraphicsUnknown;
}

/** \} */ /* OpenXR API-Layers and Extensions */

/* -------------------------------------------------------------------- */
/** \name Session management
 *
 * Manage session lifetime and delegate public calls to #GHOST_XrSession.
 * \{ */

void GHOST_XrContext::startSession(const GHOST_XrSessionBeginInfo *begin_info)
{
  m_custom_funcs.session_exit_fn = begin_info->exit_fn;
  m_custom_funcs.session_exit_customdata = begin_info->exit_customdata;

  if (m_session == nullptr) {
    m_session = std::unique_ptr<GHOST_XrSession>(new GHOST_XrSession(this));
  }
  m_session->start(begin_info);
}

void GHOST_XrContext::endSession()
{
  if (m_session) {
    if (m_session->isRunning()) {
      m_session->requestEnd();
    }
    else {
      m_session = nullptr;
    }
  }
}

bool GHOST_XrContext::isSessionRunning() const
{
  return m_session && m_session->isRunning();
}

void GHOST_XrContext::drawSessionViews(void *draw_customdata)
{
  m_session->draw(draw_customdata);
}

/**
 * Delegates event to session, allowing context to destruct the session if needed.
 */
void GHOST_XrContext::handleSessionStateChange(const XrEventDataSessionStateChanged *lifecycle)
{
  if (m_session &&
      m_session->handleStateChangeEvent(lifecycle) == GHOST_XrSession::SESSION_DESTROY) {
    m_session = nullptr;
  }
}

/** \} */ /* Session Management */

/* -------------------------------------------------------------------- */
/** \name Public Accessors and Mutators
 *
 * Public as in, exposed in the Ghost API.
 * \{ */

void GHOST_XrContext::setGraphicsContextBindFuncs(GHOST_XrGraphicsContextBindFn bind_fn,
                                                  GHOST_XrGraphicsContextUnbindFn unbind_fn)
{
  if (m_session) {
    m_session->unbindGraphicsContext();
  }
  m_custom_funcs.gpu_ctx_bind_fn = bind_fn;
  m_custom_funcs.gpu_ctx_unbind_fn = unbind_fn;
}

void GHOST_XrContext::setDrawViewFunc(GHOST_XrDrawViewFn draw_view_fn)
{
  m_custom_funcs.draw_view_fn = draw_view_fn;
}

bool GHOST_XrContext::needsUpsideDownDrawing() const
{
  /* Must only be called after the session was started */
  assert(m_session);
  return m_session->needsUpsideDownDrawing();
}

/** \} */ /* Public Accessors and Mutators */

/* -------------------------------------------------------------------- */
/** \name Ghost Internal Accessors and Mutators
 *
 * \{ */

GHOST_TXrOpenXRRuntimeID GHOST_XrContext::getOpenXRRuntimeID() const
{
  return m_runtime_id;
}

const GHOST_XrCustomFuncs &GHOST_XrContext::getCustomFuncs() const
{
  return m_custom_funcs;
}

GHOST_TXrGraphicsBinding GHOST_XrContext::getGraphicsBindingType() const
{
  return m_gpu_binding_type;
}

XrInstance GHOST_XrContext::getInstance() const
{
  return m_oxr->instance;
}

bool GHOST_XrContext::isDebugMode() const
{
  return m_debug;
}

bool GHOST_XrContext::isDebugTimeMode() const
{
  return m_debug_time;
}

/** \} */ /* Ghost Internal Accessors and Mutators */
