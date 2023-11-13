/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/* NOTE: Requires OpenXR headers to be included before this one for OpenXR types
 * (XrSpace, XrPath, etc.). */

#pragma once

#include <map>
#include <memory>
#include <string>

#include "GHOST_Util.hh"

/* -------------------------------------------------------------------- */

class GHOST_XrActionSpace {
 public:
  GHOST_XrActionSpace() = delete; /* Default constructor for map storage. */
  GHOST_XrActionSpace(XrSession session,
                      XrAction action,
                      const char *action_name,
                      const char *profile_path,
                      XrPath subaction_path,
                      const char *subaction_path_str,
                      const GHOST_XrPose &pose);
  ~GHOST_XrActionSpace();

  XrSpace getSpace() const;

 private:
  XrSpace m_space = XR_NULL_HANDLE;
};

/* -------------------------------------------------------------------- */

typedef struct GHOST_XrSubactionData {
  XrPath subaction_path = XR_NULL_PATH;
  float float_threshold;
  int16_t axis_flag;
  std::unique_ptr<GHOST_XrActionSpace> space = nullptr;
} GHOST_XrSubactionData;

/* -------------------------------------------------------------------- */

class GHOST_XrActionProfile {
 public:
  GHOST_XrActionProfile() = delete; /* Default constructor for map storage. */
  GHOST_XrActionProfile(XrInstance instance,
                        XrSession session,
                        XrAction action,
                        GHOST_XrActionType type,
                        const GHOST_XrActionProfileInfo &info);
  ~GHOST_XrActionProfile() = default;

  XrPath getProfile() const;
  const GHOST_XrSubactionData *getSubaction(XrPath subaction_path) const;
  void getBindings(XrAction action,
                   std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const;

 private:
  XrPath m_profile = XR_NULL_PATH;

  /** Sub-action data identified by user `subaction` path. */
  std::map<std::string, GHOST_XrSubactionData> m_subaction_data;
  /** Bindings identified by interaction (user `subaction` + component) path. */
  std::map<std::string, XrPath> m_bindings;
};

/* -------------------------------------------------------------------- */

class GHOST_XrAction {
 public:
  GHOST_XrAction() = delete; /* Default constructor for map storage. */
  GHOST_XrAction(XrInstance instance, XrActionSet action_set, const GHOST_XrActionInfo &info);
  ~GHOST_XrAction();

  bool createBinding(XrInstance instance,
                     XrSession session,
                     const GHOST_XrActionProfileInfo &info);
  void destroyBinding(const char *profile_path);

  void updateState(XrSession session,
                   const char *action_name,
                   XrSpace reference_space,
                   const XrTime &predicted_display_time);
  void applyHapticFeedback(XrSession session,
                           const char *action_name,
                           const char *subaction_path_str,
                           const int64_t &duration,
                           const float &frequency,
                           const float &amplitude);
  void stopHapticFeedback(XrSession session,
                          const char *action_name,
                          const char *subaction_path_str);

  void *getCustomdata();
  void getBindings(std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const;

 private:
  using SubactionIndexMap = std::map<std::string, uint32_t>;

  XrAction m_action = XR_NULL_HANDLE;
  GHOST_XrActionType m_type;
  SubactionIndexMap m_subaction_indices;
  std::vector<XrPath> m_subaction_paths;
  /** States for each subaction path. */
  void *m_states;
  /** Input thresholds/regions for each subaction path. */
  float *m_float_thresholds;
  int16_t *m_axis_flags;

  std::unique_ptr<GHOST_C_CustomDataWrapper> m_custom_data_ = nullptr; /* wmXrAction */

  /** Profiles identified by interaction profile path. */
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
  uint32_t getActionCount() const;
  void getActionCustomdataArray(void **r_customdata_array);
  void getBindings(std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const;

 private:
  XrActionSet m_action_set = XR_NULL_HANDLE;

  std::unique_ptr<GHOST_C_CustomDataWrapper> m_custom_data_ = nullptr; /* wmXrActionSet */

  std::map<std::string, GHOST_XrAction> m_actions;
};

/* -------------------------------------------------------------------- */
