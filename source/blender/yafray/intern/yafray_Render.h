#ifndef __YAFRAY_RENDER_H
#define __YAFRAY_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "MEM_guardedalloc.h"
#include "IMB_imbuf_types.h"

#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_world_types.h"

#include "BKE_global.h"

#include "render_types.h"
#include "renderdatabase.h"
/* display_draw() needs render layer info */
#include "renderpipeline.h"

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
#include <set>

class yafrayRender_t
{
	public:
		// ctor
		yafrayRender_t() {}
		// dtor
		virtual ~yafrayRender_t() {}

		// mtds
		bool exportScene(Render* re);
		void addDupliMtx(Object* obj, float mat[][4]);
		bool objectKnownData(Object* obj);

	protected:
		Render* re;
		Object* maincam_obj;
		float mainCamLens;

		bool hasworld;

		std::map<Object*, std::vector<VlakRen*> > all_objects;
		std::map<std::string, Material*> used_materials;
		std::map<std::string, MTex*> used_textures;
		std::map<std::string, std::vector<float> > dupliMtx_list;
		std::map<std::string, Object*> dup_srcob;
		std::map<void*, Object*> objectData;
		std::map<Image*, std::set<Material*> > imagetex;
		std::map<std::string, std::string> imgtex_shader;

		bool getAllMatTexObs();

		virtual void writeTextures()=0;
		virtual void writeShader(const std::string &shader_name, Material* matr, const std::string &facetexname)=0;
		virtual void writeMaterialsAndModulators()=0;
		virtual void writeObject(Object* obj, const std::vector<VlakRen*> &VLR_list, const float obmat[4][4])=0;
		virtual void writeAllObjects()=0;
		virtual void writeLamps()=0;
		virtual void writeCamera()=0;
		virtual void writeAreaLamp(LampRen* lamp, int num, float iview[4][4])=0;
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
