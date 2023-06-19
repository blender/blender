/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_DisplayManager class.
 */

#pragma once

#include "GHOST_Types.h"

#include <vector>

/**
 * Manages system displays  (platform independent implementation).
 */
class GHOST_DisplayManager {
 public:
  enum { kMainDisplay = 0 };
  /**
   * Constructor.
   */
  GHOST_DisplayManager();

  /**
   * Destructor.
   */
  virtual ~GHOST_DisplayManager();

  /**
   * Initializes the list with devices and settings.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess initialize();

  /**
   * Returns the number of display devices on this system.
   * \param numDisplays: The number of displays on this system.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getNumDisplays(uint8_t &numDisplays) const;

  /**
   * Returns the number of display settings for this display device.
   * \param display: The index of the display to query with 0 <= display < getNumDisplays().
   * \param numSettings: The number of settings of the display device with this index.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getNumDisplaySettings(uint8_t display, int32_t &numSettings) const;

  /**
   * Returns the current setting for this display device.
   * \param display: The index of the display to query with 0 <= display < getNumDisplays().
   * \param index: The setting index to be returned.
   * \param setting: The setting of the display device with this index.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getDisplaySetting(uint8_t display,
                                           int32_t index,
                                           GHOST_DisplaySetting &setting) const;

  /**
   * Returns the current setting for this display device.
   * \param display: The index of the display to query with 0 <= display < getNumDisplays().
   * \param setting: The current setting of the display device with this index.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess getCurrentDisplaySetting(uint8_t display,
                                                  GHOST_DisplaySetting &setting) const;

  /**
   * Changes the current setting for this display device.
   * The setting given to this method is matched against the available display settings.
   * The best match is activated (@see findMatch()).
   * \param display: The index of the display to query with 0 <= display < getNumDisplays().
   * \param setting: The setting of the display device to be matched and activated.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess setCurrentDisplaySetting(uint8_t display,
                                                  const GHOST_DisplaySetting &setting);

 protected:
  typedef std::vector<GHOST_DisplaySetting> GHOST_DisplaySettings;

  /**
   * Finds the best display settings match.
   * \param display: The index of the display device.
   * \param setting: The setting to match.
   * \param match: The optimal display setting.
   * \return Indication of success.
   */
  GHOST_TSuccess findMatch(uint8_t display,
                           const GHOST_DisplaySetting &setting,
                           GHOST_DisplaySetting &match) const;

  /**
   * Retrieves settings for each display device and stores them.
   * \return Indication of success.
   */
  GHOST_TSuccess initializeSettings();

  /** Tells whether the list of display modes has been stored already. */
  bool m_settingsInitialized;
  /** The list with display settings for the main display. */
  std::vector<GHOST_DisplaySettings> m_settings;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_DisplayManager")
#endif
};
