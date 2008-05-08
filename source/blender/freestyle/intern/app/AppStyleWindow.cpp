
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

#include <fstream>
#include <QCursor>
#include <QApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QString>
#include "AppStyleWindow.h"
#include "../stroke/Canvas.h"
#include "../stroke/StyleModule.h"
#include "Controller.h"
#include "AppInteractiveShaderWindow.h"
#include "AppConfig.h"

AppStyleWindow::AppStyleWindow(QWidget* parent /* = 0 */, const char* name /* = 0 */, Qt::WFlags fl /* = 0 */)
  : QDialog(parent, fl)
{
  // QDialog *widget = new QDialog(parent);
  setupUi(this);
	PlayList->setShowGrid(false);
	PlayList->verticalHeader()->setVisible(false);
	PlayList->horizontalHeader()->setClickable(false);
	PlayList->setSelectionBehavior(QAbstractItemView::SelectRows);
	PlayList->setSelectionMode(QAbstractItemView::SingleSelection);
	PlayList->setColumnCount(5);
  PlayList->setColumnWidth(0, 37);
  PlayList->setColumnWidth(1, width() - 98);
  PlayList->setColumnWidth(2, 37);
  PlayList->hideColumn(3);
  PlayList->hideColumn(4);
  PlayList->setRowCount(0);
  //PlayList->setsetLeftMargin(0);
	PlayList->setHorizontalHeaderLabels((QStringList() << "Disp." << "Style Modules" << "Mod."));
  _pInteractiveShaderWindow = new AppInteractiveShaderWindow(this);
  _pInteractiveShaderWindow->hide();
  QString projectDir(Config::Path::getInstance()->getProjectDir());
  _mod0_image = new QPixmap(QString::fromUtf8(":/icons/icons/mod0.png"));
  _mod1_image = new QPixmap(QString::fromUtf8(":/icons/icons/mod1.png"));
  _disp0_image = new QPixmap(QString::fromUtf8(":/icons/icons/eye0.png"));
  _disp1_image = new QPixmap(QString::fromUtf8(":/icons/icons/eye1.png"));

  connect(_pInteractiveShaderWindow, SIGNAL(save()), SLOT(fileSave()));
	// signals and slots connections
    connect( addButton, SIGNAL( clicked() ), this, SLOT( Add() ) );
    connect( removeButton, SIGNAL( clicked() ), this, SLOT( Remove() ) );
    connect( PlayList, SIGNAL( cellDoubleClicked(int,int) ), this, SLOT( Display(int,int) ) );
    connect( PlayList, SIGNAL( cellClicked(int,int) ), this, SLOT( ToggleLayer(int,int) ) );
    connect( clearButton, SIGNAL( clicked() ), this, SLOT( Clear() ) );
    connect( saveButton, SIGNAL( clicked() ), this, SLOT( SaveList() ) );
    connect( moveUpButton, SIGNAL( clicked() ), this, SLOT( Up() ) );
    connect( moveDownButton, SIGNAL( clicked() ), this, SLOT( Down() ) );
    connect( editButton, SIGNAL( clicked() ), this, SLOT( Edit() ) );
    connect( closeButton, SIGNAL( clicked() ), this, SLOT( Close() ) );
}

AppStyleWindow::~AppStyleWindow()
{
  delete _mod0_image;
  delete _mod1_image;
  delete _disp0_image;
  delete _disp1_image;
}

void AppStyleWindow::Add(const char* iFileName, bool iDisp) {
  //Add the item in the view box
  //PlayList->insertItem(fi.fileName());  
  // PlayList->insertItem(s);
  int currentRow;
  QFileInfo fi(iFileName);
  if(0 == PlayList->rowCount())
    {
      currentRow = -1;
    }
  else
    {
      currentRow = PlayList->currentRow();
    }
  PlayList->insertRow(currentRow+1);
	for(int i=0; i< PlayList->rowCount(); ++i){
		PlayList->setRowHeight(i, 20);
	}
  //PlayList->setRowHeight(currentRow + 1, 20);

	// eye item
	QTableWidgetItem * eye_item = new QTableWidgetItem;
	eye_item->setFlags(Qt::ItemIsEnabled);
	PlayList->setItem(currentRow + 1, 0, eye_item);
	// style module name item
	QTableWidgetItem * style_module_name_item = new QTableWidgetItem(fi.fileName());
	style_module_name_item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
  PlayList->setItem(currentRow + 1, 1, style_module_name_item);
  PlayList->setItem(currentRow + 1, 3, new QTableWidgetItem(iFileName));
	// refresh item
	QTableWidgetItem * refresh_item = new QTableWidgetItem;
	refresh_item->setFlags(Qt::ItemIsEnabled);
	PlayList->setItem(currentRow + 1, 2, refresh_item);
	
	setModified(currentRow + 1, true);
	QTableWidgetItem *checkItem = new QTableWidgetItem;
  checkItem->setFlags(Qt::ItemIsUserCheckable);
  if(iDisp)
	  checkItem->setCheckState(Qt::Checked);
  else
		checkItem->setCheckState(Qt::Unchecked);
  PlayList->setItem(currentRow + 1, 4, checkItem);
  setChecked(currentRow + 1, iDisp);
	PlayList->setCurrentCell(currentRow + 1, 1);
	//PlayList->setRangeSelected(QTableWidgetSelectionRange( currentRow+1, 0, currentRow+1, 4), true);
	QString text = (PlayList->item(currentRow + 1, 3))->text();
	PlayList->takeVerticalHeaderItem(currentRow + 1);
  _pInteractiveShaderWindow->setCurrentShaderRow(currentRow + 1);
  _pInteractiveShaderWindow->DisplayShader(text);

  // Load the shader in memory and add it to the 
  // canvas list
  g_pController->InsertStyleModule(currentRow + 1, iFileName);
  g_pController->toggleLayer(currentRow + 1, iDisp);
}

void AppStyleWindow::AddList(const char* iFileName) {
  ifstream ifs(iFileName);
  if (!ifs.is_open()) {
    cerr << "Error: Cannot load this file" << endl;
    return;
  }
  QFileInfo fi(iFileName);
  char tmp_buffer[256];
  string s;
  bool disp = true;
  while (!ifs.eof()) {
    ifs.getline(tmp_buffer, 255);
    if (!tmp_buffer[0] || tmp_buffer[0] == '#')
      continue;
    if (tmp_buffer[0] == '0')
      disp = false;
    else
      disp = true;
    s = (const char*)fi.dir().path().toAscii().data();
    s += Config::DIR_SEP;
    s += tmp_buffer + 1;
    ifstream test(s.c_str(), ios::binary);
    if (!test.is_open()) {
      cerr << "Error: Cannot load \"" << tmp_buffer + 1 << "\"" << endl;
      continue;
    }
    Add(s.c_str(), disp);
  }
}

void AppStyleWindow::SaveList() {
  QString s = QFileDialog::getSaveFileName(
						this,
	  				   "Save file dialog"
					   "Choose a file",
						g_pController->getModulesDir(),
					   "Style modules lists (*." + Config::STYLE_MODULES_LIST_EXTENSION + ")");

  if (s.isEmpty())
    return;
  QFileInfo fi( s );
  QString ext = fi.suffix();
  if (ext != Config::STYLE_MODULES_LIST_EXTENSION)
    s += "." + Config::STYLE_MODULES_LIST_EXTENSION;
  ofstream ofs(s.toAscii().data(), ios::binary);
  if (!ofs.is_open()) {
    cerr << "Error: Cannot save this file" << endl;
    return;
  }
  
  QTableWidgetItem *checkItem;
	for (unsigned i = 0 ; i < PlayList->rowCount(); i++) {
    checkItem = PlayList->item(i, 4);
		ofs << ((checkItem->checkState() ==  Qt::Checked) ? '1' : '0');
    ofs << PlayList->item(i, 1)->text().toAscii().data() << endl;
  }
  g_pController->setModulesDir(fi.dir().path());
  cout << "Style modules list saved" << endl;
}

void AppStyleWindow::Add()
{
  // Load Module
  QString s = QFileDialog::getOpenFileName(this,
						"Open file dialog"
					   "Choose a file",
						g_pController->getModulesDir(),
					   "Style modules (*." + Config::STYLE_MODULE_EXTENSION + ")"
					   ";;"
					   "Style modules lists (*." + Config::STYLE_MODULES_LIST_EXTENSION + ")");

  QFileInfo fi( s );
  QString ext = fi.suffix();   // ext is taken after the last dot.

  if (ext == Config::STYLE_MODULE_EXTENSION) {
    g_pController->setModulesDir(fi.dir().path());
    Add(s.toAscii().data());
  }
  else if (ext == Config::STYLE_MODULES_LIST_EXTENSION) {
    g_pController->setModulesDir(fi.dir().path());
    AddList(s.toAscii().data());
  }
}

void AppStyleWindow::Remove()
{
  // Remove the selected item
  g_pController->RemoveStyleModule(PlayList->currentRow());
  PlayList->removeRow(PlayList->currentRow());
  _pInteractiveShaderWindow->fileClose();
}

void AppStyleWindow::Clear()
{
  g_pController->Clear();
	for (int i = PlayList->rowCount() - 1; i >= 0; i--)
    PlayList->removeRow(i);
  _pInteractiveShaderWindow->fileClose();
}

void AppStyleWindow::ExposeInteractiveShader()
{
  _pInteractiveShaderWindow->show();
  //_pInteractiveShaderWindow->Load();
}

void AppStyleWindow::setModified(unsigned row, bool mod) {
  if (mod) {
		PlayList->item(row, 2)->setIcon(QIcon(*_mod1_image));
    return;
  }
  Canvas* canvas = Canvas::getInstance();
  StyleModule* sm = canvas->getCurrentStyleModule();
  if (sm && sm->getAlwaysRefresh())
    return;
  PlayList->item(row, 2)->setIcon(QIcon(*_mod0_image));
}

void AppStyleWindow::setChecked(unsigned row, bool check) {
  if (check)
		PlayList->item(row, 0)->setIcon(QIcon(*_disp1_image));
  else
		PlayList->item(row, 0)->setIcon(QIcon(*_disp0_image));
}

void AppStyleWindow::Edit() {
	if(PlayList->rowCount() == 0)
    return;

  int currentRow = PlayList->currentRow();

  ExposeInteractiveShader();
	QString text = (PlayList->item(currentRow, 3)->text());
  _pInteractiveShaderWindow->setCurrentShaderRow(currentRow);
  _pInteractiveShaderWindow->DisplayShader(text);
}

void AppStyleWindow::Display( int row, int col ) {
  if(col != 1)
    return;

  Edit();
}

void AppStyleWindow::ToggleLayer(int row, int col)
{
  if(0 == PlayList->rowCount())
    return;
  
  if(col != 0)
    return;

	QTableWidgetItem *checkItem = PlayList->item(row, 4);
	if(checkItem->flags() != Qt::ItemIsUserCheckable)
    return;

	bool isChecked;
	if(checkItem->checkState() == Qt::Checked){
		checkItem->setCheckState(Qt::Unchecked);
		isChecked = false;
	}else{
		checkItem->setCheckState(Qt::Checked);
		isChecked = true;
	}
  g_pController->toggleLayer(row, isChecked);
  setChecked(row, isChecked);
}

void AppStyleWindow::Up() {
  int current = PlayList->currentRow();
  if (current > 0) {
    SwapShaders(current, current - 1);
    PlayList->clearSelection();

		PlayList->setRangeSelected(QTableWidgetSelectionRange( current-1, 0, current-1, 4), true);
		PlayList->setCurrentCell(current-1, 1);
    g_pController->updateCausalStyleModules(current - 1);
		current = current-1;
  }
}

void AppStyleWindow::Down() {
  int current = PlayList->currentRow();
  if (current < PlayList->rowCount() - 1) {
    SwapShaders(current, current + 1);
    PlayList->clearSelection();

    PlayList->setRangeSelected(QTableWidgetSelectionRange( current+1, 0, current+1, 4), true);
		PlayList->setCurrentCell(current+1, 1);

    g_pController->updateCausalStyleModules(current);
		current = current +1;
  }
}

void AppStyleWindow::fileSave() {
  int current = _pInteractiveShaderWindow->getCurrentShaderRow();
	QString text = (PlayList->item(current, 3)->text());
  g_pController->ReloadStyleModule(current, text.toAscii().data());
	QTableWidgetItem *checkItem = PlayList->item(current, 4);
	bool isChecked = (checkItem->checkState() == Qt::Checked) ? true : false;
  g_pController->toggleLayer(current, isChecked);
}

void AppStyleWindow::resetModified(bool iMod)
{
  for(int i=0; i < PlayList->rowCount(); i++)
  {
    setModified(i,iMod);
  }
}

void AppStyleWindow::SwapShaders(int i1, int i2) {
  g_pController->SwapStyleModules(i1, i2);
  //PlayList->swapRows(i1, i2);
	QTableWidgetItem *first_row_items[5];
	QTableWidgetItem *second_row_items[5];
	int i;
	for(i=0;i<5;++i){
		first_row_items[i] = PlayList->takeItem(i1, i);
		second_row_items[i] = PlayList->takeItem(i2, i);
	}
	for(i=0;i<5;++i){
		PlayList->setItem(i1, i, second_row_items[i]);
		PlayList->setItem(i2, i, first_row_items[i]);
	}
}
