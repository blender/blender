#include "export_File.h"

static yafrayFileRender_t byfile;

yafrayRender_t *YAFBLEND=&byfile;

extern "C" 
{

int YAF_exportScene() { return (int)YAFBLEND->exportScene(); }
//void YAF_displayImage() { YAFBLEND->displayImage(); }
void YAF_addDupliMtx(Object* obj) { YAFBLEND->addDupliMtx(obj); }
int YAF_objectKnownData(Object* obj) { return (int)YAFBLEND->objectKnownData(obj); }

}
