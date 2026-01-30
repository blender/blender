/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <cmath>

#include "GHOST_Debug.hh"
#include "GHOST_TrackpadWin32.hh"

GHOST_DirectManipulationHelper::GHOST_DirectManipulationHelper(
    HWND hWnd,
    Microsoft::WRL::ComPtr<IDirectManipulationManager> directManipulationManager,
    Microsoft::WRL::ComPtr<IDirectManipulationUpdateManager> directManipulationUpdateManager,
    Microsoft::WRL::ComPtr<IDirectManipulationViewport> directManipulationViewport,
    Microsoft::WRL::ComPtr<GHOST_DirectManipulationViewportEventHandler>
        directManipulationEventHandler,
    DWORD directManipulationViewportHandlerCookie,
    bool isScrollDirectionInverted)
    : h_wnd_(hWnd),
      scroll_direction_reg_key_(nullptr),
      scroll_direction_change_event_(nullptr),
      direct_manipulation_manager_(directManipulationManager),
      direct_manipulation_update_manager_(directManipulationUpdateManager),
      direct_manipulation_viewport_(directManipulationViewport),
      direct_manipulation_event_handler_(directManipulationEventHandler),
      direct_manipulation_viewport_handler_cookie_(directManipulationViewportHandlerCookie),
      is_scroll_direction_inverted_(isScrollDirectionInverted)
{
}

GHOST_DirectManipulationHelper *GHOST_DirectManipulationHelper::create(HWND hWnd, uint16_t dpi)
{
#define DM_CHECK_RESULT_AND_EXIT_EARLY(hr, failMessage) \
  { \
    if (!SUCCEEDED(hr)) { \
      GHOST_PRINT(failMessage); \
      return nullptr; \
    } \
  }

  Microsoft::WRL::ComPtr<IDirectManipulationManager> directManipulationManager;
  HRESULT hr = ::CoCreateInstance(CLSID_DirectManipulationManager,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&directManipulationManager));
  DM_CHECK_RESULT_AND_EXIT_EARLY(hr, "DirectManipulationManager create failed\n");

  /* Since we want to use fake viewport, we need to send fake updates to UpdateManager. */
  Microsoft::WRL::ComPtr<IDirectManipulationUpdateManager> directManipulationUpdateManager;
  hr = directManipulationManager->GetUpdateManager(IID_PPV_ARGS(&directManipulationUpdateManager));
  DM_CHECK_RESULT_AND_EXIT_EARLY(hr, "Get UpdateManager failed\n");

  Microsoft::WRL::ComPtr<IDirectManipulationViewport> directManipulationViewport;
  hr = directManipulationManager->CreateViewport(
      nullptr, hWnd, IID_PPV_ARGS(&directManipulationViewport));
  DM_CHECK_RESULT_AND_EXIT_EARLY(hr, "Viewport create failed\n");

  DIRECTMANIPULATION_CONFIGURATION configuration =
      DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA |
      DIRECTMANIPULATION_CONFIGURATION_SCALING;

  hr = directManipulationViewport->ActivateConfiguration(configuration);
  DM_CHECK_RESULT_AND_EXIT_EARLY(hr, "Viewport set ActivateConfiguration failed\n");

  /* Since we are using fake viewport and only want to use Direct Manipulation for touchpad, we
   * need to use MANUALUPDATE option. */
  hr = directManipulationViewport->SetViewportOptions(
      DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE);
  DM_CHECK_RESULT_AND_EXIT_EARLY(hr, "Viewport set ViewportOptions failed\n");

  /* We receive Direct Manipulation transform updates in IDirectManipulationViewportEventHandler
   * callbacks. */
  Microsoft::WRL::ComPtr<GHOST_DirectManipulationViewportEventHandler>
      directManipulationEventHandler =
          Microsoft::WRL::Make<GHOST_DirectManipulationViewportEventHandler>(dpi);
  DWORD directManipulationViewportHandlerCookie;
  directManipulationViewport->AddEventHandler(
      hWnd, directManipulationEventHandler.Get(), &directManipulationViewportHandlerCookie);
  DM_CHECK_RESULT_AND_EXIT_EARLY(hr, "Viewport add EventHandler failed\n");

  /* Set default rect for viewport before activating. */
  RECT rect = {0, 0, 10000, 10000};
  hr = directManipulationViewport->SetViewportRect(&rect);
  DM_CHECK_RESULT_AND_EXIT_EARLY(hr, "Viewport set rect failed\n");

  hr = directManipulationManager->Activate(hWnd);
  DM_CHECK_RESULT_AND_EXIT_EARLY(hr, "DirectManipulationManager activate failed\n");

  hr = directManipulationViewport->Enable();
  DM_CHECK_RESULT_AND_EXIT_EARLY(hr, "Viewport enable failed\n");

  directManipulationEventHandler->resetViewport(directManipulationViewport.Get());

  bool isScrollDirectionInverted = getScrollDirectionFromReg();

  auto instance = new GHOST_DirectManipulationHelper(hWnd,
                                                     directManipulationManager,
                                                     directManipulationUpdateManager,
                                                     directManipulationViewport,
                                                     directManipulationEventHandler,
                                                     directManipulationViewportHandlerCookie,
                                                     isScrollDirectionInverted);

  instance->registerScrollDirectionChangeListener();

  return instance;

#undef DM_CHECK_RESULT_AND_EXIT_EARLY
}

bool GHOST_DirectManipulationHelper::getScrollDirectionFromReg()
{
  DWORD scrollDirectionRegValue, pcbData;
  HRESULT hr = HRESULT_FROM_WIN32(
      RegGetValueW(HKEY_CURRENT_USER,
                   L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PrecisionTouchPad\\",
                   L"ScrollDirection",
                   RRF_RT_REG_DWORD,
                   nullptr,
                   &scrollDirectionRegValue,
                   &pcbData));
  if (!SUCCEEDED(hr)) {
    GHOST_PRINT("Failed to get scroll direction from registry\n");
    return false;
  }

  return scrollDirectionRegValue == 0;
}

void GHOST_DirectManipulationHelper::registerScrollDirectionChangeListener()
{

  if (!scroll_direction_reg_key_) {
    HRESULT hr = HRESULT_FROM_WIN32(
        RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PrecisionTouchPad\\",
                      0,
                      KEY_NOTIFY,
                      &scroll_direction_reg_key_));
    if (!SUCCEEDED(hr)) {
      GHOST_PRINT("Failed to open scroll direction registry key\n");
      return;
    }
  }

  if (!scroll_direction_change_event_) {
    scroll_direction_change_event_ = CreateEventW(nullptr, true, false, nullptr);
  }
  else {
    ResetEvent(scroll_direction_change_event_);
  }
  HRESULT hr = HRESULT_FROM_WIN32(RegNotifyChangeKeyValue(scroll_direction_reg_key_,
                                                          true,
                                                          REG_NOTIFY_CHANGE_LAST_SET,
                                                          scroll_direction_change_event_,
                                                          true));
  if (!SUCCEEDED(hr)) {
    GHOST_PRINT("Failed to register scroll direction change listener\n");
    return;
  }
}

void GHOST_DirectManipulationHelper::onPointerHitTest(UINT32 pointerId)
{
  [[maybe_unused]] HRESULT hr = direct_manipulation_viewport_->SetContact(pointerId);
  GHOST_ASSERT(SUCCEEDED(hr), "Viewport set contact failed\n");

  if (WaitForSingleObject(scroll_direction_change_event_, 0) == WAIT_OBJECT_0) {
    is_scroll_direction_inverted_ = getScrollDirectionFromReg();
    registerScrollDirectionChangeListener();
  }
}

void GHOST_DirectManipulationHelper::update()
{
  if (direct_manipulation_event_handler_->dm_status == DIRECTMANIPULATION_RUNNING ||
      direct_manipulation_event_handler_->dm_status == DIRECTMANIPULATION_INERTIA)
  {
    [[maybe_unused]] HRESULT hr = direct_manipulation_update_manager_->Update(nullptr);
    GHOST_ASSERT(SUCCEEDED(hr), "DirectManipulationUpdateManager update failed\n");
  }
}

void GHOST_DirectManipulationHelper::setDPI(uint16_t dpi)
{
  direct_manipulation_event_handler_->dpi = dpi;
}

GHOST_TTrackpadInfo GHOST_DirectManipulationHelper::getTrackpadInfo()
{
  GHOST_TTrackpadInfo result = direct_manipulation_event_handler_->accumulated_values;
  result.isScrollDirectionInverted = is_scroll_direction_inverted_;

  direct_manipulation_event_handler_->accumulated_values = {0, 0, 0};
  return result;
}

GHOST_DirectManipulationHelper::~GHOST_DirectManipulationHelper()
{
  [[maybe_unused]] HRESULT hr;
  hr = direct_manipulation_viewport_->Stop();
  GHOST_ASSERT(SUCCEEDED(hr), "Viewport stop failed\n");

  hr = direct_manipulation_viewport_->RemoveEventHandler(
      direct_manipulation_viewport_handler_cookie_);
  GHOST_ASSERT(SUCCEEDED(hr), "Viewport remove event handler failed\n");

  hr = direct_manipulation_viewport_->Abandon();
  GHOST_ASSERT(SUCCEEDED(hr), "Viewport abandon failed\n");

  hr = direct_manipulation_manager_->Deactivate(h_wnd_);
  GHOST_ASSERT(SUCCEEDED(hr), "DirectManipulationManager deactivate failed\n");

  if (scroll_direction_change_event_) {
    CloseHandle(scroll_direction_change_event_);
    scroll_direction_change_event_ = nullptr;
  }
  if (scroll_direction_reg_key_) {
    RegCloseKey(scroll_direction_reg_key_);
    scroll_direction_reg_key_ = nullptr;
  }
}

GHOST_DirectManipulationViewportEventHandler::GHOST_DirectManipulationViewportEventHandler(
    uint16_t dpi)
    : accumulated_values({0, 0, 0}), dpi(dpi), dm_status(DIRECTMANIPULATION_BUILDING)
{
}

void GHOST_DirectManipulationViewportEventHandler::resetViewport(
    IDirectManipulationViewport *viewport)
{
  if (gesture_state != GESTURE_NONE) {
    [[maybe_unused]] HRESULT hr = viewport->ZoomToRect(0.0f, 0.0f, 10000.0f, 10000.0f, FALSE);
    GHOST_ASSERT(SUCCEEDED(hr), "Viewport reset failed\n");
  }

  gesture_state = GESTURE_NONE;

  last_scale = PINCH_SCALE_FACTOR;
  last_x = 0.0f;
  last_y = 0.0f;
}

HRESULT GHOST_DirectManipulationViewportEventHandler::OnViewportStatusChanged(
    IDirectManipulationViewport *viewport,
    DIRECTMANIPULATION_STATUS current,
    DIRECTMANIPULATION_STATUS previous)
{
  dm_status = current;

  if (current == previous) {
    return S_OK;
  }

  if (previous == DIRECTMANIPULATION_ENABLED || current == DIRECTMANIPULATION_READY ||
      (previous == DIRECTMANIPULATION_INERTIA && current != DIRECTMANIPULATION_INERTIA))
  {
    resetViewport(viewport);
  }

  return S_OK;
}

HRESULT GHOST_DirectManipulationViewportEventHandler::OnViewportUpdated(
    IDirectManipulationViewport * /*viewport*/)
{
  /* Nothing to do here. */
  return S_OK;
}

HRESULT GHOST_DirectManipulationViewportEventHandler::OnContentUpdated(
    IDirectManipulationViewport * /*viewport*/, IDirectManipulationContent *content)
{
  float transform[6];
  HRESULT hr = content->GetContentTransform(transform, ARRAYSIZE(transform));
  GHOST_ASSERT(SUCCEEDED(hr), "DirectManipulationContent get transform failed\n");

  const float device_scale_factor = dpi / 96.0f;

  const float scale = transform[0] * PINCH_SCALE_FACTOR;
  const float x = transform[4] / device_scale_factor;
  const float y = transform[5] / device_scale_factor;

  const float EPS = 3e-5;

  /* Ignore repeating or incorrect input. */
  if ((fabs(scale - last_scale) <= EPS && fabs(x - last_x) <= EPS && fabs(y - last_y) <= EPS) ||
      scale == 0.0f)
  {
    GHOST_PRINT("Ignoring touchpad input\n");
    return hr;
  }

  /* Assume that every gesture is a pan in the beginning.
   * If it's a pinch, the gesture will be changed below. */
  if (gesture_state == GESTURE_NONE) {
    gesture_state = GESTURE_PAN;
  }

  /* DM doesn't always immediately recognize pinch gestures,
   * so allow transition from pan to pinch. */
  if (gesture_state == GESTURE_PAN) {
    if (fabs(scale - PINCH_SCALE_FACTOR) > EPS) {
      gesture_state = GESTURE_PINCH;
    }
  }

  /* This state machine is used here because:
   *  1. Pinch and pan gestures must be differentiated and cannot be processed at the same time
   *     because XY transform values become nonsensical during pinch gesture.
   *  2. GHOST requires delta values for events while DM provides transformation matrix of the
   *     current gesture.
   *  3. GHOST events accept integer values while DM values are non-integer.
   *     Truncated fractional parts are accumulated and accounted for in following updates.
   */
  switch (gesture_state) {
    case GESTURE_PINCH: {
      int32_t dscale = roundf(scale - last_scale);

      last_scale += dscale;

      accumulated_values.scale += dscale;
      break;
    }
    case GESTURE_PAN: {
      int32_t dx = roundf(x - last_x);
      int32_t dy = roundf(y - last_y);

      last_x += dx;
      last_y += dy;

      accumulated_values.x += dx;
      accumulated_values.y += dy;
      break;
    }
    case GESTURE_NONE:
      break;
  }

  return hr;
}
