//
//  Filename         : ConfigIO.h
//  Author(s)        : Emmanuel Turquin
//  Purpose          : Configuration management
//  Date of creation : 26/02/2003
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

#ifndef  CONFIGIO_H
# define CONFIGIO_H

# include <qstring.h>
# include <qstringlist.h>
# include <qtextstream.h>
# include <qdom.h>
# include "../system/FreestyleConfig.h"

class ConfigIO
{
 public:

  ConfigIO(QString filename = "",
	   const QString& doc_type = "config_file",
	   bool automatic = false,
	   const QString& sep = "/");
  ~ConfigIO();

  QString	getDefaultFile() const;
  void		setDefaultFile(const QString& filename);

  bool		getAuto() const;
  void		setAuto(bool automatic);

  QString	getPathSep() const;
  void		setPathSep(const QString& sep);

  int		loadFile(const QString& filename = "");
  int		saveFile(const QString& filename = "") const;

  template <class T> int	getValue(const QString& path, T& res) const;
  template <class T> int	setValue(const QString& path, const T& src);

 private:

  QString		_path_sep;
  QString		_default_file;
  bool			_automatic;

  QDomDocument		_tree;
  QString		_doc_type;
};


//
// Implementation of templated methods
//
///////////////////////////////////////////////////

namespace Internal {

  template <class T>
  struct	readValue {
    void operator()(const QString &value, T &res) {
        QTextStream((QString *)&value, QIODevice::ReadOnly)>> res;
    }
  };

  template <>
  struct	readValue<QString> {
    void operator()(const QString& value, QString& res) {
      res = value;
    }
  };

  template <>
  struct	readValue<bool> {
    void operator()(const QString& value, bool& res) {
      short res_tmp;
      QTextStream((QString *)&value, QIODevice::ReadOnly) >> res_tmp;
      res = res_tmp;
    }
  };

} // end of namespace Internal


template <class T>
int	ConfigIO::getValue(const QString& path, T& res) const {

  // Split path
  QStringList strlist;
  strlist = path.split(_path_sep);

  unsigned size = strlist.size();
  if (size-- < 2)
    return 1;

  // try to find the right element
  QDomElement right_node;
  QDomElement node = _tree.documentElement().firstChild().toElement();
  for (unsigned i = 0;
       !node.isNull() && i < size;
       node = node.firstChild().toElement(), i++) {
    while (!node.isNull() && node.tagName() != strlist[i])
      node = node.nextSibling().toElement();
    right_node = node;
  }

  // and the right attribute
  if (right_node.hasAttribute(strlist[size])) {
    QString value = right_node.attribute(strlist[size]);
    Internal::readValue<T> rv;
    rv(value, res);
    return 0;
  }

  return 1;
}


template <class T>
int	ConfigIO::setValue(const QString& path, const T& src) {

  // Split path
  QStringList strlist = path.split(_path_sep);

  unsigned size = strlist.size();
  if (size-- < 2)
    return 1;

  // verify that the tree isn't empty
  // if so, create a root
  QDomElement node = _tree.documentElement();
  if (node.isNull()) {
      node = _tree.createElement(_doc_type);
      _tree.appendChild(node);
  }
  
  // find the right element
  QDomElement child = node.firstChild().toElement();
  for (unsigned i = 0;
       i < size;
       node = child, child = child.firstChild().toElement(), i++) {
    while (!child.isNull() && child.tagName() != strlist[i])
      child = child.nextSibling().toElement();
    if (child.isNull()) {
      child = _tree.createElement(strlist[i]);
      node.appendChild(child);
    }
  }

  // and set the attribute
  QString value;
  QTextStream(&value, QIODevice::WriteOnly) << src;
  node.setAttribute(strlist[size], value);

  return 0;
}

#endif // CONFIGIO_H
