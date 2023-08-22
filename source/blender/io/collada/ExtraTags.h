/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <map>
#include <string>
#include <vector>

/** \brief Class for saving \<extra\> tags for a specific UniqueId.
 */
class ExtraTags {
 public:
  /** Constructor. */
  ExtraTags(const std::string profile);

  /** Destructor. */
  virtual ~ExtraTags();

  /** Handle the beginning of an element. */
  bool addTag(std::string tag, std::string data);

  /** Set given short pointer to value of tag, if it exists. */
  bool setData(std::string tag, short *data);

  /** Set given int pointer to value of tag, if it exists. */
  bool setData(std::string tag, int *data);

  /** Set given float pointer to value of tag, if it exists. */
  bool setData(std::string tag, float *data);

  /** Set given char pointer to value of tag, if it exists. */
  bool setData(std::string tag, char *data);
  std::string setData(std::string tag, std::string &data);

  /** Return true if the extra tags is for specified profile. */
  bool isProfile(std::string profile);

 private:
  /** Disable default copy constructor. */
  ExtraTags(const ExtraTags &pre);
  /** Disable default assignment operator. */
  const ExtraTags &operator=(const ExtraTags &pre);

  /** The profile for which the tags are. */
  std::string profile;

  /** Map of tag and text pairs. */
  std::map<std::string, std::string> tags;

  /** Get text data for tag as an int. */
  int asInt(std::string tag, bool *ok);
  /** Get text data for tag as a float. */
  float asFloat(std::string tag, bool *ok);
  /** Get text data for tag as a string. */
  std::string asString(std::string tag, bool *ok);
};
