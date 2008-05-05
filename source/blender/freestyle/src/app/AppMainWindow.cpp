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
#include <QApplication>
#include <QCursor>
#include <QProgressDialog>
#include <QFileDialog>
#include <QFile>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QLayout>
#include "AppMainWindow.h"
#include "AppGLWidget.h"
#include "Controller.h"
#include "AppConfig.h"

AppMainWindow::AppMainWindow(QWidget *parent, const char *name, Qt::WindowFlags f)
	: QMainWindow(parent, f)  // parent, name, f)
{
	setupUi(this); 
	pQGLWidget = new AppGLWidget(this);
	gridLayout->addWidget(pQGLWidget);

  // setCaption(Config::APPLICATION_NAME + " " + Config::APPLICATION_VERSION);
  setGeometry(20,20,700,700);
  pQGLWidget->SetMainWindow(this);

  _ProgressBar = new QProgressDialog(Config::APPLICATION_NAME + " Progress Dialog", "Cancel", 
				     0, 100, this);
	// signals and slots connections
    connect( actionOpen, SIGNAL( triggered() ), this, SLOT( fileOpen() ) );
    connect( actionQuit, SIGNAL( triggered() ), this, SLOT( close() ) );
    connect( actionClose, SIGNAL( triggered() ), this, SLOT( fileClose() ) );
    connect( actionComputeViewMap, SIGNAL( triggered() ), this, SLOT( ComputeViewMap() ) );
    connect( actionSave, SIGNAL( triggered() ), this, SLOT( ViewMapFileSave() ) );
    connect( actionStyleModelerWindow, SIGNAL( triggered() ), this, SLOT( DisplayStylesWindow() ) );
    connect( actionOptionsWindow, SIGNAL( triggered() ), this, SLOT( DisplayOptionsWindow() ) );
    connect( actionComputeStrokes, SIGNAL( triggered() ), this, SLOT( ComputeStrokes() ) );
    connect( actionHelp, SIGNAL( triggered() ), this, SLOT( DisplayHelp() ) );
    connect( actionSaveSnapshot, SIGNAL( triggered() ), this, SLOT( Snapshot() ) );
    connect( actionCaptureMovie, SIGNAL( triggered() ), this, SLOT( captureMovie() ) );
    connect( actionResetInterpreter, SIGNAL( triggered() ), this, SLOT( ResetInterpreter() ) );
    connect( actionSaveDirectionalViewMapImages, SIGNAL( triggered() ), this, SLOT( SaveDirectionalViewMapImages() ) );
    connect( actionAbout, SIGNAL( triggered() ), this, SLOT( About() ) );
    connect( actionLoadCamera, SIGNAL( triggered() ), this, SLOT( loadCamera() ) );
    connect( actionSavePSSnapshot, SIGNAL( triggered() ), this, SLOT( PSSnapshot() ) );
    connect( actionSaveTextSnapshot, SIGNAL( triggered() ), this, SLOT( TextSnapshot() ) );
    connect( actionControlBindings, SIGNAL( triggered() ), pQGLWidget, SLOT( help() ) );
}

AppMainWindow::~AppMainWindow() {}

void AppMainWindow::fileOpen()
{
  QString s = QFileDialog::getOpenFileName(this,
	  "open file dialog"
	  "Choose a file",
	  g_pController->getModelsDir(),
					   "Scenes (*.3ds *.3DS);;ViewMaps (*." + Config::VIEWMAP_EXTENSION + ")");
  if ( s.isEmpty() ) {
    statusBar()->showMessage( "Loading aborted", 2000 );
    return;
  }

  QFileInfo fi(s);
  QString ext = fi.suffix();
  if ((ext == "3ds") || (ext == "3DS"))
    {
      QApplication::setOverrideCursor( Qt::WaitCursor );
      g_pController->Load3DSFile(s.toAscii().data()); // lunch time...
      g_pController->setModelsDir(fi.dir().path());
      QApplication::restoreOverrideCursor();
    }
  else if (ext == Config::VIEWMAP_EXTENSION)
    {
      QApplication::setOverrideCursor( Qt::WaitCursor );
      g_pController->LoadViewMapFile(s.toAscii().data()); // ...and now tea time...
      g_pController->setModelsDir(fi.dir().path());
      QApplication::restoreOverrideCursor();
    }
}

void AppMainWindow::loadCamera()
{
  QString s = QFileDialog::getOpenFileName(this,
						"open file dialog"
					   "Choose a file",
						g_pController->getModelsDir(),
					   "ViewMaps (*." + Config::VIEWMAP_EXTENSION + ")");
  if ( s.isEmpty() ) {
    statusBar()->showMessage( "Loading aborted", 2000 );
    return;
  }

  QFileInfo fi(s);
  QString ext = fi.suffix();
  if (ext == Config::VIEWMAP_EXTENSION)
    {
      QApplication::setOverrideCursor( Qt::WaitCursor );
      g_pController->LoadViewMapFile(s.toAscii().data(), true);
      QApplication::restoreOverrideCursor();
    }
}

void AppMainWindow::ViewMapFileSave() {
  QString s = QFileDialog::getSaveFileName(this,
					   "save file dialog"
					   "Choose a file",
		g_pController->getModelsDir(),
					   "ViewMaps (*." + Config::VIEWMAP_EXTENSION + ")");
  if (s.isEmpty()) {
    statusBar()->showMessage( "Saving aborted", 2000 );
    return;
  }

  QFileInfo fi(s);
  QString ext = fi.suffix();
  if(ext != Config::VIEWMAP_EXTENSION)
    s += "." + Config::VIEWMAP_EXTENSION;

  QApplication::setOverrideCursor( Qt::WaitCursor );
  g_pController->setModelsDir(fi.dir().path());
  g_pController->SaveViewMapFile(s.toAscii().data());
  QApplication::restoreOverrideCursor();
}

void AppMainWindow::fileClose()
{
  g_pController->CloseFile();
}

void AppMainWindow::DisplayStylesWindow() {
  g_pController->ExposeStyleWindow();
}

void AppMainWindow::DisplayOptionsWindow() {
  g_pController->ExposeOptionsWindow();
}

void AppMainWindow::DisplayHelp() {
  g_pController->ExposeHelpWindow();
}

void AppMainWindow::About() {
  g_pController->ExposeAboutWindow();
}

void AppMainWindow::Snapshot() {
  g_pController->saveSnapshot();
}

void AppMainWindow::captureMovie() {
  g_pController->captureMovie();
}

void AppMainWindow::ResetInterpreter() {
  g_pController->resetInterpreter();
}

//void AppMainWindow::BrutForceSilhouette()
//{
//  QApplication::setOverrideCursor( Qt::WaitCursor );
//  g_pController->ComputeSilhouette(Controller::BRUT_FORCE);
//  QApplication::restoreOverrideCursor();
//}
//
//void AppMainWindow::AppelSilhouette()
//{
//  QApplication::setOverrideCursor( Qt::WaitCursor );
//  g_pController->ComputeSilhouette();
//  QApplication::restoreOverrideCursor();
//}

void AppMainWindow::ComputeViewMap()
{
  QApplication::setOverrideCursor(Qt::WaitCursor);
  g_pController->ComputeViewMap();
  QApplication::restoreOverrideCursor();
}

void AppMainWindow::SaveDirectionalViewMapImages(){
  QApplication::setOverrideCursor(Qt::WaitCursor);
  g_pController->saveSteerableViewMapImages();
  QApplication::restoreOverrideCursor();
}

void AppMainWindow::ComputeStrokes()
{
  g_pController->DrawStrokes();
}

//void AppMainWindow::InitProgressBar(const char *title, int numSteps)
//{
//  _ProgressBar = new QProgressDialog(title, 0, numSteps, this, "progress", TRUE);
//  _ProgressBar->show();
//  _ProgressBar->setProgress(0);
//}
//
//void AppMainWindow::SetProgressLabel(const char *label)
//{
//  if(NULL == _ProgressBar)
//    return;
//  _ProgressBar->setLabelText(label);
//}
//
//void AppMainWindow::SetProgress(int i)
//{
//  _ProgressBar->setProgress(i);
//  qApp->processEvents();
//
//  if(i == _ProgressBar->totalSteps())
//  {
//    _ProgressBar->setProgress(_ProgressBar->totalSteps());
//    delete _ProgressBar;
//    _ProgressBar = NULL;
//  }
//}

void AppMainWindow::DisplayMessage(const char* msg, bool persistent)
{
  if(persistent)
    statusBar()->showMessage( msg);
  else
    statusBar()->showMessage( msg, 2000 );
}
//void AppMainWindow::toggleSilhouette(bool enabled)
//{
//  pQGLWidget->ToggleSilhouette(enabled);
//}

void AppMainWindow::PSSnapshot() {
  QString s = QFileDialog::getSaveFileName(this,
					   "save file dialog"
					   "Choose a file",
						g_pController->view()->snapshotFileName(),
					   "Encapsulated Postscript (*.eps)");
  if (s.isEmpty()) {
    statusBar()->showMessage( "Saving aborted", 2000 );
    return;
  }

  QFileInfo fi(s);
  QString ext = fi.suffix();
  if(ext != "eps")
    s += ".eps" ;

  QApplication::setOverrideCursor( Qt::WaitCursor );
  g_pController->savePSSnapshot(s);
  QApplication::restoreOverrideCursor();
}

void AppMainWindow::TextSnapshot() {
  QString s = QFileDialog::getSaveFileName(this,
						"save file dialog"
					   "Choose a file",
						g_pController->getModelsDir(),
					   "Text File (*.txt)");
  if (s.isEmpty()) {
    statusBar()->showMessage( "Saving aborted", 2000 );
    return;
  }

  QFileInfo fi(s);
  QString ext = fi.suffix();
  if(ext != "txt")
    s += ".txt" ;

  QApplication::setOverrideCursor( Qt::WaitCursor );
  g_pController->saveTextSnapshot(s);
  QApplication::restoreOverrideCursor();
}
