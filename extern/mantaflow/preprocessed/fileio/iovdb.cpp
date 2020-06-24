

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2020 Sebastian Barschkis, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Loading and writing grids and particles from and to OpenVDB files.
 *
 ******************************************************************************/

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>

#include "mantaio.h"
#include "grid.h"
#include "vector4d.h"
#include "grid4d.h"
#include "particle.h"

#if OPENVDB == 1
#  include "openvdb/openvdb.h"
#  include <openvdb/points/PointConversion.h>
#  include <openvdb/points/PointCount.h>
#endif

#define POSITION_NAME "P"
#define FLAG_NAME "U"

using namespace std;

namespace Manta {

#if OPENVDB == 1

template<class GridType, class T> void importVDB(typename GridType::Ptr from, Grid<T> *to)
{
  using ValueT = typename GridType::ValueType;
  typename GridType::Accessor accessor = from->getAccessor();

  FOR_IJK(*to)
  {
    openvdb::Coord xyz(i, j, k);
    ValueT vdbValue = accessor.getValue(xyz);
    T toMantaValue;
    convertFrom(vdbValue, &toMantaValue);
    to->set(i, j, k, toMantaValue);
  }
}

template<class VDBType, class T>
void importVDB(VDBType vdbValue, ParticleDataImpl<T> *to, int index, float voxelSize)
{
  (void)voxelSize;  // Unused
  T toMantaValue;
  convertFrom(vdbValue, &toMantaValue);
  to->set(index, toMantaValue);
}

void importVDB(openvdb::points::PointDataGrid::Ptr from,
               BasicParticleSystem *to,
               std::vector<ParticleDataBase *> &toPData,
               float voxelSize)
{
  openvdb::Index64 count = openvdb::points::pointCount(from->tree());
  to->resizeAll(count);

  int cnt = 0;
  for (auto leafIter = from->tree().cbeginLeaf(); leafIter; ++leafIter) {
    const openvdb::points::AttributeArray &positionArray = leafIter->constAttributeArray(
        POSITION_NAME);
    const openvdb::points::AttributeArray &flagArray = leafIter->constAttributeArray(FLAG_NAME);

    openvdb::points::AttributeHandle<openvdb::Vec3s> positionHandle(positionArray);
    openvdb::points::AttributeHandle<int> flagHandle(flagArray);

    // Get vdb handles to pdata objects in pdata list
    std::vector<std::tuple<int, openvdb::points::AttributeHandle<int>>> pDataHandlesInt;
    std::vector<std::tuple<int, openvdb::points::AttributeHandle<float>>> pDataHandlesReal;
    std::vector<std::tuple<int, openvdb::points::AttributeHandle<openvdb::Vec3s>>>
        pDataHandlesVec3;

    int pDataIndex = 0;
    for (ParticleDataBase *pdb : toPData) {
      std::string name = pdb->getName();
      const openvdb::points::AttributeArray &pDataArray = leafIter->constAttributeArray(name);

      if (pdb->getType() == ParticleDataBase::TypeInt) {
        openvdb::points::AttributeHandle<int> intHandle(pDataArray);
        std::tuple<int, openvdb::points::AttributeHandle<int>> tuple = std::make_tuple(pDataIndex,
                                                                                       intHandle);
        pDataHandlesInt.push_back(tuple);
      }
      else if (pdb->getType() == ParticleDataBase::TypeReal) {
        openvdb::points::AttributeHandle<float> floatHandle(pDataArray);
        std::tuple<int, openvdb::points::AttributeHandle<float>> tuple = std::make_tuple(
            pDataIndex, floatHandle);
        pDataHandlesReal.push_back(tuple);
      }
      else if (pdb->getType() == ParticleDataBase::TypeVec3) {
        openvdb::points::AttributeHandle<openvdb::Vec3s> vec3Handle(pDataArray);
        std::tuple<int, openvdb::points::AttributeHandle<openvdb::Vec3s>> tuple = std::make_tuple(
            pDataIndex, vec3Handle);
        pDataHandlesVec3.push_back(tuple);
      }
      else {
        errMsg("importVDB: unknown ParticleDataBase type");
      }
      ++pDataIndex;
    }

    for (auto indexIter = leafIter->beginIndexOn(); indexIter; ++indexIter) {
      // Extract the voxel-space position of the point (always between (-0.5, -0.5, -0.5) and (0.5,
      // 0.5, 0.5)).
      openvdb::Vec3s voxelPosition = positionHandle.get(*indexIter);
      const openvdb::Vec3d xyz = indexIter.getCoord().asVec3d();
      // Compute the world-space position of the point.
      openvdb::Vec3f worldPosition = from->transform().indexToWorld(voxelPosition + xyz);
      int flag = flagHandle.get(*indexIter);

      Vec3 toMantaValue;
      convertFrom(worldPosition, &toMantaValue);
      (*to)[cnt].pos = toMantaValue;
      (*to)[cnt].pos /= voxelSize;  // convert from world space to grid space
      (*to)[cnt].flag = flag;

      for (std::tuple<int, openvdb::points::AttributeHandle<int>> tuple : pDataHandlesInt) {
        int pDataIndex = std::get<0>(tuple);
        int vdbValue = std::get<1>(tuple).get(*indexIter);

        ParticleDataImpl<int> *pdi = dynamic_cast<ParticleDataImpl<int> *>(toPData[pDataIndex]);
        importVDB<int, int>(vdbValue, pdi, cnt, voxelSize);
      }
      for (std::tuple<int, openvdb::points::AttributeHandle<float>> tuple : pDataHandlesReal) {
        int pDataIndex = std::get<0>(tuple);
        float vdbValue = std::get<1>(tuple).get(*indexIter);

        ParticleDataImpl<Real> *pdi = dynamic_cast<ParticleDataImpl<Real> *>(toPData[pDataIndex]);
        importVDB<float, Real>(vdbValue, pdi, cnt, voxelSize);
      }
      for (std::tuple<int, openvdb::points::AttributeHandle<openvdb::Vec3s>> tuple :
           pDataHandlesVec3) {
        int pDataIndex = std::get<0>(tuple);
        openvdb::Vec3f voxelPosition = std::get<1>(tuple).get(*indexIter);

        ParticleDataImpl<Vec3> *pdi = dynamic_cast<ParticleDataImpl<Vec3> *>(toPData[pDataIndex]);
        importVDB<openvdb::Vec3s, Vec3>(voxelPosition, pdi, cnt, voxelSize);
      }
      ++cnt;
    }
  }
}

template<class GridType>
static void setGridOptions(typename GridType::Ptr grid,
                           string name,
                           openvdb::GridClass cls,
                           float voxelSize,
                           bool precisionHalf)
{
  grid->setTransform(openvdb::math::Transform::createLinearTransform(voxelSize));
  grid->setGridClass(cls);
  grid->setName(name);
  grid->setSaveFloatAsHalf(precisionHalf);
}

template<class T, class GridType> typename GridType::Ptr exportVDB(Grid<T> *from)
{
  using ValueT = typename GridType::ValueType;
  typename GridType::Ptr to = GridType::create();
  typename GridType::Accessor accessor = to->getAccessor();

  FOR_IJK(*from)
  {
    openvdb::Coord xyz(i, j, k);
    T fromMantaValue = (*from)(i, j, k);
    ValueT vdbValue;
    convertTo(&vdbValue, fromMantaValue);
    accessor.setValue(xyz, vdbValue);
  }
  return to;
}

template<class MantaType, class VDBType>
void exportVDB(ParticleDataImpl<MantaType> *from,
               openvdb::points::PointDataGrid::Ptr to,
               openvdb::tools::PointIndexGrid::Ptr pIndex,
               bool skipDeletedParts)
{
  std::vector<VDBType> vdbValues;
  std::string name = from->getName();

  FOR_PARTS(*from)
  {
    // Optionally, skip exporting particles that have been marked as deleted
    BasicParticleSystem *pp = dynamic_cast<BasicParticleSystem *>(from->getParticleSys());
    if (skipDeletedParts && !pp->isActive(idx)) {
      continue;
    }
    MantaType fromMantaValue = (*from)[idx];
    VDBType vdbValue;
    convertTo(&vdbValue, fromMantaValue);
    vdbValues.push_back(vdbValue);
  }

  openvdb::NamePair attribute =
      openvdb::points::TypedAttributeArray<VDBType, openvdb::points::NullCodec>::attributeType();
  openvdb::points::appendAttribute(to->tree(), name, attribute);

  // Create a wrapper around the vdb values vector.
  const openvdb::points::PointAttributeVector<VDBType> wrapper(vdbValues);

  // Populate the attribute on the points
  openvdb::points::populateAttribute<openvdb::points::PointDataTree,
                                     openvdb::tools::PointIndexTree,
                                     openvdb::points::PointAttributeVector<VDBType>>(
      to->tree(), pIndex->tree(), name, wrapper);
}

openvdb::points::PointDataGrid::Ptr exportVDB(BasicParticleSystem *from,
                                              std::vector<ParticleDataBase *> &fromPData,
                                              bool skipDeletedParts,
                                              float voxelSize)
{
  std::vector<openvdb::Vec3s> positions;
  std::vector<int> flags;

  FOR_PARTS(*from)
  {
    // Optionally, skip exporting particles that have been marked as deleted
    if (skipDeletedParts && !from->isActive(idx)) {
      continue;
    }
    Vector3D<float> pos = toVec3f((*from)[idx].pos);
    pos *= voxelSize;  // convert from grid space to world space
    openvdb::Vec3s posVDB(pos.x, pos.y, pos.z);
    positions.push_back(posVDB);

    int flag = (*from)[idx].flag;
    flags.push_back(flag);
  }

  const openvdb::points::PointAttributeVector<openvdb::Vec3s> positionsWrapper(positions);
  openvdb::math::Transform::Ptr transform = openvdb::math::Transform::createLinearTransform(
      voxelSize);

  openvdb::tools::PointIndexGrid::Ptr pointIndexGrid =
      openvdb::tools::createPointIndexGrid<openvdb::tools::PointIndexGrid>(positionsWrapper,
                                                                           *transform);

  // TODO (sebbas): Use custom codec for attributes?
  // using Codec = openvdb::points::FixedPointCodec</*1-byte=*/false, openvdb::points::UnitRange>;
  openvdb::points::PointDataGrid::Ptr to =
      openvdb::points::createPointDataGrid<openvdb::points::NullCodec /*Codec*/,
                                           openvdb::points::PointDataGrid>(
          *pointIndexGrid, positionsWrapper, *transform);

  openvdb::NamePair flagAttribute =
      openvdb::points::TypedAttributeArray<int,
                                           openvdb::points::NullCodec /*Codec*/>::attributeType();
  openvdb::points::appendAttribute(to->tree(), FLAG_NAME, flagAttribute);
  // Create a wrapper around the flag vector.
  openvdb::points::PointAttributeVector<int> flagWrapper(flags);
  // Populate the "flag" attribute on the points
  openvdb::points::populateAttribute<openvdb::points::PointDataTree,
                                     openvdb::tools::PointIndexTree,
                                     openvdb::points::PointAttributeVector<int>>(
      to->tree(), pointIndexGrid->tree(), FLAG_NAME, flagWrapper);

  // Add all already buffered pdata to this particle grid
  for (ParticleDataBase *pdb : fromPData) {
    if (pdb->getType() == ParticleDataBase::TypeInt) {
      debMsg("Writing int particle data '" << pdb->getName() << "'", 1);
      ParticleDataImpl<int> *pdi = dynamic_cast<ParticleDataImpl<int> *>(pdb);
      exportVDB<int, int>(pdi, to, pointIndexGrid, skipDeletedParts);
    }
    else if (pdb->getType() == ParticleDataBase::TypeReal) {
      debMsg("Writing real particle data '" << pdb->getName() << "'", 1);
      ParticleDataImpl<Real> *pdi = dynamic_cast<ParticleDataImpl<Real> *>(pdb);
      exportVDB<Real, float>(pdi, to, pointIndexGrid, skipDeletedParts);
    }
    else if (pdb->getType() == ParticleDataBase::TypeVec3) {
      debMsg("Writing Vec3 particle data '" << pdb->getName() << "'", 1);
      ParticleDataImpl<Vec3> *pdi = dynamic_cast<ParticleDataImpl<Vec3> *>(pdb);
      exportVDB<Vec3, openvdb::Vec3s>(pdi, to, pointIndexGrid, skipDeletedParts);
    }
    else {
      errMsg("exportVDB: unknown ParticleDataBase type");
    }
  }
  return to;
}

static void registerCustomCodecs()
{
  using Codec = openvdb::points::FixedPointCodec</*1-byte=*/false, openvdb::points::UnitRange>;
  openvdb::points::TypedAttributeArray<int, Codec>::registerType();
}

int writeObjectsVDB(const string &filename,
                    std::vector<PbClass *> *objects,
                    float worldSize,
                    bool skipDeletedParts,
                    int compression,
                    bool precisionHalf)
{
  openvdb::initialize();
  openvdb::io::File file(filename);
  openvdb::GridPtrVec gridsVDB;

  // TODO (sebbas): Use custom codec for flag attribute?
  // Register codecs one, this makes sure custom attributes can be read
  // registerCustomCodecs();

  std::vector<ParticleDataBase *> pdbBuffer;

  for (std::vector<PbClass *>::iterator iter = objects->begin(); iter != objects->end(); ++iter) {
    openvdb::GridClass gClass = openvdb::GRID_UNKNOWN;
    openvdb::GridBase::Ptr vdbGrid;

    PbClass *object = dynamic_cast<PbClass *>(*iter);
    const Real dx = object->getParent()->getDx();
    const Real voxelSize = worldSize * dx;
    const string objectName = object->getName();

    if (GridBase *mantaGrid = dynamic_cast<GridBase *>(*iter)) {

      if (mantaGrid->getType() & GridBase::TypeInt) {
        debMsg("Writing int grid '" << mantaGrid->getName() << "' to vdb file " << filename, 1);
        Grid<int> *mantaIntGrid = (Grid<int> *)mantaGrid;
        vdbGrid = exportVDB<int, openvdb::Int32Grid>(mantaIntGrid);
        gridsVDB.push_back(vdbGrid);
      }
      else if (mantaGrid->getType() & GridBase::TypeReal) {
        debMsg("Writing real grid '" << mantaGrid->getName() << "' to vdb file " << filename, 1);
        gClass = (mantaGrid->getType() & GridBase::TypeLevelset) ? openvdb::GRID_LEVEL_SET :
                                                                   openvdb::GRID_FOG_VOLUME;
        Grid<Real> *mantaRealGrid = (Grid<Real> *)mantaGrid;
        vdbGrid = exportVDB<Real, openvdb::FloatGrid>(mantaRealGrid);
        gridsVDB.push_back(vdbGrid);
      }
      else if (mantaGrid->getType() & GridBase::TypeVec3) {
        debMsg("Writing vec3 grid '" << mantaGrid->getName() << "' to vdb file " << filename, 1);
        gClass = (mantaGrid->getType() & GridBase::TypeMAC) ? openvdb::GRID_STAGGERED :
                                                              openvdb::GRID_UNKNOWN;
        Grid<Vec3> *mantaVec3Grid = (Grid<Vec3> *)mantaGrid;
        vdbGrid = exportVDB<Vec3, openvdb::Vec3SGrid>(mantaVec3Grid);
        gridsVDB.push_back(vdbGrid);
      }
      else {
        errMsg("writeObjectsVDB: unknown grid type");
        return 0;
      }
    }
    else if (BasicParticleSystem *mantaPP = dynamic_cast<BasicParticleSystem *>(*iter)) {
      debMsg("Writing particle system '" << mantaPP->getName()
                                         << "' (and buffered pData) to vdb file " << filename,
             1);
      vdbGrid = exportVDB(mantaPP, pdbBuffer, skipDeletedParts, voxelSize);
      gridsVDB.push_back(vdbGrid);
      pdbBuffer.clear();
    }
    // Particle data will only be saved if there is a particle system too.
    else if (ParticleDataBase *mantaPPImpl = dynamic_cast<ParticleDataBase *>(*iter)) {
      debMsg("Buffering particle data '" << mantaPPImpl->getName() << "' to vdb file " << filename,
             1);
      pdbBuffer.push_back(mantaPPImpl);
    }
    else {
      errMsg("writeObjectsVDB: Unsupported Python object. Cannot write to .vdb file " << filename);
      return 0;
    }

    // Set additional grid attributes, e.g. name, grid class, compression level, etc.
    if (vdbGrid) {
      setGridOptions<openvdb::GridBase>(vdbGrid, objectName, gClass, voxelSize, precisionHalf);
    }
  }

  // Give out a warning if pData items were present but could not be saved due to missing particle
  // system.
  if (!pdbBuffer.empty()) {
    for (ParticleDataBase *pdb : pdbBuffer) {
      debMsg("writeObjectsVDB Warning: Particle data '"
                 << pdb->getName()
                 << "' has not been saved. It's parent particle system was needs to be given too.",
             1);
    }
  }

  // Write only if the is at least one grid, optionally write with compression.
  if (gridsVDB.size()) {
    int vdb_flags = openvdb::io::COMPRESS_ACTIVE_MASK;
    switch (compression) {
      case COMPRESSION_NONE: {
        vdb_flags = openvdb::io::COMPRESS_NONE;
        break;
      }
      case COMPRESSION_ZIP: {
        vdb_flags |= openvdb::io::COMPRESS_ZIP;
        break;
      }
      case COMPRESSION_BLOSC: {
#  if OPENVDB_BLOSC == 1
        vdb_flags |= openvdb::io::COMPRESS_BLOSC;
#  else
        debMsg("OpenVDB was built without Blosc support, using Zip compression instead", 1);
        vdb_flags |= openvdb::io::COMPRESS_ZIP;
#  endif  // OPENVDB_BLOSC==1
        break;
      }
    }
    file.setCompression(vdb_flags);
    file.write(gridsVDB);
  }
  file.close();
  return 1;
}

int readObjectsVDB(const string &filename, std::vector<PbClass *> *objects, float worldSize)
{

  openvdb::initialize();
  openvdb::io::File file(filename);
  openvdb::GridPtrVec gridsVDB;

  // TODO (sebbas): Use custom codec for flag attribute?
  // Register codecs one, this makes sure custom attributes can be read
  // registerCustomCodecs();

  try {
    file.setCopyMaxBytes(0);
    file.open();
    gridsVDB = *(file.getGrids());
    openvdb::MetaMap::Ptr metadata = file.getMetadata();
    (void)metadata;  // Unused for now
  }
  catch (const openvdb::IoError &e) {
    debMsg("readObjectsVDB: Could not open vdb file " << filename, 1);
    file.close();
    return 0;
  }
  file.close();

  // A buffer to store a handle to pData objects. These will be read alongside a particle system.
  std::vector<ParticleDataBase *> pdbBuffer;

  for (std::vector<PbClass *>::iterator iter = objects->begin(); iter != objects->end(); ++iter) {

    if (gridsVDB.empty()) {
      debMsg("readObjectsVDB: No vdb grids in file " << filename, 1);
    }
    // If there is just one grid in this file, load it regardless of name match (to vdb caches per
    // grid).
    bool onlyGrid = (gridsVDB.size() == 1);

    PbClass *object = dynamic_cast<PbClass *>(*iter);
    const Real dx = object->getParent()->getDx();
    const Real voxelSize = worldSize * dx;

    // Particle data objects are treated separately - buffered and inserted when reading the
    // particle system
    if (ParticleDataBase *mantaPPImpl = dynamic_cast<ParticleDataBase *>(*iter)) {
      debMsg("Buffering particle data '" << mantaPPImpl->getName() << "' from vdb file "
                                         << filename,
             1);
      pdbBuffer.push_back(mantaPPImpl);
      continue;
    }

    // For every manta object, we loop through the vdb grid list and check for a match
    for (const openvdb::GridBase::Ptr &vdbGrid : gridsVDB) {
      bool nameMatch = (vdbGrid->getName() == (*iter)->getName());

      // Sanity checks: Only load valid grids and make sure names match.
      if (!vdbGrid) {
        debMsg("Skipping invalid vdb grid '" << vdbGrid->getName() << "' in file " << filename, 1);
        continue;
      }
      if (!nameMatch && !onlyGrid) {
        continue;
      }
      if (GridBase *mantaGrid = dynamic_cast<GridBase *>(*iter)) {

        if (mantaGrid->getType() & GridBase::TypeInt) {
          debMsg("Reading into grid '" << mantaGrid->getName() << "' from int grid '"
                                       << vdbGrid->getName() << "' in vdb file " << filename,
                 1);
          openvdb::Int32Grid::Ptr vdbIntGrid = openvdb::gridPtrCast<openvdb::Int32Grid>(vdbGrid);
          Grid<int> *mantaIntGrid = (Grid<int> *)mantaGrid;
          importVDB<openvdb::Int32Grid, int>(vdbIntGrid, mantaIntGrid);
        }
        else if (mantaGrid->getType() & GridBase::TypeReal) {
          debMsg("Reading into grid '" << mantaGrid->getName() << "' from real grid '"
                                       << vdbGrid->getName() << "' in vdb file " << filename,
                 1);
          openvdb::FloatGrid::Ptr vdbFloatGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(vdbGrid);
          Grid<Real> *mantaRealGrid = (Grid<Real> *)mantaGrid;
          importVDB<openvdb::FloatGrid, Real>(vdbFloatGrid, mantaRealGrid);
        }
        else if (mantaGrid->getType() & GridBase::TypeVec3) {
          debMsg("Reading into grid '" << mantaGrid->getName() << "' from vec3 grid '"
                                       << vdbGrid->getName() << "' in vdb file " << filename,
                 1);
          openvdb::Vec3SGrid::Ptr vdbVec3Grid = openvdb::gridPtrCast<openvdb::Vec3SGrid>(vdbGrid);
          Grid<Vec3> *mantaVec3Grid = (Grid<Vec3> *)mantaGrid;
          importVDB<openvdb::Vec3SGrid, Vec3>(vdbVec3Grid, mantaVec3Grid);
        }
        else {
          errMsg("readObjectsVDB: unknown grid type");
          return 0;
        }
      }
      else if (BasicParticleSystem *mantaPP = dynamic_cast<BasicParticleSystem *>(*iter)) {
        debMsg("Reading into particle system '" << mantaPP->getName() << "' from particle system '"
                                                << vdbGrid->getName() << "' in vdb file "
                                                << filename,
               1);
        openvdb::points::PointDataGrid::Ptr vdbPointGrid =
            openvdb::gridPtrCast<openvdb::points::PointDataGrid>(vdbGrid);
        importVDB(vdbPointGrid, mantaPP, pdbBuffer, voxelSize);
        pdbBuffer.clear();
      }
      else {
        errMsg("readObjectsVDB: Unsupported Python object. Cannot read from .vdb file "
               << filename);
        return 0;
      }
    }
  }

  // Give out a warning if pData items were present but could not be read due to missing particle
  // system.
  if (!pdbBuffer.empty()) {
    for (ParticleDataBase *pdb : pdbBuffer) {
      debMsg("readObjectsVDB Warning: Particle data '"
                 << pdb->getName()
                 << "' has not been read. The parent particle system needs to be given too.",
             1);
    }
  }

  return 1;
}

template void importVDB<int, int>(int vdbValue,
                                  ParticleDataImpl<int> *to,
                                  int index,
                                  float voxelSize = 1.0);
template void importVDB<float, Real>(float vdbValue,
                                     ParticleDataImpl<Real> *to,
                                     int index,
                                     float voxelSize = 1.0);
template void importVDB<openvdb::Vec3f, Vec3>(openvdb::Vec3s vdbValue,
                                              ParticleDataImpl<Vec3> *to,
                                              int index,
                                              float voxelSize = 1.0);

void importVDB(openvdb::points::PointDataGrid::Ptr from,
               BasicParticleSystem *to,
               std::vector<ParticleDataBase *> &toPData,
               float voxelSize = 1.0);
template void importVDB<openvdb::Int32Grid, int>(openvdb::Int32Grid::Ptr from, Grid<int> *to);
template void importVDB<openvdb::FloatGrid, Real>(openvdb::FloatGrid::Ptr from, Grid<Real> *to);
template void importVDB<openvdb::Vec3SGrid, Vec3>(openvdb::Vec3SGrid::Ptr from, Grid<Vec3> *to);

template openvdb::Int32Grid::Ptr exportVDB<int, openvdb::Int32Grid>(Grid<int> *from);
template openvdb::FloatGrid::Ptr exportVDB<Real, openvdb::FloatGrid>(Grid<Real> *from);
template openvdb::Vec3SGrid::Ptr exportVDB<Vec3, openvdb::Vec3SGrid>(Grid<Vec3> *from);

openvdb::points::PointDataGrid::Ptr exportVDB(BasicParticleSystem *from,
                                              std::vector<ParticleDataBase *> &fromPData,
                                              bool skipDeletedParts = false,
                                              float voxelSize = 1.0);
template void exportVDB<int, int>(ParticleDataImpl<int> *from,
                                  openvdb::points::PointDataGrid::Ptr to,
                                  openvdb::tools::PointIndexGrid::Ptr pIndex,
                                  bool skipDeletedParts = false);
template void exportVDB<Real, float>(ParticleDataImpl<Real> *from,
                                     openvdb::points::PointDataGrid::Ptr to,
                                     openvdb::tools::PointIndexGrid::Ptr pIndex,
                                     bool skipDeletedParts = false);
template void exportVDB<Vec3, openvdb::Vec3s>(ParticleDataImpl<Vec3> *from,
                                              openvdb::points::PointDataGrid::Ptr to,
                                              openvdb::tools::PointIndexGrid::Ptr pIndex,
                                              bool skipDeletedParts = false);

#else

int writeObjectsVDB(const string &filename,
                    std::vector<PbClass *> *objects,
                    float worldSize,
                    bool skipDeletedParts,
                    int compression,
                    bool precisionHalf)
{
  errMsg("Cannot save to .vdb file. Mantaflow has not been built with OpenVDB support.");
  return 0;
}

int readObjectsVDB(const string &filename, std::vector<PbClass *> *objects, float worldSize)
{
  errMsg("Cannot load from .vdb file. Mantaflow has not been built with OpenVDB support.");
  return 0;
}

#endif  // OPENVDB==1

}  // namespace Manta
