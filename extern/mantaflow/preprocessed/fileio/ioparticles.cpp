

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011-2016 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Loading and writing grids and meshes to disk
 *
 ******************************************************************************/

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#if NO_ZLIB != 1
extern "C" {
#  include <zlib.h>
}
#endif

#include "mantaio.h"
#include "grid.h"
#include "particle.h"
#include "vector4d.h"
#include "grid4d.h"

using namespace std;

namespace Manta {

static const int STR_LEN_PDATA = 256;

//! pdata uni header, v3  (similar to grid header)
typedef struct {
  int dim;               // number of partilces
  int dimX, dimY, dimZ;  // underlying solver resolution (all data in local coordinates!)
  int elementType, bytesPerElement;  // type id and byte size
  char info[STR_LEN_PDATA];          // mantaflow build information
  unsigned long long timestamp;      // creation time
} UniPartHeader;

//*****************************************************************************
// conversion functions for double precision
// (note - uni files always store single prec. values)
//*****************************************************************************

#if NO_ZLIB != 1

template<class T>
void pdataConvertWrite(gzFile &gzf, ParticleDataImpl<T> &pdata, void *ptr, UniPartHeader &head)
{
  errMsg("pdataConvertWrite: unknown type, not yet supported");
}

template<>
void pdataConvertWrite(gzFile &gzf, ParticleDataImpl<int> &pdata, void *ptr, UniPartHeader &head)
{
  gzwrite(gzf, &head, sizeof(UniPartHeader));
  gzwrite(gzf, &pdata[0], sizeof(int) * head.dim);
}
template<>
void pdataConvertWrite(gzFile &gzf,
                       ParticleDataImpl<double> &pdata,
                       void *ptr,
                       UniPartHeader &head)
{
  head.bytesPerElement = sizeof(float);
  gzwrite(gzf, &head, sizeof(UniPartHeader));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < pdata.size(); ++i, ++ptrf) {
    *ptrf = (float)pdata[i];
  }
  gzwrite(gzf, ptr, sizeof(float) * head.dim);
}
template<>
void pdataConvertWrite(gzFile &gzf, ParticleDataImpl<Vec3> &pdata, void *ptr, UniPartHeader &head)
{
  head.bytesPerElement = sizeof(Vector3D<float>);
  gzwrite(gzf, &head, sizeof(UniPartHeader));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < pdata.size(); ++i) {
    for (int c = 0; c < 3; ++c) {
      *ptrf = (float)pdata[i][c];
      ptrf++;
    }
  }
  gzwrite(gzf, ptr, sizeof(Vector3D<float>) * head.dim);
}

template<class T>
void pdataReadConvert(gzFile &gzf, ParticleDataImpl<T> &grid, void *ptr, int bytesPerElement)
{
  errMsg("pdataReadConvert: unknown pdata type, not yet supported");
}

template<>
void pdataReadConvert<int>(gzFile &gzf,
                           ParticleDataImpl<int> &pdata,
                           void *ptr,
                           int bytesPerElement)
{
  gzread(gzf, ptr, sizeof(int) * pdata.size());
  assertMsg(bytesPerElement == sizeof(int),
            "pdata element size doesn't match " << bytesPerElement << " vs " << sizeof(int));
  // int dont change in double precision mode - copy over
  memcpy(&(pdata[0]), ptr, sizeof(int) * pdata.size());
}

template<>
void pdataReadConvert<double>(gzFile &gzf,
                              ParticleDataImpl<double> &pdata,
                              void *ptr,
                              int bytesPerElement)
{
  gzread(gzf, ptr, sizeof(float) * pdata.size());
  assertMsg(bytesPerElement == sizeof(float),
            "pdata element size doesn't match " << bytesPerElement << " vs " << sizeof(float));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < pdata.size(); ++i, ++ptrf) {
    pdata[i] = double(*ptrf);
  }
}

template<>
void pdataReadConvert<Vec3>(gzFile &gzf,
                            ParticleDataImpl<Vec3> &pdata,
                            void *ptr,
                            int bytesPerElement)
{
  gzread(gzf, ptr, sizeof(Vector3D<float>) * pdata.size());
  assertMsg(bytesPerElement == sizeof(Vector3D<float>),
            "pdata element size doesn't match " << bytesPerElement << " vs "
                                                << sizeof(Vector3D<float>));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < pdata.size(); ++i) {
    Vec3 v;
    for (int c = 0; c < 3; ++c) {
      v[c] = double(*ptrf);
      ptrf++;
    }
    pdata[i] = v;
  }
}

#endif  // NO_ZLIB!=1

//*****************************************************************************
// particles and particle data
//*****************************************************************************

static const int PartSysSize = sizeof(Vector3D<float>) + sizeof(int);

int writeParticlesUni(const std::string &name, const BasicParticleSystem *parts)
{
  debMsg("writing particles " << parts->getName() << " to uni file " << name, 1);

#if NO_ZLIB != 1
  char ID[5] = "PB02";
  UniPartHeader head;
  head.dim = parts->size();
  Vec3i gridSize = parts->getParent()->getGridSize();
  head.dimX = gridSize.x;
  head.dimY = gridSize.y;
  head.dimZ = gridSize.z;
  head.bytesPerElement = PartSysSize;
  head.elementType = 0;  // 0 for base data
  snprintf(head.info, STR_LEN_PDATA, "%s", buildInfoString().c_str());
  MuTime stamp;
  head.timestamp = stamp.time;

  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "wb1");  // do some compression
  if (!gzf) {
    errMsg("can't open file " << name);
    return 0;
  }

  gzwrite(gzf, ID, 4);
#  if FLOATINGPOINT_PRECISION != 1
  // warning - hard coded conversion of byte size here...
  gzwrite(gzf, &head, sizeof(UniPartHeader));
  for (int i = 0; i < parts->size(); ++i) {
    Vector3D<float> pos = toVec3f((*parts)[i].pos);
    int flag = (*parts)[i].flag;
    gzwrite(gzf, &pos, sizeof(Vector3D<float>));
    gzwrite(gzf, &flag, sizeof(int));
  }
#  else
  assertMsg(sizeof(BasicParticleData) == PartSysSize, "particle data size doesn't match");
  gzwrite(gzf, &head, sizeof(UniPartHeader));
  gzwrite(gzf, &((*parts)[0]), PartSysSize * head.dim);
#  endif
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
};

int readParticlesUni(const std::string &name, BasicParticleSystem *parts)
{
  debMsg("reading particles " << parts->getName() << " from uni file " << name, 1);

#if NO_ZLIB != 1
  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "rb");
  if (!gzf) {
    errMsg("can't open file " << name);
    return 0;
  }

  char ID[5] = {0, 0, 0, 0, 0};
  gzread(gzf, ID, 4);

  if (!strcmp(ID, "PB01")) {
    errMsg("particle uni file format v01 not supported anymore");
    return 0;
  }
  else if (!strcmp(ID, "PB02")) {
    // current file format
    UniPartHeader head;
    assertMsg(gzread(gzf, &head, sizeof(UniPartHeader)) == sizeof(UniPartHeader),
              "can't read file, no header present");
    assertMsg(((head.bytesPerElement == PartSysSize) && (head.elementType == 0)),
              "particle type doesn't match");

    // re-allocate all data
    parts->resizeAll(head.dim);

    assertMsg(head.dim == parts->size(), "particle size doesn't match");
#  if FLOATINGPOINT_PRECISION != 1
    for (int i = 0; i < parts->size(); ++i) {
      Vector3D<float> pos;
      int flag;
      gzread(gzf, &pos, sizeof(Vector3D<float>));
      gzread(gzf, &flag, sizeof(int));
      (*parts)[i].pos = toVec3d(pos);
      (*parts)[i].flag = flag;
    }
#  else
    assertMsg(sizeof(BasicParticleData) == PartSysSize, "particle data size doesn't match");
    IndexInt bytes = PartSysSize * head.dim;
    IndexInt readBytes = gzread(gzf, &(parts->getData()[0]), bytes);
    assertMsg(bytes == readBytes,
              "can't read uni file, stream length does not match, " << bytes << " vs "
                                                                    << readBytes);
#  endif

    parts->transformPositions(Vec3i(head.dimX, head.dimY, head.dimZ),
                              parts->getParent()->getGridSize());
  }
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
};

template<class T> int writePdataUni(const std::string &name, ParticleDataImpl<T> *pdata)
{
  debMsg("writing particle data " << pdata->getName() << " to uni file " << name, 1);

#if NO_ZLIB != 1
  char ID[5] = "PD01";
  UniPartHeader head;
  head.dim = pdata->size();
  Vec3i gridSize = pdata->getParent()->getGridSize();
  head.dimX = gridSize.x;
  head.dimY = gridSize.y;
  head.dimZ = gridSize.z;
  head.bytesPerElement = sizeof(T);
  head.elementType = 1;  // 1 for particle data, todo - add sub types?
  snprintf(head.info, STR_LEN_PDATA, "%s", buildInfoString().c_str());
  MuTime stamp;
  head.timestamp = stamp.time;

  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "wb1");  // do some compression
  if (!gzf) {
    errMsg("can't open file " << name);
    return 0;
  }
  gzwrite(gzf, ID, 4);

#  if FLOATINGPOINT_PRECISION != 1
  // always write float values, even if compiled with double precision (as for grids)
  ParticleDataImpl<T> temp(pdata->getParent());
  temp.resize(pdata->size());
  pdataConvertWrite(gzf, *pdata, &(temp[0]), head);
#  else
  gzwrite(gzf, &head, sizeof(UniPartHeader));
  gzwrite(gzf, &(pdata->get(0)), sizeof(T) * head.dim);
#  endif
  return (gzclose(gzf) == Z_OK);

#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
};

template<class T> int readPdataUni(const std::string &name, ParticleDataImpl<T> *pdata)
{
  debMsg("reading particle data " << pdata->getName() << " from uni file " << name, 1);

#if NO_ZLIB != 1
  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "rb");
  if (!gzf) {
    errMsg("can't open file " << name);
    return 0;
  }

  char ID[5] = {0, 0, 0, 0, 0};
  gzread(gzf, ID, 4);

  if (!strcmp(ID, "PD01")) {
    UniPartHeader head;
    assertMsg(gzread(gzf, &head, sizeof(UniPartHeader)) == sizeof(UniPartHeader),
              "can't read file, no header present");
    pdata->resize(head.dim);

    assertMsg(head.dim == pdata->size(), "pdata size doesn't match");
#  if FLOATINGPOINT_PRECISION != 1
    ParticleDataImpl<T> temp(pdata->getParent());
    temp.resize(pdata->size());
    pdataReadConvert<T>(gzf, *pdata, &(temp[0]), head.bytesPerElement);
#  else
    assertMsg(((head.bytesPerElement == sizeof(T)) && (head.elementType == 1)),
              "pdata type doesn't match");
    IndexInt bytes = sizeof(T) * head.dim;
    IndexInt readBytes = gzread(gzf, &(pdata->get(0)), sizeof(T) * head.dim);
    assertMsg(bytes == readBytes,
              "can't read uni file, stream length does not match, " << bytes << " vs "
                                                                    << readBytes);
#  endif
  }
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
}

// explicit instantiation
template int writePdataUni<int>(const std::string &name, ParticleDataImpl<int> *pdata);
template int writePdataUni<Real>(const std::string &name, ParticleDataImpl<Real> *pdata);
template int writePdataUni<Vec3>(const std::string &name, ParticleDataImpl<Vec3> *pdata);
template int readPdataUni<int>(const std::string &name, ParticleDataImpl<int> *pdata);
template int readPdataUni<Real>(const std::string &name, ParticleDataImpl<Real> *pdata);
template int readPdataUni<Vec3>(const std::string &name, ParticleDataImpl<Vec3> *pdata);

}  // namespace Manta
