
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
#include <qgl.h>
#include "Controller.h"
#include "AppMainWindow.h"
#include "AppConfig.h"

// Global
Controller	*g_pController;

int main(int argc, char** argv)
{
  // sets the paths
	QApplication::setColorSpec(QApplication::ManyColor);
  QApplication *app = new QApplication(argc, argv);
  Q_INIT_RESOURCE(freestyle);

  Config::Path pathconfig;
  
  QGLFormat myformat;
  myformat.setAlpha(true);
  QGLFormat::setDefaultFormat( myformat );
  
  AppMainWindow mainWindow(NULL, "Freestyle");
  //app->setMainWidget(mainWindow); // QT3

  g_pController = new Controller;
  g_pController->SetMainWindow(&mainWindow);
  g_pController->SetView(mainWindow.pQGLWidget);
  
  mainWindow.show();

  int res = app->exec();

  delete g_pController;
  
  return res;
}
