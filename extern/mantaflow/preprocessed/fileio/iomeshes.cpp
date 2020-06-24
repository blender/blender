

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
#if NO_ZLIB != 1
extern "C" {
#  include <zlib.h>
}
#endif

#include "mantaio.h"
#include "grid.h"
#include "mesh.h"
#include "vortexsheet.h"
#include <cstring>

using namespace std;

namespace Manta {

static const int STR_LEN_PDATA = 256;

//! mdata uni header, v3  (similar to grid header and mdata header)
typedef struct {
  int dim;               // number of vertices
  int dimX, dimY, dimZ;  // underlying solver resolution (all data in local coordinates!)
  int elementType, bytesPerElement;  // type id and byte size
  char info[STR_LEN_PDATA];          // mantaflow build information
  unsigned long long timestamp;      // creation time
} UniMeshHeader;

//*****************************************************************************
// conversion functions for double precision
// (note - uni files always store single prec. values)
//*****************************************************************************

#if NO_ZLIB != 1

template<class T>
void mdataConvertWrite(gzFile &gzf, MeshDataImpl<T> &mdata, void *ptr, UniMeshHeader &head)
{
  errMsg("mdataConvertWrite: unknown type, not yet supported");
}

template<>
void mdataConvertWrite(gzFile &gzf, MeshDataImpl<int> &mdata, void *ptr, UniMeshHeader &head)
{
  gzwrite(gzf, &head, sizeof(UniMeshHeader));
  gzwrite(gzf, &mdata[0], sizeof(int) * head.dim);
}
template<>
void mdataConvertWrite(gzFile &gzf, MeshDataImpl<double> &mdata, void *ptr, UniMeshHeader &head)
{
  head.bytesPerElement = sizeof(float);
  gzwrite(gzf, &head, sizeof(UniMeshHeader));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < mdata.size(); ++i, ++ptrf) {
    *ptrf = (float)mdata[i];
  }
  gzwrite(gzf, ptr, sizeof(float) * head.dim);
}
template<>
void mdataConvertWrite(gzFile &gzf, MeshDataImpl<Vec3> &mdata, void *ptr, UniMeshHeader &head)
{
  head.bytesPerElement = sizeof(Vector3D<float>);
  gzwrite(gzf, &head, sizeof(UniMeshHeader));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < mdata.size(); ++i) {
    for (int c = 0; c < 3; ++c) {
      *ptrf = (float)mdata[i][c];
      ptrf++;
    }
  }
  gzwrite(gzf, ptr, sizeof(Vector3D<float>) * head.dim);
}

template<class T>
void mdataReadConvert(gzFile &gzf, MeshDataImpl<T> &grid, void *ptr, int bytesPerElement)
{
  errMsg("mdataReadConvert: unknown mdata type, not yet supported");
}

template<>
void mdataReadConvert<int>(gzFile &gzf, MeshDataImpl<int> &mdata, void *ptr, int bytesPerElement)
{
  gzread(gzf, ptr, sizeof(int) * mdata.size());
  assertMsg(bytesPerElement == sizeof(int),
            "mdata element size doesn't match " << bytesPerElement << " vs " << sizeof(int));
  // int dont change in double precision mode - copy over
  memcpy(&(mdata[0]), ptr, sizeof(int) * mdata.size());
}

template<>
void mdataReadConvert<double>(gzFile &gzf,
                              MeshDataImpl<double> &mdata,
                              void *ptr,
                              int bytesPerElement)
{
  gzread(gzf, ptr, sizeof(float) * mdata.size());
  assertMsg(bytesPerElement == sizeof(float),
            "mdata element size doesn't match " << bytesPerElement << " vs " << sizeof(float));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < mdata.size(); ++i, ++ptrf) {
    mdata[i] = double(*ptrf);
  }
}

template<>
void mdataReadConvert<Vec3>(gzFile &gzf, MeshDataImpl<Vec3> &mdata, void *ptr, int bytesPerElement)
{
  gzread(gzf, ptr, sizeof(Vector3D<float>) * mdata.size());
  assertMsg(bytesPerElement == sizeof(Vector3D<float>),
            "mdata element size doesn't match " << bytesPerElement << " vs "
                                                << sizeof(Vector3D<float>));
  float *ptrf = (float *)ptr;
  for (int i = 0; i < mdata.size(); ++i) {
    Vec3 v;
    for (int c = 0; c < 3; ++c) {
      v[c] = double(*ptrf);
      ptrf++;
    }
    mdata[i] = v;
  }
}

#endif  // NO_ZLIB!=1

//*****************************************************************************
// mesh data
//*****************************************************************************

int readBobjFile(const string &name, Mesh *mesh, bool append)
{
  debMsg("reading mesh file " << name, 1);
  if (!append)
    mesh->clear();
  else {
    errMsg("readBobj: append not yet implemented!");
    return 0;
  }

#if NO_ZLIB != 1
  const Real dx = mesh->getParent()->getDx();
  const Vec3 gs = toVec3(mesh->getParent()->getGridSize());

  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "rb1");  // do some compression
  if (!gzf) {
    errMsg("readBobj: unable to open file");
    return 0;
  }

  // read vertices
  int num = 0;
  gzread(gzf, &num, sizeof(int));
  mesh->resizeNodes(num);
  debMsg("read mesh , verts " << num, 1);
  for (int i = 0; i < num; i++) {
    Vector3D<float> pos;
    gzread(gzf, &pos.value[0], sizeof(float) * 3);
    mesh->nodes(i).pos = toVec3(pos);

    // convert to grid space
    mesh->nodes(i).pos /= dx;
    mesh->nodes(i).pos += gs * 0.5;
  }

  // normals
  num = 0;
  gzread(gzf, &num, sizeof(int));
  for (int i = 0; i < num; i++) {
    Vector3D<float> pos;
    gzread(gzf, &pos.value[0], sizeof(float) * 3);
    mesh->nodes(i).normal = toVec3(pos);
  }

  // read tris
  num = 0;
  gzread(gzf, &num, sizeof(int));
  mesh->resizeTris(num);
  for (int t = 0; t < num; t++) {
    for (int j = 0; j < 3; j++) {
      int trip = 0;
      gzread(gzf, &trip, sizeof(int));
      mesh->tris(t).c[j] = trip;
    }
  }
  // note - vortex sheet info ignored for now... (see writeBobj)
  debMsg("read mesh , triangles " << mesh->numTris() << ", vertices " << mesh->numNodes() << " ",
         1);
  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
}

int writeBobjFile(const string &name, Mesh *mesh)
{
  debMsg("writing mesh file " << name, 1);
#if NO_ZLIB != 1
  const Real dx = mesh->getParent()->getDx();
  const Vec3i gs = mesh->getParent()->getGridSize();

  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "wb1");  // do some compression
  if (!gzf) {
    errMsg("writeBobj: unable to open file");
    return 0;
  }

  // write vertices
  int numVerts = mesh->numNodes();
  gzwrite(gzf, &numVerts, sizeof(int));
  for (int i = 0; i < numVerts; i++) {
    Vector3D<float> pos = toVec3f(mesh->nodes(i).pos);
    // normalize to unit cube around 0
    pos -= toVec3f(gs) * 0.5;
    pos *= dx;
    gzwrite(gzf, &pos.value[0], sizeof(float) * 3);
  }

  // normals
  mesh->computeVertexNormals();
  gzwrite(gzf, &numVerts, sizeof(int));
  for (int i = 0; i < numVerts; i++) {
    Vector3D<float> pos = toVec3f(mesh->nodes(i).normal);
    gzwrite(gzf, &pos.value[0], sizeof(float) * 3);
  }

  // write tris
  int numTris = mesh->numTris();
  gzwrite(gzf, &numTris, sizeof(int));
  for (int t = 0; t < numTris; t++) {
    for (int j = 0; j < 3; j++) {
      int trip = mesh->tris(t).c[j];
      gzwrite(gzf, &trip, sizeof(int));
    }
  }

  // per vertex smoke densities
  if (mesh->getType() == Mesh::TypeVortexSheet) {
    VortexSheetMesh *vmesh = (VortexSheetMesh *)mesh;
    int densId[4] = {0, 'v', 'd', 'e'};
    gzwrite(gzf, &densId[0], sizeof(int) * 4);

    // compute densities
    vector<float> triDensity(numTris);
    for (int tri = 0; tri < numTris; tri++) {
      Real area = vmesh->getFaceArea(tri);
      if (area > 0)
        triDensity[tri] = vmesh->sheet(tri).smokeAmount;
    }

    // project triangle data to vertex
    vector<int> triPerVertex(numVerts);
    vector<float> density(numVerts);
    for (int tri = 0; tri < numTris; tri++) {
      for (int c = 0; c < 3; c++) {
        int vertex = mesh->tris(tri).c[c];
        density[vertex] += triDensity[tri];
        triPerVertex[vertex]++;
      }
    }

    // averaged smoke densities
    for (int point = 0; point < numVerts; point++) {
      float dens = 0;
      if (triPerVertex[point] > 0)
        dens = density[point] / triPerVertex[point];
      gzwrite(gzf, &dens, sizeof(float));
    }
  }

  // vertex flags
  if (mesh->getType() == Mesh::TypeVortexSheet) {
    int Id[4] = {0, 'v', 'x', 'f'};
    gzwrite(gzf, &Id[0], sizeof(int) * 4);

    // averaged smoke densities
    for (int point = 0; point < numVerts; point++) {
      float alpha = (mesh->nodes(point).flags & Mesh::NfMarked) ? 1 : 0;
      gzwrite(gzf, &alpha, sizeof(float));
    }
  }

  return (gzclose(gzf) == Z_OK);
#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
}

int readObjFile(const std::string &name, Mesh *mesh, bool append)
{
  ifstream ifs(name.c_str());

  if (!ifs.good()) {
    errMsg("can't open file '" + name + "'");
    return 0;
  }

  if (!append)
    mesh->clear();
  int nodebase = mesh->numNodes();
  int cnt = nodebase;
  while (ifs.good() && !ifs.eof()) {
    string id;
    ifs >> id;

    if (id[0] == '#') {
      // comment
      getline(ifs, id);
      continue;
    }
    if (id == "vt") {
      // tex coord, ignore
    }
    else if (id == "vn") {
      // normals
      if (!mesh->numNodes()) {
        errMsg("invalid amount of nodes");
        return 0;
      }
      Node n = mesh->nodes(cnt);
      ifs >> n.normal.x >> n.normal.y >> n.normal.z;
      cnt++;
    }
    else if (id == "v") {
      // vertex
      Node n;
      ifs >> n.pos.x >> n.pos.y >> n.pos.z;
      mesh->addNode(n);
    }
    else if (id == "g") {
      // group
      string group;
      ifs >> group;
    }
    else if (id == "f") {
      // face
      string face;
      Triangle t;
      for (int i = 0; i < 3; i++) {
        ifs >> face;
        if (face.find('/') != string::npos)
          face = face.substr(0, face.find('/'));  // ignore other indices
        int idx = atoi(face.c_str()) - 1;
        if (idx < 0) {
          errMsg("invalid face encountered");
          return 0;
        }
        idx += nodebase;
        t.c[i] = idx;
      }
      mesh->addTri(t);
    }
    else {
      // whatever, ignore
    }
    // kill rest of line
    getline(ifs, id);
  }
  ifs.close();
  return 1;
}

// write regular .obj file, in line with bobj.gz output (but only verts & tris for now)
int writeObjFile(const string &name, Mesh *mesh)
{
  const Real dx = mesh->getParent()->getDx();
  const Vec3i gs = mesh->getParent()->getGridSize();

  ofstream ofs(name.c_str());
  if (!ofs.good()) {
    errMsg("writeObjFile: can't open file " << name);
    return 0;
  }

  ofs << "o MantaMesh\n";

  // write vertices
  int numVerts = mesh->numNodes();
  for (int i = 0; i < numVerts; i++) {
    Vector3D<float> pos = toVec3f(mesh->nodes(i).pos);
    // normalize to unit cube around 0
    pos -= toVec3f(gs) * 0.5;
    pos *= dx;
    ofs << "v " << pos.value[0] << " " << pos.value[1] << " " << pos.value[2] << " "
        << "\n";
  }

  // write normals
  for (int i = 0; i < numVerts; i++) {
    Vector3D<float> n = toVec3f(mesh->nodes(i).normal);
    // normalize to unit cube around 0
    ofs << "vn " << n.value[0] << " " << n.value[1] << " " << n.value[2] << " "
        << "\n";
  }

  // write tris
  int numTris = mesh->numTris();
  for (int t = 0; t < numTris; t++) {
    ofs << "f " << (mesh->tris(t).c[0] + 1) << " " << (mesh->tris(t).c[1] + 1) << " "
        << (mesh->tris(t).c[2] + 1) << " "
        << "\n";
  }

  ofs.close();
  return 1;
}

template<class T> int readMdataUni(const std::string &name, MeshDataImpl<T> *mdata)
{
  debMsg("reading mesh data " << mdata->getName() << " from uni file " << name, 1);

#if NO_ZLIB != 1
  gzFile gzf = (gzFile)safeGzopen(name.c_str(), "rb");
  if (!gzf) {
    errMsg("can't open file " << name);
    return 0;
  }

  char ID[5] = {0, 0, 0, 0, 0};
  gzread(gzf, ID, 4);

  if (!strcmp(ID, "MD01")) {
    UniMeshHeader head;
    assertMsg(gzread(gzf, &head, sizeof(UniMeshHeader)) == sizeof(UniMeshHeader),
              "can't read file, no header present");
    mdata->resize(head.dim);

    assertMsg(head.dim == mdata->size(), "mdata size doesn't match");
#  if FLOATINGPOINT_PRECISION != 1
    MeshDataImpl<T> temp(mdata->getParent());
    temp.resize(mdata->size());
    mdataReadConvert<T>(gzf, *mdata, &(temp[0]), head.bytesPerElement);
#  else
    assertMsg(((head.bytesPerElement == sizeof(T)) && (head.elementType == 1)),
              "mdata type doesn't match");
    IndexInt bytes = sizeof(T) * head.dim;
    IndexInt readBytes = gzread(gzf, &(mdata->get(0)), sizeof(T) * head.dim);
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

template<class T> int writeMdataUni(const std::string &name, MeshDataImpl<T> *mdata)
{
  debMsg("writing mesh data " << mdata->getName() << " to uni file " << name, 1);

#if NO_ZLIB != 1
  char ID[5] = "MD01";
  UniMeshHeader head;
  head.dim = mdata->size();
  head.bytesPerElement = sizeof(T);
  head.elementType = 1;  // 1 for mesh data, todo - add sub types?
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
  MeshDataImpl<T> temp(mdata->getParent());
  temp.resize(mdata->size());
  mdataConvertWrite(gzf, *mdata, &(temp[0]), head);
#  else
  gzwrite(gzf, &head, sizeof(UniMeshHeader));
  gzwrite(gzf, &(mdata->get(0)), sizeof(T) * head.dim);
#  endif
  return (gzclose(gzf) == Z_OK);

#else
  debMsg("file format not supported without zlib", 1);
  return 0;
#endif
};

// explicit instantiation
template int writeMdataUni<int>(const std::string &name, MeshDataImpl<int> *mdata);
template int writeMdataUni<Real>(const std::string &name, MeshDataImpl<Real> *mdata);
template int writeMdataUni<Vec3>(const std::string &name, MeshDataImpl<Vec3> *mdata);
template int readMdataUni<int>(const std::string &name, MeshDataImpl<int> *mdata);
template int readMdataUni<Real>(const std::string &name, MeshDataImpl<Real> *mdata);
template int readMdataUni<Vec3>(const std::string &name, MeshDataImpl<Vec3> *mdata);

}  // namespace Manta
