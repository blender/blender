#ifndef __YAFINTERFACE_H
#define __YAFINTERFACE_H

#include<vector>
#include<string>
#include<list>
#include<map>

namespace yafray
{

typedef float PFLOAT;
typedef float GFLOAT;
typedef float CFLOAT;

class point3d_t
{
	public:
		point3d_t() { x = y = z = 0; }
		point3d_t(PFLOAT ix, PFLOAT iy, PFLOAT iz=0) { x=ix;  y=iy;  z=iz; }
		point3d_t(const point3d_t &s) { x=s.x;  y=s.y;  z=s.z; }
		void set(PFLOAT ix, PFLOAT iy, PFLOAT iz=0) { x=ix;  y=iy;  z=iz; }
		point3d_t& operator= (const point3d_t &s) { x=s.x;  y=s.y;  z=s.z;  return *this; }
		~point3d_t() {};
		PFLOAT x,y,z;
};

class color_t
{
	public:
		color_t() {R=G=B=0;};
		color_t(CFLOAT r,CFLOAT g,CFLOAT b) {R=r;G=g;B=b;};
		~color_t() {};
		void set(CFLOAT r,CFLOAT g,CFLOAT b) {R=r;G=g;B=b;};

		CFLOAT R,G,B;
};

class colorA_t : public color_t
{
	public:
		colorA_t() { A=1; }
		colorA_t(const color_t &c):color_t(c) { A=1; }
		colorA_t(CFLOAT r, CFLOAT g, CFLOAT b, CFLOAT a=0):color_t(r,g,b) {A=a;}
		~colorA_t() {};
		void set(CFLOAT r, CFLOAT g, CFLOAT b, CFLOAT a=0) {color_t::set(r,g,b);A=a; };

	protected:
		CFLOAT A;
};

#define TYPE_FLOAT  0
#define TYPE_STRING 1
#define TYPE_POINT  2
#define TYPE_COLOR  3
#define TYPE_NONE   -1

class parameter_t
{
	public:
		parameter_t(const std::string &s);
		parameter_t(float f);
		parameter_t(const colorA_t &c);
		parameter_t(const point3d_t &p);
		parameter_t();
		~parameter_t();

		const std::string 		&getStr() {used=true;return str;};
		float 					&getFnum() {used=true;return fnum;};
		const point3d_t &getP() {used=true;return P;};
		const color_t 	&getC() {used=true;return C;};
		const colorA_t 	&getAC() {used=true;return C;};
		int type;
		bool used;
	protected:
		std::string str;
		float fnum;
		point3d_t P;
		colorA_t C;
};

class paramMap_t
{
	public:
		parameter_t & operator [] (const std::string &key);
		void clear();
		~paramMap_t() {};
	protected:
		std::map<std::string,parameter_t> dicc;
};

class light_t;
class shader_t;
class texture_t;
class filter_t;
class background_t;

class renderEnvironment_t
{
	public:
		typedef light_t * light_factory_t(paramMap_t &,renderEnvironment_t &);
		typedef shader_t *shader_factory_t(paramMap_t &,std::list<paramMap_t> &,
				renderEnvironment_t &);
		typedef texture_t *texture_factory_t(paramMap_t &,renderEnvironment_t &);
		typedef filter_t *filter_factory_t(paramMap_t &,renderEnvironment_t &);
		typedef background_t *background_factory_t(paramMap_t &,renderEnvironment_t &);
		
		virtual shader_t *getShader(const std::string name)const=0;
		virtual texture_t *getTexture(const std::string name)const=0;

		virtual void repeatFirstPass()=0;

		virtual void registerFactory(const std::string &name,light_factory_t *f)=0;
		virtual void registerFactory(const std::string &name,shader_factory_t *f)=0;
		virtual void registerFactory(const std::string &name,texture_factory_t *f)=0;
		virtual void registerFactory(const std::string &name,filter_factory_t *f)=0;
		virtual void registerFactory(const std::string &name,background_factory_t *f)=0;

		renderEnvironment_t() {};
		virtual ~renderEnvironment_t() {};

};

class colorOutput_t
{
	public:
		virtual ~colorOutput_t() {};
		virtual bool putPixel(int x, int y,const color_t &c, 
				CFLOAT alpha=0,PFLOAT depth=0)=0;
		virtual void flush()=0;
};

class yafrayInterface_t : public renderEnvironment_t
{
	public:
		virtual void transformPush(float *m)=0;
		virtual void transformPop()=0;
		virtual void addObject_trimesh(const std::string &name,
				std::vector<point3d_t> &verts, const std::vector<int> &faces,
				std::vector<GFLOAT> &uvcoords, std::vector<CFLOAT> &vcol,
				const std::vector<std::string> &shaders,const std::vector<int> &faceshader,
				float sm_angle,bool castShadows,bool useR,bool receiveR,bool caus,bool has_orco,
				const color_t &caus_rcolor,const color_t &caus_tcolor,float caus_IOR)=0;

		virtual void addObject_reference(const std::string &name,const std::string &original)=0;
		// lights
		virtual void addLight(paramMap_t &p)=0;
    // textures
		virtual void addTexture(paramMap_t &p)=0;
		// shaders
		virtual void addShader(paramMap_t &p,std::list<paramMap_t> &modulators)=0;
		// filters
		virtual void addFilter(paramMap_t &p)=0;
		// backgrounds
		virtual void addBackground(paramMap_t &p)=0;
		//camera
    virtual void addCamera(paramMap_t &p)=0;
		//render
		virtual void render(paramMap_t &p)=0;
		//render
		virtual void render(paramMap_t &p,colorOutput_t &output)=0;
		
		virtual void clear()=0;

		virtual ~yafrayInterface_t() {};
};

typedef yafrayInterface_t * yafrayConstructor(int,const std::string &);

}

#define YAFRAY_SYMBOL "getYafray"

#endif
