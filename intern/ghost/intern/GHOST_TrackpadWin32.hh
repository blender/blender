/* SPDX-FileCopyrightText: 2022-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST DirectManipulation classes.
 */

#pragma once

#ifndef WIN32
#  error WIN32 only!
#endif  // WIN32

#include "GHOST_Types.h"

#include <directmanipulation.h>
#include <wrl.h>

#define PINCH_SCALE_FACTOR 125.0f

typedef struct {
  int32_t x, y, scale;
  bool isScrollDirectionInverted;
} GHOST_TTrackpadInfo;

class GHOST_DirectManipulationHelper;

class GHOST_DirectManipulationViewportEventHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
              Microsoft::WRL::FtmBase,
              IDirectManipulationViewportEventHandler>> {
 public:
  GHOST_DirectManipulationViewportEventHandler(uint16_t dpi);

  /*
   * Resets viewport and tracked touchpad state.
   */
  void resetViewport(IDirectManipulationViewport *viewport);

  /* DirectManipulation callbacks. */
  HRESULT STDMETHODCALLTYPE OnViewportStatusChanged(IDirectManipulationViewport *viewport,
                                                    DIRECTMANIPULATION_STATUS current,
                                                    DIRECTMANIPULATION_STATUS previous) override;

  HRESULT STDMETHODCALLTYPE OnViewportUpdated(IDirectManipulationViewport *viewport) override;

  HRESULT STDMETHODCALLTYPE OnContentUpdated(IDirectManipulationViewport *viewport,
                                             IDirectManipulationContent *content) override;

 private:
  enum { GESTURE_NONE, GESTURE_PAN, GESTURE_PINCH } gesture_state;

  int32_t last_x, last_y, last_scale;
  GHOST_TTrackpadInfo accumulated_values;
  uint16_t dpi;
  DIRECTMANIPULATION_STATUS dm_status;

  friend class GHOST_DirectManipulationHelper;
};

class GHOST_DirectManipulationHelper {
 public:
  /*
   * Creates a GHOST_DirectManipulationHelper for the provided window.
   * \param hWnd: The window receiving DirectManipulation events.
   * \param dpi: The current DPI.
   * \return Pointer to the new GHOST_DirectManipulationHelper if created, nullptr if there was an
   * error.
   */
  static GHOST_DirectManipulationHelper *create(HWND hWnd, uint16_t dpi);

  ~GHOST_DirectManipulationHelper();

  /*
   * Drives the DirectManipulation context.
   * DirectManipulation's intended use is to tie user input into DirectComposition's compositor
   * scaling and translating. We are not using DirectComposition and therefore must drive
   * DirectManipulation manually.
   */
  void update();

  /*
   * Sets pointer in contact with the DirectManipulation context.
   * \param pointerId: ID of the pointer in contact.
   */
  void onPointerHitTest(UINT32 pointerId);

  /*
   * Updates DPI information for touchpad scaling.
   * \param dpi: The new DPI.
   */
  void setDPI(uint16_t dpi);

  /*
   * Retrieves trackpad input.
   * \return The accumulated trackpad translation and scale since last call.
   */
  GHOST_TTrackpadInfo getTrackpadInfo();

 private:
  GHOST_DirectManipulationHelper(
      HWND hWnd,
      Microsoft::WRL::ComPtr<IDirectManipulationManager> directManipulationManager,
      Microsoft::WRL::ComPtr<IDirectManipulationUpdateManager> directManipulationUpdateManager,
      Microsoft::WRL::ComPtr<IDirectManipulationViewport> directManipulationViewport,
      Microsoft::WRL::ComPtr<GHOST_DirectManipulationViewportEventHandler>
          directManipulationEventHandler,
      DWORD directManipulationViewportHandlerCookie,
      bool isScrollDirectionInverted);

  /*
   * Retrieves the scroll direction from the registry.
   * \return True if scroll direction is inverted.
   */
  static bool getScrollDirectionFromReg();

  /*
   * Registers listener for registry scroll direction entry changes.
   */
  void registerScrollDirectionChangeListener();

  HWND m_hWnd;

  HKEY m_scrollDirectionRegKey;
  HANDLE m_scrollDirectionChangeEvent;

  Microsoft::WRL::ComPtr<IDirectManipulationManager> m_directManipulationManager;
  Microsoft::WRL::ComPtr<IDirectManipulationUpdateManager> m_directManipulationUpdateManager;
  Microsoft::WRL::ComPtr<IDirectManipulationViewport> m_directManipulationViewport;
  Microsoft::WRL::ComPtr<GHOST_DirectManipulationViewportEventHandler>
      m_directManipulationEventHandler;
  DWORD m_directManipulationViewportHandlerCookie;

  bool m_isScrollDirectionInverted;
};
