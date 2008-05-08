
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

// Must be included before any QT header, because of moc
#include "../system/PythonInterpreter.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QString>
#include <QStringList>
#include <QListWidgetItem>
#include "../stroke/StrokeRenderer.h"
#include "AppConfig.h"
#include "Controller.h"
#include "../view_map/ViewMapIO.h"
#include "AppOptionsWindow.h"

AppOptionsWindow::AppOptionsWindow(QWidget *parent, const char *name, bool modal, Qt::WFlags fl)
: QDialog(parent, fl) { // parent, name, modal, fl) {

	setupUi(this);
  const QString sep(Config::DIR_SEP.c_str());
  QString filename;

  // Create a ConfigIO object
  filename = Config::Path::getInstance()->getHomeDir() + sep + Config::OPTIONS_DIR + sep + Config::OPTIONS_FILE;
  _options = new ConfigIO(filename, Config::APPLICATION_NAME + "Options");
  _options->loadFile();

  // Set the widgets to correct values

  // -> Directories tab
  QString str;
  Config::Path * cpath = Config::Path::getInstance();
  if (_options->getValue("default_path/models/path", str))
    str = cpath->getModelsPath();
  modelsPathLineEdit->setText(str);
  if (_options->getValue("default_path/patterns/path", str))
    str = cpath->getPatternsPath();
  patternsPathLineEdit->setText(str);
  if (_options->getValue("default_path/brushes/path", str))
    str = cpath->getBrushesPath();
  brushesPathLineEdit->setText(str);
  if (_options->getValue("default_path/python/path", str))
    str = cpath->getPythonPath();
  pythonPathLineEdit->setText(str);

  // -> Papers Textures tab
  unsigned papers_nb;
  QStringList sl;
  if (_options->getValue("papers/nb", papers_nb)) {
    sl.push_back(cpath->getPapersDir() + Config::DEFAULT_PAPER_TEXTURE);
  } else {
    for (unsigned i = 0; i < papers_nb; i++) {
      QString path;
      QTextStream(&path) << "papers/texture" << i << "/filename";
      _options->getValue(path, str);
	  paperTexturesList->insertItem(paperTexturesList->count(), str);
    }
  }
  

  // -> Help tab
  if (_options->getValue("default_browser/cmd", str))
    str = cpath->getBrowserCmd();
  browserCmdLineEdit->setText(str);
  if (_options->getValue("default_path/help/index", str))
    str = cpath->getHelpIndexpath();
  helpIndexPathLineEdit->setText(str);

  // -> Misc tab
  bool b;
  if (_options->getValue("default_viewmap_format/float_vectors", b))
    b = false;
  asFloatCheckBox->setChecked(b);
  if (_options->getValue("default_viewmap_format/no_occluders", b))
    b = false;
  noOccluderListCheckBox->setChecked(b);
  if (_options->getValue("default_viewmap_format/compute_steerable", b))
    b = false;
  steerableViewMapCheckBox->setChecked(b);
  if (_options->getValue("default_visibility/exhaustive_computation", b))
    b = true;
  qiCheckBox->setChecked(b);
  if (_options->getValue("default_drawing_buffers/back_buffer", b))
    b = true;
  backBufferCheckBox->setChecked(b);
  if (_options->getValue("default_drawing_buffers/front_buffer", b))
    b = false;
  frontBufferCheckBox->setChecked(b);
  real r;
  if (_options->getValue("default_ridges/sphere_radius", r))
    r = Config::DEFAULT_SPHERE_RADIUS;
  sphereRadiusLineEdit->setText(QString(QString::number(r)));
  if (_options->getValue("default_ridges/enable", b))
    b = false;
  ridgeValleyCheckBox->setChecked(b);
  if (_options->getValue("default_suggestive_contours/enable", b))
    b = false;
  suggestiveContoursCheckBox->setChecked(b);
  if (_options->getValue("default_suggestive_contours/dkr_epsilon", r))
    r = Config::DEFAULT_DKR_EPSILON;
  krEpsilonLineEdit->setText(QString(QString::number(r)));

  // Propagate changes
  Propagate();

	// signals and slots connections
    connect( okButton, SIGNAL( clicked() ), this, SLOT( Ok() ) );
    connect( applyButton, SIGNAL( clicked() ), this, SLOT( Apply() ) );
    connect( closeButton, SIGNAL( clicked() ), this, SLOT( Cancel() ) );
    connect( patternsPathAddButton, SIGNAL( clicked() ), this, SLOT( PatternsAdd() ) );
    connect( modelPathAddButton, SIGNAL( clicked() ), this, SLOT( ModelsAdd() ) );
    connect( addPaperTextureButton, SIGNAL( clicked() ), this, SLOT( PaperAdd() ) );
    connect( removePaperTextureButton, SIGNAL( clicked() ), this, SLOT( PaperRemove() ) );
    connect( moveUpPaperTextureButton, SIGNAL( clicked() ), this, SLOT( PaperUp() ) );
    connect( moveDownPaperTextureButton, SIGNAL( clicked() ), this, SLOT( PaperDown() ) );
    connect( clearPaperTextureButton, SIGNAL( clicked() ), this, SLOT( PaperClear() ) );
    connect( pythonPathAddButton, SIGNAL( clicked() ), this, SLOT( PythonAdd() ) );
    connect( helpIndexPathButton, SIGNAL( clicked() ), this, SLOT( HelpAdd() ) );
    connect( brushesPathAddButton, SIGNAL( clicked() ), this, SLOT( BrushesAdd() ) );
}

AppOptionsWindow::~AppOptionsWindow() {
  delete _options;
}

void AppOptionsWindow::Propagate() {

  // Directories
  ViewMapIO::Options::setModelsPath((const char*)modelsPathLineEdit->text().toAscii().data());
  PythonInterpreter::Options::setPythonPath((const char*)pythonPathLineEdit->text().toAscii().data());
  TextureManager::Options::setPatternsPath((const char*)patternsPathLineEdit->text().toAscii().data());
  TextureManager::Options::setBrushesPath((const char*)brushesPathLineEdit->text().toAscii().data());
  g_pController->setBrowserCmd(browserCmdLineEdit->text());
  g_pController->setHelpIndex(helpIndexPathLineEdit->text());

  // ViewMap Format
  if (asFloatCheckBox->isChecked())
    ViewMapIO::Options::addFlags(ViewMapIO::Options::FLOAT_VECTORS);
  else
    ViewMapIO::Options::rmFlags(ViewMapIO::Options::FLOAT_VECTORS);
  if (noOccluderListCheckBox->isChecked())
    ViewMapIO::Options::addFlags(ViewMapIO::Options::NO_OCCLUDERS);
  else
    ViewMapIO::Options::rmFlags(ViewMapIO::Options::NO_OCCLUDERS);
  g_pController->setComputeSteerableViewMapFlag(steerableViewMapCheckBox->isChecked());

  // Visibility
  if (qiCheckBox->isChecked())
    g_pController->setQuantitativeInvisibility(true);
  else
    g_pController->setQuantitativeInvisibility(false);

  // Papers Textures
  vector<string> sl;
  for (unsigned i = 0; i < paperTexturesList->count(); i++) {
    sl.push_back(paperTexturesList->item(i)->text().toAscii().constData());
  }
  TextureManager::Options::setPaperTextures(sl);

   // Drawing Buffers
  if (frontBufferCheckBox->isChecked())
    g_pController->setFrontBufferFlag(true);
  else
    g_pController->setFrontBufferFlag(false);
  if (backBufferCheckBox->isChecked())
    g_pController->setBackBufferFlag(true);
  else
    g_pController->setBackBufferFlag(false);

  // Ridges and Valleys
  g_pController->setComputeRidgesAndValleysFlag(ridgeValleyCheckBox->isChecked());
  // Suggestive Contours
  g_pController->setComputeSuggestiveContoursFlag(suggestiveContoursCheckBox->isChecked());
  bool ok;
  real r = sphereRadiusLineEdit->text().toFloat(&ok);
  if(ok)
    g_pController->setSphereRadius(r);
  else
    sphereRadiusLineEdit->setText(QString(QString::number(g_pController->getSphereRadius())));
  r = krEpsilonLineEdit->text().toFloat(&ok);
  if(ok)
    g_pController->setSuggestiveContourKrDerivativeEpsilon(r);
  else
    krEpsilonLineEdit->setText(QString(QString::number(g_pController->getSuggestiveContourKrDerivativeEpsilon())));
}

void AppOptionsWindow::Ok() {
  Apply();
  close();
}

void AppOptionsWindow::Apply() {

  // Propagate changes
  Propagate();

  // Update values of the Options DOM Tree accordingly
  _options->setValue("default_path/models/path", modelsPathLineEdit->text());
  _options->setValue("default_path/patterns/path", patternsPathLineEdit->text());
  _options->setValue("default_path/brushes/path", brushesPathLineEdit->text());
  _options->setValue("default_path/python/path", pythonPathLineEdit->text());
  _options->setValue("default_browser/cmd", browserCmdLineEdit->text());
  _options->setValue("default_path/help/index", helpIndexPathLineEdit->text());
  _options->setValue("default_viewmap_format/float_vectors", asFloatCheckBox->isChecked());
  _options->setValue("default_viewmap_format/no_occluders", noOccluderListCheckBox->isChecked());
  _options->setValue("default_visibility/exhaustive_computation", qiCheckBox->isChecked());
  _options->setValue("default_drawing_buffers/front_buffer", frontBufferCheckBox->isChecked());
  _options->setValue("default_drawing_buffers/back_buffer", backBufferCheckBox->isChecked());

  // -> Papers Textures tab
  unsigned papers_nb = paperTexturesList->count();
  _options->setValue("papers/nb", papers_nb);
  for (unsigned i = 0; i < papers_nb; i++) {
    QString path;
    QTextStream(&path) << "papers/texture" << i << "/filename";
    _options->setValue(path, paperTexturesList->item(i)->text());
  }

  // -> Help tab
  _options->setValue("default_browser/cmd", browserCmdLineEdit->text());
  _options->setValue("default_path/help/index", helpIndexPathLineEdit->text());
   
  // -> Misc tab
  _options->setValue("default_viewmap_format/float_vectors", asFloatCheckBox->isChecked());
  _options->setValue("default_viewmap_format/no_occluders", noOccluderListCheckBox->isChecked());
  _options->setValue("default_viewmap_format/compute_steerable", steerableViewMapCheckBox->isChecked());
  _options->setValue("default_visibility/exhaustive_computation", qiCheckBox->isChecked());
  _options->setValue("default_drawing_buffers/back_buffer", backBufferCheckBox->isChecked());
  _options->setValue("default_drawing_buffers/front_buffer", frontBufferCheckBox->isChecked());
  _options->setValue("default_ridges/enable", ridgeValleyCheckBox->isChecked());
  _options->setValue("default_suggestive_contours/enable", suggestiveContoursCheckBox->isChecked());
  bool ok;
  real r = sphereRadiusLineEdit->text().toFloat(&ok);
  if(!ok)
    r = Config::DEFAULT_SPHERE_RADIUS;
  _options->setValue("default_ridges/sphere_radius", r);
  r = krEpsilonLineEdit->text().toFloat(&ok);
  if(!ok)
    r = Config::DEFAULT_DKR_EPSILON;
  _options->setValue("default_suggestive_contours/dkr_epsilon", r);

  // Save options to disk
  _options->saveFile();
}

void AppOptionsWindow::Cancel() {

  // Directories
  QString qstr;
  qstr = ViewMapIO::Options::getModelsPath().c_str();
  modelsPathLineEdit->setText(qstr);
  qstr = PythonInterpreter::Options::getPythonPath().c_str();
  pythonPathLineEdit->setText(qstr);
  qstr = TextureManager::Options::getPatternsPath().c_str();
  patternsPathLineEdit->setText(qstr);
  qstr = TextureManager::Options::getBrushesPath().c_str();
  brushesPathLineEdit->setText(qstr);
  qstr = g_pController->getBrowserCmd();
  browserCmdLineEdit->setText(qstr);
  qstr = g_pController->getHelpIndex();
  helpIndexPathLineEdit->setText(qstr);

  // ViewMap Format
  updateViewMapFormat();
  steerableViewMapCheckBox->setChecked(g_pController->getComputeSteerableViewMapFlag());

  // Visibility
  qiCheckBox->setChecked(g_pController->getQuantitativeInvisibility());

  // Drawing buffers
  frontBufferCheckBox->setChecked(g_pController->getFrontBufferFlag());
  backBufferCheckBox->setChecked(g_pController->getBackBufferFlag());

  // Ridges and Valleys
  ridgeValleyCheckBox->setChecked(g_pController->getComputeRidgesAndValleysFlag());
  // suggestive contours
  suggestiveContoursCheckBox->setChecked(g_pController->getComputeSuggestiveContoursFlag());
  sphereRadiusLineEdit->setText(QString::number(g_pController->getSphereRadius()));
  krEpsilonLineEdit->setText(QString(QString::number(g_pController->getSuggestiveContourKrDerivativeEpsilon())));

  close();
}

void AppOptionsWindow::updateViewMapFormat() {
  asFloatCheckBox->setChecked(ViewMapIO::Options::getFlags() & ViewMapIO::Options::FLOAT_VECTORS);
  noOccluderListCheckBox->setChecked(ViewMapIO::Options::getFlags() & ViewMapIO::Options::NO_OCCLUDERS);
}

void AppOptionsWindow::ModelsAdd() {
  QString s = modelsPathLineEdit->text();
  QString new_s = DirDialog();
  if (new_s.isEmpty())
    return;
  if (!s.isEmpty())
    s += Config::PATH_SEP.c_str();
  s += new_s;
  modelsPathLineEdit->setText(s);
}

void AppOptionsWindow::PatternsAdd() {
  QString s = patternsPathLineEdit->text();
  QString new_s = DirDialog();
  if (new_s.isEmpty())
    return;
  if (!s.isEmpty())
    s += Config::PATH_SEP.c_str();
  s += new_s;
  patternsPathLineEdit->setText(s);
}

void AppOptionsWindow::BrushesAdd() {
  QString s = brushesPathLineEdit->text();
  QString new_s = DirDialog();
  if (new_s.isEmpty())
    return;
  if (!s.isEmpty())
    s += Config::PATH_SEP.c_str();
  s += new_s;
  brushesPathLineEdit->setText(s);
}

void AppOptionsWindow::PythonAdd() {
  QString s = pythonPathLineEdit->text();
  QString new_s = DirDialog();
  if (new_s.isEmpty())
    return;
  if (!s.isEmpty())
    s += Config::PATH_SEP.c_str();
  s += new_s;
  pythonPathLineEdit->setText(s);
}

void AppOptionsWindow::HelpAdd() {
  QString s = QFileDialog::getOpenFileName((QWidget *)this,
	  "Open file dialog"
					   "Choose a file",
	  g_pController->getHelpIndex(),
					   "HTML files (*.html *.htm)");
  if (s.isEmpty())
    return;
  helpIndexPathLineEdit->setText(s);
}

QString AppOptionsWindow::DirDialog() {
  QString s = QFileDialog::getExistingDirectory((QWidget *)this,
						"get existing directory"
						"Choose a directory",
						".",
						QFileDialog::ShowDirsOnly| QFileDialog::DontResolveSymlinks);
  return s;
}

void AppOptionsWindow::PaperAdd() {
  QStringList sl = QFileDialog::getOpenFileNames((QWidget *)this,
	  "open files dialog"
						 "Choose a file",
						 g_pController->getPapersDir(),
	  "Images (*.bmp *.png *.jpg *.xpm)");
  paperTexturesList->insertItems(paperTexturesList->count(), sl);
}

void AppOptionsWindow::PaperRemove() {
	paperTexturesList->takeItem(paperTexturesList->currentRow());
}

void AppOptionsWindow::PaperUp() {
  int current = paperTexturesList->currentRow();
  if (current < 1)
    return;
  QString s = paperTexturesList->currentItem()->text();
  paperTexturesList->item(current)->setText(paperTexturesList->item(current - 1)->text());
  paperTexturesList->item(current - 1)->setText(s);
  paperTexturesList->setCurrentRow(current - 1);
}

void AppOptionsWindow::PaperDown() {
	int current = paperTexturesList->currentRow();
  if (current > paperTexturesList->count() - 2)
    return;
  QString s = paperTexturesList->currentItem()->text();
  paperTexturesList->item(current)->setText(paperTexturesList->item(current + 1)->text());
  paperTexturesList->item(current + 1)->setText(s);
  paperTexturesList->setCurrentRow(current + 1);
}

void AppOptionsWindow::PaperClear() {
  paperTexturesList->clear();
}

