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
		virtual ~yafrayRender_t() {}

		// mtds
		bool exportScene();

		void addDupliMtx(Object* obj);

		bool objectKnownData(Object* obj);

	protected:
		Object* maincam_obj;
		float mainCamLens;

		int maxraydepth;
		bool hasworld;

		std::map<Object*, std::vector<VlakRen*> > all_objects;
		std::map<std::string, Material*> used_materials;
		std::map<std::string, std::pair<Material*, MTex*> > used_textures;
		std::map<std::string, std::vector<float> > dupliMtx_list;
		std::map<std::string, Object*> dup_srcob;
		std::map<void*, Object*> objectData;

		Object* findObject(const char* name);
		bool getAllMatTexObs();

		virtual void writeTextures()=0;
		virtual void writeMaterialsAndModulators()=0;
		virtual void writeObject(Object* obj, const std::vector<VlakRen*> &VLR_list, const float obmat[4][4])=0;
		virtual void writeAllObjects()=0;
		virtual void writeLamps()=0;
		virtual void writeCamera()=0;
		virtual void writeHemilight()=0;
		virtual void writePathlight()=0;
		virtual bool writeWorld()=0;
		virtual bool writeRender()=0;
		virtual bool initExport()=0;
		virtual bool finishExport()=0;
		
		void clearAll();

};

#endif


#endif /*__YAFRAY_RENDER_H */
