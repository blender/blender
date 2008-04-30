//
//  Filename         : QStyleModuleSyntaxHighlighter.h
//  Author           : Stephane Grabli
//  Purpose          : Class to define the syntax highlighting
//                     of the style module
//  Date of creation : 07/01/2004
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

#ifndef QSTYLEMODULESYNTAXHIGHLIGHTER_H
#define QSTYLEMODULESYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <vector>

class QStyleModuleSyntaxHighlighter : public QSyntaxHighlighter
{
public:
  QStyleModuleSyntaxHighlighter(QTextEdit *iTextEdit);
  virtual ~QStyleModuleSyntaxHighlighter();
  
  virtual void highlightBlock ( const QString & text) ;
  
protected:
  void highlightKeywords(const QString& text, std::vector<QString>& iKeywords, const QColor& iColor);
  void dynamicHighlight(const QString& text);
  void highlightComment(const QString& text);

private:
  std::vector<QString> _operators; 
  std::vector<QString> _functors;
  std::vector<QString> _python;
  QColor _defaultColor; 
};

#endif
