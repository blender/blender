
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
#include "QStyleModuleSyntaxHighlighter.h"
#include <QTextEdit>
#include <QRegExp> 
#include <iostream>
using namespace std;

QStyleModuleSyntaxHighlighter::QStyleModuleSyntaxHighlighter(QTextEdit *iTextEdit) 
    : QSyntaxHighlighter(iTextEdit){
  _operators.push_back("Operators");
  _operators.push_back("select");
  _operators.push_back("chain");
  _operators.push_back("bidirectionalChain");
  _operators.push_back("sequentialSplit");
  _operators.push_back("recursiveSplit");
  _operators.push_back("sort");
  _operators.push_back("create");

  _functors.push_back("StrokeShader");
  _functors.push_back("UnaryPredicate1D");
  _functors.push_back("UnaryPredicate0D");
  _functors.push_back("BinaryPredicate1D");
  _functors.push_back("ChainingIterator");
  //  _functors.push_back("getName");
  //  _functors.push_back("shade");
  //  _functors.push_back("getObject");

  _python.push_back("class ");
  _python.push_back("from ");
  _python.push_back("import ");
  _python.push_back("__init__");
  _python.push_back("__call__");
  _python.push_back("def ");
  _python.push_back("self");
  _python.push_back("return");
  //_python.push_back("print");
  //  _python.push_back("for");
  //  _python.push_back("if");
  //  _python.push_back("while");
  //  _python.push_back("range");
  //  _python.push_back("in");

	_defaultColor = iTextEdit->textColor();
} 

QStyleModuleSyntaxHighlighter::~QStyleModuleSyntaxHighlighter(){
  _operators.clear();
  _functors.clear();
  _python.clear();
}

void QStyleModuleSyntaxHighlighter::highlightKeywords(const QString& text, vector<QString>& iKeywords, const QColor& iColor){
  int pos = 0;
  int pos1 = 0;
  int pos2 = 0;
  bool found = false;
  for(vector<QString>::iterator o=iKeywords.begin(), oend=iKeywords.end();
  o!=oend;
  ++o){
    pos =0;
    QString word = *o;
    while ( ( pos = text.indexOf(word,pos) ) != -1 ){
      setFormat( pos , word.length() , iColor);
      pos += text.length()+1;
    }

    //    while ( ( pos = text.find(QRegExp("(^|\\W)"+word+"\\W"),pos) ) != -1 ){
    //      setFormat( pos , word.length()+1 , iColor);
    //      pos += text.length()+1;
    //    }
  }
}

void QStyleModuleSyntaxHighlighter::dynamicHighlight(const QString& text){
  int pos = 0;
  int pos1 = 0;
  int pos2 = 0;
  while((pos1 = text.indexOf("class", pos, Qt::CaseSensitive) ) != -1 ){
    int tmpPos = pos1+6;
	if( ( pos2 = text.indexOf('(',tmpPos, Qt::CaseSensitive) ) != -1 ){
      setFormat( tmpPos , pos2-tmpPos , QColor(0,0,255));
      pos += pos2 - pos1+1;
    } else{
      setFormat( tmpPos, text.length()-tmpPos, QColor(0,0,255));
      pos += text.length()+1;
    }
  }

  while((pos1 = text.indexOf("def",pos, Qt::CaseSensitive) ) != -1 ){
    int tmpPos = pos1+4;
    if( ( pos2 = text.indexOf('(',tmpPos, Qt::CaseSensitive) ) != -1 ){
      setFormat( tmpPos , pos2-tmpPos , QColor(136,0,0));
      pos += pos2 - pos1+1;
    } else{
      setFormat( tmpPos, text.length()-tmpPos, QColor(136,0,0));
      pos += text.length()+1;
    }
  }

  pos = 0;
  while((pos1 = text.indexOf("UnaryFunction", pos) ) != -1 ){
    if( ( pos2 = text.indexOf(QRegExp("\\W"), pos1) ) != -1 ){
      setFormat( pos1 , pos2-pos1 , QColor(0,0,255));
      pos += pos2 - pos1+1;
    } else{
      setFormat( pos1, text.length()-pos1, QColor(0,0,255));
      pos += text.length()+1;
    }
  }
}

void QStyleModuleSyntaxHighlighter::highlightComment(const QString& text){
  int pos = 0;
  int pos1 = 0;
  int pos2 = 0;
  while((pos1 = text.indexOf('#',pos, Qt::CaseSensitive) ) != -1 ){
    if( ( pos2 = text.indexOf('\n',pos1, Qt::CaseSensitive) ) != -1 ){
      setFormat( pos1 , pos2 , QColor(0,128,0));
      pos += pos2 - pos1;
      //setFormat( pos , text.length()-pos , _defaultColor );
    } else{
      setFormat( pos1 , text.length()-pos1, QColor(0,128,0));
      pos += text.length()+1;
    }
  }
}

void QStyleModuleSyntaxHighlighter::highlightBlock ( const QString & text) {
  setFormat( 0 , text.length() , _defaultColor );

  highlightKeywords(text, _python, QColor(128,128,128));
  highlightKeywords(text, _functors, QColor(136,0,0));
  dynamicHighlight(text);
  highlightKeywords(text, _operators, QColor(0,0,255));
  highlightComment(text);
}