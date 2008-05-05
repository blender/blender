
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
#include <QTextEdit>
#include <QFileDialog>
#include "AppConfig.h"
#include "Controller.h"
#include "AppInteractiveShaderWindow.h"
#include "QStyleModuleSyntaxHighlighter.h"

AppInteractiveShaderWindow::AppInteractiveShaderWindow(QWidget* parent /* = 0 */, const char* name /* = 0 */, bool modal /* = FALSE */, Qt::WFlags fl /* = 0  */)
: InteractiveShaderWindow() // parent, name, modal, fl)
{
	setupUi(this);
  _CurrentShaderRow = -1;
  _syntaxHighlighter = new QStyleModuleSyntaxHighlighter(TextArea);
	// signals and slots connections
    connect( CancelButton, SIGNAL( clicked() ), this, SLOT( fileClose() ) );
    connect( SaveButton, SIGNAL( clicked() ), this, SLOT( fileSave() ) );
    connect( SaveAsButton, SIGNAL( clicked() ), this, SLOT( fileSaveAs() ) );
    connect( OkButton, SIGNAL( clicked() ), this, SLOT( fileOk() ) );
}

AppInteractiveShaderWindow::~AppInteractiveShaderWindow()
{
  if(_syntaxHighlighter){
    delete _syntaxHighlighter;
  }
}

void AppInteractiveShaderWindow::fileOk()
{
  fileSave();
  fileClose();
}

void AppInteractiveShaderWindow::fileClose()
{
  TextArea->clear();
  close();
}

void AppInteractiveShaderWindow::fileSave()
{
  QFile file(_CurrentShader);
  if ( !file.open( QIODevice::WriteOnly ) )
    return;
  QTextStream ts( &file );
  ts << TextArea->toPlainText();

  file.close();
  emit save();
  g_pController->setModified(_CurrentShaderRow, true);
}

void AppInteractiveShaderWindow::fileSaveAs()
{
  QFileInfo fi1(_CurrentShader);
  QString ext1 = fi1.suffix();
  QString fn;

  if (ext1 == Config::STYLE_MODULE_EXTENSION)
    fn = QFileDialog::getSaveFileName(this,
						"save file dialog"
				      "Choose a file",
						g_pController->getModulesDir(),
				      "Style modules (*." + Config::STYLE_MODULE_EXTENSION + ")");
  if (!fn.isEmpty() && (_CurrentShader == fn))
    fileSave();
  else if (!fn.isEmpty())
    {
      QFileInfo fi2(fn);
      QString ext2 = fi2.suffix();
      if (ext1 != ext2)
	fn += "." + ext1;
      QFile file(fn);
	  if ( !file.open( QIODevice::WriteOnly ) )
	return;
      QTextStream ts( &file );
      ts << TextArea->toPlainText();
      file.close();
      g_pController->AddStyleModule(fn.toAscii().data());
      g_pController->setModulesDir(fi2.dir().path());
    }
}

void AppInteractiveShaderWindow::DisplayShader(QString& iName)
{
  _CurrentShader = iName;
  QFile file( iName);
  if ( !file.open( QIODevice::ReadOnly ) )
	  return;
  
  QTextStream ts( &file );
  TextArea->setText( ts.readAll() );
  TextArea->viewport()->setFocus();

  // Set window title:
  QFileInfo fi(iName);
  setWindowTitle(fi.fileName());
  g_pController->setModulesDir(fi.dir().path());
}
