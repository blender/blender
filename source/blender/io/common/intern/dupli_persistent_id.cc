/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "dupli_parent_finder.hh"

#include <climits>
#include <cstring>
#include <ostream>
#include <sstream>

namespace blender::io {

PersistentID::PersistentID()
{
  persistent_id_[0] = INT_MAX;
}

PersistentID::PersistentID(const DupliObject *dupli_ob)
{
  for (int index = 0; index < array_length_; ++index) {
    persistent_id_[index] = dupli_ob->persistent_id[index];
  }
}

PersistentID::PersistentID(const PIDArray &persistent_id_values)
{
  persistent_id_ = persistent_id_values;
}

bool PersistentID::is_from_same_instancer_as(const PersistentID &other) const
{
  if (persistent_id_[0] == INT_MAX || other.persistent_id_[0] == INT_MAX) {
    /* Either one or the other is not instanced at all, so definitely not from the same instancer.
     */
    return false;
  }

  /* Start at index 1 to skip the first digit. */
  for (int index = 1; index < array_length_; ++index) {
    const int pid_digit_a = persistent_id_[index];
    const int pid_digit_b = other.persistent_id_[index];

    if (pid_digit_a != pid_digit_b) {
      return false;
    }

    if (pid_digit_a == INT_MAX) {
      /* Both persistent IDs were identical so far, and this marks the end of the useful data. */
      break;
    }
  }
  return true;
}

PersistentID PersistentID::instancer_pid() const
{
  if (persistent_id_[0] == INT_MAX) {
    return PersistentID();
  }

  /* Left-shift the entire PID by 1. */
  PIDArray new_pid_values;
  int index;
  for (index = 0; index < array_length_ - 1; ++index) {
    new_pid_values[index] = persistent_id_[index + 1];
  }
  new_pid_values[index] = INT_MAX;

  return PersistentID(new_pid_values);
}

std::string PersistentID::as_object_name_suffix() const
{
  std::stringstream stream;

  /* Find one past the last index. */
  int index;
  for (index = 0; index < array_length_ && persistent_id_[index] < INT_MAX; ++index) {
    ;
  }

  /* Iterate backward to construct the string. */
  --index;
  for (; index >= 0; --index) {
    stream << persistent_id_[index];
    if (index > 0) {
      stream << "-";
    }
  }

  return stream.str();
}

bool operator<(const PersistentID &persistent_id_a, const PersistentID &persistent_id_b)
{
  const PersistentID::PIDArray &pid_a = persistent_id_a.persistent_id_;
  const PersistentID::PIDArray &pid_b = persistent_id_b.persistent_id_;

  for (int index = 0; index < PersistentID::array_length_; ++index) {
    const int pid_digit_a = pid_a[index];
    const int pid_digit_b = pid_b[index];

    if (pid_digit_a != pid_digit_b) {
      return pid_digit_a < pid_digit_b;
    }

    if (pid_a[index] == INT_MAX) {
      break;
    }
  }
  /* Both Persistent IDs were equal, so not less-than. */
  return false;
}

bool operator==(const PersistentID &persistent_id_a, const PersistentID &persistent_id_b)
{
  const PersistentID::PIDArray &pid_a = persistent_id_a.persistent_id_;
  const PersistentID::PIDArray &pid_b = persistent_id_b.persistent_id_;

  for (int index = 0; index < PersistentID::array_length_; ++index) {
    const int pid_digit_a = pid_a[index];
    const int pid_digit_b = pid_b[index];

    if (pid_digit_a != pid_digit_b) {
      return false;
    }

    if (pid_a[index] == INT_MAX) {
      break;
    }
  }
  return true;
}

std::ostream &operator<<(std::ostream &os, const PersistentID &persistent_id)
{
  if (persistent_id.persistent_id_[0] == INT_MAX) {
    return os;
  }

  const PersistentID::PIDArray &pid_array = persistent_id.persistent_id_;
  for (int index = 0; index < PersistentID::array_length_ && pid_array[index] < INT_MAX; ++index) {
    if (index > 0) {
      os << "-";
    }
    os << pid_array[index];
  }
  return os;
}

}  // namespace blender::io
