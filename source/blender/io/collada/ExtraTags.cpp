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
 * \ingroup collada
 */

#include <stddef.h>
#include <stdlib.h>
#include "BLI_string.h"

#include <iostream>

#include "ExtraTags.h"

ExtraTags::ExtraTags(std::string profile)
{
  this->profile = profile;
  this->tags = std::map<std::string, std::string>();
}

ExtraTags::~ExtraTags()
{
}

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
  return (float)atof(tags[tag].c_str());
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
    *data = (short)tmp;
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
    *data = (char)tmp;
  }
  return ok;
}

std::string ExtraTags::setData(std::string tag, std::string &data)
{
  bool ok = false;
  std::string tmp = asString(tag, &ok);
  return (ok) ? tmp : data;
}
