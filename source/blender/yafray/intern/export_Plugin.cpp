#include"export_Plugin.h"

#include <math.h>

using namespace std;


#ifdef WIN32 

#include<windows.h>

#ifndef FILE_MAXDIR
#define FILE_MAXDIR  160
#endif

#ifndef FILE_MAXFILE
#define FILE_MAXFILE 80
#endif


static string find_path()
{
	HKEY	hkey;
	DWORD dwType, dwSize;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,"Software\\YafRay Team\\YafRay",0,KEY_READ,&hkey)==ERROR_SUCCESS)
	{
		dwType = REG_EXPAND_SZ;
	 	dwSize = MAX_PATH;
		DWORD dwStat;

		char *pInstallDir=new char[MAX_PATH];

  		dwStat=RegQueryValueEx(hkey, TEXT("InstallDir"), 
			NULL, NULL,(LPBYTE)pInstallDir, &dwSize);
		
		if (dwStat == NO_ERROR)
		{
			string res=pInstallDir;
			delete [] pInstallDir;
			return res;
		}
		else
			cout << "Couldn't READ \'InstallDir\' value. Is yafray correctly installed?\n";
		delete [] pInstallDir;

		RegCloseKey(hkey);
	}	
	else
		cout << "Couldn't FIND registry key for yafray, is it installed?\n";

	return string("");

}

static int createDir(char* name)
{
	if (BLI_exists(name))
		return 2;	//exists
	if (CreateDirectory((LPCTSTR)(name), NULL)) {
		cout << "Directory: " << name << " created\n";
		return 1;	// created
	}
	else	{
		cout << "Could not create directory: " << name << endl;
		return 0;	// fail
	}
}

extern "C" { extern char bprogname[]; }

// add drive character if not in path string, using blender executable location as reference
static void addDrive(string &path)
{
	int sp = path.find_first_of(":");
	if (sp==-1) {
		string blpath = bprogname;
		sp = blpath.find_first_of(":");
		if (sp!=-1) path = blpath.substr(0, sp+1) + path;
	}
}

#else

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#endif

static string YafrayPath()
{
#ifdef WIN32
	string path=find_path();
	return path+"\\libyafrayplugin.dll";
#else
	static char *alternative[]=
	{
		"/usr/local/lib/",
		"/usr/lib/",
		NULL
	};

	for(int i=0;alternative[i]!=NULL;++i)
	{
		string fp=string(alternative[i])+"libyafrayplugin.so";
		struct stat st;
		if(stat(fp.c_str(),&st)<0) continue;
		if(st.st_mode&S_IXOTH) return fp;
	}
	return "";
#endif
}

static string YafrayPluginPath()
{
#ifdef WIN32
	return find_path();
#else
	static char *alternative[]=
	{
		"/usr/local/lib/yafray",
		"/usr/lib/yafray",
		NULL
	};

	for(int i=0;alternative[i]!=NULL;++i)
	{
		struct stat st;
		if(stat(alternative[i],&st)<0) continue;
		if(S_ISDIR(st.st_mode) && (st.st_mode&S_IXOTH)) return alternative[i];
	}
	return "";
#endif
}



yafrayPluginRender_t::~yafrayPluginRender_t()
{
	if(yafrayGate!=NULL) delete yafrayGate;
	if(handle!=NULL) PIL_dynlib_close(handle);
}

bool yafrayPluginRender_t::initExport()
{
	imgout="YBPtest.tga";
	if(handle==NULL)
	{
		string location=YafrayPath();
		//handle=dlopen(location.c_str(),RTLD_NOW);
		handle=PIL_dynlib_open((char *)location.c_str());
		 if(handle==NULL)
		{
			//cerr<<"Error loading yafray plugin: "<<dlerror()<<endl;
			cerr<<"Error loading yafray plugin: "<<PIL_dynlib_get_error_as_string(handle)<<endl;
			return false;
		}
	}
	yafray::yafrayConstructor *constructor;
	//constructor=(yafray::yafrayConstructor *)dlsym(handle,YAFRAY_SYMBOL);
	constructor=(yafray::yafrayConstructor *)PIL_dynlib_find_symbol(handle,YAFRAY_SYMBOL);
	if(constructor==NULL)
	{
		cerr<<"Error loading yafray plugin: "<<PIL_dynlib_get_error_as_string(handle)<<endl;
		return false;
	}
	if(yafrayGate!=NULL) delete yafrayGate;
	yafrayGate=constructor(1,YafrayPluginPath());
	
	cout<<"YafRay plugin loaded"<<endl;
	
	if(R.rectot == NULL)
		R.rectot = (unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");

	for (unsigned short y=0;y<R.recty;y++) {
		unsigned char* bpt = (unsigned char*)R.rectot + ((((R.recty-1)-y)*R.rectx)<<2);
		for (unsigned short x=0;x<R.rectx;x++) {
			bpt[2] = 128;
			bpt[1] = 0;
			bpt[0] = 0;
			bpt[3] = 255;
			bpt += 4;
		}
	}
	
	cout<<"Image allocated"<<endl;
	return true;
}

bool yafrayPluginRender_t::writeRender()
{
	yafray::paramMap_t params;
	params["camera_name"]=yafray::parameter_t("MAINCAM");
	params["raydepth"]=yafray::parameter_t(R.r.YF_raydepth);
	params["gamma"]=yafray::parameter_t(R.r.YF_gamma);
	params["exposure"]=yafray::parameter_t(R.r.YF_exposure);
	if(R.r.YF_AA)
	{
		params["AA_passes"]=yafray::parameter_t(R.r.YF_AApasses);
		params["AA_minsamples"]=yafray::parameter_t(R.r.YF_AAsamples);
	}
	else
	{
		if ((R.r.GImethod!=0) && (R.r.GIquality>1) && (!R.r.GIcache))
		{
			params["AA_passes"]=yafray::parameter_t(5);
			params["AA_minsamples"]=yafray::parameter_t(5);
		}
		else if ((R.r.mode & R_OSA) && (R.r.osa)) 
		{
			params["AA_passes"]=yafray::parameter_t((R.r.osa%4)==0 ? R.r.osa/4 : 1);
			params["AA_minsamples"]=yafray::parameter_t((R.r.osa%4)==0 ? 4 : R.r.osa);
		}
		else 
		{
			params["AA_passes"]=yafray::parameter_t(0);
			params["AA_minsamples"]=yafray::parameter_t(1);
		}
	}
	if (hasworld) params["background_name"]=yafray::parameter_t("world_background");
	params["AA_pixelwidth"]=yafray::parameter_t(1.5);
	params["AA_threshold"]=yafray::parameter_t(0.05);
	params["bias"]=yafray::parameter_t(R.r.YF_raybias);
	//params["outfile"]=yafray::parameter_t(imgout);
	blenderYafrayOutput_t output;
	yafrayGate->render(params,output);
	cout<<"render finished"<<endl;
	yafrayGate->clear();
	return true;
}

bool yafrayPluginRender_t::finishExport()
{
	//displayImage();
	return true;
}

// displays the image rendered with xml export
// Now loads rendered image into blender renderbuf.
void yafrayPluginRender_t::displayImage()
{
	// although it is possible to load the image using blender,
	// maybe it is best to just do a read here, for now the yafray output is always a raw tga anyway

	// rectot already freed in initrender
	R.rectot = (unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");

	FILE* fp = fopen(imgout.c_str(), "rb");
	if (fp==NULL) {
		cout << "YAF_displayImage(): Could not open image file\n";
		return;
	}

	unsigned char header[18];
	fread(&header, 1, 18, fp);
	unsigned short width = (unsigned short)(header[12] + (header[13]<<8));
	unsigned short height = (unsigned short)(header[14] + (header[15]<<8));
	unsigned char byte_per_pix = (unsigned char)(header[16]>>3);
	// read past any id (none in this case though)
	unsigned int idlen = (unsigned int)header[0];
	if (idlen) fseek(fp, idlen, SEEK_CUR);

	// read data directly into buffer, picture is upside down
	for (unsigned short y=0;y<height;y++) {
		unsigned char* bpt = (unsigned char*)R.rectot + ((((height-1)-y)*width)<<2);
		for (unsigned short x=0;x<width;x++) {
			bpt[2] = (unsigned char)fgetc(fp);
			bpt[1] = (unsigned char)fgetc(fp);
			bpt[0] = (unsigned char)fgetc(fp);
			if (byte_per_pix==4)
				bpt[3] = (unsigned char)fgetc(fp);
			else
				bpt[3] = 255;
			bpt += 4;
		}
	}

	fclose(fp);
	fp = NULL;
}


void yafrayPluginRender_t::writeTextures()
{
	for (map<string, pair<Material*, MTex*> >::const_iterator blendtex=used_textures.begin();
						blendtex!=used_textures.end();++blendtex) 
	{
		yafray::paramMap_t params;
		list<yafray::paramMap_t> lparams;
		MTex* mtex = blendtex->second.second;
		Tex* tex = mtex->tex;
		params["name"]=yafray::parameter_t(blendtex->first);
		switch (tex->type) {
			case TEX_STUCCI:
				// stucci is clouds as bump, but could be added to yafray to handle both wall in/out as well.
				// noisedepth must be at least 1 in yafray
			case TEX_CLOUDS: 
				params["type"]=yafray::parameter_t("clouds");
				params["depth"]=yafray::parameter_t(tex->noisedepth+1);
				break;
			case TEX_WOOD:
			{
				params["type"]=yafray::parameter_t("wood");
				params["depth"]=yafray::parameter_t(tex->noisedepth+1);
				params["turbulence"]=yafray::parameter_t(tex->turbul);
				params["ringscale_x"]=yafray::parameter_t(mtex->size[0]);
				params["ringscale_y"]=yafray::parameter_t(mtex->size[1]);
				string ts = "on";
				if (tex->noisetype==TEX_NOISESOFT) ts = "off";
				params["hard"]=yafray::parameter_t(ts);
				break;
			}
			case TEX_MARBLE: 
			{
				params["type"]=yafray::parameter_t("marble");
				params["depth"]=yafray::parameter_t(tex->noisedepth+1);
				params["turbulence"]=yafray::parameter_t(tex->turbul);
				string ts = "on";
				if (tex->noisetype==TEX_NOISESOFT) ts = "off";
				params["hard"]=yafray::parameter_t(ts);
				if (tex->stype==1)
					params["sharpness"]=yafray::parameter_t(5);
				else if (tex->stype==2)
					params["sharpness"]=yafray::parameter_t(10);
				else
					params["sharpness"]=yafray::parameter_t(1);
				break;
			}
			case TEX_IMAGE: 
			{
				Image* ima = tex->ima;
				if (ima) {
					params["type"]=yafray::parameter_t("image");
					// image->name is full path
					string texpath = ima->name;
#ifdef WIN32
					// add drive char if not there
					addDrive(texpath);
#endif
					params["filename"]=yafray::parameter_t(texpath);
				}
				break;
			}
			default:
				cout << "Unsupported texture type\n";
		}
		yafrayGate->addShader(params,lparams);
		// colorbands
		if (tex->flag & TEX_COLORBAND) 
		{
			ColorBand* cb = tex->coba;
			if (cb) 
			{
				params.clear();
				params["type"]=yafray::parameter_t("colorband");
				params["name"]=yafray::parameter_t(blendtex->first + "_coba");
				params["input"]=yafray::parameter_t(blendtex->first);
				for (int i=0;i<cb->tot;i++) 
				{
					yafray::paramMap_t mparams;
					mparams["value"]=yafray::parameter_t(cb->data[i].pos);
					mparams["color"]=yafray::parameter_t(yafray::colorA_t(cb->data[i].r,
																																cb->data[i].g,
																																cb->data[i].b,
																																cb->data[i].a));
					lparams.push_back(mparams);
				}
				yafrayGate->addShader(params,lparams);
			}
		}

	}
}


// write all materials & modulators
void yafrayPluginRender_t::writeMaterialsAndModulators()
{
	  
	for (map<string, Material*>::const_iterator blendmat=used_materials.begin();
		blendmat!=used_materials.end();++blendmat) 
	{
		Material* matr = blendmat->second;
		// blendermappers
		for (int m=0;m<8;m++) 
		{
			if (matr->septex & (1<<m)) continue;// all active channels
			// ignore null mtex
			MTex* mtex = matr->mtex[m];
			if (mtex==NULL) continue;
			// ignore null tex
			Tex* tex = mtex->tex;
			if (tex==NULL) continue;

			// now included the full name
			map<string, pair<Material*, MTex*> >::const_iterator mtexL = used_textures.find(string(tex->id.name));
			if (mtexL!=used_textures.end()) 
			{
				yafray::paramMap_t params;
				list<yafray::paramMap_t> lparams;
				//params.clear();
				//lparams.clear();
				char temp[16];
				sprintf(temp,"%d",m);
				params["type"]=yafray::parameter_t("blendermapper");
				params["name"]=yafray::parameter_t(blendmat->first + "_map"+temp);
				if ((mtex->texco & TEXCO_OBJECT) || (mtex->texco & TEXCO_REFL))
				{
					// For object & reflection mapping, add the object matrix to the modulator,
					// as in LF script, use camera matrix if no object specified.
					// In this case this means the inverse of that matrix
					float texmat[4][4], itexmat[4][4];
					if ((mtex->texco & TEXCO_OBJECT) && (mtex->object))
						MTC_Mat4CpyMat4(texmat, mtex->object->obmat);
					else	// also for refl. map
						MTC_Mat4CpyMat4(texmat, maincam_obj->obmat);
					MTC_Mat4Invert(itexmat, texmat);
#define flp yafray::parameter_t
					params["m00"]=flp(itexmat[0][0]);params["m01"]=flp(itexmat[1][0]);
					params["m02"]=flp(itexmat[2][0]);params["m03"]=flp(itexmat[3][0]);
					params["m10"]=flp(itexmat[0][1]);params["m11"]=flp(itexmat[1][1]);
					params["m12"]=flp(itexmat[2][1]);params["m13"]=flp(itexmat[3][1]);
					params["m20"]=flp(itexmat[0][2]);params["m21"]=flp(itexmat[1][2]);
					params["m22"]=flp(itexmat[2][2]);params["m23"]=flp(itexmat[3][2]);
					params["m30"]=flp(itexmat[0][3]);params["m31"]=flp(itexmat[1][3]);
					params["m32"]=flp(itexmat[2][3]);params["m33"]=flp(itexmat[3][3]);
#undef flp
				}
				if ((tex->flag & TEX_COLORBAND) & (tex->coba!=NULL))
					params["input"]=yafray::parameter_t(mtexL->first + "_coba");
				else
					params["input"]=yafray::parameter_t(mtexL->first);

				// size, if the texturetype is clouds/marble/wood, also take noisesize into account
				float sc = 1;
				if ((tex->type==TEX_CLOUDS) || (tex->type==TEX_MARBLE) || (tex->type==TEX_WOOD)) 
				{
					sc = tex->noisesize;
					if (sc!=0) sc = 1.f/sc;
				}
				// texture size
				params["sizex"]=yafray::parameter_t(mtex->size[0]*sc);
				params["sizey"]=yafray::parameter_t(mtex->size[1]*sc);
				params["sizez"]=yafray::parameter_t(mtex->size[2]*sc);

				// texture offset
				params["ofsx"]=yafray::parameter_t(mtex->ofs[0]*sc);
				params["ofsy"]=yafray::parameter_t(mtex->ofs[1]*sc);
				params["ofsz"]=yafray::parameter_t(mtex->ofs[2]*sc);

				// texture coordinates, have to disable 'sticky' in Blender
				if ((mtex->texco & TEXCO_UV) || (matr->mode & MA_FACETEXTURE))
					params["texco"]=yafray::parameter_t("uv");
				else if ((mtex->texco & TEXCO_GLOB) || (mtex->texco & TEXCO_OBJECT))
					// object mode is also set as global, but the object matrix 
					// was specified above with <modulator..>
					params["texco"]=yafray::parameter_t("global");
				else if (mtex->texco & TEXCO_ORCO)
					params["texco"]=yafray::parameter_t("orco");
				else if (mtex->texco & TEXCO_WINDOW)
					params["texco"]=yafray::parameter_t("window");
				else if (mtex->texco & TEXCO_NORM)
					params["texco"]=yafray::parameter_t("normal");
				else if (mtex->texco & TEXCO_REFL)
					params["texco"]=yafray::parameter_t("reflect");

				// texture mapping parameters only relevant to image type
				if (tex->type==TEX_IMAGE) 
				{
					if (mtex->mapping==MTEX_FLAT)
						params["mapping"]=yafray::parameter_t("flat");
					else if (mtex->mapping==MTEX_CUBE)
						params["mapping"]=yafray::parameter_t("cube");
					else if (mtex->mapping==MTEX_TUBE)
						params["mapping"]=yafray::parameter_t("tube");
					else if (mtex->mapping==MTEX_SPHERE)
						params["mapping"]=yafray::parameter_t("sphere");

					// texture projection axes
					string proj = "nxyz";		// 'n' for 'none'
					params["proj_x"]=yafray::parameter_t(string(1,proj[mtex->projx]));
					params["proj_y"]=yafray::parameter_t(string(1,proj[mtex->projy]));
					params["proj_z"]=yafray::parameter_t(string(1,proj[mtex->projz]));

					// repeat
					params["xrepeat"]=yafray::parameter_t(tex->xrepeat);
					params["yrepeat"]=yafray::parameter_t(tex->yrepeat);

					// clipping
					if (tex->extend==TEX_EXTEND)
						params["clipping"]=yafray::parameter_t("extend");
					else if (tex->extend==TEX_CLIP)
						params["clipping"]=yafray::parameter_t("clip");
					else if (tex->extend==TEX_CLIPCUBE)
						params["clipping"]=yafray::parameter_t("clipcube");
					else
						params["clipping"]=yafray::parameter_t("repeat");

					// crop min/max
					params["cropmin_x"]=yafray::parameter_t(tex->cropxmin);
					params["cropmin_y"]=yafray::parameter_t(tex->cropymin);
					params["cropmax_x"]=yafray::parameter_t(tex->cropxmax);
					params["cropmax_y"]=yafray::parameter_t(tex->cropymax);

					// rot90 flag
					if (tex->imaflag & TEX_IMAROT) 
						params["rot90"]=yafray::parameter_t("on");
					else
						params["rot90"]=yafray::parameter_t("off");
				}
				yafrayGate->addShader(params,lparams);
			}
		}
		yafray::paramMap_t params;
		// blendershaders + modulators
		params["type"]=yafray::parameter_t("blendershader");
		params["name"]=yafray::parameter_t(blendmat->first);
		float diff=matr->alpha;
		params["color"]=yafray::parameter_t(yafray::color_t(matr->r*diff,matr->g*diff,matr->b*diff));
		params["specular_color"]=yafray::parameter_t(yafray::color_t(matr->specr,
																																 matr->specg,
																																 matr->specb));
		params["mirror_color"]=yafray::parameter_t(yafray::color_t(matr->mirr, matr->mirg,matr->mirb));
		params["diffuse_reflect"]=yafray::parameter_t(matr->ref);
		params["specular_amount"]=yafray::parameter_t(matr->spec);
		params["hard"]=yafray::parameter_t(matr->har);
		params["alpha"]=yafray::parameter_t(matr->alpha);
		params["emit"]=yafray::parameter_t(matr->emit * R.r.GIpower);

		// reflection/refraction
		if ( (matr->mode & MA_RAYMIRROR) || (matr->mode & MA_RAYTRANSP) )
			params["IOR"]=yafray::parameter_t(matr->ang);
		if (matr->mode & MA_RAYMIRROR) 
		{
			float rf = matr->ray_mirror;
			// blender uses mir color for reflection as well
			params["reflected"]=yafray::parameter_t(yafray::color_t(matr->mirr, matr->mirg,matr->mirb));
			params["min_refle"]=yafray::parameter_t(rf);
			if (matr->ray_depth>maxraydepth) maxraydepth = matr->ray_depth;
		}
		if (matr->mode & MA_RAYTRANSP) 
		{
			float tr=1.0-matr->alpha;
			params["transmitted"]=yafray::parameter_t(yafray::color_t(matr->r*tr,matr->g*tr,matr->b*tr));
			// tir on by default
			params["tir"]=yafray::parameter_t("on");
			if (matr->ray_depth_tra>maxraydepth) maxraydepth = matr->ray_depth_tra;
		}

		string Mmode = "";
		if (matr->mode & MA_TRACEBLE) Mmode += "traceable";
		if (matr->mode & MA_SHADOW) Mmode += " shadow";
		if (matr->mode & MA_SHLESS) Mmode += " shadeless";
		if (matr->mode & MA_VERTEXCOL) Mmode += " vcol_light";
		if (matr->mode & MA_VERTEXCOLP) Mmode += " vcol_paint";
		if (matr->mode & MA_ZTRA) Mmode += " ztransp";
		if (matr->mode & MA_ONLYSHADOW) Mmode += " onlyshadow";
		if (Mmode!="") params["matmodes"]=yafray::parameter_t(Mmode);

		// modulators
		list<yafray::paramMap_t> lparams;
		for (int m2=0;m2<8;m2++) 
		{
			if (matr->septex & (1<<m2)) continue;// all active channels
			// ignore null mtex
			MTex* mtex = matr->mtex[m2];
			if (mtex==NULL) continue;
			// ignore null tex
			Tex* tex = mtex->tex;
			if (tex==NULL) continue;

			map<string, pair<Material*, MTex*> >::const_iterator mtexL = used_textures.find(string(tex->id.name));
			if (mtexL!=used_textures.end()) 
			{
				yafray::paramMap_t mparams;
				char temp[16];
				sprintf(temp,"%d",m2);
				mparams["input"]=yafray::parameter_t(blendmat->first + "_map" + temp);
				// blendtype
				string ts = "mix";
				if (mtex->blendtype==MTEX_MUL) ts="mul";
				else if (mtex->blendtype==MTEX_ADD) ts="add";
				else if (mtex->blendtype==MTEX_SUB) ts="sub";
				mparams["mode"]=yafray::parameter_t(ts);

				// texture color (for use with MUL and/or no_rgb etc..)
				mparams["texcol"]=yafray::parameter_t(yafray::color_t(mtex->r,mtex->g,mtex->b));
				// texture contrast, brightness & color adjustment
				mparams["filtercolor"]=yafray::parameter_t(yafray::color_t(tex->rfac,tex->gfac,tex->bfac));
				mparams["contrast"]=yafray::parameter_t(tex->contrast);
				mparams["brightness"]=yafray::parameter_t(tex->bright);
				// all texture flags now are switches, having the value 1 or -1 (negative option)
				// the negative option only used for the intensity modulation options.

				// material (diffuse) color, amount controlled by colfac (see below)
				if (mtex->mapto & MAP_COL)
					mparams["color"]=yafray::parameter_t(1.0);
				// bumpmapping
				if ((mtex->mapto & MAP_NORM) || (mtex->maptoneg & MAP_NORM)) 
				{
					// for yafray, bump factor is negated (unless negative option of 'Nor', 
					// is not affected by 'Neg')
					// scaled down quite a bit for yafray when image type, otherwise used directly
					float nf = -mtex->norfac;
					if (mtex->maptoneg & MAP_NORM) nf *= -1.f;
					if (tex->type==TEX_IMAGE) nf *= 2e-3f;
					mparams["normal"]=yafray::parameter_t(nf);
				}

				// all blender texture modulation as switches, either 1 or -1 (negative state of button)
				// Csp, specular color modulation
				if (mtex->mapto & MAP_COLSPEC)
					mparams["colspec"]=yafray::parameter_t(1.0);
				// CMir, mirror color  modulation
				if (mtex->mapto & MAP_COLMIR)
					mparams["colmir"]=yafray::parameter_t(1.0);

				// Ref, diffuse reflection amount  modulation
				if ((mtex->mapto & MAP_REF) || (mtex->maptoneg & MAP_REF)) 
				{
					int t = 1;
					if (mtex->maptoneg & MAP_REF) t = -1;
					mparams["difref"]=yafray::parameter_t(t);
				}

				// Spec, specular amount mod
				if ((mtex->mapto & MAP_SPEC) || (mtex->maptoneg & MAP_SPEC)) 
				{
					int t = 1;
					if (mtex->maptoneg & MAP_SPEC) t = -1;
					mparams["specular"]=yafray::parameter_t(t);
				}

				// hardness modulation
				if ((mtex->mapto & MAP_HAR) || (mtex->maptoneg & MAP_HAR)) 
				{
					int t = 1;
					if (mtex->maptoneg & MAP_HAR) t = -1;
					mparams["hard"]=yafray::parameter_t(t);
				}
 
				// alpha modulation
				if ((mtex->mapto & MAP_ALPHA) || (mtex->maptoneg & MAP_ALPHA)) 
				{
					int t = 1;
					if (mtex->maptoneg & MAP_ALPHA) t = -1;
					mparams["alpha"]=yafray::parameter_t(t);
				}

				// emit modulation
				if ((mtex->mapto & MAP_EMIT) || (mtex->maptoneg & MAP_EMIT)) {
					int t = 1;
					if (mtex->maptoneg & MAP_EMIT) t = -1;
					mparams["emit"]=yafray::parameter_t(t);
				}

				// texture flag, combination of strings
				if (mtex->texflag & (MTEX_RGBTOINT | MTEX_STENCIL | MTEX_NEGATIVE)) {
					ts = "";
					if (mtex->texflag & MTEX_RGBTOINT) ts += "no_rgb ";
					if (mtex->texflag & MTEX_STENCIL) ts += "stencil ";
					if (mtex->texflag & MTEX_NEGATIVE) ts += "negative";
					mparams["texflag"]=yafray::parameter_t(ts);
				}

				// colfac, controls amount of color modulation
				mparams["colfac"]=yafray::parameter_t(mtex->colfac);
				// def_var
				mparams["def_var"]=yafray::parameter_t(mtex->def_var);
				//varfac
				mparams["varfac"]=yafray::parameter_t(mtex->varfac);

				if ((tex->imaflag & (TEX_CALCALPHA | TEX_USEALPHA)) || (tex->flag & TEX_NEGALPHA)) 
				{
					ts = "";
					if (tex->imaflag & TEX_CALCALPHA) ts += "calc_alpha ";
					if (tex->imaflag & TEX_USEALPHA) ts += "use_alpha ";
					if (tex->flag & TEX_NEGALPHA) ts += "neg_alpha";
					mparams["alpha_flag"]=yafray::parameter_t(ts);
				}
				lparams.push_back(mparams);
			}
		}
		yafrayGate->addShader(params,lparams);
	}
}

void yafrayPluginRender_t::genUVcoords(vector<yafray::GFLOAT> &uvcoords,VlakRen *vlr,TFace* uvc)
{
	if (uvc) 
	{
	// use correct uv coords for this triangle
		if (vlr->flag & R_FACE_SPLIT) 
		{
			uvcoords.push_back(uvc->uv[0][0]);uvcoords.push_back(1-uvc->uv[0][1]);
			uvcoords.push_back(uvc->uv[2][0]);uvcoords.push_back(1-uvc->uv[2][1]);
			uvcoords.push_back(uvc->uv[3][0]);uvcoords.push_back(1-uvc->uv[3][1]);
		}
		else 
		{
			uvcoords.push_back(uvc->uv[0][0]);uvcoords.push_back(1-uvc->uv[0][1]);
			uvcoords.push_back(uvc->uv[1][0]);uvcoords.push_back(1-uvc->uv[1][1]);
			uvcoords.push_back(uvc->uv[2][0]);uvcoords.push_back(1-uvc->uv[2][1]);
		}
	}
	else
	{
		uvcoords.push_back(0);uvcoords.push_back(0);
		uvcoords.push_back(0);uvcoords.push_back(0);
		uvcoords.push_back(0);uvcoords.push_back(0);
	}
}

void yafrayPluginRender_t::genCompleUVcoords(vector<yafray::GFLOAT> &uvcoords,/*VlakRen *vlr,*/TFace* uvc)
{
	if (uvc) 
	{
	// use correct uv coords for this triangle
		uvcoords.push_back(uvc->uv[2][0]);uvcoords.push_back(1-uvc->uv[2][1]);
		uvcoords.push_back(uvc->uv[3][0]);uvcoords.push_back(1-uvc->uv[3][1]);
		uvcoords.push_back(uvc->uv[0][0]);uvcoords.push_back(1-uvc->uv[0][1]);
	}
	else
	{
		uvcoords.push_back(0);uvcoords.push_back(0);
		uvcoords.push_back(0);uvcoords.push_back(0);
		uvcoords.push_back(0);uvcoords.push_back(0);
	}
}

void yafrayPluginRender_t::genVcol(vector<yafray::CFLOAT> &vcol,VlakRen *vlr,
																		int p1,int p2,int p3,bool EXPORT_VCOL)
{
	if ((EXPORT_VCOL) && (vlr->vcol)) 
	{
		// vertex colors
		float vr, vg, vb;
		vr = ((vlr->vcol[p1] >> 24) & 255)/255.0;
		vg = ((vlr->vcol[p1] >> 16) & 255)/255.0;
		vb = ((vlr->vcol[p1] >> 8) & 255)/255.0;
		vcol.push_back(vr);vcol.push_back(vg);vcol.push_back(vb);
		vr = ((vlr->vcol[p2] >> 24) & 255)/255.0;
		vg = ((vlr->vcol[p2] >> 16) & 255)/255.0;
		vb = ((vlr->vcol[p2] >> 8) & 255)/255.0;
		vcol.push_back(vr);vcol.push_back(vg);vcol.push_back(vb);
		vr = ((vlr->vcol[p3] >> 24) & 255)/255.0;
		vg = ((vlr->vcol[p3] >> 16) & 255)/255.0;
		vb = ((vlr->vcol[p3] >> 8) & 255)/255.0;
		vcol.push_back(vr);vcol.push_back(vg);vcol.push_back(vb);
	}
	else
	{
		vcol.push_back(0);vcol.push_back(0);vcol.push_back(0);
		vcol.push_back(0);vcol.push_back(0);vcol.push_back(0);
		vcol.push_back(0);vcol.push_back(0);vcol.push_back(0);
	}
}

void yafrayPluginRender_t::genFace(vector<int> &faces,vector<string> &shaders,vector<int> &faceshader,
														vector<yafray::GFLOAT> &uvcoords,vector<yafray::CFLOAT> &vcol,
														map<VertRen*, int> &vert_idx,VlakRen *vlr,
														bool has_orco,bool has_uv,bool has_vcol)
{
	Material* fmat = vlr->mat;
	bool EXPORT_VCOL = ((fmat->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))!=0);
	string fmatname = fmat->id.name;
	if (fmatname=="") fmatname = "blender_default";
	bool newmat=true;
	for(unsigned int i=0;i<shaders.size();++i)
		if(shaders[i]==fmatname)
		{
			newmat=false;
			faceshader.push_back(i);
			break;
		}
	if(newmat)
	{
		shaders.push_back(fmatname);
		faceshader.push_back(shaders.size()-1);
	}
	//else fmatname+=2;	//skip MA id
	TFace* uvc = vlr->tface;	// possible uvcoords (v upside down)
	int idx1, idx2, idx3;

	idx1 = vert_idx.find(vlr->v1)->second;
	idx2 = vert_idx.find(vlr->v2)->second;
	idx3 = vert_idx.find(vlr->v3)->second;

	// make sure the indices point to the vertices when orco coords exported
	if (has_orco) { idx1*=2;  idx2*=2;  idx3*=2; }

	faces.push_back(idx1);faces.push_back(idx2);faces.push_back(idx3);

	if(has_uv) genUVcoords(uvcoords,vlr,uvc);

	// since Blender seems to need vcols when uvs are used, for yafray only export when the material actually uses vcols
	if(has_vcol) genVcol(vcol,vlr,0,1,2,EXPORT_VCOL);
}

void yafrayPluginRender_t::genCompleFace(vector<int> &faces,/*vector<string> &shaders,*/vector<int> &faceshader,
														vector<yafray::GFLOAT> &uvcoords,vector<yafray::CFLOAT> &vcol,
														map<VertRen*, int> &vert_idx,VlakRen *vlr,
														bool has_orco,bool has_uv,bool has_vcol)
{
	Material* fmat = vlr->mat;
	bool EXPORT_VCOL = ((fmat->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))!=0);

	faceshader.push_back(faceshader.back());
	TFace* uvc = vlr->tface;	// possible uvcoords (v upside down)
	int idx1, idx2, idx3;
	idx1 = vert_idx.find(vlr->v3)->second;
	idx2 = vert_idx.find(vlr->v4)->second;
	idx3 = vert_idx.find(vlr->v1)->second;

	// make sure the indices point to the vertices when orco coords exported
	if (has_orco) { idx1*=2;  idx2*=2;  idx3*=2; }

	faces.push_back(idx1);faces.push_back(idx2);faces.push_back(idx3);

	if(has_uv) genCompleUVcoords(uvcoords,/*vlr,*/uvc);
	if(has_vcol) genVcol(vcol,vlr,2,3,0,EXPORT_VCOL);
}

void yafrayPluginRender_t::genVertices(vector<yafray::point3d_t> &verts,int &vidx,
																			 map<VertRen*, int> &vert_idx,VlakRen* vlr,bool has_orco)
{
	VertRen* ver;
	if (vert_idx.find(vlr->v1)==vert_idx.end()) 
	{
		vert_idx[vlr->v1] = vidx++;
		ver = vlr->v1;
		verts.push_back(yafray::point3d_t(ver->co[0],ver->co[1],ver->co[2]));
		if (has_orco) 
			verts.push_back(yafray::point3d_t(ver->orco[0],ver->orco[1],ver->orco[2]));
	}
	if (vert_idx.find(vlr->v2)==vert_idx.end()) 
	{
		vert_idx[vlr->v2] = vidx++;
		ver = vlr->v2;
		verts.push_back(yafray::point3d_t(ver->co[0],ver->co[1],ver->co[2]));
		if (has_orco)
			verts.push_back(yafray::point3d_t(ver->orco[0],ver->orco[1],ver->orco[2]));
	}
	if (vert_idx.find(vlr->v3)==vert_idx.end()) 
	{
		vert_idx[vlr->v3] = vidx++;
		ver = vlr->v3;
		verts.push_back(yafray::point3d_t(ver->co[0],ver->co[1],ver->co[2]));
		if (has_orco)
			verts.push_back(yafray::point3d_t(ver->orco[0],ver->orco[1],ver->orco[2]));
	}
	if ((vlr->v4) && (vert_idx.find(vlr->v4)==vert_idx.end())) 
	{
		vert_idx[vlr->v4] = vidx++;
		ver = vlr->v4;
		verts.push_back(yafray::point3d_t(ver->co[0],ver->co[1],ver->co[2]));
		if (has_orco)
			verts.push_back(yafray::point3d_t(ver->orco[0],ver->orco[1],ver->orco[2]));
	}
}

void yafrayPluginRender_t::writeObject(Object* obj, const vector<VlakRen*> &VLR_list, const float obmat[4][4])
{
	float mtr[4*4];
	mtr[0*4+0]=obmat[0][0];mtr[0*4+1]=obmat[1][0];mtr[0*4+2]=obmat[2][0];mtr[0*4+3]=obmat[3][0];
	mtr[1*4+0]=obmat[0][1];mtr[1*4+1]=obmat[1][1];mtr[1*4+2]=obmat[2][1];mtr[1*4+3]=obmat[3][1];
	mtr[2*4+0]=obmat[0][2];mtr[2*4+1]=obmat[1][2];mtr[2*4+2]=obmat[2][2];mtr[2*4+3]=obmat[3][2];
	mtr[3*4+0]=obmat[0][3];mtr[3*4+1]=obmat[1][3];mtr[3*4+2]=obmat[2][3];mtr[3*4+3]=obmat[3][3];
	yafrayGate->transformPush(mtr);
	string name=string(obj->id.name+2);
	bool castShadows=VLR_list[0]->mat->mode & MA_TRACEBLE;
	float caus_IOR=1.0;
	yafray::color_t caus_tcolor(0.0,0.0,0.0),caus_rcolor(0.0,0.0,0.0);
	bool caus=false;
	if (VLR_list[0]->mat->mode & MA_RAYTRANSP) 
	{
		caus_IOR=VLR_list[0]->mat->ang;
		float tr=1.0-VLR_list[0]->mat->alpha;
		caus_tcolor.set(VLR_list[0]->mat->r * tr,VLR_list[0]->mat->g * tr,VLR_list[0]->mat->b * tr);
		caus=true;
	}
	bool has_orco=(VLR_list[0]->v1->orco!=NULL);
	float sm_angle=0.1;
	if (obj->type==OB_MESH) 
	{
		Mesh* mesh = (Mesh*)obj->data;
		if (mesh->flag & ME_AUTOSMOOTH) 
			sm_angle=mesh->smoothresh;
		else
			if (VLR_list[0]->flag & ME_SMOOTH)	sm_angle=90;
	}
	// Guess if we need to set vertex colors Could be faster? sure
	bool has_vcol=false;
	for(int i=0;i<obj->totcol;++i)
	{
		Material *fmat=obj->mat[i];
		if(fmat==NULL) continue;
		if((fmat->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))!=0) {has_vcol=true;break;};
	}
	vector<yafray::point3d_t> verts;
	vector<yafray::CFLOAT> vcol;
	// now all vertices
	map<VertRen*, int> vert_idx;	// for removing duplicate verts and creating an index list
	int vidx = 0;	// vertex index counter
	bool has_uv=false;
	for (vector<VlakRen*>::const_iterator fci=VLR_list.begin();
				fci!=VLR_list.end();++fci)
	{
		VlakRen* vlr = *fci;
		genVertices(verts,vidx,vert_idx,vlr,has_orco);
		if(vlr->tface) has_uv=true;
	}
	// all faces using the index list created above
	vector<int> faces;
	vector<string> shaders;
	vector<int> faceshader;
	vector<yafray::GFLOAT> uvcoords;
	for (vector<VlakRen*>::const_iterator fci2=VLR_list.begin();
				fci2!=VLR_list.end();++fci2)
	{
		VlakRen* vlr = *fci2;
		genFace(faces,shaders,faceshader,uvcoords,vcol,vert_idx,vlr,has_orco,has_uv,has_vcol);
		if (vlr->v4) 
			genCompleFace(faces,/*shaders,*/faceshader,uvcoords,vcol,vert_idx,vlr,has_orco,has_uv,has_vcol);
	}

	yafrayGate->addObject_trimesh(name,verts,faces,uvcoords,vcol,
			shaders,faceshader,sm_angle,castShadows,true,true,caus,has_orco,
			caus_rcolor,caus_tcolor,caus_IOR);
	yafrayGate->transformPop();
}


// write all objects
void yafrayPluginRender_t::writeAllObjects()
{

	// first all objects except dupliverts (and main instance object for dups)
	for (map<Object*, vector<VlakRen*> >::const_iterator obi=all_objects.begin();
			obi!=all_objects.end(); ++obi)
	{
	  // skip main duplivert object if in dupliMtx_list, written later
		Object* obj = obi->first;
		if (dupliMtx_list.find(string(obj->id.name))!=dupliMtx_list.end()) continue;
		writeObject(obj, obi->second, obi->first->obmat);
	}

	// Now all duplivert objects (if any) as instances of main object
	// The original object has been included in the VlakRen renderlist above (see convertBlenderScene.c)
	// but is written here which all other duplis are instances of.
	float obmat[4][4], cmat[4][4], imat[4][4], nmat[4][4];
	for (map<string, vector<float> >::const_iterator dupMtx=dupliMtx_list.begin();
		dupMtx!=dupliMtx_list.end();++dupMtx) {

		// original inverse matrix, not actual matrix of object, but first duplivert.
		for (int i=0;i<4;i++)
			for (int j=0;j<4;j++)
				obmat[i][j] = dupMtx->second[(i<<2)+j];
		MTC_Mat4Invert(imat, obmat);

		// first object written as normal (but with transform of first duplivert)
		Object* obj = dup_srcob[dupMtx->first];
		writeObject(obj, all_objects[obj], obmat);

		// all others instances of first
		for (unsigned int curmtx=16;curmtx<dupMtx->second.size();curmtx+=16) 
		{	// number of 4x4 matrices
			// new mtx
			for (int i=0;i<4;i++)
				for (int j=0;j<4;j++)
					nmat[i][j] = dupMtx->second[curmtx+(i<<2)+j];

			MTC_Mat4MulMat4(cmat, imat, nmat);	// transform with respect to original = inverse_original * new

			float mtr[4*4];
			mtr[0*4+0]=cmat[0][0];mtr[0*4+1]=cmat[1][0];mtr[0*4+2]=cmat[2][0];mtr[0*4+3]=cmat[3][0];
			mtr[1*4+0]=cmat[0][1];mtr[1*4+1]=cmat[1][1];mtr[1*4+2]=cmat[2][1];mtr[1*4+3]=cmat[3][1];
			mtr[2*4+0]=cmat[0][2];mtr[2*4+1]=cmat[1][2];mtr[2*4+2]=cmat[2][2];mtr[2*4+3]=cmat[3][2];
			mtr[3*4+0]=cmat[0][3];mtr[3*4+1]=cmat[1][3];mtr[3*4+2]=cmat[2][3];mtr[3*4+3]=cmat[3][3];
			yafrayGate->transformPush(mtr);

			// new name from original
			string name=(obj->id.name+2);
			char temp[16];
			sprintf(temp,"_dup%d",(curmtx>>4));
			name+=temp;
			yafrayGate->addObject_reference(name,obj->id.name+2);
			yafrayGate->transformPop();
		}

	}

}

void yafrayPluginRender_t::writeAreaLamp(LampRen* lamp, int num)
{
	yafray::paramMap_t params;
	
	if (lamp->area_shape!=LA_AREA_SQUARE) return;
	float *a=lamp->area[0], *b=lamp->area[1], *c=lamp->area[2], *d=lamp->area[3];
	float power=lamp->energy;
	
	string md = "off";
	if (R.r.GIphotons) {md = "on";power*=R.r.GIpower;}
	params["type"]=yafray::parameter_t("arealight");
	char temp[16];
	sprintf(temp,"LAMP%d",num+1);
	params["name"]=yafray::parameter_t(temp);
	params["dummy"]=yafray::parameter_t(md);
	params["power"]=yafray::parameter_t(power);
	if (!R.r.GIphotons) 
	{
		int psm=0, sm = lamp->ray_totsamp;
		if (sm>=64) psm = sm/4;
		params["samples"]=yafray::parameter_t(sm);
		params["psamples"]=yafray::parameter_t(psm);
	}
	params["a"]=yafray::parameter_t(yafray::point3d_t(a[0],a[1],a[2]));
	params["b"]=yafray::parameter_t(yafray::point3d_t(b[0],b[1],b[2]));
	params["c"]=yafray::parameter_t(yafray::point3d_t(c[0],c[1],c[2]));
	params["d"]=yafray::parameter_t(yafray::point3d_t(d[0],d[1],d[2]));
	params["color"]=yafray::parameter_t(yafray::color_t(lamp->r,lamp->g,lamp->b));
	yafrayGate->addLight(params);
}

void yafrayPluginRender_t::writeLamps()
{
	// all lamps
	for (int i=0;i<R.totlamp;i++)
	{
		yafray::paramMap_t params;
		string type="";
		LampRen* lamp = R.la[i];
		if (lamp->type==LA_AREA) { writeAreaLamp(lamp, i);  continue; }
		// TODO: add decay setting in yafray
		if (lamp->type==LA_LOCAL)
			params["type"]=yafray::parameter_t("pointlight");
		else if (lamp->type==LA_SPOT)
			params["type"]=yafray::parameter_t("spotlight");
		else if ((lamp->type==LA_SUN) || (lamp->type==LA_HEMI))	// for now, hemi same as sun
			params["type"]=yafray::parameter_t("sunlight");
		else 
		{
			// possibly unknown type, ignore
			cout << "Unknown Blender lamp type: " << lamp->type << endl;
			continue;
		}
		//no name available here, create one
		char temp[16];
		sprintf(temp,"LAMP%d",i+1);
		params["name"]=yafray::parameter_t(temp);
		// color already premultiplied by energy, so only need distance here
		float pwr;
		if (lamp->mode & LA_SPHERE) 
		{
			// best approx. as used in LFexport script (LF d.f.m. 4pi?)
			pwr = lamp->dist*(lamp->dist+1)*(0.25/M_PI);
			//decay = 2;
		}
		else 
		{
			if ((lamp->type==LA_LOCAL) || (lamp->type==LA_SPOT)) 
				pwr = lamp->dist;
			else pwr = 1;	// sun/hemi distance irrelevent.
		}
		params["power"]=yafray::parameter_t(pwr);
		string lpmode="off";
		// shadows only when Blender has shadow button enabled, only spots use LA_SHAD flag
		if (R.r.mode & R_SHADOW)
			params["cast_shadows"]=yafray::parameter_t("on");
		else
			params["cast_shadows"]=yafray::parameter_t("off");
		// spot specific stuff
		if (lamp->type==LA_SPOT) 
		{
			// conversion already changed spotsize to cosine of half angle
			float ld = 1-lamp->spotsi;	//convert back to blender slider setting
			if (ld!=0) ld = 1.f/ld;
			params["size"]=yafray::parameter_t(acos(lamp->spotsi)*180.0/M_PI);
			params["blend"]=yafray::parameter_t(lamp->spotbl*ld);
			params["beam_falloff"]=yafray::parameter_t(2.0);
		}
		params["from"]=yafray::parameter_t(yafray::point3d_t(lamp->co[0],lamp->co[1],lamp->co[2]));
		// position
		// 'to' for spot, already calculated by Blender
		if (lamp->type==LA_SPOT)
			params["to"]=yafray::parameter_t(yafray::point3d_t(lamp->co[0]+lamp->vec[0],
																													 lamp->co[1]+lamp->vec[1],
																													 lamp->co[2]+lamp->vec[2]));
		// color
		// rgb in LampRen is premultiplied by energy, power is compensated for that above
		params["color"]=yafray::parameter_t(yafray::color_t(lamp->r,lamp->g,lamp->b));
		yafrayGate->addLight(params);
	}
}


// write main camera
void yafrayPluginRender_t::writeCamera()
{
	yafray::paramMap_t params;
	params["name"]=yafray::parameter_t("MAINCAM");
	params["resx"]=yafray::parameter_t(R.r.xsch);
	params["resy"]=yafray::parameter_t(R.r.ysch);
	float aspect = 1;
	if (R.r.xsch < R.r.ysch) aspect = float(R.r.xsch)/float(R.r.ysch);

	params["focal"]=yafray::parameter_t(mainCamLens/(aspect*32.0));
	float camtx[4][4];
	MTC_Mat4CpyMat4(camtx, maincam_obj->obmat);
	MTC_normalise3DF(camtx[1]);	//up
	MTC_normalise3DF(camtx[2]);	//dir
	params["from"]=yafray::parameter_t(
			yafray::point3d_t(camtx[3][0],camtx[3][1],camtx[3][2]));
	Object* dofob = findObject("OBFOCUS");
	float fdist=1;
	if (dofob) {
		// dof empty found, modify lookat point accordingly
		// location from matrix, in case animated
		float fdx = dofob->obmat[3][0] - camtx[3][0];
		float fdy = dofob->obmat[3][1] - camtx[3][1];
		float fdz = dofob->obmat[3][2] - camtx[3][2];
		fdist = sqrt(fdx*fdx + fdy*fdy + fdz*fdz);
		cout << "FOCUS object found, distance is: " << fdist << endl;
	}
	params["to"]=yafray::parameter_t(
			yafray::point3d_t(camtx[3][0] - fdist*camtx[2][0],
												camtx[3][1] - fdist*camtx[2][1],
												camtx[3][2] - fdist*camtx[2][2]));
	params["up"]=yafray::parameter_t(
			yafray::point3d_t(camtx[3][0] + camtx[1][0],
												camtx[3][1] + camtx[1][1],
												camtx[3][2] + camtx[1][2]));
	yafrayGate->addCamera(params);
}

void yafrayPluginRender_t::writeHemilight()
{
	yafray::paramMap_t params;
	params["type"]=yafray::parameter_t("hemilight");
	params["name"]=yafray::parameter_t("hemi_LT");
	params["power"]=yafray::parameter_t(1.0);
	switch (R.r.GIquality)
	{
		case 1 :
		case 2 : params["samples"]=yafray::parameter_t(16);  break;
		case 3 : params["samples"]=yafray::parameter_t(36);  break;
		case 4 : params["samples"]=yafray::parameter_t(64);  break;
		case 5 : params["samples"]=yafray::parameter_t(128);  break;
		default: params["samples"]=yafray::parameter_t(25);
	}
	yafrayGate->addLight(params);
}

void yafrayPluginRender_t::writePathlight()
{
	if(R.r.GIphotons)
	{
		yafray::paramMap_t params;
		params["type"]=yafray::parameter_t("globalphotonlight");
		params["name"]=yafray::parameter_t("gpm");
		params["photons"]=yafray::parameter_t(R.r.GIphotoncount);
		params["radius"]=yafray::parameter_t(R.r.GIphotonradius);
		params["depth"]=yafray::parameter_t(((R.r.GIdepth>2) ? (R.r.GIdepth-1) : 1));
		params["caus_depth"]=yafray::parameter_t(R.r.GIcausdepth);
		params["search"]=yafray::parameter_t(R.r.GImixphotons);
		yafrayGate->addLight(params);
	}
	yafray::paramMap_t params;
	params["type"]=yafray::parameter_t("pathlight");
	params["name"]=yafray::parameter_t("path_LT");
	params["power"]=yafray::parameter_t(1.0);
	params["depth"]=yafray::parameter_t(((R.r.GIphotons) ? 1 : R.r.GIdepth));
	params["caus_depth"]=yafray::parameter_t(R.r.GIcausdepth);
	if(R.r.GIdirect && R.r.GIphotons) params["direct"]=yafray::parameter_t("on");
	if (R.r.GIcache && ! (R.r.GIdirect && R.r.GIphotons))
	{
		switch (R.r.GIquality)
		{
			case 1 : params["samples"]=yafray::parameter_t(128);break;
			case 2 : params["samples"]=yafray::parameter_t(256);break;
			case 3 : params["samples"]=yafray::parameter_t(512);break;
			case 4 : params["samples"]=yafray::parameter_t(1024);break;
			case 5 : params["samples"]=yafray::parameter_t(2048);break;
			default: params["samples"]=yafray::parameter_t(256);
		}
		float aspect = 1;
		if (R.r.xsch < R.r.ysch) aspect = float(R.r.xsch)/float(R.r.ysch);
		float sbase = 2.0*atan(0.5/(mainCamLens/(aspect*32.0)))/float(R.r.xsch);
		params["cache"]=yafray::parameter_t("on");
		params["use_QMC"]=yafray::parameter_t("on");
		params["threshold"]=yafray::parameter_t(R.r.GIrefinement);
		params["cache_size"]=yafray::parameter_t(sbase*R.r.GIpixelspersample);
		params["shadow_threshold"]=yafray::parameter_t(1.0 - R.r.GIshadowquality);
		params["grid"]=yafray::parameter_t(82);
		params["search"]=yafray::parameter_t(35);
		//params["gradient"]=yafray::parameter_t("off");
	}
	else
	{
		switch (R.r.GIquality)
		{
			case 1 : params["samples"]=yafray::parameter_t(16);break;
			case 2 : params["samples"]=yafray::parameter_t(36);break;
			case 3 : params["samples"]=yafray::parameter_t(64);break;
			case 4 : params["samples"]=yafray::parameter_t(128);break;
			case 5 : params["samples"]=yafray::parameter_t(256);break;
			default: params["samples"]=yafray::parameter_t(25);break;
		}
	}
	yafrayGate->addLight(params);
}

bool yafrayPluginRender_t::writeWorld()
{
	World *world = G.scene->world;
	short i=0,j=0;
	if (R.r.GIquality!=0) {
		if (R.r.GImethod==1) {
			if (world==NULL) cout << "WARNING: need world background for skydome!\n";
			writeHemilight();
		}
		else if (R.r.GImethod==2) writePathlight();
	}

	if (world==NULL) return false;

	for(i=0;i<8;i++){
		if(world->mtex[i] != NULL)
		{
			if(world->mtex[i]->tex->type == TEX_IMAGE && world->mtex[i]->tex->ima != NULL){
				
				for(j=0;j<160;j++){
					if(world->mtex[i]->tex->ima->name[j] == '\0' && j > 3){
						if(
							(world->mtex[i]->tex->ima->name[j-3] == 'h' || world->mtex[i]->tex->ima->name[j-3] == 'H' ) &&
							(world->mtex[i]->tex->ima->name[j-2] == 'd' || world->mtex[i]->tex->ima->name[j-2] == 'D' ) &&
							(world->mtex[i]->tex->ima->name[j-1] == 'r' || world->mtex[i]->tex->ima->name[j-1] == 'R' )
							)
						{
								yafray::paramMap_t params;
								params["type"]=yafray::parameter_t("HDRI");
								params["name"]=yafray::parameter_t("world_background");
								params["exposure_adjust"]=yafray::parameter_t(world->mtex[i]->tex->bright-1);
								params["mapping"]=yafray::parameter_t("probe");
								params["filename"]=yafray::parameter_t(world->mtex[i]->tex->ima->name);
								yafrayGate->addBackground(params);
								return true;						
						}
					}
				}

				yafray::paramMap_t params;
				params["type"]=yafray::parameter_t("image");
				params["name"]=yafray::parameter_t("world_background");
				params["power"]=yafray::parameter_t(world->mtex[i]->tex->bright);
				params["filename"]=yafray::parameter_t(world->mtex[i]->tex->ima->name);
				yafrayGate->addBackground(params);
				return true;
			}
		}
	}

	yafray::paramMap_t params;
	params["type"]=yafray::parameter_t("constant");
	params["name"]=yafray::parameter_t("world_background");
	// if no GI used, the GIpower parameter is not always initialized, so in that case ignore it  (have to change method to init yafray vars in Blender)
	float bg_mult;
	if (R.r.GImethod==0) bg_mult=1; else bg_mult=R.r.GIpower;
	params["color"]=yafray::parameter_t(yafray::color_t(world->horr * bg_mult,
																											world->horg * bg_mult,
																											world->horb * bg_mult));
	yafrayGate->addBackground(params);
	return true;
}

#include "RE_callbacks.h"

bool blenderYafrayOutput_t::putPixel(int x, int y,const yafray::color_t &c, 
		yafray::CFLOAT alpha,yafray::PFLOAT depth)
{
	unsigned char* bpt = (unsigned char*)R.rectot + ((((R.recty-1)-y)*R.rectx)<<2);
	int temp=(int)(c.R*255.0+0.5);
	if(temp>255) temp=255;
	bpt[4*x]=temp;
	temp=(int)(c.G*255.0+0.5);
	if(temp>255) temp=255;
	bpt[4*x+1]=temp;
	temp=(int)(c.B*255.0+0.5);
	if(temp>255) temp=255;
	bpt[4*x+2]=temp;
	temp=(int)(alpha*255.0+0.5);
	if(temp>255) temp=255;
	bpt[4*x+3]=temp;
	out++;
	if(out==4096)
	{
		RE_local_render_display(0,R.recty-1, R.rectx, R.recty, R.rectot);
		out=0;
	}
	if(RE_local_test_break())
		return false;
	return true;
}
