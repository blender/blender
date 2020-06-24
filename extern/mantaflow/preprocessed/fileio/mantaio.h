

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Loading and writing grids and meshes to disk
 *
 ******************************************************************************/

#ifndef _FILEIO_H
#define _FILEIO_H

#include <string>

#include "manta.h"

// OpenVDB compression flags
#define COMPRESSION_NONE 0
#define COMPRESSION_ZIP 1
#define COMPRESSION_BLOSC 2

namespace Manta {

// Forward declations
class Mesh;
class FlagGrid;
class GridBase;
template<class T> class Grid;
template<class T> class Grid4d;
class BasicParticleSystem;
template<class T> class ParticleDataImpl;
template<class T> class MeshDataImpl;

// Obj format
int writeObjFile(const std::string &name, Mesh *mesh);
int writeBobjFile(const std::string &name, Mesh *mesh);
int readObjFile(const std::string &name, Mesh *mesh, bool append);
int readBobjFile(const std::string &name, Mesh *mesh, bool append);

// Other formats (Raw, Uni, Vol)
template<class T> int readGridUni(const std::string &name, Grid<T> *grid);
template<class T> int readGridRaw(const std::string &name, Grid<T> *grid);
template<class T> int readGridVol(const std::string &name, Grid<T> *grid);
int readGridsRaw(const std::string &name, std::vector<PbClass *> *grids);
int readGridsUni(const std::string &name, std::vector<PbClass *> *grids);
int readGridsVol(const std::string &name, std::vector<PbClass *> *grids);
int readGridsTxt(const std::string &name, std::vector<PbClass *> *grids);

template<class T> int writeGridRaw(const std::string &name, Grid<T> *grid);
template<class T> int writeGridUni(const std::string &name, Grid<T> *grid);
template<class T> int writeGridVol(const std::string &name, Grid<T> *grid);
template<class T> int writeGridTxt(const std::string &name, Grid<T> *grid);
int writeGridsRaw(const std::string &name, std::vector<PbClass *> *grids);
int writeGridsUni(const std::string &name, std::vector<PbClass *> *grids);
int writeGridsVol(const std::string &name, std::vector<PbClass *> *grids);
int writeGridsTxt(const std::string &name, std::vector<PbClass *> *grids);

// OpenVDB
int writeObjectsVDB(const std::string &filename,
                    std::vector<PbClass *> *objects,
                    float scale = 1.0,
                    bool skipDeletedParts = false,
                    int compression = COMPRESSION_ZIP,
                    bool precisionHalf = true);
int readObjectsVDB(const std::string &filename,
                   std::vector<PbClass *> *objects,
                   float scale = 1.0);

// Numpy
template<class T> int writeGridNumpy(const std::string &name, Grid<T> *grid);
template<class T> int readGridNumpy(const std::string &name, Grid<T> *grid);

int writeGridsNumpy(const std::string &name, std::vector<PbClass *> *grids);
int readGridsNumpy(const std::string &name, std::vector<PbClass *> *grids);

// 4D Grids
template<class T> int writeGrid4dUni(const std::string &name, Grid4d<T> *grid);
template<class T>
int readGrid4dUni(const std::string &name,
                  Grid4d<T> *grid,
                  int readTslice = -1,
                  Grid4d<T> *slice = NULL,
                  void **fileHandle = NULL);
void readGrid4dUniCleanup(void **fileHandle);
template<class T> int writeGrid4dRaw(const std::string &name, Grid4d<T> *grid);
template<class T> int readGrid4dRaw(const std::string &name, Grid4d<T> *grid);

// Particles + particle data
int writeParticlesUni(const std::string &name, const BasicParticleSystem *parts);
int readParticlesUni(const std::string &name, BasicParticleSystem *parts);

template<class T> int writePdataUni(const std::string &name, ParticleDataImpl<T> *pdata);
template<class T> int readPdataUni(const std::string &name, ParticleDataImpl<T> *pdata);

// Mesh data
template<class T> int writeMdataUni(const std::string &name, MeshDataImpl<T> *mdata);
template<class T> int readMdataUni(const std::string &name, MeshDataImpl<T> *mdata);

// Helpers
void getUniFileSize(
    const std::string &name, int &x, int &y, int &z, int *t = NULL, std::string *info = NULL);
void *safeGzopen(const char *filename, const char *mode);
#if OPENVDB == 1
template<class S, class T> void convertFrom(S &in, T *out);
template<class S, class T> void convertTo(S *out, T &in);
#endif

}  // namespace Manta

#endif
