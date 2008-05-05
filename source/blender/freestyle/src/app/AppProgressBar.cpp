
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
#include <qprogressdialog.h>
#include <qapplication.h>
#include "AppProgressBar.h"

AppProgressBar::AppProgressBar()
	: ProgressBar()
{
	_qtProgressBar = 0;
}

void AppProgressBar::reset()
{
  ProgressBar::reset();
  if(NULL == _qtProgressBar)
    return;

  _qtProgressBar->reset();
  _qtProgressBar->show();
}

void AppProgressBar::setTotalSteps(unsigned n)
{
  ProgressBar::setTotalSteps(n);
  if(NULL == _qtProgressBar)
    return;
  
   _qtProgressBar->setRange(0,_numtotalsteps);
}

void AppProgressBar::setProgress(unsigned i)
{
  if(i > _numtotalsteps)
    return;

  ProgressBar::setProgress(i);
  if(NULL == _qtProgressBar)
    return;

  _qtProgressBar->setValue(_progress);
  qApp->processEvents();

  if(i == _numtotalsteps)
  {
    _qtProgressBar->setValue(_numtotalsteps);
    
    _qtProgressBar->reset();
    ProgressBar::reset();
    _qtProgressBar->hide();
  }
}

void AppProgressBar::setLabelText(const string& label)
{
  ProgressBar::setLabelText(label);
  if (NULL == _qtProgressBar)
    return;
  _qtProgressBar->setLabelText(_label.c_str());
}
