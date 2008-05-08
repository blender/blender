//
//  Filename         : AppOptionsWindow.h
//  Author           : Emmanuel Turquin, Stephane Grabli
//  Purpose          : Class to define the options window
//  Date of creation : 27/01/2002
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

#ifndef ARTOPTIONSWINDOW_H
#define ARTOPTIONSWINDOW_H

#include "ConfigIO.h"
#include "ui_dir/ui_optionswindow4.h"

using namespace Ui;

class AppOptionsWindow : public QDialog, public OptionsWindow
{ 
    Q_OBJECT

public:

	AppOptionsWindow(QWidget *parent = 0, const char *name = 0, bool modal = FALSE, Qt::WFlags fl = 0);
    ~AppOptionsWindow();

    virtual void updateViewMapFormat();

public slots:

    virtual void Ok();
    virtual void Apply();
    virtual void Cancel();

    virtual void ModelsAdd();
    virtual void PatternsAdd();
    virtual void BrushesAdd();
    virtual void PythonAdd();
    virtual void HelpAdd();

    virtual void PaperAdd();
    virtual void PaperRemove();
    virtual void PaperUp();
    virtual void PaperDown();
    virtual void PaperClear();

 private:

    virtual QString DirDialog();
    virtual void Propagate();
    
    ConfigIO* _options;
};

#endif // ARTOPTIONSWINDOW_H
