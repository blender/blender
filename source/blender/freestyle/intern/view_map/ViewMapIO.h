/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Functions to manage I/O for the view map
 */

#include <fstream>
#include <string>

#include "ViewMap.h"

#include "../system/FreestyleConfig.h"
#include "../system/ProgressBar.h"

namespace Freestyle {

namespace ViewMapIO {

static const unsigned ZERO = UINT_MAX;

int load(istream &in, ViewMap *vm, ProgressBar *pb = NULL);

int save(ostream &out, ViewMap *vm, ProgressBar *pb = NULL);

namespace Options {

static const unsigned char FLOAT_VECTORS = 1;
static const unsigned char NO_OCCLUDERS = 2;

void setFlags(unsigned char flags);

void addFlags(unsigned char flags);

void rmFlags(unsigned char flags);

unsigned char getFlags();

void setModelsPath(const string &path);

string getModelsPath();

};  // namespace Options

#ifdef IRIX

namespace Internal {

template<unsigned S> ostream &write(ostream &out, const char *str)
{
  out.put(str[S - 1]);
  return write<S - 1>(out, str);
}

template<> ostream &write<1>(ostream &out, const char *str)
{
  return out.put(str[0]);
}

template<> ostream &write<0>(ostream &out, const char *)
{
  return out;
}

template<unsigned S> istream &read(istream &in, char *str)
{
  in.get(str[S - 1]);
  return read<S - 1>(in, str);
}

template<> istream &read<1>(istream &in, char *str)
{
  return in.get(str[0]);
}

template<> istream &read<0>(istream &in, char *)
{
  return in;
}

}  // End of namespace Internal

#endif  // IRIX

}  // End of namespace ViewMapIO

} /* namespace Freestyle */
