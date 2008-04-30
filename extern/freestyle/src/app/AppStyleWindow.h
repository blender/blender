//
//  Filename         : AppStyleWindow.h
//  Author           : Stephane Grabli
//  Purpose          : Class to define the style window
//  Date of creation : 18/12/2002
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

#ifndef  ARTSTYLEWINDOW_H
#define  ARTSTYLEWINDOW_H

#include <QKeyEvent>
#include <QWidget>
#include <QPainter>
#include <QColorGroup>
#include "ui_dir/ui_stylewindow4.h"

using namespace Ui;

class AppInteractiveShaderWindow;
class AppStyleWindow : public QDialog, public StyleWindow
{
  Q_OBJECT
public:
	AppStyleWindow(QWidget* parent = 0, const char* name = 0, Qt::WFlags fl = 0);
  virtual ~AppStyleWindow();

  void ExposeInteractiveShader();
  /*! sets all layers to visible */
  //void resetLayers();
  
  virtual int  currentRow() const { return PlayList->currentRow(); }
  virtual void setModified(unsigned row, bool mod);
  virtual void resetModified(bool iMod = false);
  virtual void setChecked(unsigned row, bool check);

public slots:
    virtual void Add();
    virtual void Add(const char* iFileName, bool iDisp = true);
    virtual void SaveList();
    virtual void Remove();
    virtual void Clear();
    virtual void Up();
    virtual void Down();
    virtual void Edit();
    virtual void Close() { close(); }
    virtual void Display( int row, int col);
    virtual void ToggleLayer(int row, int col);
    virtual void SwapShaders(int i1, int i2);
    void fileSave();

 protected:
    
    void keyPressEvent(QKeyEvent *e) {
		if (e->key() == Qt::Key_Escape)
	return;
		QDialog::keyPressEvent(e);
    }

private:

    void AddList(const char* iFileName);

    AppInteractiveShaderWindow* _pInteractiveShaderWindow;

    QPixmap* _mod0_image;
    QPixmap* _mod1_image;
    QPixmap* _disp0_image;
    QPixmap* _disp1_image;
};

#endif // ARTSTYLEWINDOW_H
