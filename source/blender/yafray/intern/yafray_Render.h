#ifndef __YAFRAY_RENDER_H
#define __YAFRAY_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "MEM_guardedalloc.h"
#include "IMB_imbuf_types.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_ika_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_radio_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "render.h"

/* useful matrix & vector operations */
#include "MTC_matrixops.h"
#include "MTC_vectorops.h"

#include "BLI_blenlib.h"

/* need error(), so extern declare here */
extern void error (char *fmt, ...);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <map>
#include <string>
#include <vector>

class yafrayRender_t
{
public:
	// ctor
	yafrayRender_t() {}
	// dtor
	~yafrayRender_t() {}

	// mtds
	bool exportScene();

	void displayImage();
	void addDupliMtx(Object* obj);

	bool objectKnownData(Object* obj);

private:
	Object* maincam_obj;
	float mainCamLens;

	int maxraydepth;

	std::string imgout;

	std::ofstream xmlfile;
	std::ostringstream ostr;
	std::map<Object*, std::vector<VlakRen*> > all_objects;
	std::map<std::string, Material*> used_materials;
	std::map<std::string, std::pair<Material*, MTex*> > used_textures;
	std::map<std::string, std::vector<float> > dupliMtx_list;
	std::map<std::string, Object*> dup_srcob;
	std::map<void*, Object*> objectData;

	Object* findObject(const char* name);
	bool getAllMatTexObs();
	void writeTextures();
	void writeMaterialsAndModulators();
	void writeObject(Object* obj, const std::vector<VlakRen*> &VLR_list, const float obmat[4][4]);
	void writeAllObjects();
	void writeLamps();
	void writeCamera();
	void writeHemilight();
	void writePathlight();
	bool writeWorld();
	void clearAll();

};

/* C access to yafray */
extern yafrayRender_t YAFBLEND;
#endif


#endif /*__YAFRAY_RENDER_H */
