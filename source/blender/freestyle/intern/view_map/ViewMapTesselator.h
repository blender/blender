//
//  Filename         : ViewMapTesselator.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to build a Node Tree designed to be displayed 
//                     from a Silhouette View Map structure.
//  Date of creation : 26/03/2002
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

#ifndef  VIEWMAPTESSELATOR_H
# define VIEWMAPTESSELATOR_H

# include "Silhouette.h"
# include "ViewMap.h"
# include "../scene_graph/NodeShape.h"
# include "../winged_edge/WEdge.h"
# include "../scene_graph/NodeGroup.h"
# include "../scene_graph/LineRep.h"
# include "../scene_graph/OrientedLineRep.h"
# include "../scene_graph/VertexRep.h"

class NodeShape;
class NodeGroup;
class SShape;
class WShape;

class LIB_VIEW_MAP_EXPORT ViewMapTesselator
{
public:

  inline ViewMapTesselator() {_nature = Nature::SILHOUETTE | Nature::BORDER | Nature::CREASE;_FrsMaterial.setDiffuse(0,0,0,1);_overloadFrsMaterial=false;}
  virtual ~ViewMapTesselator() {}

  /*! Builds a set of lines rep contained under a 
   *  a NodeShape, itself contained under a NodeGroup from a ViewMap
   */
  NodeGroup* Tesselate(ViewMap* iViewMap) ;

  /*! Builds a set of lines rep contained under a 
   *  a NodeShape, itself contained under a NodeGroup from a 
   *  set of view edges
   */
  template<class ViewEdgesIterator>
  NodeGroup* Tesselate(ViewEdgesIterator begin, ViewEdgesIterator end) ;

  /*! Builds a set of lines rep contained among a 
   *  a NodeShape, from a WShape
   */
  NodeGroup* Tesselate(WShape* iWShape);

  
  inline void setNature(Nature::EdgeNature iNature) {_nature = iNature;}
  inline void setFrsMaterial(const FrsMaterial& iMaterial) {_FrsMaterial=iMaterial;_overloadFrsMaterial=true;}
  inline Nature::EdgeNature nature() {return _nature;}
  inline const FrsMaterial& frs_material() const {return _FrsMaterial;}

protected:
  virtual void AddVertexToLine(LineRep *iLine, SVertex *v) = 0;
  
private:
  Nature::EdgeNature _nature;
  FrsMaterial _FrsMaterial;
  bool _overloadFrsMaterial;
};

/*! Class to tesselate the 2D projected silhouette */
class ViewMapTesselator2D : public ViewMapTesselator
{
public:
  inline ViewMapTesselator2D() : ViewMapTesselator() {}
  virtual ~ViewMapTesselator2D() {}

protected:
  virtual void AddVertexToLine(LineRep *iLine, SVertex *v)
  {
    iLine->AddVertex(v->point2D());
  }
};

/*! Class to tesselate the 3D silhouette */
class ViewMapTesselator3D : public ViewMapTesselator
{
public:
  inline ViewMapTesselator3D() : ViewMapTesselator() {}
  virtual ~ViewMapTesselator3D() {}

protected:
  virtual void AddVertexToLine(LineRep *iLine, SVertex *v)
  {
    iLine->AddVertex(v->point3D());
  }
};

//
// Implementation
//
///////////////////////////////////////////////

template<class ViewEdgesIterator>
NodeGroup * ViewMapTesselator::Tesselate(ViewEdgesIterator begin, ViewEdgesIterator end) 
{
  NodeGroup *group = new NodeGroup;
  NodeShape *tshape = new NodeShape;
  group->AddChild(tshape);
  //tshape->frs_material().setDiffuse(0.f, 0.f, 0.f, 1.f);
  tshape->setFrsMaterial(_FrsMaterial);

  LineRep* line;


  FEdge *firstEdge;
  FEdge *nextFEdge, *currentEdge;
  
  int id=0;
  //  for(vector<ViewEdge*>::const_iterator c=viewedges.begin(),cend=viewedges.end();
  //      c!=cend;
  //      c++)
  for(ViewEdgesIterator c=begin, cend=end;
  c!=cend;
  c++)
    {
    //    if((*c)->qi() > 0){
    //      continue;
    //    }
        //      if(!((*c)->nature() & (_nature)))
        //        continue;
        //      
      firstEdge = (*c)->fedgeA(); 

      //      if(firstEdge->invisibility() > 0)
      //        continue;
      
      line = new OrientedLineRep();
      if(_overloadFrsMaterial)
        line->setFrsMaterial(_FrsMaterial);

      // there might be chains containing a single element
      if(0 == (firstEdge)->nextEdge())
      {
        line->setStyle(LineRep::LINES);
        // line->AddVertex((*c)->vertexA()->point3D());
        // line->AddVertex((*c)->vertexB()->point3D());
        AddVertexToLine(line, firstEdge->vertexA());
        AddVertexToLine(line, firstEdge->vertexB());
      }
      else
      {
        line->setStyle(LineRep::LINE_STRIP);

        //firstEdge = (*c);
        nextFEdge = firstEdge;
        currentEdge = firstEdge;
        do
        {
          //line->AddVertex(nextFEdge->vertexA()->point3D());
          AddVertexToLine(line, nextFEdge->vertexA());
           currentEdge = nextFEdge;
           nextFEdge = nextFEdge->nextEdge();
         }while((nextFEdge != NULL) && (nextFEdge != firstEdge));
        // Add the last vertex
        //line->AddVertex(currentEdge->vertexB()->point3D());
        AddVertexToLine(line, currentEdge->vertexB());
                
      }

      line->setId((*c)->getId().getFirst());
      line->ComputeBBox();
      tshape->AddRep(line);
      id++;
    }
  
  return group;
}

#endif // VIEWMAPTESSELATOR_H
