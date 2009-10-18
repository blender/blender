#include "BlenderTextureManager.h"


BlenderTextureManager::BlenderTextureManager ()
: TextureManager()
{
  //_brushes_path = Config::getInstance()...
}

BlenderTextureManager::~BlenderTextureManager ()
{
}

void BlenderTextureManager::loadStandardBrushes()
{
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/charcoalAlpha.bmp", Stroke::HUMID_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/washbrushAlpha.bmp", Stroke::HUMID_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/oil.bmp", Stroke::HUMID_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/oilnoblend.bmp", Stroke::HUMID_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/charcoalAlpha.bmp", Stroke::DRY_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/washbrushAlpha.bmp", Stroke::DRY_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/opaqueDryBrushAlpha.bmp", Stroke::OPAQUE_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/opaqueBrushAlpha.bmp", Stroke::OPAQUE_MEDIUM);
  //_defaultTextureId = getBrushTextureIndex("smoothAlpha.bmp", Stroke::OPAQUE_MEDIUM);
}


unsigned
BlenderTextureManager::loadBrush(string sname, Stroke::MediumType mediumType)
{
//   GLuint texId;
//   glGenTextures(1, &texId);
//   bool found = false;
//   vector<string> pathnames;
//   string path; //soc
//   StringUtils::getPathName(TextureManager::Options::getBrushesPath(),
//   sname,
//   pathnames);
//   for (vector<string>::const_iterator j = pathnames.begin(); j != pathnames.end(); j++) {
//     path = j->c_str();
//     //soc if(QFile::exists(path)){
// 	if( BLI_exists( const_cast<char *>(path.c_str()) ) ) {
//       found = true;
//       break;
//     }
//   }
//   if(!found)
//     return 0;
//   // Brush texture
//   cout << "Loading brush texture..." << endl;
//   switch(mediumType){
//   case Stroke::DRY_MEDIUM:
//     //soc prepareTextureLuminance((const char*)path.toAscii(), texId);
// 	prepareTextureLuminance(StringUtils::toAscii(path), texId);
//     break;
//   case Stroke::HUMID_MEDIUM:
//   case Stroke::OPAQUE_MEDIUM:
//   default:
//     //soc prepareTextureAlpha((const char*)path.toAscii(), texId);
// 	prepareTextureAlpha(StringUtils::toAscii(path), texId);
//     break;
//   }
//   cout << "Done." << endl << endl;
// 
//   return texId;
//
	return 0;
}



