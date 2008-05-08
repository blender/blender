//
//  Filename         : AppDensityCurvesWindow.h
//  Author           : Stephane Grabli
//  Purpose          : Class to define the density curves display window
//  Date of creation : 14/03/04
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

#ifndef  APPDENSITYCURVESWINDOW_H
# define APPDENSITYCURVESWINDOW_H

#include "ui_dir/ui_densitycurveswindow4.h"
#include <vector>
#include "../geometry/Geom.h"
using namespace std;
using namespace Geometry;
using namespace Ui;

class NodeGroup;

class AppDensityCurvesWindow : public QDialog, public DensityCurvesWindow
{
Q_OBJECT
public:
	AppDensityCurvesWindow(QWidget *parent = 0, const char *name = 0, bool modal = FALSE, Qt::WFlags fl = 0);
  virtual ~AppDensityCurvesWindow();

  /*! Sets the node that contains the orientation curve i in 
   *  viewer i (among 5).
   *  \param i
   *    The number of the viewer where the curve must be displayed.(0<=i<5).
   *  \param vmin
   *    The x,y min of the curve
   *  \param vmax
   *    The x,y max of the curve
   *  \param iCurve
   *    The array of XYZ coordinates of the points.
   *  \param xlabel
   *    The label of the x-axis
   *  \param ylabel
   *    The label of the y-axis
   */
  void SetOrientationCurve(int i, const Vec2d& vmin, const Vec2d& vmax, const vector<Vec3r>& iCurve, const char *xlabel, const char *ylabel);

  /*! Sets the node that contains the level curve i in 
   *  viewer i (i in [1,8]).
   *  \param i
   *    The number of the viewer where the curve must be displayed.(0<=i<5).
   *  \param vmin
   *    The x,y min of the curve
   *  \param vmax
   *    The x,y max of the curve
   *  \param iCurve
   *    The array of XYZ coordinates of the points.
   *  \param xlabel
   *    The label of the x-axis
   *  \param ylabel
   *    The label of the y-axis
   */
  void SetLevelCurve(int i, const Vec2d& vmin, const Vec2d& vmax, const vector<Vec3r>& iCurve, const char *xlabel, const char *ylabel);
};

#endif // APPDENSITYCURVESWINDOW_H


