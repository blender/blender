#ifndef ARTINTERACTIVESHADERWINDOW_H
#define ARTINTERACTIVESHADERWINDOW_H

//------------------------------------------------------------------------------------------//
//
//                        FileName          : AppInteractiveShaderWindow.h
//                        Author            : Stephane Grabli
//                        Purpose           : Class to define the graphic window displaying the interactive shader
//                        Date Of Creation  : 21/10/2002
//
//------------------------------------------------------------------------------------------//

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
#include "ui_dir/ui_interactiveshaderwindow4.h"
#include <QKeyEvent>

using namespace Ui;

class QStyleModuleSyntaxHighlighter;
class AppInteractiveShaderWindow : public QDialog, public InteractiveShaderWindow
{
  Q_OBJECT
    public:
		AppInteractiveShaderWindow(QWidget *parent = 0, const char *name = 0, bool modal = FALSE, Qt::WFlags fl = 0);
  virtual ~AppInteractiveShaderWindow();

public slots:
  virtual void fileOk();
  virtual void fileClose();
  virtual void fileSave();
  virtual void fileSaveAs();
  
  void DisplayShader(QString& iName);
  void setCurrentShaderRow(int current) { _CurrentShaderRow = current; }
  int getCurrentShaderRow() const { return _CurrentShaderRow; }

 signals:
  void save(  );

 protected:

	 void keyPressEvent(QKeyEvent *e) {
		 if (e->key() == Qt::Key_Escape)
	return;
      QDialog::keyPressEvent(e);
  }

 private:
  int _CurrentShaderRow;
  QString _CurrentShader;
  QStyleModuleSyntaxHighlighter *_syntaxHighlighter;
};

#endif

