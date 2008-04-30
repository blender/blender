
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

#include <algorithm>
#include "ViewMapBuilder.h"

using namespace std;

ViewMap* ViewMapBuilder::BuildViewMap(WingedEdge& we, visibility_algo iAlgo, real epsilon) {
  _ViewMap = new ViewMap;
  _currentId = 1;
  _currentFId = 0;
  _currentSVertexId = 0;
  
  // Builds initial view edges
  computeInitialViewEdges(we);
  
  // Detects cusps
  computeCusps(_ViewMap); 
  
  // Compute intersections
  ComputeIntersections(_ViewMap, sweep_line, epsilon);
  
  // Compute visibility
  ComputeEdgesVisibility(_ViewMap, iAlgo, _Grid, epsilon);
  
  return _ViewMap;
}

void ViewMapBuilder::computeInitialViewEdges(WingedEdge& we)
{
  vector<WShape*> wshapes = we.getWShapes();
  SShape* psShape;

  for (vector<WShape*>::const_iterator it = wshapes.begin();
       it != wshapes.end();
       it++) {
    // create the embedding
    psShape = new SShape;
    psShape->SetId((*it)->GetId());
    psShape->SetMaterials((*it)->materials()); // FIXME

    // create the view shape
    ViewShape * vshape = new ViewShape(psShape);
    // add this view shape to the view map:
    _ViewMap->AddViewShape(vshape);
  
    _pViewEdgeBuilder->SetCurrentViewId(_currentId); // we want to number the view edges in a unique way for the while scene.
    _pViewEdgeBuilder->SetCurrentFId(_currentFId); // we want to number the feature edges in a unique way for the while scene.
    _pViewEdgeBuilder->SetCurrentSVertexId(_currentFId); // we want to number the SVertex in a unique way for the while scene.
    _pViewEdgeBuilder->BuildViewEdges(dynamic_cast<WXShape*>(*it), vshape, 
				      _ViewMap->ViewEdges(), 
				      _ViewMap->ViewVertices(), 
				      _ViewMap->FEdges(), 
				      _ViewMap->SVertices());
  
    _currentId = _pViewEdgeBuilder->currentViewId()+1;
    _currentFId = _pViewEdgeBuilder->currentFId()+1;
    _currentSVertexId = _pViewEdgeBuilder->currentSVertexId()+1;

    psShape->ComputeBBox();
  }
}

void ViewMapBuilder::computeCusps(ViewMap *ioViewMap){
  vector<ViewVertex*> newVVertices;
  vector<ViewEdge*> newVEdges;
  ViewMap::viewedges_container& vedges = ioViewMap->ViewEdges();
  ViewMap::viewedges_container::iterator ve=vedges.begin(), veend=vedges.end();
  for(;
  ve!=veend;
  ++ve){
    if((!((*ve)->getNature() & Nature::SILHOUETTE)) || (!((*ve)->fedgeA()->isSmooth())))
      continue;
    FEdge *fe = (*ve)->fedgeA();
    FEdge * fefirst = fe;
    bool first = true;
    bool positive = true;
    do{
      FEdgeSmooth * fes = dynamic_cast<FEdgeSmooth*>(fe);
      Vec3r A((fes)->vertexA()->point3d());
      Vec3r B((fes)->vertexB()->point3d());
      Vec3r AB(B-A);
      AB.normalize();
      Vec3r m((A+B)/2.0);
      Vec3r crossP(AB^(fes)->normal()); 
      crossP.normalize();
      Vec3r viewvector(m-_viewpoint);
      viewvector.normalize();
      if(first){
        if(((crossP)*(viewvector)) > 0)
          positive = true;
        else
          positive = false;
        first = false;
      }
      // If we're in a positive part, we need 
      // a stronger negative value to change
      NonTVertex *cusp = 0;
      if(positive){
        if(((crossP)*(viewvector)) < -0.1){
          // state changes
          positive = false;
          // creates and insert cusp
          cusp = dynamic_cast<NonTVertex*>(ioViewMap->InsertViewVertex(fes->vertexA(), newVEdges));
          if(cusp!=0)
            cusp->setNature(cusp->getNature()|Nature::CUSP);
        }
          
      }else{
        // If we're in a negative part, we need 
        // a stronger negative value to change
        if(((crossP)*(viewvector)) > 0.1){
          positive = true;
          cusp = dynamic_cast<NonTVertex*>(ioViewMap->InsertViewVertex(fes->vertexA(), newVEdges));
          if(cusp!=0)
            cusp->setNature(cusp->getNature()|Nature::CUSP);
        }
      }
      fe = fe->nextEdge();
    }while((fe!=0) && (fe!=fefirst));
  }
  for(ve=newVEdges.begin(), veend=newVEdges.end();
  ve!=veend;
  ++ve){
    (*ve)->viewShape()->AddEdge(*ve);
    vedges.push_back(*ve);
  }
}
void ViewMapBuilder::ComputeEdgesVisibility(ViewMap *ioViewMap, visibility_algo iAlgo,  Grid *iGrid, real epsilon)
{
  if((iAlgo == ray_casting ||
      iAlgo == ray_casting_fast ||
      iAlgo == ray_casting_very_fast) && (NULL == iGrid))
  {
    cerr << "Error: can't cast ray, no grid defined" << endl;
    return;
  }

  switch(iAlgo)
  {
  case ray_casting:
    ComputeRayCastingVisibility(ioViewMap, iGrid, epsilon);
    break;
  case ray_casting_fast:
    ComputeFastRayCastingVisibility(ioViewMap, iGrid, epsilon);
    break;
  case ray_casting_very_fast:
    ComputeVeryFastRayCastingVisibility(ioViewMap, iGrid, epsilon);
    break;
  default:
    break;
  }
}

static const unsigned gProgressBarMaxSteps = 10;
static const unsigned gProgressBarMinSize = 2000;

void ViewMapBuilder::ComputeRayCastingVisibility(ViewMap *ioViewMap, Grid* iGrid, real epsilon)
{
  vector<ViewEdge*>& vedges = ioViewMap->ViewEdges();
  bool progressBarDisplay = false;
  unsigned progressBarStep = 0;
  unsigned vEdgesSize = vedges.size();
  unsigned fEdgesSize = ioViewMap->FEdges().size();

  if(_pProgressBar != NULL && fEdgesSize > gProgressBarMinSize) {
    unsigned progressBarSteps = min(gProgressBarMaxSteps, vEdgesSize);
    progressBarStep = vEdgesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Computing Ray casting Visibility");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
    progressBarDisplay = true;
  }
  
  unsigned counter = progressBarStep;
  FEdge * fe, *festart;
  int nSamples = 0;
  vector<Polygon3r*> aFaces;
  Polygon3r *aFace = 0;
  unsigned tmpQI = 0;
  unsigned qiClasses[256];
  unsigned maxIndex, maxCard;
  unsigned qiMajority;
  static unsigned timestamp = 1;
  for(vector<ViewEdge*>::iterator ve=vedges.begin(), veend=vedges.end();
  ve!=veend;
  ve++)
  {
    festart = (*ve)->fedgeA();
    fe = (*ve)->fedgeA();
    qiMajority = 1;
    do {
       qiMajority++;
       fe = fe->nextEdge();
    } while (fe && fe != festart);
    qiMajority >>= 1;

    tmpQI = 0;
    maxIndex = 0;
    maxCard = 0;
    nSamples = 0;
    fe = (*ve)->fedgeA();
    memset(qiClasses, 0, 256 * sizeof(*qiClasses));
    set<ViewShape*> occluders;
    do
    {
      if((maxCard < qiMajority)) {
	tmpQI = ComputeRayCastingVisibility(fe, iGrid, epsilon, occluders, &aFace, timestamp++);
	
	if(tmpQI >= 256)
	  cerr << "Warning: too many occluding levels" << endl;

	if (++qiClasses[tmpQI] > maxCard) {
	  maxCard = qiClasses[tmpQI];
	  maxIndex = tmpQI;
	}
      }
      else
	FindOccludee(fe, iGrid, epsilon, &aFace, timestamp++);

      if(aFace) { 
	fe->SetaFace(*aFace);
	aFaces.push_back(aFace);
	fe->SetOccludeeEmpty(false);
      }
      else
	fe->SetOccludeeEmpty(true);

      ++nSamples;
      fe = fe->nextEdge();
    }
    while((maxCard < qiMajority) && (0!=fe) && (fe!=festart));

    // ViewEdge
    // qi --
    (*ve)->SetQI(maxIndex);
    // occluders --
    for(set<ViewShape*>::iterator o=occluders.begin(), oend=occluders.end();
    o!=oend;
    ++o)
      (*ve)->AddOccluder((*o));
    // occludee --
    if(!aFaces.empty())
    {
      if(aFaces.size() <= (float)nSamples/2.f)
      {
        (*ve)->SetaShape(0);
      }
      else
      {
        vector<Polygon3r*>::iterator p = aFaces.begin();
        WFace * wface = (WFace*)((*p)->userdata);
        ViewShape *vshape = ioViewMap->viewShape(wface->GetVertex(0)->shape()->GetId());
        ++p;
        (*ve)->SetaShape(vshape);
      }
    }
    
    if(progressBarDisplay) {  
      counter--;
      if (counter <= 0) {
	counter = progressBarStep;
	_pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }   
    aFaces.clear();
  }    
}

void ViewMapBuilder::ComputeFastRayCastingVisibility(ViewMap *ioViewMap, Grid* iGrid, real epsilon)
{
  vector<ViewEdge*>& vedges = ioViewMap->ViewEdges();
  bool progressBarDisplay = false;
  unsigned progressBarStep = 0;
  unsigned vEdgesSize = vedges.size();
  unsigned fEdgesSize = ioViewMap->FEdges().size();

  if(_pProgressBar != NULL && fEdgesSize > gProgressBarMinSize) {
    unsigned progressBarSteps = min(gProgressBarMaxSteps, vEdgesSize);
    progressBarStep = vEdgesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Computing Ray casting Visibility");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
    progressBarDisplay = true;
  }

  unsigned counter = progressBarStep;  
  FEdge * fe, *festart;
  unsigned nSamples = 0;
  vector<Polygon3r*> aFaces;
  Polygon3r *aFace = 0;
  unsigned tmpQI = 0;
  unsigned qiClasses[256];
  unsigned maxIndex, maxCard;
  unsigned qiMajority;
  static unsigned timestamp = 1;
  bool even_test;
  for(vector<ViewEdge*>::iterator ve=vedges.begin(), veend=vedges.end();
  ve!=veend;
  ve++)
  {
    festart = (*ve)->fedgeA();
    fe = (*ve)->fedgeA();
    qiMajority = 1;
    do {
       qiMajority++;
       fe = fe->nextEdge();
    } while (fe && fe != festart);
    if (qiMajority >= 4)
      qiMajority >>= 2;
    else
      qiMajority = 1;

    set<ViewShape*> occluders;

    even_test = true;
    maxIndex = 0;
    maxCard = 0;
    nSamples = 0;
    memset(qiClasses, 0, 256 * sizeof(*qiClasses));
    fe = (*ve)->fedgeA();
    do
    {
      if (even_test)
      {
	if((maxCard < qiMajority)) {
	  tmpQI = ComputeRayCastingVisibility(fe, iGrid, epsilon, occluders, &aFace, timestamp++);
	  
	  if(tmpQI >= 256)
	    cerr << "Warning: too many occluding levels" << endl;

	  if (++qiClasses[tmpQI] > maxCard) {
	    maxCard = qiClasses[tmpQI];
	    maxIndex = tmpQI;
	  }
	}
	else
	  FindOccludee(fe, iGrid, epsilon, &aFace, timestamp++);

        if(aFace)
        { 
          fe->SetaFace(*aFace);
          aFaces.push_back(aFace);
        } 
        ++nSamples;
	even_test = false;
      }
      else
	even_test = true;
      fe = fe->nextEdge();
    } while ((maxCard < qiMajority) && (0!=fe) && (fe!=festart));

    (*ve)->SetQI(maxIndex);

    if(!aFaces.empty())
    {
      if(aFaces.size() < nSamples / 2)
      {
        (*ve)->SetaShape(0);
      }
      else
      {
        vector<Polygon3r*>::iterator p = aFaces.begin();
        WFace * wface = (WFace*)((*p)->userdata);
        ViewShape *vshape = ioViewMap->viewShape(wface->GetVertex(0)->shape()->GetId());
        ++p;
        //        for(;
        //        p!=pend;
        //        ++p)
        //        { 
        //          WFace *f = (WFace*)((*p)->userdata);
        //          ViewShape *vs = ioViewMap->viewShape(f->GetVertex(0)->shape()->GetId());
        //          if(vs != vshape)
        //          { 
        //            sameShape = false;
        //            break;
        //          } 
        //        } 
        //        if(sameShape)
        (*ve)->SetaShape(vshape);
      }
    }
    
    //(*ve)->SetaFace(aFace);
    
    if(progressBarDisplay) {  
      counter--;
      if (counter <= 0) {
	counter = progressBarStep;
	_pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }
    aFaces.clear();
  }
}

void ViewMapBuilder::ComputeVeryFastRayCastingVisibility(ViewMap *ioViewMap, Grid* iGrid, real epsilon)
{
  vector<ViewEdge*>& vedges = ioViewMap->ViewEdges();
  bool progressBarDisplay = false;
  unsigned progressBarStep = 0;
  unsigned vEdgesSize = vedges.size();
  unsigned fEdgesSize = ioViewMap->FEdges().size();

  if(_pProgressBar != NULL && fEdgesSize > gProgressBarMinSize) {
    unsigned progressBarSteps = min(gProgressBarMaxSteps, vEdgesSize);
    progressBarStep = vEdgesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Computing Ray casting Visibility");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
    progressBarDisplay = true;
  }

  unsigned counter = progressBarStep;
  FEdge* fe;
  unsigned qi = 0;
  Polygon3r *aFace = 0;
  static unsigned timestamp = 1;
  for(vector<ViewEdge*>::iterator ve=vedges.begin(), veend=vedges.end();
  ve!=veend;
  ve++)
  {
    set<ViewShape*> occluders;

    fe = (*ve)->fedgeA();
    qi = ComputeRayCastingVisibility(fe, iGrid, epsilon, occluders, &aFace, timestamp++);
    if(aFace)
    { 
      fe->SetaFace(*aFace);
      WFace * wface = (WFace*)(aFace->userdata);
      ViewShape *vshape = ioViewMap->viewShape(wface->GetVertex(0)->shape()->GetId());
      (*ve)->SetaShape(vshape);
    } 
    else
    {
      (*ve)->SetaShape(0);
    }

    (*ve)->SetQI(qi);
    
    if(progressBarDisplay) {  
      counter--;
      if (counter <= 0) {
	counter = progressBarStep;
	_pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }
  }  
}


void ViewMapBuilder::FindOccludee(FEdge *fe, Grid* iGrid, real epsilon, Polygon3r** oaPolygon, unsigned timestamp, 
				  Vec3r& u, Vec3r& A, Vec3r& origin, Vec3r& edge, vector<WVertex*>& faceVertices)
{
  WFace *face = 0;
  if(fe->isSmooth()){
    FEdgeSmooth * fes = dynamic_cast<FEdgeSmooth*>(fe);
    face = (WFace*)fes->face();
  }
  OccludersSet occluders;
  WFace * oface;
  bool skipFace;
  
  WVertex::incoming_edge_iterator ie;
  OccludersSet::iterator p, pend;
  
  *oaPolygon = 0;
  if(((fe)->getNature() & Nature::SILHOUETTE) || ((fe)->getNature() & Nature::BORDER))
  {
    occluders.clear();
    // we cast a ray from A in the same direction but looking behind
    Vec3r v(-u[0],-u[1],-u[2]);
    iGrid->castInfiniteRay(A, v, occluders, timestamp);
    
    bool noIntersection = true;
    real mint=FLT_MAX;
    // we met some occluders, let us fill the aShape field 
    // with the first intersected occluder
    for(p=occluders.begin(),pend=occluders.end();
    p!=pend;
    p++)
    { 
      // check whether the edge and the polygon plane are coincident:
      //-------------------------------------------------------------
      //first let us compute the plane equation.
      oface = (WFace*)(*p)->userdata;
      Vec3r v1(((*p)->getVertices())[0]);
      Vec3r normal((*p)->getNormal());
      real d = -(v1 * normal);
      real t,t_u,t_v;
      
      if(0 != face)
      { 
        skipFace = false;
        
        if(face == oface)
          continue;
        
        if(faceVertices.empty())
          continue;
        
        for(vector<WVertex*>::iterator fv=faceVertices.begin(), fvend=faceVertices.end();
        fv!=fvend;
        ++fv)
        { 
          if((*fv)->isBoundary())
            continue;
          WVertex::incoming_edge_iterator iebegin=(*fv)->incoming_edges_begin();
          WVertex::incoming_edge_iterator ieend=(*fv)->incoming_edges_end();
          for(ie=iebegin;ie!=ieend; ++ie)
          {  
            if((*ie) == 0)
              continue;
            
            WFace * sface = (*ie)->GetbFace();
            if(sface == oface)
            { 
              skipFace = true;
              break;
            } 
          }  
          if(skipFace)
            break;
        } 
        if(skipFace)
          continue;
      }  
      else
      {
        if(GeomUtils::COINCIDENT == GeomUtils::intersectRayPlane(origin, edge, normal, d, t, epsilon))
          continue;
      }
      if((*p)->rayIntersect(A, v, t,t_u,t_v))
      {
        if (fabs(v * normal) > 0.0001)
          if ((t>0.0)) // && (t<1.0))
          {
            if (t<mint)
            {
              *oaPolygon = (*p);
              mint = t;
              noIntersection = false;
              fe->SetOccludeeIntersection(Vec3r(A+t*v));
            }
          }
      }  
    }
    
    if(noIntersection)
      *oaPolygon = 0;
  }
}

void ViewMapBuilder::FindOccludee(FEdge *fe, Grid* iGrid, real epsilon, Polygon3r** oaPolygon, unsigned timestamp)
{
  OccludersSet occluders;

  Vec3r A;
  Vec3r edge;
  Vec3r origin;
  A = Vec3r(((fe)->vertexA()->point3D() + (fe)->vertexB()->point3D())/2.0);
  edge = Vec3r((fe)->vertexB()->point3D()-(fe)->vertexA()->point3D());
  origin = Vec3r((fe)->vertexA()->point3D());
  Vec3r u(_viewpoint-A);
  u.normalize();
  if(A < iGrid->getOrigin())
    cerr << "Warning: point is out of the grid for fedge " << fe->getId().getFirst() << "-" << fe->getId().getSecond() << endl;

  vector<WVertex*> faceVertices;
     
  WFace *face = 0;
  if(fe->isSmooth()){
    FEdgeSmooth * fes = dynamic_cast<FEdgeSmooth*>(fe);
    face = (WFace*)fes->face();
  }
  if(0 != face)
    face->RetrieveVertexList(faceVertices);
  
  return FindOccludee(fe,iGrid, epsilon, oaPolygon, timestamp, 
		      u, A, origin, edge, faceVertices);
}

int ViewMapBuilder::ComputeRayCastingVisibility(FEdge *fe, Grid* iGrid, real epsilon, set<ViewShape*>& oOccluders,
						Polygon3r** oaPolygon, unsigned timestamp)
{
  OccludersSet occluders;
  int qi = 0;

  Vec3r center;
  Vec3r edge;
  Vec3r origin;

  center = fe->center3d();
  edge = Vec3r(fe->vertexB()->point3D() - fe->vertexA()->point3D());
  origin = Vec3r(fe->vertexA()->point3D());
  //
  //   // Is the edge outside the view frustum ?
  Vec3r gridOrigin(iGrid->getOrigin());
  Vec3r gridExtremity(iGrid->getOrigin()+iGrid->gridSize());
  
  if( (center.x() < gridOrigin.x()) || (center.y() < gridOrigin.y()) || (center.z() < gridOrigin.z())
    ||(center.x() > gridExtremity.x()) || (center.y() > gridExtremity.y()) || (center.z() > gridExtremity.z())){
     cerr << "Warning: point is out of the grid for fedge " << fe->getId() << endl;
    //return 0;
  }

 
  //  Vec3r A(fe->vertexA()->point2d());
  //  Vec3r B(fe->vertexB()->point2d());
  //  int viewport[4];
  //  SilhouetteGeomEngine::retrieveViewport(viewport);
  //  if( (A.x() < viewport[0]) || (A.x() > viewport[2]) || (A.y() < viewport[1]) || (A.y() > viewport[3])
  //    ||(B.x() < viewport[0]) || (B.x() > viewport[2]) || (B.y() < viewport[1]) || (B.y() > viewport[3])){
  //    cerr << "Warning: point is out of the grid for fedge " << fe->getId() << endl;
  //    //return 0;
  //  }

  Vec3r u(_viewpoint - center);
  real raylength = u.norm();
  u.normalize();
  //cout << "grid origin " << iGrid->getOrigin().x() << "," << iGrid->getOrigin().y() << "," << iGrid->getOrigin().z() << endl;
  //cout << "center " << center.x() << "," << center.y() << "," << center.z() << endl;
  
  iGrid->castRay(center, Vec3r(_viewpoint), occluders, timestamp);

  WFace *face = 0;
  if(fe->isSmooth()){
    FEdgeSmooth * fes = dynamic_cast<FEdgeSmooth*>(fe);
    face = (WFace*)fes->face();
  }
  vector<WVertex*> faceVertices;
  WVertex::incoming_edge_iterator ie;
            
  WFace * oface;
  bool skipFace;
  OccludersSet::iterator p, pend;
  if(face)
    face->RetrieveVertexList(faceVertices);

  for(p=occluders.begin(),pend=occluders.end();
      p!=pend;
      p++)
    { 
      // If we're dealing with an exact silhouette, check whether 
      // we must take care of this occluder of not.
      // (Indeed, we don't consider the occluders that 
      // share at least one vertex with the face containing 
      // this edge).
      //-----------
      oface = (WFace*)(*p)->userdata;
      Vec3r v1(((*p)->getVertices())[0]);
      Vec3r normal((*p)->getNormal());
      real d = -(v1 * normal);
      real t, t_u, t_v;
            
      if(0 != face)
	{
	  skipFace = false;
              
	  if(face == oface)
	    continue;
              
              
	  for(vector<WVertex*>::iterator fv=faceVertices.begin(), fvend=faceVertices.end();
              fv!=fvend;
              ++fv)
	    {
                if((*fv)->isBoundary())
                  continue;

	      WVertex::incoming_edge_iterator iebegin=(*fv)->incoming_edges_begin();
	      WVertex::incoming_edge_iterator ieend=(*fv)->incoming_edges_end();
	      for(ie=iebegin;ie!=ieend; ++ie)
                { 
                  if((*ie) == 0)
                    continue;
                  
                  WFace * sface = (*ie)->GetbFace();
                  //WFace * sfacea = (*ie)->GetaFace();
                  //if((sface == oface) || (sfacea == oface))
                  if(sface == oface)
		    {
		      skipFace = true;
		      break;
		    }
                }
	      if(skipFace)
		break;
	    }
	  if(skipFace)
	    continue;
	}
      else
	{
	  // check whether the edge and the polygon plane are coincident:
	  //-------------------------------------------------------------
	  //first let us compute the plane equation.
           
	  if(GeomUtils::COINCIDENT == GeomUtils::intersectRayPlane(origin, edge, normal, d, t, epsilon))
	    continue;
	}

      if((*p)->rayIntersect(center, u, t, t_u, t_v))
	{
	  if (fabs(u * normal) > 0.0001)
	    if ((t>0.0) && (t<raylength))
	      {
		WFace *f = (WFace*)((*p)->userdata);
		ViewShape *vshape = _ViewMap->viewShape(f->GetVertex(0)->shape()->GetId());
		oOccluders.insert(vshape);
		++qi;
    if(!_EnableQI)
      break;
	      }
	}
    }

  // Find occludee
  FindOccludee(fe,iGrid, epsilon, oaPolygon, timestamp, 
	       u, center, edge, origin, faceVertices);

  return qi;
}

void ViewMapBuilder::ComputeIntersections(ViewMap *ioViewMap, intersection_algo iAlgo, real epsilon)
{
  switch(iAlgo)
  {
  case sweep_line:
    ComputeSweepLineIntersections(ioViewMap, epsilon);
    break;
  default:
    break;
  }
  ViewMap::viewvertices_container& vvertices = ioViewMap->ViewVertices();
  for(ViewMap::viewvertices_container::iterator vv=vvertices.begin(), vvend=vvertices.end();
  vv!=vvend;
  ++vv)
  {
    if((*vv)->getNature() == Nature::T_VERTEX)
    {
      TVertex *tvertex = (TVertex*)(*vv);
      cout << "TVertex " << tvertex->getId() << " has :" << endl;
      cout << "FrontEdgeA: " << tvertex->frontEdgeA().first << endl;
      cout << "FrontEdgeB: " << tvertex->frontEdgeB().first << endl;
      cout << "BackEdgeA: " << tvertex->backEdgeA().first << endl;
      cout << "BackEdgeB: " << tvertex->backEdgeB().first << endl << endl;
    }
  }
}

struct less_SVertex2D : public binary_function<SVertex*, SVertex*, bool> 
{
  real epsilon;
  less_SVertex2D(real eps)
    : binary_function<SVertex*,SVertex*,bool>()
  {
    epsilon = eps;
  }
	bool operator()(SVertex* x, SVertex* y) 
  {
    Vec3r A = x->point2D();
    Vec3r B = y->point2D();
    for(unsigned int i=0; i<3; i++)
    {
      if((fabs(A[i] - B[i])) < epsilon)
        continue;
      if(A[i] < B[i])
        return true;
      if(A[i] > B[i])
        return false;
    }
    
    return false;
  }
};

typedef Segment<FEdge*,Vec3r > segment;
typedef Intersection<segment> intersection;

struct less_Intersection : public binary_function<intersection*, intersection*, bool> 
{
  segment *edge;
  less_Intersection(segment *iEdge)
    : binary_function<intersection*,intersection*,bool>()
  {
    edge = iEdge;
  }
	bool operator()(intersection* x, intersection* y) 
  {
    real tx = x->getParameter(edge);
    real ty = y->getParameter(edge);
    if(tx > ty)
      return true;
    return false;
  }
};

struct silhouette_binary_rule : public binary_rule<segment,segment>
{
  silhouette_binary_rule() : binary_rule<segment,segment>() {}
  virtual bool operator() (segment& s1, segment& s2)
  {
    FEdge * f1 = s1.edge();
    FEdge * f2 = s2.edge();

    if((!(((f1)->getNature() & Nature::SILHOUETTE) || ((f1)->getNature() & Nature::BORDER))) && (!(((f2)->getNature() & Nature::SILHOUETTE) || ((f2)->getNature() & Nature::BORDER))))
      return false;
      
    return true;
  }
};

void ViewMapBuilder::ComputeSweepLineIntersections(ViewMap *ioViewMap, real epsilon)
{
  vector<SVertex*>& svertices = ioViewMap->SVertices();
  bool progressBarDisplay = false;
  unsigned sVerticesSize = svertices.size();
  unsigned fEdgesSize = ioViewMap->FEdges().size();
  //  ViewMap::fedges_container& fedges = ioViewMap->FEdges();
  //  for(ViewMap::fedges_container::const_iterator f=fedges.begin(), end=fedges.end();
  //  f!=end;
  //  ++f){
  //    cout << (*f)->aMaterialIndex() << "-" << (*f)->bMaterialIndex() << endl;
  //  }
    
  unsigned progressBarStep = 0;
  
  if(_pProgressBar != NULL && fEdgesSize > gProgressBarMinSize) {
    unsigned progressBarSteps = min(gProgressBarMaxSteps, sVerticesSize);
    progressBarStep = sVerticesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Computing Sweep Line Intersections");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
    progressBarDisplay = true;
  }
  
  unsigned counter = progressBarStep;

  sort(svertices.begin(), svertices.end(), less_SVertex2D(epsilon));

  SweepLine<FEdge*,Vec3r> SL;

  vector<FEdge*>& ioEdges = ioViewMap->FEdges();

  vector<segment* > segments;

  vector<FEdge*>::iterator fe,fend;

  for(fe=ioEdges.begin(), fend=ioEdges.end();
  fe!=fend;
  fe++)
  {
    segment * s = new segment((*fe), (*fe)->vertexA()->point2D(), (*fe)->vertexB()->point2D());
    (*fe)->userdata = s;    
    segments.push_back(s);
  }

  vector<segment*> vsegments;
  for(vector<SVertex*>::iterator sv=svertices.begin(),svend=svertices.end();
  sv!=svend;
  sv++)
  {
    const vector<FEdge*>& vedges = (*sv)->fedges();
    
    for(vector<FEdge*>::const_iterator sve=vedges.begin(), sveend=vedges.end();
    sve!=sveend;
    sve++)
    {
      vsegments.push_back((segment*)((*sve)->userdata));
    }

    Vec3r evt((*sv)->point2D());
    silhouette_binary_rule sbr;
    SL.process(evt, vsegments, sbr);

    if(progressBarDisplay) {  
      counter--;
      if (counter <= 0) {
	counter = progressBarStep;
	_pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }
    vsegments.clear();
  }

  // reset userdata:
  for(fe=ioEdges.begin(), fend=ioEdges.end();
  fe!=fend;
  fe++)
    (*fe)->userdata = NULL;
 
  // list containing the new edges resulting from splitting operations. 
  vector<FEdge*> newEdges;
  
  // retrieve the intersected edges:
  vector<segment* >& iedges = SL.intersectedEdges();
  // retrieve the intersections:
  vector<intersection*>& intersections = SL.intersections();
  
  int id=0;
  // create a view vertex for each intersection and linked this one 
  // with the intersection object
  vector<intersection*>::iterator i, iend;
  for(i=intersections.begin(),iend=intersections.end();
  i!=iend;
  i++)
  {
    FEdge *fA = (*i)->EdgeA->edge();
    FEdge *fB = (*i)->EdgeB->edge();
    
    Vec3r A1 = fA->vertexA()->point3D();
    Vec3r A2 = fA->vertexB()->point3D();
    Vec3r B1 = fB->vertexA()->point3D();
    Vec3r B2 = fB->vertexB()->point3D();

    Vec3r a1 = fA->vertexA()->point2D();
    Vec3r a2 = fA->vertexB()->point2D();
    Vec3r b1 = fB->vertexA()->point2D();
    Vec3r b2 = fB->vertexB()->point2D();

    real ta = (*i)->tA;
    real tb = (*i)->tB;

    if((ta < -epsilon) || (ta > 1+epsilon))
        cerr << "Warning: intersection out of range for edge " << fA->vertexA()->getId() << " - " << fA->vertexB()->getId() << endl;
    
    if((tb < -epsilon) || (tb > 1+epsilon))
        cerr << "Warning: intersection out of range for edge " << fB->vertexA()->getId() << " - " << fB->vertexB()->getId() << endl;
    
    real Ta = SilhouetteGeomEngine::ImageToWorldParameter(fA, ta);
    real Tb = SilhouetteGeomEngine::ImageToWorldParameter(fB, tb);

    TVertex * tvertex = ioViewMap->CreateTVertex(Vec3r(A1 + Ta*(A2-A1)), Vec3r(a1 + ta*(a2-a1)), fA, 
                                                 Vec3r(B1 + Tb*(B2-B1)), Vec3r(b1 + tb*(b2-b1)), fB, id);
     
    (*i)->userdata = tvertex;
    ++id;
  }

  progressBarStep = 0;

  if(progressBarDisplay) {
    unsigned iEdgesSize = iedges.size();
    unsigned progressBarSteps = min(gProgressBarMaxSteps, iEdgesSize);
    progressBarStep = iEdgesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Splitting intersected edges");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
  }
  
  counter = progressBarStep;

  vector<TVertex*> edgeVVertices;
  vector<ViewEdge*> newVEdges;
  vector<segment* >::iterator s, send;
  for(s=iedges.begin(),send=iedges.end();
  s!=send;
  s++)
  {
    edgeVVertices.clear();
    newEdges.clear();
    newVEdges.clear();
    
    FEdge* fedge = (*s)->edge();
    ViewEdge *vEdge = fedge->viewedge();
    ViewShape *shape = vEdge->viewShape();
    
    vector<intersection*>& eIntersections = (*s)->intersections();
    // we first need to sort these intersections from farther to closer to A
    sort(eIntersections.begin(), eIntersections.end(), less_Intersection(*s));
    for(i=eIntersections.begin(),iend=eIntersections.end();
    i!=iend;
    i++)
      edgeVVertices.push_back((TVertex*)(*i)->userdata);

    shape->SplitEdge(fedge, edgeVVertices, ioViewMap->FEdges(), ioViewMap->ViewEdges()); 

    if(progressBarDisplay) {  
      counter--;
      if (counter <= 0) {
	counter = progressBarStep;
	_pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }
  }

  // reset userdata:
  for(fe=ioEdges.begin(), fend=ioEdges.end();
  fe!=fend;
  fe++)
    (*fe)->userdata = NULL;

  // delete segments
  //  if(!segments.empty()){
  //    for(s=segments.begin(),send=segments.end();
  //    s!=send;
  //    s++){
  //      delete *s;
  //    }
  //    segments.clear();
  //  }
}
