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
  XrSpace space_ = XR_NULL_HANDLE;
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
  XrPath profile_ = XR_NULL_PATH;

  /** Sub-action data identified by user `subaction` path. */
  std::map<std::string, GHOST_XrSubactionData> subaction_data_;
  /** Bindings identified by interaction (user `subaction` + component) path. */
  std::map<std::string, XrPath> bindings_;
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

  XrAction action_ = XR_NULL_HANDLE;
  GHOST_XrActionType type_;
  SubactionIndexMap subaction_indices_;
  std::vector<XrPath> subaction_paths_;
  /** States for each subaction path. */
  void *states_;
  /** Input thresholds/regions for each subaction path. */
  float *float_thresholds_;
  int16_t *axis_flags_;

  std::unique_ptr<GHOST_C_CustomDataWrapper> custom_data_ = nullptr; /* wmXrAction */

  /** Profiles identified by interaction profile path. */
  std::map<std::string, GHOST_XrActionProfile> profiles_;
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
  XrActionSet action_set_ = XR_NULL_HANDLE;

  std::unique_ptr<GHOST_C_CustomDataWrapper> custom_data_ = nullptr; /* wmXrActionSet */

  std::map<std::string, GHOST_XrAction> actions_;
};

/* -------------------------------------------------------------------- */
