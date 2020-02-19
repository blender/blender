

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

namespace Manta {

// forward decl.
class Mesh;
class FlagGrid;
template<class T> class Grid;
template<class T> class Grid4d;
class BasicParticleSystem;
template<class T> class ParticleDataImpl;
template<class T> class MeshDataImpl;

void writeObjFile(const std::string &name, Mesh *mesh);
void writeBobjFile(const std::string &name, Mesh *mesh);
void readObjFile(const std::string &name, Mesh *mesh, bool append);
void readBobjFile(const std::string &name, Mesh *mesh, bool append);

template<class T> void writeGridRaw(const std::string &name, Grid<T> *grid);
template<class T> void writeGridUni(const std::string &name, Grid<T> *grid);
template<class T> void writeGridVol(const std::string &name, Grid<T> *grid);
template<class T> void writeGridTxt(const std::string &name, Grid<T> *grid);

#if OPENVDB == 1
template<class T> void writeGridVDB(const std::string &name, Grid<T> *grid);
template<class T> void readGridVDB(const std::string &name, Grid<T> *grid);
#endif  // OPENVDB==1
template<class T> void writeGridNumpy(const std::string &name, Grid<T> *grid);
template<class T> void readGridNumpy(const std::string &name, Grid<T> *grid);

template<class T> void readGridUni(const std::string &name, Grid<T> *grid);
template<class T> void readGridRaw(const std::string &name, Grid<T> *grid);
template<class T> void readGridVol(const std::string &name, Grid<T> *grid);

template<class T> void writeGrid4dUni(const std::string &name, Grid4d<T> *grid);
template<class T>
void readGrid4dUni(const std::string &name,
                   Grid4d<T> *grid,
                   int readTslice = -1,
                   Grid4d<T> *slice = NULL,
                   void **fileHandle = NULL);
void readGrid4dUniCleanup(void **fileHandle);
template<class T> void writeGrid4dRaw(const std::string &name, Grid4d<T> *grid);
template<class T> void readGrid4dRaw(const std::string &name, Grid4d<T> *grid);

void writeParticlesUni(const std::string &name, const BasicParticleSystem *parts);
void readParticlesUni(const std::string &name, BasicParticleSystem *parts);

template<class T> void writePdataUni(const std::string &name, ParticleDataImpl<T> *pdata);
template<class T> void readPdataUni(const std::string &name, ParticleDataImpl<T> *pdata);

template<class T> void writeMdataUni(const std::string &name, MeshDataImpl<T> *mdata);
template<class T> void readMdataUni(const std::string &name, MeshDataImpl<T> *mdata);

void getUniFileSize(
    const std::string &name, int &x, int &y, int &z, int *t = NULL, std::string *info = NULL);

void *safeGzopen(const char *filename, const char *mode);

}  // namespace Manta

#endif
