#ifndef __EXPORT_PLUGIN_H
#define __EXPORT_PLUGIN_H

#include"yafray_Render.h"
#include"yafexternal.h"
extern "C"
{
#include"PIL_dynlib.h"
}

class yafrayPluginRender_t : public yafrayRender_t
{
	public:
		yafrayPluginRender_t() 
		{
			plugin_loaded = false;
			handle=NULL;
#ifdef WIN32
			corehandle=NULL;
#endif
			yafrayGate=NULL;
		}
		virtual ~yafrayPluginRender_t();
	protected:
		bool plugin_loaded;
		std::string imgout;
		PILdynlib *handle;
#ifdef WIN32
		PILdynlib *corehandle;
#endif

		yafray::yafrayInterface_t *yafrayGate;

		void displayImage();
		virtual void writeTextures();
		virtual void writeShader(const std::string &shader_name, Material* matr, const std::string &facetexname="");
		virtual void writeMaterialsAndModulators();
		virtual void writeObject(Object* obj, ObjectRen *obr,
				const std::vector<VlakRen*> &VLR_list, const float obmat[4][4]);
		virtual void writeAllObjects();
		virtual void writeAreaLamp(LampRen* lamp, int num, float iview[4][4]);
		virtual void writeLamps();
		virtual void writeCamera();
		virtual void writeHemilight();
		virtual void writePathlight();
		virtual bool writeWorld();
		virtual bool writeRender();
		virtual bool initExport();
		virtual bool finishExport();

		void genUVcoords(std::vector<yafray::GFLOAT> &uvcoords,ObjectRen *obr,VlakRen *vlr,MTFace* uvc, bool comple=false);
		void genVcol(std::vector<yafray::CFLOAT> &vcol, ObjectRen *obr, VlakRen *vlr, bool comple=false);
		void genFace(std::vector<int> &faces,std::vector<std::string> &shaders,std::vector<int> &faceshader,
				std::vector<yafray::GFLOAT> &uvcoords,std::vector<yafray::CFLOAT> &vcol,
				std::map<VertRen*, int> &vert_idx,ObjectRen *obr,VlakRen *vlr,
				int has_orco,bool has_uv);
		void genCompleFace(std::vector<int> &faces,/*std::vector<std::string> &shaders,*/std::vector<int> &faceshader,
				std::vector<yafray::GFLOAT> &uvcoords,std::vector<yafray::CFLOAT> &vcol,
				std::map<VertRen*, int> &vert_idx,ObjectRen *obr, VlakRen *vlr,
				int has_orco,bool has_uv);
		void genVertices(std::vector<yafray::point3d_t> &verts, int &vidx,
										 std::map<VertRen*, int> &vert_idx, ObjectRen *obr, VlakRen* vlr, int has_orco, Object* obj);
};

class blenderYafrayOutput_t : public yafray::colorOutput_t
{
	public:
		blenderYafrayOutput_t(Render* re):out(0) { this->re = re; }
		virtual ~blenderYafrayOutput_t() {}
		virtual bool putPixel(int x, int y, const yafray::color_t &c,
				yafray::CFLOAT alpha=0, yafray::PFLOAT depth=0);
		virtual void flush() {}
	protected:
		Render* re;
		int out;
};

#endif
