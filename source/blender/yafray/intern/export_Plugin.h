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
		yafrayPluginRender_t() {handle=NULL;yafrayGate=NULL;}
		virtual ~yafrayPluginRender_t();
	protected:
		std::string imgout;
		//void *handle;
		PILdynlib *handle;

		yafray::yafrayInterface_t *yafrayGate;

		void displayImage();
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

		void genUVcoords(std::vector<yafray::GFLOAT> &uvcoords,VlakRen *vlr,TFace* uvc);
		void genCompleUVcoords(std::vector<yafray::GFLOAT> &uvcoords,/*VlakRen *vlr,*/TFace* uvc);
		void genVcol(std::vector<yafray::CFLOAT> &vcol,VlakRen *vlr,
								int p1,int p2,int p3,bool EXPORT_VCOL);
		void genFace(std::vector<int> &faces,std::vector<std::string> &shaders,std::vector<int> &faceshader,
				std::vector<yafray::GFLOAT> &uvcoords,std::vector<yafray::CFLOAT> &vcol,
				std::map<VertRen*, int> &vert_idx,VlakRen *vlr,
				bool has_orco,bool has_uv,bool has_vcol);
		void genCompleFace(std::vector<int> &faces,/*std::vector<std::string> &shaders,*/std::vector<int> &faceshader,
				std::vector<yafray::GFLOAT> &uvcoords,std::vector<yafray::CFLOAT> &vcol,
				std::map<VertRen*, int> &vert_idx,VlakRen *vlr,
				bool has_orco,bool has_uv,bool has_vcol);
		void genVertices(std::vector<yafray::point3d_t> &verts,int &vidx,
										 std::map<VertRen*, int> &vert_idx,VlakRen* vlr,bool has_orco);
};

class blenderYafrayOutput_t : public yafray::colorOutput_t
{
	public:
		blenderYafrayOutput_t() {out=0;};
		virtual ~blenderYafrayOutput_t() {};
		virtual bool putPixel(int x, int y,const yafray::color_t &c, 
				yafray::CFLOAT alpha=0,yafray::PFLOAT depth=0);
		virtual void flush() {};
	protected:
		int out;
};

#endif
