#ifndef __YAFRAY_API_H
#define __YAFRAY_API_H

/* C interface for Blender */
#ifdef __cplusplus
extern "C" {
#endif
void YAF_switchPlugin();
void YAF_switchFile();
int YAF_exportScene();
//void YAF_displayImage();
void YAF_addDupliMtx(Object* obj);
int YAF_objectKnownData(Object* obj);
#ifdef __cplusplus
}
#endif

#endif
