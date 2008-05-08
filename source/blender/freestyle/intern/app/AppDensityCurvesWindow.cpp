
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
#include "AppDensityCurvesWindow.h"
#include "../scene_graph/NodeGroup.h"
#include "AppGL2DCurvesViewer.h"
#include <vector>
using namespace std;


AppDensityCurvesWindow::AppDensityCurvesWindow(QWidget *parent, const char *name, bool modal, Qt::WFlags fl)
 : QDialog(parent, fl)
{
	setupUi(this);
}
AppDensityCurvesWindow::~AppDensityCurvesWindow(){ 
}

void AppDensityCurvesWindow::SetOrientationCurve(int i, const Vec2d& vmin, const Vec2d& vmax, const vector<Vec3r>& iCurve, const char *xlabel, const char *ylabel){
  AppGL2DCurvesViewer * viewer = 0;
  switch(i){
  case 0:viewer = CurvesViewer0; break;
  case 1:viewer = CurvesViewer1; break;
  case 2:viewer = CurvesViewer2; break;
  case 3:viewer = CurvesViewer3; break;
  case 4:viewer = CurvesViewer4; break;
  default:return;
  }
  
  viewer->SetCurve(vmin, vmax, iCurve, xlabel, ylabel);
}

void AppDensityCurvesWindow::SetLevelCurve(int i, const Vec2d& vmin, const Vec2d& vmax, const vector<Vec3r>& iCurve, const char *xlabel, const char *ylabel){
  AppGL2DCurvesViewer * viewer = 0;
  switch(i){
  case 1:viewer = LevelCurveViewer1; break;
  case 2:viewer = LevelCurveViewer2; break;
  case 3:viewer = LevelCurveViewer3; break;
  case 4:viewer = LevelCurveViewer4; break;
  case 5:viewer = LevelCurveViewer5; break;
  case 6:viewer = LevelCurveViewer6; break;
  case 7:viewer = LevelCurveViewer7; break;
  case 8:viewer = LevelCurveViewer8; break;
  default:return;
  }
  
  viewer->SetCurve(vmin, vmax, iCurve, xlabel, ylabel);
}
