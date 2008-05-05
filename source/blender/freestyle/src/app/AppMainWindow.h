
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
#ifndef ARTMAINWINDOW_H
#define ARTMAINWINDOW_H

#include <QKeyEvent>
#include <QWidget>
#include <QPainter>
#include <QColorGroup>
#include <QToolBar>
#include <QMainWindow>
#include "ui_dir/ui_appmainwindowbase4.h"

using namespace Ui;

class QProgressDialog;
class AppGLWidget;
class AppMainWindow : public QMainWindow, public AppMainWindowBase
{
  Q_OBJECT

public:

	AppMainWindow(QWidget *parent = 0, const char *name = 0, Qt::WindowFlags f = Qt::Window);
  ~AppMainWindow();

  QToolBar *pTools;

public slots:
  virtual void fileOpen();
  virtual void fileClose();
  virtual void loadCamera();
  virtual void DisplayStylesWindow();
  virtual void DisplayOptionsWindow();
  virtual void DisplayHelp();
  virtual void About();
  virtual void ViewMapFileSave();
  //  virtual void AppelSilhouette();
  //  virtual void BrutForceSilhouette();
  virtual void ComputeViewMap();
  virtual void SaveDirectionalViewMapImages();
  virtual void ComputeStrokes();
  virtual void Snapshot();
  virtual void captureMovie();
  virtual void ResetInterpreter();
  virtual void PSSnapshot();
  virtual void TextSnapshot();

public:
  //  void InitProgressBar(const char *title, int numSteps)
  ;
  //  void SetProgressLabel(const char *label);
  //  void SetProgress(int i);
  //  
  void DisplayMessage(const char* msg, bool persistent = false);

  QProgressDialog * qtProgressDialog() {return _ProgressBar;}
  AppGLWidget * pQGLWidget;

private:
  QProgressDialog* _ProgressBar;
};


#endif
