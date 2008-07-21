//
//  Filename         : MaxFileLoader.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class used to load 3ds models.
//  Date of creation : 10/10/2002
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef  MAX_FILE_LOADER_H
# define MAX_FILE_LOADER_H

# include <string.h>              
# include <float.h>

//soc - modified to adapt Blender's in
// lib3ds includes
# include <lib3ds/file.h>
# include <lib3ds/node.h>
# include <lib3ds/camera.h>
# include <lib3ds/mesh.h>
# include <lib3ds/material.h>
# include <lib3ds/matrix.h>
# include <lib3ds/vector.h>
# include <lib3ds/file.h>

# include "../system/FreestyleConfig.h"
# include "NodeGroup.h"
# include "NodeTransform.h"
# include "NodeShape.h"
# include "IndexedFaceSet.h"
# include "../geometry/BBox.h"
# include "../geometry/Geom.h"
# include "../geometry/GeomCleaner.h"


class NodeGroup;

class LIB_SCENE_GRAPH_EXPORT MaxFileLoader
{
public:
  /*! Builds a MaxFileLoader */
  MaxFileLoader();
  /*! Builds a MaxFileLoader to load the iFileName
      file.
        iFileName
          The name of the 3dsMax file to load
   */
  explicit MaxFileLoader(const char *iFileName);
  virtual ~MaxFileLoader();

  /*! Sets the name of the 3dsMax file to load */
  void setFileName(const char *iFileName);

  /*! Loads the 3D scene and returns 
   *  a pointer to the scene root node
   */
  NodeGroup * Load();
  //void Load(const char *iFileName);

  /*! Gets the number of read faces */
  inline unsigned int numFacesRead() {return _numFacesRead;}

  /*! Gets the smallest edge size read */
  inline real minEdgeSize() {return _minEdgeSize;}

protected:
  void RenderNode(Lib3dsNode *iNode);

protected:
  char *_FileName;
  Lib3dsFile *_3dsFile;
  NodeGroup* _Scene;
  unsigned _numFacesRead;
  real _minEdgeSize;
};

#endif // MAX_FILE_LOADER_H
