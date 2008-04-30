#ifndef ARTPROGRESSBAR_H
#define ARTPROGRESSBAR_H

//
//
//                        FileName          : AppProgressBar.h
//                        Author            : Stephane Grabli
//                        Purpose           : Class to define the App progress bar
//                        Date Of Creation  : 27/08/2002
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
#include "../system/ProgressBar.h"

class QProgressDialog;
class AppProgressBar : public ProgressBar
{
public:
  AppProgressBar();

  virtual ~AppProgressBar() {}

  virtual void reset();
  virtual void setTotalSteps(unsigned n);
  virtual void setProgress(unsigned i);
  virtual void setLabelText(const string& text) ;

  void setQTProgressBar(QProgressDialog* qtBar) {_qtProgressBar = qtBar;}

  QProgressDialog * getQTProgressBar() {return _qtProgressBar;}

private:
  QProgressDialog *_qtProgressBar;
};

#endif
