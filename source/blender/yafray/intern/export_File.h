#ifndef __EXPORT_FILE_H
#define __EXPORT_FILE_H

#include"yafray_Render.h"

class yafrayFileRender_t : public yafrayRender_t
{
	public:
		virtual ~yafrayFileRender_t() {}
	protected:
		std::string imgout;
		std::ofstream xmlfile;
		std::string xmlpath;
		std::ostringstream ostr;

		void displayImage();
		bool executeYafray(const std::string &xmlpath);
		virtual void writeTextures();
		virtual void writeMaterialsAndModulators();
		virtual void writeObject(Object* obj, 
				const std::vector<VlakRen*> &VLR_list, const float obmat[4][4]);
		virtual void writeAllObjects();
		void writeAreaLamp(LampRen* lamp,int num);
		virtual void writeLamps();
		virtual void writeCamera();
		virtual void writeHemilight();
		virtual void writePathlight();
		virtual bool writeWorld();
		virtual bool writeRender();
		virtual bool initExport();
		virtual bool finishExport();
};

#endif
