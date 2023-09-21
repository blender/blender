/* SPDX-FileCopyrightText: 2002-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#include "BLI_string.h"
#include <cstddef>
#include <cstdlib>

#include <iostream>
#include <regex>

#include "ExtraTags.h"

ExtraTags::ExtraTags(std::string profile)
{
  this->profile = profile;
  this->tags = std::map<std::string, std::string>();
}

ExtraTags::~ExtraTags() = default;

bool ExtraTags::isProfile(std::string profile)
{
  return this->profile == profile;
}

bool ExtraTags::addTag(std::string tag, std::string data)
{
  tags[tag] = data;

  return true;
}

int ExtraTags::asInt(std::string tag, bool *ok)
{
  if (tags.find(tag) == tags.end()) {
    *ok = false;
    return -1;
  }
  *ok = true;
  return atoi(tags[tag].c_str());
}

float ExtraTags::asFloat(std::string tag, bool *ok)
{
  if (tags.find(tag) == tags.end()) {
    *ok = false;
    return -1.0f;
  }
  *ok = true;
  return float(atof(tags[tag].c_str()));
}

std::string ExtraTags::asString(std::string tag, bool *ok)
{
  if (tags.find(tag) == tags.end()) {
    *ok = false;
    return "";
  }
  *ok = true;
  return tags[tag];
}

bool ExtraTags::setData(std::string tag, short *data)
{
  bool ok = false;
  int tmp = asInt(tag, &ok);
  if (ok) {
    *data = short(tmp);
  }
  return ok;
}

bool ExtraTags::setData(std::string tag, int *data)
{
  bool ok = false;
  int tmp = asInt(tag, &ok);
  if (ok) {
    *data = tmp;
  }
  return ok;
}

bool ExtraTags::setData(std::string tag, float *data)
{
  bool ok = false;
  float tmp = asFloat(tag, &ok);
  if (ok) {
    *data = tmp;
  }
  return ok;
}

bool ExtraTags::setData(std::string tag, char *data)
{
  bool ok = false;
  int tmp = asInt(tag, &ok);
  if (ok) {
    *data = char(tmp);
  }
  return ok;
}

std::string ExtraTags::setData(std::string tag, std::string &data)
{
  bool ok = false;
  std::string tmp = asString(tag, &ok);
  return (ok) ? tmp : data;
}

std::vector<std::string> ExtraTags::dataSplitString(const std::string &tag)
{
  bool ok = false;
  const std::string value = asString(tag, &ok);
  if (!ok) {
    return std::vector<std::string>();
  }

  std::vector<std::string> values;

  const std::regex newline_re("[^\\s][^\\r\\n]+");
  const std::sregex_token_iterator end;
  std::sregex_token_iterator iter(value.begin(), value.end(), newline_re);
  for (; iter != end; iter++) {
    values.push_back(*iter);
  }

  return values;
}
