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

/* Note: Requires OpenXR headers to be included before this one for OpenXR types (XrSpace, XrPath,
 * etc.). */

#pragma once

#include <map>
#include <memory>
#include <string>

#include "GHOST_Util.h"

/* -------------------------------------------------------------------- */

class GHOST_XrActionSpace {
 public:
  GHOST_XrActionSpace() = delete; /* Default constructor for map storage. */
  GHOST_XrActionSpace(XrInstance instance,
                      XrSession session,
                      XrAction action,
                      const GHOST_XrActionSpaceInfo &info,
                      uint32_t subaction_idx);
  ~GHOST_XrActionSpace();

  XrSpace getSpace() const;
  const XrPath &getSubactionPath() const;

 private:
  XrSpace m_space = XR_NULL_HANDLE;
  XrPath m_subaction_path = XR_NULL_PATH;
};

/* -------------------------------------------------------------------- */

class GHOST_XrActionProfile {
 public:
  GHOST_XrActionProfile() = delete; /* Default constructor for map storage. */
  GHOST_XrActionProfile(XrInstance instance,
                        XrAction action,
                        const char *profile_path,
                        const GHOST_XrActionBindingInfo &info);
  ~GHOST_XrActionProfile() = default;

  void getBindings(XrAction action,
                   std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const;

 private:
  XrPath m_profile = XR_NULL_PATH;
  /* Bindings identified by interaction (user (subaction) + component) path. */
  std::map<std::string, XrPath> m_bindings;
};

/* -------------------------------------------------------------------- */

class GHOST_XrAction {
 public:
  GHOST_XrAction() = delete; /* Default constructor for map storage. */
  GHOST_XrAction(XrInstance instance, XrActionSet action_set, const GHOST_XrActionInfo &info);
  ~GHOST_XrAction();

  bool createSpace(XrInstance instance, XrSession session, const GHOST_XrActionSpaceInfo &info);
  void destroySpace(const char *subaction_path);

  bool createBinding(XrInstance instance,
                     const char *profile_path,
                     const GHOST_XrActionBindingInfo &info);
  void destroyBinding(const char *profile_path);

  void updateState(XrSession session,
                   const char *action_name,
                   XrSpace reference_space,
                   const XrTime &predicted_display_time);
  void applyHapticFeedback(XrSession session,
                           const char *action_name,
                           const GHOST_TInt64 &duration,
                           const float &frequency,
                           const float &amplitude);
  void stopHapticFeedback(XrSession session, const char *action_name);

  void *getCustomdata();
  void getBindings(std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const;

 private:
  XrAction m_action = XR_NULL_HANDLE;
  GHOST_XrActionType m_type;
  std::vector<XrPath> m_subaction_paths;
  /** States for each subaction path. */
  void *m_states;

  std::unique_ptr<GHOST_C_CustomDataWrapper> m_custom_data_ = nullptr; /* wmXrAction */

  /* Spaces identified by user (subaction) path. */
  std::map<std::string, GHOST_XrActionSpace> m_spaces;
  /* Profiles identified by interaction profile path. */
  std::map<std::string, GHOST_XrActionProfile> m_profiles;
};

/* -------------------------------------------------------------------- */

class GHOST_XrActionSet {
 public:
  GHOST_XrActionSet() = delete; /* Default constructor for map storage. */
  GHOST_XrActionSet(XrInstance instance, const GHOST_XrActionSetInfo &info);
  ~GHOST_XrActionSet();

  bool createAction(XrInstance instance, const GHOST_XrActionInfo &info);
  void destroyAction(const char *action_name);
  GHOST_XrAction *findAction(const char *action_name);

  void updateStates(XrSession session,
                    XrSpace reference_space,
                    const XrTime &predicted_display_time);

  XrActionSet getActionSet() const;
  void *getCustomdata();
  void getBindings(std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const;

 private:
  XrActionSet m_action_set = XR_NULL_HANDLE;

  std::unique_ptr<GHOST_C_CustomDataWrapper> m_custom_data_ = nullptr; /* wmXrActionSet */

  std::map<std::string, GHOST_XrAction> m_actions;
};

/* -------------------------------------------------------------------- */
