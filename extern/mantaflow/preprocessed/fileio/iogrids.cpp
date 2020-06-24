

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

#include "cnpy.h"
#include "mantaio.h"
#include "grid.h"
#include "vector4d.h"
#include "grid4d.h"

using namespace std;

namespace Manta {

static const int STR_LEN_GRID = 252;

//! uni file header, v4
typedef struct {
  int dimX, dimY, dimZ;                        // grid size
  int gridType, elementType, bytesPerElement;  // data type info
  char info[STR_LEN_GRID];                     // mantaflow build information
  int dimT;                                    // optionally store forth dimension for 4d grids
  unsigned long long timestamp;                // creation time
} UniHeader;

// note: header v4 only uses 4 bytes of the info string to store the fourth dimension, not needed
// for pdata

//*****************************************************************************
// conversion functions for double precision
// (note - uni files always store single prec. values)
//*****************************************************************************

#if NO_ZLIB != 1
template<class GRIDT> void gridConvertWrite(gzFile &gzf, GRIDT &grid, void *ptr, UniHeader &head)
{
  errMsg("gridConvertWrite: unknown type, not yet supported");
}

template<> void gridConvertWrite(gzFile &gzf, Grid<int> &grid, void *ptr, UniHeader &head)
{
  gzwrite(gzf, &head, sizeof(UniHeader));
  gzwrite(gzf, &grid[0], sizeof(int) * head.dimX * head.dimY * head.dimZ);
}
template<> void gridConvertWrite(gzFile &gzf, Grid<double> &grid, void *ptr, UniHeader &head)
{
  head.bytesPerElement = sizeof(float);
  gzwrite(gzf, &head, sizeof(UniHeader));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < grid.getSizeX() * grid.getSizeY() * grid.getSizeZ(); ++i, ++ptrf) {
    *ptrf = (float)grid[i];
  }
  gzwrite(gzf, ptr, sizeof(float) * head.dimX * head.dimY * head.dimZ);
}
template<>
void gridConvertWrite(gzFile &gzf, Grid<Vector3D<double>> &grid, void *ptr, UniHeader &head)
{
  head.bytesPerElement = sizeof(Vector3D<float>);
  gzwrite(gzf, &head, sizeof(UniHeader));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < grid.getSizeX() * grid.getSizeY() * grid.getSizeZ(); ++i) {
    for (int c = 0; c < 3; ++c) {
      *ptrf = (float)grid[i][c];
      ptrf++;
    }
  }
  gzwrite(gzf, ptr, sizeof(Vector3D<float>) * head.dimX * head.dimY * head.dimZ);
}

template<> void gridConvertWrite(gzFile &gzf, Grid4d<int> &grid, void *ptr, UniHeader &head)
{
  gzwrite(gzf, &head, sizeof(UniHeader));
  gzwrite(gzf, &grid[0], sizeof(int) * head.dimX * head.dimY * head.dimZ * head.dimT);
}
template<> void gridConvertWrite(gzFile &gzf, Grid4d<double> &grid, void *ptr, UniHeader &head)
{
  head.bytesPerElement = sizeof(float);
  gzwrite(gzf, &head, sizeof(UniHeader));
  float *ptrf = (float *)ptr;
  IndexInt s = grid.getStrideT() * grid.getSizeT();
  for (IndexInt i = 0; i < s; ++i, ++ptrf) {
    *ptrf = (float)grid[i];
  }
  gzwrite(gzf, ptr, sizeof(float) * s);
}
template<>
void gridConvertWrite(gzFile &gzf, Grid4d<Vector3D<double>> &grid, void *ptr, UniHeader &head)
{
  head.bytesPerElement = sizeof(Vector3D<float>);
  gzwrite(gzf, &head, sizeof(UniHeader));
  float *ptrf = (float *)ptr;
  IndexInt s = grid.getStrideT() * grid.getSizeT();
  for (IndexInt i = 0; i < s; ++i) {
    for (int c = 0; c < 3; ++c) {
      *ptrf = (float)grid[i][c];
      ptrf++;
    }
  }
  gzwrite(gzf, ptr, sizeof(Vector3D<float>) * s);
}
template<>
void gridConvertWrite(gzFile &gzf, Grid4d<Vector4D<double>> &grid, void *ptr, UniHeader &head)
{
  head.bytesPerElement = sizeof(Vector4D<float>);
  gzwrite(gzf, &head, sizeof(UniHeader));
  float *ptrf = (float *)ptr;
  IndexInt s = grid.getStrideT() * grid.getSizeT();
  for (IndexInt i = 0; i < s; ++i) {
    for (int c = 0; c < 4; ++c) {
      *ptrf = (float)grid[i][c];
      ptrf++;
    }
  }
  gzwrite(gzf, ptr, sizeof(Vector4D<float>) * s);
}

template<class T> void gridReadConvert(gzFile &gzf, Grid<T> &grid, void *ptr, int bytesPerElement)
{
  errMsg("gridReadConvert: unknown type, not yet supported");
}

template<> void gridReadConvert<int>(gzFile &gzf, Grid<int> &grid, void *ptr, int bytesPerElement)
{
  gzread(gzf, ptr, sizeof(int) * grid.getSizeX() * grid.getSizeY() * grid.getSizeZ());
  assertMsg(bytesPerElement == sizeof(int),
            "grid element size doesn't match " << bytesPerElement << " vs " << sizeof(int));
  // easy, nothing to do for ints
  memcpy(&(grid[0]), ptr, sizeof(int) * grid.getSizeX() * grid.getSizeY() * grid.getSizeZ());
}

template<>
void gridReadConvert<double>(gzFile &gzf, Grid<double> &grid, void *ptr, int bytesPerElement)
{
  gzread(gzf, ptr, sizeof(float) * grid.getSizeX() * grid.getSizeY() * grid.getSizeZ());
  assertMsg(bytesPerElement == sizeof(float),
            "grid element size doesn't match " << bytesPerElement << " vs " << sizeof(float));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < grid.getSizeX() * grid.getSizeY() * grid.getSizeZ(); ++i, ++ptrf) {
    grid[i] = (double)(*ptrf);
  }
}

template<>
void gridReadConvert<Vec3>(gzFile &gzf, Grid<Vec3> &grid, void *ptr, int bytesPerElement)
{
  gzread(gzf, ptr, sizeof(Vector3D<float>) * grid.getSizeX() * grid.getSizeY() * grid.getSizeZ());
  assertMsg(bytesPerElement == sizeof(Vector3D<float>),
            "grid element size doesn't match " << bytesPerElement << " vs "
                                               << sizeof(Vector3D<float>));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < grid.getSizeX() * grid.getSizeY() * grid.getSizeZ(); ++i) {
    Vec3 v;
    for (int c = 0; c < 3; ++c) {
      v[c] = double(*ptrf);
      ptrf++;
    }
    grid[i] = v;
  }
}

template<class T>
void gridReadConvert4d(gzFile &gzf, Grid4d<T> &grid, void *ptr, int bytesPerElement, int t)
{
  errMsg("gridReadConvert4d: unknown type, not yet supported");
}

template<>
void gridReadConvert4d<int>(gzFile &gzf, Grid4d<int> &grid, void *ptr, int bytesPerElement, int t)
{
  gzread(gzf, ptr, sizeof(int) * grid.getSizeX() * grid.getSizeY() * grid.getSizeZ());
  assertMsg(bytesPerElement == sizeof(int),
            "grid element size doesn't match " << bytesPerElement << " vs " << sizeof(int));
  // nothing to do for ints
  memcpy(&(grid[grid.getSizeX() * grid.getSizeY() * grid.getSizeZ() * t]),
         ptr,
         sizeof(int) * grid.getSizeX() * grid.getSizeY() * grid.getSizeZ());
}

template<>
void gridReadConvert4d<double>(
    gzFile &gzf, Grid4d<double> &grid, void *ptr, int bytesPerElement, int t)
{
  assertMsg(bytesPerElement == sizeof(float),
            "grid element size doesn't match " << bytesPerElement << " vs " << sizeof(float));

  float *ptrf = (float *)ptr;
  gzread(gzf, ptr, sizeof(float) * grid.getSizeX() * grid.getSizeY() * grid.getSizeZ());
  for (IndexInt i = 0; i < grid.getSizeX() * grid.getSizeY() * grid.getSizeZ(); ++i, ++ptrf) {
    grid[grid.getSizeX() * grid.getSizeY() * grid.getSizeZ() * t + i] = (double)(*ptrf);
  }
}

template<>
void gridReadConvert4d<Vec3>(
    gzFile &gzf, Grid4d<Vec3> &grid, void *ptr, int bytesPerElement, int t)
{
  assertMsg(bytesPerElement == sizeof(Vector3D<float>),
            "grid element size doesn't match " << bytesPerElement << " vs " << sizeof(float));

  gzread(gzf, ptr, sizeof(Vector3D<float>) * grid.getSizeX() * grid.getSizeY() * grid.getSizeZ());
  float *ptrf = (float *)ptr;
  for (IndexInt i = 0; i < grid.getSizeX() * grid.getSizeY() * grid.getSizeZ(); ++i) {
    Vec3 v;
    for (int c = 0; c < 3; ++c) {
      v[c] = double(*ptrf);
      ptrf++;
    }
    grid[grid.getSizeX() * grid.getSizeY() * grid.getSizeZ() * t + i] = v;
  }
}

template<>
void gridReadConvert4d<Vec4>(
    gzFile &gzf, Grid4d<Vec4> &grid, void *ptr, int bytesPerElement, int t)
{
  assertMsg(bytesPerElement == sizeof(Vector4D<float>),
            "grid element size doesn't match " << bytesPerElement << " vs " << sizeof(float));

  gzread(gzf, ptr, sizeof(Vector4D<float>) * grid.getSizeX() * grid.getSizeY() * grid.getSizeZ());
  float *ptrf = (float *)ptr;
  for (IndexInt i = 0; i < grid.getSizeX() * grid.getSizeY() * grid.getSizeZ(); ++i) {
    Vec4 v;
    for (int c = 0; c < 4; ++c) {
      v[c] = double(*ptrf);
      ptrf++;
    }
    grid[grid.getSizeX() * grid.getSizeY() * grid.getSizeZ() * t + i] = v;
  }
}

// make sure compatible grid types dont lead to errors...
static int unifyGridType(int type)
{
  // real <> levelset
  if (type & GridBase::TypeReal)
    type |= GridBase::TypeLevelset;
  if (type & GridBase::TypeLevelset)
    type |= GridBase::TypeReal;
  // vec3 <> mac
  if (type & GridBase::TypeVec3)
    type |= GridBase::TypeMAC;
  if (type & GridBase::TypeMAC)
    type |= GridBase::TypeVec3;
  return type;
}

#endif  // NO_ZLIB!=1

//*****************************************************************************
// grid data
//*****************************************************************************

template<class T> int writeGridTxt(const string &name, Grid<T> *grid)
{
  debMsg("writing grid " << grid->getName() << " to text file " << name, 1);

  ofstream ofs(name.c_str());
  if (!ofs.good())
    errMsg("writeGridTxt: can't open file " << name);
  return 0;
  FOR_IJK(*grid)
  {
    ofs << Vec3i(i, j, k) << " = " << (*grid)(i, j, k) << "\n";
  }
  ofs.close();
  return 1;
}

int writeGridsTxt(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("writeGridsTxt: writing multiple grids to one .txt file not supported yet");
  return 0;
}

int readGridsTxt(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("readGridsTxt: writing multiple grids from one .txt file not supported yet");
  return 0;
}

template<class T> int writeGridRaw(const string &name, Grid<T> *grid)
{
  debMsg("writing grid " << grid->getName() << " to raw file " << name, 1);

#if NO_ZLIB != 1
  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "wb1");  // do some compression
  if (!gzf) {
    errMsg("writeGridRaw: can't open file " << name);
    return 0;
  }

  gzwrite(gzf, &((*grid)[0]), sizeof(T) * grid->getSizeX() * grid->getSizeY() * grid->getSizeZ());
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
}

template<class T> int readGridRaw(const string &name, Grid<T> *grid)
{
  debMsg("reading grid " << grid->getName() << " from raw file " << name, 1);

#if NO_ZLIB != 1
  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "rb");
  if (!gzf) {
    errMsg("readGridRaw: can't open file " << name);
    return 0;
  }

  IndexInt bytes = sizeof(T) * grid->getSizeX() * grid->getSizeY() * grid->getSizeZ();
  IndexInt readBytes = gzread(gzf, &((*grid)[0]), bytes);
  assertMsg(bytes == readBytes,
            "can't read raw file, stream length does not match, " << bytes << " vs " << readBytes);
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
}

int writeGridsRaw(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("writeGridsRaw: writing multiple grids to one .raw file not supported yet");
  return 0;
}

int readGridsRaw(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("readGridsRaw: reading multiple grids from one .raw file not supported yet");
  return 0;
}

//! legacy headers for reading old files
typedef struct {
  int dimX, dimY, dimZ;
  int frames, elements, elementType, bytesPerElement, bytesPerFrame;
} UniLegacyHeader;

typedef struct {
  int dimX, dimY, dimZ;
  int gridType, elementType, bytesPerElement;
} UniLegacyHeader2;

typedef struct {
  int dimX, dimY, dimZ;
  int gridType, elementType, bytesPerElement;
  char info[256];
  unsigned long long timestamp;
} UniLegacyHeader3;

//! for auto-init & check of results of test runs , optionally returns info string of header
void getUniFileSize(const string &name, int &x, int &y, int &z, int *t, std::string *info)
{
  x = y = z = 0;
#if NO_ZLIB != 1
  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "rb");
  if (gzf) {
    char ID[5] = {0, 0, 0, 0, 0};
    gzread(gzf, ID, 4);

    // v3
    if ((!strcmp(ID, "MNT2")) || (!strcmp(ID, "M4T2"))) {
      UniLegacyHeader3 head;
      assertMsg(gzread(gzf, &head, sizeof(UniLegacyHeader3)) == sizeof(UniLegacyHeader3),
                "can't read file, no header present");
      x = head.dimX;
      y = head.dimY;
      z = head.dimZ;

      // optionally , read fourth dim
      if ((!strcmp(ID, "M4T2")) && t) {
        int dimT = 0;
        gzread(gzf, &dimT, sizeof(int));
        (*t) = dimT;
      }
    }

    // v4
    if ((!strcmp(ID, "MNT3")) || (!strcmp(ID, "M4T3"))) {
      UniHeader head;
      assertMsg(gzread(gzf, &head, sizeof(UniHeader)) == sizeof(UniHeader),
                "can't read file, no header present");
      x = head.dimX;
      y = head.dimY;
      z = head.dimZ;
      if (t)
        (*t) = head.dimT;
    }

    gzclose(gzf);
  }
#endif
  if (info) {
    std::ostringstream out;
    out << x << "," << y << "," << z;
    if (t && (*t) > 0)
      out << "," << (*t);
    *info = out.str();
  }
}
Vec3 getUniFileSize(const string &name)
{
  int x, y, z;
  getUniFileSize(name, x, y, z);
  return Vec3(Real(x), Real(y), Real(z));
}
static PyObject *_W_0(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getUniFileSize", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const string &name = _args.get<string>("name", 0, &_lock);
      _retval = toPy(getUniFileSize(name));
      _args.check();
    }
    pbFinalizePlugin(parent, "getUniFileSize", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getUniFileSize", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getUniFileSize("", "getUniFileSize", _W_0);
extern "C" {
void PbRegister_getUniFileSize()
{
  KEEP_UNUSED(_RP_getUniFileSize);
}
}

//! for test run debugging
void printUniFileInfoString(const string &name)
{
  std::string info("<file not found>");
  int x = -1, y = -1, z = -1, t = -1;
  // use getUniFileSize to parse the different headers
  getUniFileSize(name, x, y, z, &t, &info);
  debMsg("File '" << name << "' info: " << info, 1);
}
static PyObject *_W_1(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "printUniFileInfoString", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const string &name = _args.get<string>("name", 0, &_lock);
      _retval = getPyNone();
      printUniFileInfoString(name);
      _args.check();
    }
    pbFinalizePlugin(parent, "printUniFileInfoString", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("printUniFileInfoString", e.what());
    return 0;
  }
}
static const Pb::Register _RP_printUniFileInfoString("", "printUniFileInfoString", _W_1);
extern "C" {
void PbRegister_printUniFileInfoString()
{
  KEEP_UNUSED(_RP_printUniFileInfoString);
}
}

// actual read/write functions

template<class T> int writeGridUni(const string &name, Grid<T> *grid)
{
  debMsg("Writing grid " << grid->getName() << " to uni file " << name, 1);

#if NO_ZLIB != 1
  char ID[5] = "MNT3";
  UniHeader head;
  head.dimX = grid->getSizeX();
  head.dimY = grid->getSizeY();
  head.dimZ = grid->getSizeZ();
  head.dimT = 0;
  head.gridType = grid->getType();
  head.bytesPerElement = sizeof(T);
  snprintf(head.info, STR_LEN_GRID, "%s", buildInfoString().c_str());
  MuTime stamp;
  head.timestamp = stamp.time;

  if (grid->getType() & GridBase::TypeInt)
    head.elementType = 0;
  else if (grid->getType() & GridBase::TypeReal)
    head.elementType = 1;
  else if (grid->getType() & GridBase::TypeVec3)
    head.elementType = 2;
  else {
    errMsg("writeGridUni: unknown element type");
    return 0;
  }

  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "wb1");  // do some compression
  if (!gzf) {
    errMsg("writeGridUni: can't open file " << name);
    return 0;
  }

  gzwrite(gzf, ID, 4);
#  if FLOATINGPOINT_PRECISION != 1
  // always write float values, even if compiled with double precision...
  Grid<T> temp(grid->getParent());
  // "misuse" temp grid as storage for floating point values (we have double, so it will always
  // fit)
  gridConvertWrite(gzf, *grid, &(temp[0]), head);
#  else
  void *ptr = &((*grid)[0]);
  gzwrite(gzf, &head, sizeof(UniHeader));
  gzwrite(gzf, ptr, sizeof(T) * head.dimX * head.dimY * head.dimZ);
#  endif
  return (gzclose(gzf) == Z_OK);

#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
};

template<class T> int readGridUni(const string &name, Grid<T> *grid)
{
  debMsg("Reading grid " << grid->getName() << " from uni file " << name, 1);

#if NO_ZLIB != 1
  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "rb");
  if (!gzf) {
    errMsg("readGridUni: can't open file " << name);
    return 0;
  }

  char ID[5] = {0, 0, 0, 0, 0};
  gzread(gzf, ID, 4);

  if (!strcmp(ID, "DDF2")) {
    // legacy file format
    UniLegacyHeader head;
    assertMsg(gzread(gzf, &head, sizeof(UniLegacyHeader)) == sizeof(UniLegacyHeader),
              "can't read file, no header present");
    assertMsg(head.dimX == grid->getSizeX() && head.dimY == grid->getSizeY() &&
                  head.dimZ == grid->getSizeZ(),
              "grid dim doesn't match");
    assertMsg(head.bytesPerElement * head.elements == sizeof(T), "grid type doesn't match");
    // skip flags
    int numEl = head.dimX * head.dimY * head.dimZ;
    gzseek(gzf, numEl, SEEK_CUR);
    // actual grid read
    gzread(gzf, &((*grid)[0]), sizeof(T) * numEl);
  }
  else if (!strcmp(ID, "MNT1")) {
    // legacy file format 2
    UniLegacyHeader2 head;
    assertMsg(gzread(gzf, &head, sizeof(UniLegacyHeader2)) == sizeof(UniLegacyHeader2),
              "can't read file, no header present");
    assertMsg(head.dimX == grid->getSizeX() && head.dimY == grid->getSizeY() &&
                  head.dimZ == grid->getSizeZ(),
              "grid dim doesn't match, " << Vec3(head.dimX, head.dimY, head.dimZ) << " vs "
                                         << grid->getSize());
    assertMsg(head.gridType == grid->getType(),
              "grid type doesn't match " << head.gridType << " vs " << grid->getType());
    assertMsg(head.bytesPerElement == sizeof(T),
              "grid element size doesn't match " << head.bytesPerElement << " vs " << sizeof(T));
    gzread(gzf, &((*grid)[0]), sizeof(T) * head.dimX * head.dimY * head.dimZ);
  }
  else if (!strcmp(ID, "MNT2")) {
    // a bit ugly, almost identical to MNT3
    UniLegacyHeader3 head;
    assertMsg(gzread(gzf, &head, sizeof(UniLegacyHeader3)) == sizeof(UniLegacyHeader3),
              "can't read file, no header present");
    assertMsg(head.dimX == grid->getSizeX() && head.dimY == grid->getSizeY() &&
                  head.dimZ == grid->getSizeZ(),
              "grid dim doesn't match, " << Vec3(head.dimX, head.dimY, head.dimZ) << " vs "
                                         << grid->getSize());
    assertMsg(unifyGridType(head.gridType) == unifyGridType(grid->getType()),
              "grid type doesn't match " << head.gridType << " vs " << grid->getType());
#  if FLOATINGPOINT_PRECISION != 1
    Grid<T> temp(grid->getParent());
    void *ptr = &(temp[0]);
    gridReadConvert<T>(gzf, *grid, ptr, head.bytesPerElement);
#  else
    assertMsg(head.bytesPerElement == sizeof(T),
              "grid element size doesn't match " << head.bytesPerElement << " vs " << sizeof(T));
    gzread(gzf, &((*grid)[0]), sizeof(T) * head.dimX * head.dimY * head.dimZ);
#  endif
  }
  else if (!strcmp(ID, "MNT3")) {
    // current file format
    UniHeader head;
    assertMsg(gzread(gzf, &head, sizeof(UniHeader)) == sizeof(UniHeader),
              "can't read file, no header present");
    assertMsg(head.dimX == grid->getSizeX() && head.dimY == grid->getSizeY() &&
                  head.dimZ == grid->getSizeZ(),
              "grid dim doesn't match, " << Vec3(head.dimX, head.dimY, head.dimZ) << " vs "
                                         << grid->getSize());
    assertMsg(unifyGridType(head.gridType) == unifyGridType(grid->getType()),
              "grid type doesn't match " << head.gridType << " vs " << grid->getType());
#  if FLOATINGPOINT_PRECISION != 1
    // convert float to double
    Grid<T> temp(grid->getParent());
    void *ptr = &(temp[0]);
    gridReadConvert<T>(gzf, *grid, ptr, head.bytesPerElement);
#  else
    assertMsg(head.bytesPerElement == sizeof(T),
              "grid element size doesn't match " << head.bytesPerElement << " vs " << sizeof(T));
    gzread(gzf, &((*grid)[0]), sizeof(T) * head.dimX * head.dimY * head.dimZ);
#  endif
  }
  else {
    errMsg("readGridUni: Unknown header '" << ID << "' ");
    return 0;
  }
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
};

int writeGridsUni(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("writeGridsUni: writing multiple grids to one .uni file not supported yet");
  return 0;
}

int readGridsUni(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("readGridsUni: reading multiple grids from one .uni file not supported yet");
  return 0;
}

template<class T> int writeGridVol(const string &name, Grid<T> *grid)
{
  debMsg("writing grid " << grid->getName() << " to vol file " << name, 1);
  errMsg("writeGridVol: Type not yet supported!");
  return 0;
}

int writeGridsVol(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("writeGridsVol: writing multiple grids to one .vol file not supported yet");
  return 0;
}

int readGridsVol(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("readGridsVol: reading multiple grids from one .vol file not supported yet");
  return 0;
}

struct volHeader {
  char ID[3];
  char version;
  int encoding;
  int dimX, dimY, dimZ;
  int channels;
  Vec3 bboxMin, bboxMax;
};

template<> int writeGridVol<Real>(const string &name, Grid<Real> *grid)
{
  debMsg("writing real grid " << grid->getName() << " to vol file " << name, 1);

  volHeader header;
  header.ID[0] = 'V';
  header.ID[1] = 'O';
  header.ID[2] = 'L';
  header.version = 3;
  header.encoding = 1;  // float32 precision
  header.dimX = grid->getSizeX();
  header.dimY = grid->getSizeY();
  header.dimZ = grid->getSizeZ();
  header.channels = 1;  // only 1 channel
  header.bboxMin = Vec3(-0.5);
  header.bboxMax = Vec3(0.5);

  FILE *fp = fopen(name.c_str(), "wb");
  if (fp == NULL) {
    errMsg("writeGridVol: Cannot open '" << name << "'");
    return 0;
  }

  fwrite(&header, sizeof(volHeader), 1, fp);

#if FLOATINGPOINT_PRECISION == 1
  // for float, write one big chunk
  fwrite(&(*grid)[0], sizeof(float), grid->getSizeX() * grid->getSizeY() * grid->getSizeZ(), fp);
#else
  // explicitly convert each entry to float - we might have double precision in mantaflow
  FOR_IDX(*grid)
  {
    float value = (*grid)[idx];
    fwrite(&value, sizeof(float), 1, fp);
  }
#endif
  return (!fclose(fp));
};

template<class T> int readGridVol(const string &name, Grid<T> *grid)
{
  debMsg("writing grid " << grid->getName() << " to vol file " << name, 1);
  errMsg("readGridVol: Type not yet supported!");
  return 0;
}

template<> int readGridVol<Real>(const string &name, Grid<Real> *grid)
{
  debMsg("reading real grid " << grid->getName() << " from vol file " << name, 1);

  volHeader header;
  FILE *fp = fopen(name.c_str(), "rb");
  if (fp == NULL) {
    errMsg("readGridVol: Cannot open '" << name << "'");
    return 0;
  }

  // note, only very basic file format checks here!
  assertMsg(fread(&header, 1, sizeof(volHeader), fp) == sizeof(volHeader),
            "can't read file, no header present");
  if (header.dimX != grid->getSizeX() || header.dimY != grid->getSizeY() ||
      header.dimZ != grid->getSizeZ())
    errMsg("grid dim doesn't match, " << Vec3(header.dimX, header.dimY, header.dimZ) << " vs "
                                      << grid->getSize());
#if FLOATINGPOINT_PRECISION != 1
  errMsg("readGridVol: Double precision not yet supported");
  return 0;
#else
  const unsigned int s = sizeof(float) * header.dimX * header.dimY * header.dimZ;
  assertMsg(fread(&((*grid)[0]), 1, s, fp) == s, "can't read file, no / not enough data");
#endif
  return (!fclose(fp));
};

// 4d grids IO

template<class T> int writeGrid4dUni(const string &name, Grid4d<T> *grid)
{
  debMsg("writing grid4d " << grid->getName() << " to uni file " << name, 1);

#if NO_ZLIB != 1
  char ID[5] = "M4T3";
  UniHeader head;
  head.dimX = grid->getSizeX();
  head.dimY = grid->getSizeY();
  head.dimZ = grid->getSizeZ();
  head.dimT = grid->getSizeT();
  head.gridType = grid->getType();
  head.bytesPerElement = sizeof(T);
  snprintf(head.info, STR_LEN_GRID, "%s", buildInfoString().c_str());
  MuTime stamp;
  stamp.get();
  head.timestamp = stamp.time;

  if (grid->getType() & Grid4dBase::TypeInt)
    head.elementType = 0;
  else if (grid->getType() & Grid4dBase::TypeReal)
    head.elementType = 1;
  else if (grid->getType() & Grid4dBase::TypeVec3)
    head.elementType = 2;
  else if (grid->getType() & Grid4dBase::TypeVec4)
    head.elementType = 2;
  else {
    errMsg("writeGrid4dUni: unknown element type");
    return 0;
  }

  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "wb1");  // do some compression
  if (!gzf) {
    errMsg("writeGrid4dUni: can't open file " << name);
    return 0;
  }

  gzwrite(gzf, ID, 4);
#  if FLOATINGPOINT_PRECISION != 1
  Grid4d<T> temp(grid->getParent());
  gridConvertWrite<Grid4d<T>>(gzf, *grid, &(temp[0]), head);
#  else
  gzwrite(gzf, &head, sizeof(UniHeader));

  // can be too large - write in chunks
  for (int t = 0; t < head.dimT; ++t) {
    void *ptr = &((*grid)[head.dimX * head.dimY * head.dimZ * t]);
    gzwrite(gzf, ptr, sizeof(T) * head.dimX * head.dimY * head.dimZ * 1);
  }
#  endif
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
};

//! note, reading 4d uni grids is slightly more complicated than 3d ones
//! as it optionally supports sliced reading
template<class T>
int readGrid4dUni(
    const string &name, Grid4d<T> *grid, int readTslice, Grid4d<T> *slice, void **fileHandle)
{
  if (grid)
    debMsg("reading grid " << grid->getName() << " from uni file " << name, 1);
  if (slice)
    debMsg("reading slice " << slice->getName() << ",t=" << readTslice << " from uni file "
                            << name,
           1);

#if NO_ZLIB != 1
  gzFile gzf = NULL;
  char ID[5] = {0, 0, 0, 0, 0};

  // optionally - reuse file handle, if valid one is passed in fileHandle pointer...
  if ((!fileHandle) || (fileHandle && (*fileHandle == NULL))) {
    gzf = (gzFile)safeGzopen(name.c_str(), "rb");
    if (!gzf) {
      errMsg("readGrid4dUni: can't open file " << name);
      return 0;
    }

    gzread(gzf, ID, 4);
    if (fileHandle) {
      *fileHandle = gzf;
    }
  }
  else {
    // optimized read - reduced sanity checks
    gzf = (gzFile)(*fileHandle);
    void *ptr = &((*slice)[0]);
    gzread(gzf, ptr, sizeof(T) * slice->getStrideT() * 1);  // quick and dirty...
    return 1;
  }

  if ((!strcmp(ID, "M4T2")) || (!strcmp(ID, "M4T3"))) {
    int headerSize = -1;

    // current file format
    UniHeader head;
    if (!strcmp(ID, "M4T3")) {
      headerSize = sizeof(UniHeader);
      assertMsg(gzread(gzf, &head, sizeof(UniHeader)) == sizeof(UniHeader),
                "can't read file, no 4d header present");
      if (FLOATINGPOINT_PRECISION == 1)
        assertMsg(head.bytesPerElement == sizeof(T),
                  "4d grid element size doesn't match " << head.bytesPerElement << " vs "
                                                        << sizeof(T));
    }
    // old header
    if (!strcmp(ID, "M4T2")) {
      UniLegacyHeader3 lhead;
      headerSize = sizeof(UniLegacyHeader3) + sizeof(int);
      assertMsg(gzread(gzf, &lhead, sizeof(UniLegacyHeader3)) == sizeof(UniLegacyHeader3),
                "can't read file, no 4dl header present");
      if (FLOATINGPOINT_PRECISION == 1)
        assertMsg(lhead.bytesPerElement == sizeof(T),
                  "4d grid element size doesn't match " << lhead.bytesPerElement << " vs "
                                                        << sizeof(T));

      int fourthDim = 0;
      gzread(gzf, &fourthDim, sizeof(fourthDim));

      head.dimX = lhead.dimX;
      head.dimY = lhead.dimY;
      head.dimZ = lhead.dimZ;
      head.dimT = fourthDim;
      head.gridType = lhead.gridType;
    }

    if (readTslice < 0) {
      assertMsg(head.dimX == grid->getSizeX() && head.dimY == grid->getSizeY() &&
                    head.dimZ == grid->getSizeZ(),
                "grid dim doesn't match, " << Vec3(head.dimX, head.dimY, head.dimZ) << " vs "
                                           << grid->getSize());
      assertMsg(unifyGridType(head.gridType) == unifyGridType(grid->getType()),
                "grid type doesn't match " << head.gridType << " vs " << grid->getType());

      // read full 4d grid
      assertMsg(head.dimT == grid->getSizeT(),
                "grid dim4 doesn't match, " << head.dimT << " vs " << grid->getSize());

      // can be too large - read in chunks
#  if FLOATINGPOINT_PRECISION != 1
      Grid4d<T> temp(grid->getParent());
      void *ptr = &(temp[0]);
      for (int t = 0; t < head.dimT; ++t) {
        gridReadConvert4d<T>(gzf, *grid, ptr, head.bytesPerElement, t);
      }
#  else
      for (int t = 0; t < head.dimT; ++t) {
        void *ptr = &((*grid)[head.dimX * head.dimY * head.dimZ * t]);
        gzread(gzf, ptr, sizeof(T) * head.dimX * head.dimY * head.dimZ * 1);
      }
#  endif
    }
    else {
      // read chosen slice only
      assertMsg(head.dimX == slice->getSizeX() && head.dimY == slice->getSizeY() &&
                    head.dimZ == slice->getSizeZ(),
                "grid dim doesn't match, " << Vec3(head.dimX, head.dimY, head.dimZ) << " vs "
                                           << slice->getSize());
      assertMsg(unifyGridType(head.gridType) == unifyGridType(slice->getType()),
                "grid type doesn't match " << head.gridType << " vs " << slice->getType());

#  if FLOATINGPOINT_PRECISION != 1
      errMsg("readGrid4dUni: NYI (2)");  // slice read not yet supported for double
      return 0;
#  else
      assertMsg(slice, "No 3d slice grid data given");
      assertMsg(readTslice < head.dimT,
                "grid dim4 slice too large " << readTslice << " vs " << head.dimT);
      void *ptr = &((*slice)[0]);
      gzseek(gzf,
             sizeof(T) * head.dimX * head.dimY * head.dimZ * readTslice + headerSize + 4,
             SEEK_SET);
      gzread(gzf, ptr, sizeof(T) * head.dimX * head.dimY * head.dimZ * 1);
#  endif
    }
  }
  else {
    debMsg("Unknown header!", 1);
  }

  if (!fileHandle) {
    return (gzclose(gzf) == Z_OK);
  }
  return 1;
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
};
void readGrid4dUniCleanup(void **fileHandle)
{
  gzFile gzf = NULL;
  if (fileHandle) {
    gzf = (gzFile)(*fileHandle);
    gzclose(gzf);
    *fileHandle = NULL;
  }
}

template<class T> int writeGrid4dRaw(const string &name, Grid4d<T> *grid)
{
  debMsg("writing grid4d " << grid->getName() << " to raw file " << name, 1);

#if NO_ZLIB != 1
  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "wb1");  // do some compression
  if (!gzf) {
    errMsg("writeGrid4dRaw: can't open file " << name);
    return 0;
  }
  gzwrite(gzf,
          &((*grid)[0]),
          sizeof(T) * grid->getSizeX() * grid->getSizeY() * grid->getSizeZ() * grid->getSizeT());
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
}

template<class T> int readGrid4dRaw(const string &name, Grid4d<T> *grid)
{
  debMsg("reading grid4d " << grid->getName() << " from raw file " << name, 1);

#if NO_ZLIB != 1
  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "rb");
  if (!gzf) {
    errMsg("readGrid4dRaw: can't open file " << name);
    return 0;
  }
  IndexInt bytes = sizeof(T) * grid->getSizeX() * grid->getSizeY() * grid->getSizeZ() *
                   grid->getSizeT();
  IndexInt readBytes = gzread(gzf, &((*grid)[0]), bytes);
  assertMsg(bytes == readBytes,
            "can't read raw file, stream length does not match, " << bytes << " vs " << readBytes);
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
}

//*****************************************************************************
// npz file support (warning - read works, but write generates uncompressed npz; i.e. not
// recommended for large volumes)

template<class T> int writeGridNumpy(const string &name, Grid<T> *grid)
{
#if NO_ZLIB == 1
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
#if FLOATINGPOINT_PRECISION != 1
  errMsg("writeGridNumpy: Double precision not yet supported");
  return 0;
#endif

  // find suffix to differentiate between npy <-> npz , TODO: check for actual "npy" string
  std::string::size_type idx;
  bool bUseNpz = false;
  idx = name.rfind('.');
  if (idx != std::string::npos) {
    bUseNpz = name.substr(idx + 1) == "npz";
    debMsg("Writing grid " << grid->getName() << " to npz file " << name, 1);
  }
  else {
    debMsg("Writing grid " << grid->getName() << " to npy file " << name, 1);
  }

  // storage code
  size_t uDim = 1;
  if (grid->getType() & GridBase::TypeInt || grid->getType() & GridBase::TypeReal ||
      grid->getType() & GridBase::TypeLevelset)
    uDim = 1;
  else if (grid->getType() & GridBase::TypeVec3 || grid->getType() & GridBase::TypeMAC)
    uDim = 3;
  else {
    errMsg("writeGridNumpy: unknown element type");
    return 0;
  }

  const std::vector<size_t> shape = {static_cast<size_t>(grid->getSizeZ()),
                                     static_cast<size_t>(grid->getSizeY()),
                                     static_cast<size_t>(grid->getSizeX()),
                                     uDim};

  if (bUseNpz) {
    // note, the following generates a zip file without compression
    if (grid->getType() & GridBase::TypeVec3 || grid->getType() & GridBase::TypeMAC) {
      // cast to float* for export!
      float *ptr = (float *)&((*grid)[0]);
      cnpy::npz_save(name, "arr_0", ptr, shape, "w");
    }
    else {
      T *ptr = &((*grid)[0]);
      cnpy::npz_save(name, "arr_0", ptr, shape, "w");
    }
  }
  else {
    cnpy::npy_save(name, &grid[0], shape, "w");
  }
  return 1;
};

template<class T> int readGridNumpy(const string &name, Grid<T> *grid)
{
#if NO_ZLIB == 1
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
#if FLOATINGPOINT_PRECISION != 1
  errMsg("readGridNumpy: Double precision not yet supported");
  return 0;
#endif

  // find suffix to differentiate between npy <-> npz
  std::string::size_type idx;
  bool bUseNpz = false;
  idx = name.rfind('.');
  if (idx != std::string::npos) {
    bUseNpz = name.substr(idx + 1) == "npz";
    debMsg("Reading grid " << grid->getName() << " as npz file " << name, 1);
  }
  else {
    debMsg("Reading grid " << grid->getName() << " as npy file " << name, 1);
  }

  cnpy::NpyArray gridArr;
  if (bUseNpz) {
    cnpy::npz_t fNpz = cnpy::npz_load(name);
    gridArr = fNpz["arr_0"];
  }
  else {
    gridArr = cnpy::npy_load(name);
  }

  // Check the file meta information
  assertMsg(gridArr.shape[2] == grid->getSizeX() && gridArr.shape[1] == grid->getSizeY() &&
                gridArr.shape[0] == grid->getSizeZ(),
            "grid dim doesn't match, "
                << Vec3(gridArr.shape[2], gridArr.shape[1], gridArr.shape[0]) << " vs "
                << grid->getSize());
  size_t uDim = 1;
  if (grid->getType() & GridBase::TypeInt || grid->getType() & GridBase::TypeReal ||
      grid->getType() & GridBase::TypeLevelset)
    uDim = 1;
  else if (grid->getType() & GridBase::TypeVec3 || grid->getType() & GridBase::TypeMAC)
    uDim = 3;
  else {
    errMsg("readGridNumpy: unknown element type");
    return 0;
  }
  assertMsg(gridArr.shape[3] == uDim,
            "grid data dim doesn't match, " << gridArr.shape[3] << " vs " << uDim);

  if (grid->getType() & GridBase::TypeVec3 || grid->getType() & GridBase::TypeMAC) {
    // treated as float* for export , thus consider 3 elements
    assertMsg(3 * gridArr.word_size == sizeof(T),
              "vec3 grid data size doesn't match, " << 3 * gridArr.word_size << " vs "
                                                    << sizeof(T));
  }
  else {
    assertMsg(gridArr.word_size == sizeof(T),
              "grid data size doesn't match, " << gridArr.word_size << " vs " << sizeof(T));
  }

  // copy back, TODO: beautify...
  memcpy(&((*grid)[0]),
         gridArr.data<T>(),
         sizeof(T) * grid->getSizeX() * grid->getSizeY() * grid->getSizeZ());
  return 1;
};

int writeGridsNumpy(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("writeGridsNumpy: writing multiple grids to one .npz file not supported yet");
  return 0;
}

int readGridsNumpy(const string &name, std::vector<PbClass *> *grids)
{
  errMsg("readGridsNumpy: reading multiple grids from one .npz file not supported yet");
  return 0;
}

// adopted from getUniFileSize
void getNpzFileSize(
    const string &name, int &x, int &y, int &z, int *t = NULL, std::string *info = NULL)
{
  x = y = z = 0;
#if NO_ZLIB != 1
  debMsg("file format not supported without zlib", 1);
  return;
#endif
#if FLOATINGPOINT_PRECISION != 1
  errMsg("getNpzFileSize: Double precision not yet supported");
#endif
  // find suffix to differentiate between npy <-> npz
  cnpy::NpyArray gridArr;
  cnpy::npz_t fNpz = cnpy::npz_load(name);
  gridArr = fNpz["arr_0"];

  z = gridArr.shape[0];
  y = gridArr.shape[1];
  x = gridArr.shape[2];
  if (t)
    (*t) = 0;  // unused for now
}
Vec3 getNpzFileSize(const string &name)
{
  int x, y, z;
  getNpzFileSize(name, x, y, z);
  return Vec3(Real(x), Real(y), Real(z));
}
static PyObject *_W_2(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "getNpzFileSize", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      const string &name = _args.get<string>("name", 0, &_lock);
      _retval = toPy(getNpzFileSize(name));
      _args.check();
    }
    pbFinalizePlugin(parent, "getNpzFileSize", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("getNpzFileSize", e.what());
    return 0;
  }
}
static const Pb::Register _RP_getNpzFileSize("", "getNpzFileSize", _W_2);
extern "C" {
void PbRegister_getNpzFileSize()
{
  KEEP_UNUSED(_RP_getNpzFileSize);
}
}

//*****************************************************************************
// helper functions

void quantizeReal(Real &v, const Real step)
{
  int q = int(v / step + step * 0.5);
  double qd = q * (double)step;
  v = (Real)qd;
}
struct knQuantize : public KernelBase {
  knQuantize(Grid<Real> &grid, Real step) : KernelBase(&grid, 0), grid(grid), step(step)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<Real> &grid, Real step) const
  {
    quantizeReal(grid(idx), step);
  }
  inline Grid<Real> &getArg0()
  {
    return grid;
  }
  typedef Grid<Real> type0;
  inline Real &getArg1()
  {
    return step;
  }
  typedef Real type1;
  void runMessage()
  {
    debMsg("Executing kernel knQuantize ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, grid, step);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Real> &grid;
  Real step;
};
void quantizeGrid(Grid<Real> &grid, Real step)
{
  knQuantize(grid, step);
}
static PyObject *_W_3(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "quantizeGrid", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Real> &grid = *_args.getPtr<Grid<Real>>("grid", 0, &_lock);
      Real step = _args.get<Real>("step", 1, &_lock);
      _retval = getPyNone();
      quantizeGrid(grid, step);
      _args.check();
    }
    pbFinalizePlugin(parent, "quantizeGrid", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("quantizeGrid", e.what());
    return 0;
  }
}
static const Pb::Register _RP_quantizeGrid("", "quantizeGrid", _W_3);
extern "C" {
void PbRegister_quantizeGrid()
{
  KEEP_UNUSED(_RP_quantizeGrid);
}
}

struct knQuantizeVec3 : public KernelBase {
  knQuantizeVec3(Grid<Vec3> &grid, Real step) : KernelBase(&grid, 0), grid(grid), step(step)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, Grid<Vec3> &grid, Real step) const
  {
    for (int c = 0; c < 3; ++c)
      quantizeReal(grid(idx)[c], step);
  }
  inline Grid<Vec3> &getArg0()
  {
    return grid;
  }
  typedef Grid<Vec3> type0;
  inline Real &getArg1()
  {
    return step;
  }
  typedef Real type1;
  void runMessage()
  {
    debMsg("Executing kernel knQuantizeVec3 ", 3);
    debMsg("Kernel range"
               << " x " << maxX << " y " << maxY << " z " << minZ << " - " << maxZ << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, grid, step);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  Grid<Vec3> &grid;
  Real step;
};
void quantizeGridVec3(Grid<Vec3> &grid, Real step)
{
  knQuantizeVec3(grid, step);
}
static PyObject *_W_4(PyObject *_self, PyObject *_linargs, PyObject *_kwds)
{
  try {
    PbArgs _args(_linargs, _kwds);
    FluidSolver *parent = _args.obtainParent();
    bool noTiming = _args.getOpt<bool>("notiming", -1, 0);
    pbPreparePlugin(parent, "quantizeGridVec3", !noTiming);
    PyObject *_retval = 0;
    {
      ArgLocker _lock;
      Grid<Vec3> &grid = *_args.getPtr<Grid<Vec3>>("grid", 0, &_lock);
      Real step = _args.get<Real>("step", 1, &_lock);
      _retval = getPyNone();
      quantizeGridVec3(grid, step);
      _args.check();
    }
    pbFinalizePlugin(parent, "quantizeGridVec3", !noTiming);
    return _retval;
  }
  catch (std::exception &e) {
    pbSetError("quantizeGridVec3", e.what());
    return 0;
  }
}
static const Pb::Register _RP_quantizeGridVec3("", "quantizeGridVec3", _W_4);
extern "C" {
void PbRegister_quantizeGridVec3()
{
  KEEP_UNUSED(_RP_quantizeGridVec3);
}
}

// explicit instantiation
template int writeGridRaw<int>(const string &name, Grid<int> *grid);
template int writeGridRaw<Real>(const string &name, Grid<Real> *grid);
template int writeGridRaw<Vec3>(const string &name, Grid<Vec3> *grid);
template int writeGridUni<int>(const string &name, Grid<int> *grid);
template int writeGridUni<Real>(const string &name, Grid<Real> *grid);
template int writeGridUni<Vec3>(const string &name, Grid<Vec3> *grid);
template int writeGridVol<int>(const string &name, Grid<int> *grid);
template int writeGridVol<Vec3>(const string &name, Grid<Vec3> *grid);
template int writeGridTxt<int>(const string &name, Grid<int> *grid);
template int writeGridTxt<Real>(const string &name, Grid<Real> *grid);
template int writeGridTxt<Vec3>(const string &name, Grid<Vec3> *grid);

template int readGridRaw<int>(const string &name, Grid<int> *grid);
template int readGridRaw<Real>(const string &name, Grid<Real> *grid);
template int readGridRaw<Vec3>(const string &name, Grid<Vec3> *grid);
template int readGridUni<int>(const string &name, Grid<int> *grid);
template int readGridUni<Real>(const string &name, Grid<Real> *grid);
template int readGridUni<Vec3>(const string &name, Grid<Vec3> *grid);
template int readGridVol<int>(const string &name, Grid<int> *grid);
template int readGridVol<Vec3>(const string &name, Grid<Vec3> *grid);

template int readGrid4dUni<int>(
    const string &name, Grid4d<int> *grid, int readTslice, Grid4d<int> *slice, void **fileHandle);
template int readGrid4dUni<Real>(const string &name,
                                 Grid4d<Real> *grid,
                                 int readTslice,
                                 Grid4d<Real> *slice,
                                 void **fileHandle);
template int readGrid4dUni<Vec3>(const string &name,
                                 Grid4d<Vec3> *grid,
                                 int readTslice,
                                 Grid4d<Vec3> *slice,
                                 void **fileHandle);
template int readGrid4dUni<Vec4>(const string &name,
                                 Grid4d<Vec4> *grid,
                                 int readTslice,
                                 Grid4d<Vec4> *slice,
                                 void **fileHandle);
template int writeGrid4dUni<int>(const string &name, Grid4d<int> *grid);
template int writeGrid4dUni<Real>(const string &name, Grid4d<Real> *grid);
template int writeGrid4dUni<Vec3>(const string &name, Grid4d<Vec3> *grid);
template int writeGrid4dUni<Vec4>(const string &name, Grid4d<Vec4> *grid);

template int readGrid4dRaw<int>(const string &name, Grid4d<int> *grid);
template int readGrid4dRaw<Real>(const string &name, Grid4d<Real> *grid);
template int readGrid4dRaw<Vec3>(const string &name, Grid4d<Vec3> *grid);
template int readGrid4dRaw<Vec4>(const string &name, Grid4d<Vec4> *grid);
template int writeGrid4dRaw<int>(const string &name, Grid4d<int> *grid);
template int writeGrid4dRaw<Real>(const string &name, Grid4d<Real> *grid);
template int writeGrid4dRaw<Vec3>(const string &name, Grid4d<Vec3> *grid);
template int writeGrid4dRaw<Vec4>(const string &name, Grid4d<Vec4> *grid);

template int writeGridNumpy<int>(const string &name, Grid<int> *grid);
template int writeGridNumpy<Real>(const string &name, Grid<Real> *grid);
template int writeGridNumpy<Vec3>(const string &name, Grid<Vec3> *grid);
template int readGridNumpy<int>(const string &name, Grid<int> *grid);
template int readGridNumpy<Real>(const string &name, Grid<Real> *grid);
template int readGridNumpy<Vec3>(const string &name, Grid<Vec3> *grid);

}  // namespace Manta
