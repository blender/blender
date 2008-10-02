#include "export_Plugin.h"

#include <math.h>

#include <cstring>

using namespace std;


#ifdef WIN32 
#define WIN32_SKIP_HKEY_PROTECTION
#include "BLI_winstuff.h"

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
	size_t sp = path.find_first_of(":");
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
	return path;
#else
	static const char *alternative[]=
	{
		"/usr/local/lib/",
#ifdef __x86_64__
		"/usr/lib64/",
#endif
		"/usr/lib/",
		NULL
	};

	for(int i=0;alternative[i]!=NULL;++i)
	{
		string fp = string(alternative[i]) + "libyafrayplugin.so";
		struct stat st;
		if (stat(fp.c_str(), &st)<0) continue;
		if (st.st_mode & S_IROTH) return fp;
	}
	return "";
#endif
}

static string YafrayPluginPath()
{
#ifdef WIN32
	return find_path()+"\\plugins";
#else
	static const char *alternative[]=
	{
		"/usr/local/lib/yafray",
#ifdef __x86_64__
		"/usr/lib64/yafray",
#endif
		"/usr/lib/yafray",
		NULL
	};

	for(int i=0;alternative[i]!=NULL;++i)
	{
		struct stat st;
		if (stat(alternative[i], &st)<0) continue;
		if (S_ISDIR(st.st_mode) && (st.st_mode & S_IXOTH)) return alternative[i];
	}
	return "";
#endif
}



yafrayPluginRender_t::~yafrayPluginRender_t()
{
	if (yafrayGate!=NULL) delete yafrayGate;
	if (handle!=NULL) PIL_dynlib_close(handle);
#ifdef WIN32
	if (corehandle!=NULL) PIL_dynlib_close(corehandle);
#endif
}

bool yafrayPluginRender_t::initExport()
{
	// bug #1897: when forcing render without yafray present, handle can be valid,
	// but find_symbol might have failed, trying second time will crash.
	// So make sure plugin loaded correctly and only get handle once.
	if ((!plugin_loaded) || (handle==NULL))
	{
		string location = YafrayPath();
#ifdef WIN32
		/* Win 32 loader cannot find needed libs in yafray dir, so we have to load them
		 * by hand. This could be fixed using setdlldirectory function, but it is not
		 * available in all win32 versions
		 */
		corehandle = PIL_dynlib_open((char *)(location + "\\yafraycore.dll").c_str());
		if (corehandle==NULL)
		{
			cerr << "Error loading yafray plugin: " << PIL_dynlib_get_error_as_string(corehandle) << endl;
			return false;
		}
		location += "\\yafrayplugin.dll";
#endif

		if (handle==NULL) {
			handle = PIL_dynlib_open((char *)location.c_str());
			if (handle==NULL)
			{
				cerr << "Error loading yafray plugin: " << PIL_dynlib_get_error_as_string(handle) << endl;
				return false;
			}
		}
		yafray::yafrayConstructor *constructor;
		constructor = (yafray::yafrayConstructor *)PIL_dynlib_find_symbol(handle, YAFRAY_SYMBOL);
		if (constructor==NULL)
		{
			cerr << "Error loading yafray plugin: " << PIL_dynlib_get_error_as_string(handle) << endl;
			return false;
		}
		yafrayGate = constructor(re->r.threads, YafrayPluginPath());
		
		cout << "YafRay plugin loaded" << endl;
		plugin_loaded = true;
	}
	
	return true;
}

bool yafrayPluginRender_t::writeRender()
{
	yafray::paramMap_t params;
	params["camera_name"]=yafray::parameter_t("MAINCAM");
	params["raydepth"]=yafray::parameter_t((float)re->r.YF_raydepth);
	params["gamma"]=yafray::parameter_t(re->r.YF_gamma);
	params["exposure"]=yafray::parameter_t(re->r.YF_exposure);
	if (re->r.YF_AA)
	{
		params["AA_passes"] = yafray::parameter_t((int)re->r.YF_AApasses);
		params["AA_minsamples"] = yafray::parameter_t(re->r.YF_AAsamples);
		params["AA_pixelwidth"] = yafray::parameter_t(re->r.YF_AApixelsize);
		params["AA_threshold"] = yafray::parameter_t(re->r.YF_AAthreshold);
	}
	else
	{
		// removed the default AA settings for midquality GI, better leave it to user
		if ((re->r.mode & R_OSA) && (re->r.osa)) 
		{
			params["AA_passes"] = yafray::parameter_t((re->r.osa & 3)==0 ? (re->r.osa >> 2) : 1);
			params["AA_minsamples"] = yafray::parameter_t((re->r.osa & 3)==0 ? 4 : re->r.osa);
		}
		else 
		{
			params["AA_passes"] = yafray::parameter_t(0);
			params["AA_minsamples"] = yafray::parameter_t(1);
		}
		params["AA_pixelwidth"] = yafray::parameter_t(1.5);
		params["AA_threshold"] = yafray::parameter_t(0.05f);
	}
	if (re->r.mode & R_BORDER)
	{
		params["border_xmin"] = yafray::parameter_t(2.f*re->r.border.xmin - 1.f);
		params["border_xmax"] = yafray::parameter_t(2.f*re->r.border.xmax - 1.f);
		params["border_ymin"] = yafray::parameter_t(2.f*re->r.border.ymin - 1.f);
		params["border_ymax"] = yafray::parameter_t(2.f*re->r.border.ymax - 1.f);
	}
	if (hasworld) {
		World *world = G.scene->world;
		if (world->mode & WO_MIST) {
			// basic fog
			float fd = world->mistdist;
			if (fd>0) fd=1.f/fd; else fd=1;
			params["fog_density"] = yafray::parameter_t(fd);
			params["fog_color"] = yafray::parameter_t(yafray::color_t(world->horr, world->horg, world->horb));
		}
		params["background_name"] = yafray::parameter_t("world_background");
	}
	params["bias"] = yafray::parameter_t(re->r.YF_raybias);
	params["clamp_rgb"] = yafray::parameter_t((re->r.YF_clamprgb==0) ? "on" : "off");
	// lynx request
	params["threads"] = yafray::parameter_t((int)re->r.threads);
	blenderYafrayOutput_t output(re);
	yafrayGate->render(params, output);
	cout << "render finished" << endl;
	yafrayGate->clear();
	return true;
}

bool yafrayPluginRender_t::finishExport()
{
	return true;
}


// displayImage() not for plugin, see putPixel() below

#ifdef WIN32
#define MAXPATHLEN MAX_PATH
#else
#include <sys/param.h>
#endif
static void adjustPath(string &path)
{
	// if relative, expand to full path
	char cpath[MAXPATHLEN];
	strcpy(cpath, path.c_str());
	BLI_convertstringcode(cpath, G.sce);
	path = cpath;
#ifdef WIN32
	// add drive char if not there
	addDrive(path);
#endif
}


static string noise2string(short nbtype)
{
	switch (nbtype) {
		case TEX_BLENDER:
			return "blender";
		case TEX_STDPERLIN:
			return "stdperlin";
		case TEX_VORONOI_F1:
			return "voronoi_f1";
		case TEX_VORONOI_F2:
			return "voronoi_f2";
		case TEX_VORONOI_F3:
			return "voronoi_f3";
		case TEX_VORONOI_F4:
			return "voronoi_f4";
		case TEX_VORONOI_F2F1:
			return "voronoi_f2f1";
		case TEX_VORONOI_CRACKLE:
			return "voronoi_crackle";
		case TEX_CELLNOISE:
			return "cellnoise";
		default:
		case TEX_NEWPERLIN:
			return "newperlin";
	}
}

void yafrayPluginRender_t::writeTextures()
{
	// used to keep track of images already written
	// (to avoid duplicates if also in imagetex for material TexFace texture)
	set<Image*> dupimg;

	yafray::paramMap_t params;
	list<yafray::paramMap_t> lparams;
	for (map<string, MTex*>::const_iterator blendtex=used_textures.begin();
						blendtex!=used_textures.end();++blendtex) 
	{
		lparams.clear();
		params.clear();
		
		MTex* mtex = blendtex->second;
		Tex* tex = mtex->tex;
		// name is image name instead of texture name when type is image (see TEX_IMAGE case below)
		// (done because of possible combinations of 'TexFace' images and regular image textures, to avoid duplicates)
		if (tex->type!=TEX_IMAGE) params["name"] = yafray::parameter_t(blendtex->first);

		float nsz = tex->noisesize;
		if (nsz!=0.f) nsz=1.f/nsz;

		// noisebasis type
		string ntype = noise2string(tex->noisebasis);
		string ts, hardnoise=(tex->noisetype==TEX_NOISESOFT) ? "off" : "on";

		switch (tex->type) {
			case TEX_STUCCI:
				// stucci is clouds as bump, only difference is an extra parameter to handle wall in/out
				// turbulence value is not used, so for large values will not match well
			case TEX_CLOUDS: {
				params["type"] = yafray::parameter_t("clouds");
				params["size"] = yafray::parameter_t(nsz);
				params["hard"] = yafray::parameter_t(hardnoise);
				if (tex->type==TEX_STUCCI) {
					if (tex->stype==1)
						ts = "positive";
					else if (tex->stype==2)
						ts = "negative";
					else ts = "none";
					params["bias"] = yafray::parameter_t(ts);
					params["depth"] = yafray::parameter_t(0);	// for stucci always 0
				}
				else params["depth"] = yafray::parameter_t(tex->noisedepth);
				params["color_type"] = yafray::parameter_t(tex->stype);
				params["noise_type"] = yafray::parameter_t(ntype);
				break;
			}
			case TEX_WOOD:
			{
				params["type"] = yafray::parameter_t("wood");
				// blender does not use depth value for wood, always 0
				params["depth"] = yafray::parameter_t(0);
				float turb = (tex->stype<2) ? 0.0 : tex->turbul;
				params["turbulence"] = yafray::parameter_t(turb);
				params["size"] = yafray::parameter_t(nsz);
				params["hard"] = yafray::parameter_t(hardnoise);
				ts = (tex->stype & 1) ? "rings" : "bands";	//stype 1&3 ringtype
				params["wood_type"] = yafray::parameter_t(ts);
				params["noise_type"] = yafray::parameter_t(ntype);
				// shape parameter, for some reason noisebasis2 is used...
				ts = "sin";
				if (tex->noisebasis2==1) ts="saw"; else if (tex->noisebasis2==2) ts="tri";
				params["shape"] = yafray::parameter_t(ts);
				break;
			}
			case TEX_MARBLE: 
			{
				params["type"] = yafray::parameter_t("marble");
				params["depth"] = yafray::parameter_t(tex->noisedepth);
				params["turbulence"] = yafray::parameter_t(tex->turbul);
				params["size"] = yafray::parameter_t(nsz);
				params["hard"] = yafray::parameter_t(hardnoise);
				params["sharpness"] = yafray::parameter_t((float)(1<<tex->stype));
				params["noise_type"] = yafray::parameter_t(ntype);
				ts = "sin";
				if (tex->noisebasis2==1) ts="saw"; else if (tex->noisebasis2==2) ts="tri";
				params["shape"] = yafray::parameter_t(ts);
				break;
			}
			case TEX_VORONOI:
			{
				params["type"] = yafray::parameter_t("voronoi");
				ts = "int";
				if (tex->vn_coltype==1)
					ts = "col1";
				else if (tex->vn_coltype==2)
					ts = "col2";
				else if (tex->vn_coltype==3)
					ts = "col3";
				params["color_type"] = yafray::parameter_t(ts);
				params["weight1"] = yafray::parameter_t(tex->vn_w1);
				params["weight2"] = yafray::parameter_t(tex->vn_w2);
				params["weight3"] = yafray::parameter_t(tex->vn_w3);
				params["weight4"] = yafray::parameter_t(tex->vn_w4);
				params["mk_exponent"] = yafray::parameter_t(tex->vn_mexp);
				params["intensity"] = yafray::parameter_t(tex->ns_outscale);
				params["size"] = yafray::parameter_t(nsz);
				ts = "actual";
				if (tex->vn_distm==TEX_DISTANCE_SQUARED)
					ts = "squared";
				else if (tex->vn_distm==TEX_MANHATTAN)
					ts = "manhattan";
				else if (tex->vn_distm==TEX_CHEBYCHEV)
					ts = "chebychev";
				else if (tex->vn_distm==TEX_MINKOVSKY_HALF)
					ts = "minkovsky_half";
				else if (tex->vn_distm==TEX_MINKOVSKY_FOUR)
					ts = "minkovsky_four";
				else if (tex->vn_distm==TEX_MINKOVSKY)
					ts = "minkovsky";
				params["distance_metric"] = yafray::parameter_t(ts);
				break;
			}
			case TEX_MUSGRAVE:
			{
				params["type"] = yafray::parameter_t("musgrave");
				switch (tex->stype) {
					case TEX_MFRACTAL:
						ts = "multifractal";
						break;
					case TEX_RIDGEDMF:
						ts = "ridgedmf";
						break;
					case TEX_HYBRIDMF:
						ts = "hybridmf";
						break;
					case TEX_HTERRAIN:
						ts = "heteroterrain";
						break;
					default:
					case TEX_FBM:
						ts = "fBm";
				}
				params["musgrave_type"] = yafray::parameter_t(ts);
				params["noise_type"] = yafray::parameter_t(ntype);
				params["H"] = yafray::parameter_t(tex->mg_H);
				params["lacunarity"] = yafray::parameter_t(tex->mg_lacunarity);
				params["octaves"] = yafray::parameter_t(tex->mg_octaves);
				if ((tex->stype==TEX_HTERRAIN) || (tex->stype==TEX_RIDGEDMF) || (tex->stype==TEX_HYBRIDMF)) {
					params["offset"] = yafray::parameter_t(tex->mg_offset);
					if ((tex->stype==TEX_RIDGEDMF) || (tex->stype==TEX_HYBRIDMF))
						params["gain"] = yafray::parameter_t(tex->mg_gain);
				}
				params["size"] = yafray::parameter_t(nsz);
				params["intensity"] = yafray::parameter_t(tex->ns_outscale);
				break;
			}
			case TEX_DISTNOISE:
			{
				params["type"] = yafray::parameter_t("distorted_noise");
				params["distort"] = yafray::parameter_t(tex->dist_amount);
				params["size"] = yafray::parameter_t(nsz);
				params["noise_type1"] = yafray::parameter_t(ntype);
				params["noise_type2"] = yafray::parameter_t(noise2string(tex->noisebasis2));
				break;
			}
			case TEX_BLEND:
			{
				params["type"] = yafray::parameter_t("gradient");
				switch (tex->stype) {
					case 1:  ts="quadratic"; break;
					case 2:  ts="cubic";     break;
					case 3:  ts="diagonal";  break;
					case 4:  ts="sphere";    break;
					case 5:  ts="halo";      break;
					default:
					case 0:  ts="linear";    break;
				}
				params["gradient_type"] = yafray::parameter_t(ts);
				if (tex->flag & TEX_FLIPBLEND) ts="on"; else ts="off";
				params["flip_xy"] = yafray::parameter_t(ts);
				break;
			}
			case TEX_NOISE:
			{
				params["type"] = yafray::parameter_t("random_noise");
				params["depth"] = yafray::parameter_t(tex->noisedepth);
				break;
			}
			case TEX_IMAGE: 
			{
				Image* ima = tex->ima;
				if (ima) {
					// remember image to avoid duplicates later if also in imagetex
					// (formerly done by removing from imagetex, but need image/material link)
					dupimg.insert(ima);
					params["type"] = yafray::parameter_t("image");
					params["name"] = yafray::parameter_t(ima->id.name);
					string texpath = ima->name;
					adjustPath(texpath);
					params["filename"] = yafray::parameter_t(texpath);
					params["interpolate"] = yafray::parameter_t((tex->imaflag & TEX_INTERPOL) ? "bilinear" : "none");
				}
				break;
			}
			default:
				cout << "Unsupported texture type\n";
		}
		yafrayGate->addShader(params, lparams);

		// colorbands
		if (tex->flag & TEX_COLORBAND) 
		{
			ColorBand* cb = tex->coba;
			if (cb) 
			{
				lparams.clear();
				params.clear();
				params["type"] = yafray::parameter_t("colorband");
				params["name"] = yafray::parameter_t(blendtex->first + "_coba");
				params["input"] = yafray::parameter_t(blendtex->first);
				for (int i=0;i<cb->tot;i++) 
				{
					yafray::paramMap_t mparams;
					mparams["value"] = yafray::parameter_t(cb->data[i].pos);
					mparams["color"] = yafray::parameter_t(yafray::colorA_t(cb->data[i].r,
																																cb->data[i].g,
																																cb->data[i].b,
																																cb->data[i].a));
					lparams.push_back(mparams);
				}
				yafrayGate->addShader(params, lparams);
			}
		}

	}

	// If used, textures for the material 'TexFace' case
	if (!imagetex.empty()) {
		for (map<Image*, set<Material*> >::const_iterator imgtex=imagetex.begin();
					imgtex!=imagetex.end();++imgtex)
		{
			// skip if already written above
			if (dupimg.find(imgtex->first)==dupimg.end()) {
				lparams.clear();
				params.clear();
				params["name"] = yafray::parameter_t(imgtex->first->id.name);
				params["type"] = yafray::parameter_t("image");
				string texpath(imgtex->first->name);
				adjustPath(texpath);
				params["filename"] = yafray::parameter_t(texpath);
				yafrayGate->addShader(params, lparams);
			}
		}
	}

}


void yafrayPluginRender_t::writeShader(const string &shader_name, Material* matr, const string &facetexname)
{
	yafray::paramMap_t params;
	list<yafray::paramMap_t> lparams;

	// if material has ramps, export colorbands first
	if (matr->mode & (MA_RAMP_COL|MA_RAMP_SPEC))
	{
		// both colorbands without input shader
		ColorBand* cb = matr->ramp_col;
		if ((matr->mode & MA_RAMP_COL) && (cb!=NULL))
		{
			params["type"] = yafray::parameter_t("colorband");
			params["name"] = yafray::parameter_t(shader_name+"_difframp");
			for (int i=0;i<cb->tot;i++) {
				yafray::paramMap_t mparams;
				mparams["value"] = yafray::parameter_t(cb->data[i].pos);
				mparams["color"] = yafray::parameter_t(yafray::colorA_t(cb->data[i].r, cb->data[i].g, cb->data[i].b, cb->data[i].a));
				lparams.push_back(mparams);
			}
			yafrayGate->addShader(params, lparams);
		}
		cb = matr->ramp_spec;
		if ((matr->mode & MA_RAMP_SPEC) && (cb!=NULL))
		{
			lparams.clear();
			params.clear();
			params["type"] = yafray::parameter_t("colorband");
			params["name"] = yafray::parameter_t(shader_name+"_specramp");
			for (int i=0;i<cb->tot;i++) {
				yafray::paramMap_t mparams;
				mparams["value"] = yafray::parameter_t(cb->data[i].pos);
				mparams["color"] = yafray::parameter_t(yafray::colorA_t(cb->data[i].r, cb->data[i].g, cb->data[i].b, cb->data[i].a));
				lparams.push_back(mparams);
			}
			yafrayGate->addShader(params, lparams);
		}
		lparams.clear();
		params.clear();
	}

	params["type"] = yafray::parameter_t("blendershader");
	params["name"] = yafray::parameter_t(shader_name);
	params["color"] = yafray::parameter_t(yafray::color_t(matr->r, matr->g, matr->b));
	float sr=matr->specr, sg=matr->specg, sb=matr->specb;
	if (matr->spec_shader==MA_SPEC_WARDISO) {
		// ........
		sr /= M_PI;
		sg /= M_PI;
		sb /= M_PI;
	}
	params["specular_color"] = yafray::parameter_t(yafray::color_t(sr, sg, sb));
	params["mirror_color"] = yafray::parameter_t(yafray::color_t(matr->mirr, matr->mirg, matr->mirb));
	params["diffuse_reflect"] = yafray::parameter_t(matr->ref);
	params["specular_amount"] = yafray::parameter_t(matr->spec);
	params["alpha"] = yafray::parameter_t(matr->alpha);
	
	// if no GI used, the GIpower parameter is not always initialized, so in that case ignore it
	float bg_mult = (re->r.GImethod==0) ? 1 : re->r.GIpower;
	params["emit"]=yafray::parameter_t(matr->emit*bg_mult);

	// reflection/refraction
	if ( (matr->mode & MA_RAYMIRROR) || (matr->mode & MA_RAYTRANSP) )
		params["IOR"] = yafray::parameter_t(matr->ang);

	if (matr->mode & MA_RAYMIRROR)
	{
		// Sofar yafray's min_refle parameter (which misleadingly actually controls fresnel reflection offset)
		// has been mapped to Blender's ray_mirror parameter.
		// This causes it be be misinterpreted and misused as a reflection amount control however.
		// Besides that, it also causes extra complications for the yafray Blendershader.
		// So added an actual amount of reflection parameter instead, and another
		// extra parameter 'frsOfs' to actually control fresnel offset (re-uses Blender fresnel_mir_i param).
		params["reflect"] = yafray::parameter_t("on");
		params["reflect_amount"] = yafray::parameter_t(matr->ray_mirror);
		float fo = 1.f-(matr->fresnel_mir_i-1.f)*0.25f;	// blender param range [1,5], also here reversed (1 in Blender -> no fresnel)
		params["fresnel_offset"] = yafray::parameter_t(fo);

		// for backward compatibility, also add old 'reflected' parameter, copy of mirror_color
		params["reflected"] = yafray::parameter_t(yafray::color_t(matr->mirr, matr->mirg, matr->mirb));
		// same for 'min_refle' param. Instead of the ray_mirror parameter that was used before, since now
		// the parameter's function is taken over by the fresnel offset parameter, use that instead.
		params["min_refle"] = yafray::parameter_t(fo);

	}

	if (matr->mode & MA_RAYTRANSP) 
	{
		params["refract"] = yafray::parameter_t("on");
		params["transmit_filter"] = yafray::parameter_t(matr->filter);
		// tir on by default
		params["tir"] = yafray::parameter_t("on");

		// transmit absorption color
		// to make things easier(?) for user it now specifies the actual color at 1 unit / YF_dscale of distance
		const float maxlog = -log(1e-38);
		float ar = (matr->YF_ar>0) ? -log(matr->YF_ar) : maxlog;
		float ag = (matr->YF_ag>0) ? -log(matr->YF_ag) : maxlog;
		float ab = (matr->YF_ab>0) ? -log(matr->YF_ab) : maxlog;
		float sc = matr->YF_dscale;
		if (sc!=0.f) sc=1.f/sc;
		params["absorption"] = yafray::parameter_t(yafray::color_t(ar*sc, ag*sc, ab*sc));
		// dispersion
		params["dispersion_power"] = yafray::parameter_t(matr->YF_dpwr);
		params["dispersion_samples"] = yafray::parameter_t(matr->YF_dsmp);
		params["dispersion_jitter"] = yafray::parameter_t(matr->YF_djit ? "on" : "off");

		// for backward compatibility, also add old 'transmitted' parameter, copy of 'color' * (1-alpha)
		float na = 1.f-matr->alpha;
		params["transmitted"] = yafray::parameter_t(yafray::color_t(matr->r*na, matr->g*na, matr->b*na));
	}

	string Mmode = "";
	if (matr->mode & MA_TRACEBLE) Mmode += "traceable";
	if (matr->mode & MA_SHADOW) Mmode += " shadow";
	if (matr->mode & MA_SHLESS) Mmode += " shadeless";
	if (matr->mode & MA_VERTEXCOL) Mmode += " vcol_light";
	if (matr->mode & MA_VERTEXCOLP) Mmode += " vcol_paint";
	if (matr->mode & MA_ZTRA) Mmode += " ztransp";
	if (matr->mode & MA_ONLYSHADOW) Mmode += " onlyshadow";
	if (Mmode!="") params["matmodes"] = yafray::parameter_t(Mmode);

	// diffuse & specular brdf, lambert/cooktorr defaults
	// diffuse
	if (matr->diff_shader==MA_DIFF_ORENNAYAR) {
		params["diffuse_brdf"] = yafray::parameter_t("oren_nayar");
		params["roughness"] = yafray::parameter_t(matr->roughness);
	}
	else if (matr->diff_shader==MA_DIFF_TOON) {
		params["diffuse_brdf"] = yafray::parameter_t("toon");
		params["toondiffuse_size"] = yafray::parameter_t(matr->param[0]);
		params["toondiffuse_smooth"] = yafray::parameter_t(matr->param[1]);
	}
	else if (matr->diff_shader==MA_DIFF_MINNAERT) {
		params["diffuse_brdf"] = yafray::parameter_t("minnaert");
		params["darkening"] = yafray::parameter_t(matr->darkness);
	}
	else params["diffuse_brdf"] = yafray::parameter_t("lambert");
	// specular
	if (matr->spec_shader==MA_SPEC_PHONG) {
		params["specular_brdf"] = yafray::parameter_t("phong");
		params["hard"] = yafray::parameter_t(matr->har);
	}
	else if (matr->spec_shader==MA_SPEC_BLINN) {
		params["specular_brdf"] = yafray::parameter_t("blinn");
		params["blinn_ior"] = yafray::parameter_t(matr->refrac);
		params["hard"] = yafray::parameter_t(matr->har);
	}
	else if (matr->spec_shader==MA_SPEC_TOON) {
		params["specular_brdf"] = yafray::parameter_t("toon");
		params["toonspecular_size"] = yafray::parameter_t(matr->param[2]);
		params["toonspecular_smooth"] = yafray::parameter_t(matr->param[3]);
	}
	else if (matr->spec_shader==MA_SPEC_WARDISO) {
		params["specular_brdf"] = yafray::parameter_t("ward");
		params["u_roughness"] = yafray::parameter_t(matr->rms);
		params["v_roughness"] = yafray::parameter_t(matr->rms);
	}
	else {
		params["specular_brdf"] = yafray::parameter_t("blender_cooktorr");
		params["hard"] = yafray::parameter_t(matr->har);
	}

	// ramps, if used
	if (matr->mode & (MA_RAMP_COL|MA_RAMP_SPEC))
	{
		const string rm_blend[9] = {"mix", "add", "mul", "sub", "screen", "divide", "difference", "darken", "lighten"};
		const string rm_mode[4] = {"shader", "energy", "normal", "result"};
		// diffuse
		if ((matr->mode & MA_RAMP_COL) && (matr->ramp_col!=NULL))
		{
			params["diffuse_ramp"] = yafray::parameter_t(shader_name+"_difframp");
			params["diffuse_ramp_mode"] = yafray::parameter_t(rm_mode[(int)matr->rampin_col]);
			params["diffuse_ramp_blend"] = yafray::parameter_t(rm_blend[(int)matr->rampblend_col]);
			params["diffuse_ramp_factor"] = yafray::parameter_t(matr->rampfac_col);
		}
		// specular
		if ((matr->mode & MA_RAMP_SPEC) && (matr->ramp_spec!=NULL)) {
			params["specular_ramp"] = yafray::parameter_t(shader_name+"_specramp");
			params["specular_ramp_mode"] = yafray::parameter_t(rm_mode[(int)matr->rampin_spec]);
			params["specular_ramp_blend"] = yafray::parameter_t(rm_blend[(int)matr->rampblend_spec]);
			params["specular_ramp_factor"] = yafray::parameter_t(matr->rampfac_spec);
		}
	}

	// modulators
	// first modulator is the texture of the face, if used (TexFace mode)
	if (facetexname.length()!=0) {
			yafray::paramMap_t mparams;
			mparams["input"] = yafray::parameter_t(facetexname);
			mparams["color"] = yafray::parameter_t(1);
			lparams.push_back(mparams);
	}
	
	for (int m2=0;m2<MAX_MTEX;m2++)
	{
		if (matr->septex & (1<<m2)) continue;// all active channels
		// ignore null mtex
		MTex* mtex = matr->mtex[m2];
		if (mtex==NULL) continue;
		// ignore null tex
		Tex* tex = mtex->tex;
		if (tex==NULL) continue;

		map<string, MTex*>::const_iterator mtexL = used_textures.find(string(tex->id.name));
		if (mtexL!=used_textures.end()) 
		{
			yafray::paramMap_t mparams;
			// when no facetex used, shader_name is created from original material name
			char temp[32];
			sprintf(temp,"_map%d", m2);
			if (facetexname.length()!=0)
				mparams["input"] = yafray::parameter_t(string(matr->id.name) + string(temp));
			else
				mparams["input"] = yafray::parameter_t(shader_name + temp);

			// blendtype, would have been nice if the order would have been the same as for ramps...
			const string blendtype[MTEX_NUM_BLENDTYPES] = {"mix", "mul", "add", "sub", "divide", "darken", "difference", "lighten", "screen", "hue", "sat", "val", "color"};
			mparams["mode"] = yafray::parameter_t(blendtype[(int)mtex->blendtype]);

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
				// for yafray, bump factor is negated (unless tex is stucci, not affected by 'Neg')
				// scaled down quite a bit
				float nf = mtex->norfac;
				if (tex->type!=TEX_STUCCI) nf *= -1.f;
				if (mtex->maptoneg & MAP_NORM) nf *= -1.f;
				mparams["normal"] = yafray::parameter_t(nf/60.f);
			}

			// all blender texture modulation as switches, either 1 or -1 (negative state of button)
			// Csp, specular color modulation
			if (mtex->mapto & MAP_COLSPEC)
				mparams["colspec"] = yafray::parameter_t(1.0);
			// CMir, mirror color  modulation
			if (mtex->mapto & MAP_COLMIR)
				mparams["colmir"] = yafray::parameter_t(1.0);

			// Ref, diffuse reflection amount  modulation
			if ((mtex->mapto & MAP_REF) || (mtex->maptoneg & MAP_REF)) 
			{
				int t = 1;
				if (mtex->maptoneg & MAP_REF) t = -1;
				mparams["difref"] = yafray::parameter_t(t);
			}

			// Spec, specular amount mod
			if ((mtex->mapto & MAP_SPEC) || (mtex->maptoneg & MAP_SPEC)) 
			{
				int t = 1;
				if (mtex->maptoneg & MAP_SPEC) t = -1;
				mparams["specular"] = yafray::parameter_t(t);
			}

			// hardness modulation
			if ((mtex->mapto & MAP_HAR) || (mtex->maptoneg & MAP_HAR)) 
			{
				int t = 1;
				if (mtex->maptoneg & MAP_HAR) t = -1;
				mparams["hard"] = yafray::parameter_t(t);
			}

			// alpha modulation
			if ((mtex->mapto & MAP_ALPHA) || (mtex->maptoneg & MAP_ALPHA)) 
			{
				int t = 1;
				if (mtex->maptoneg & MAP_ALPHA) t = -1;
				mparams["alpha"] = yafray::parameter_t(t);
			}

			// emit modulation
			if ((mtex->mapto & MAP_EMIT) || (mtex->maptoneg & MAP_EMIT)) {
				int t = 1;
				if (mtex->maptoneg & MAP_EMIT) t = -1;
				mparams["emit"] = yafray::parameter_t(t);
			}

			// raymir modulation
			if ((mtex->mapto & MAP_RAYMIRR) || (mtex->maptoneg & MAP_RAYMIRR)) {
				int t = 1;
				if (mtex->maptoneg & MAP_RAYMIRR) t = -1;
				mparams["raymir"] = yafray::parameter_t(t);
			}

			// texture flag, combination of strings
			string ts;
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
				mparams["alpha_flag"] = yafray::parameter_t(ts);
			}

			// image as normalmap flag
			if (tex->imaflag & TEX_NORMALMAP) mparams["normalmap"] = yafray::parameter_t("on");

			lparams.push_back(mparams);
		}
	}
	yafrayGate->addShader(params, lparams);

}

// write all materials & modulators
void yafrayPluginRender_t::writeMaterialsAndModulators()
{
	// shaders/mappers for regular texture (or non-texture) mode
	// In case material has texface mode, and all faces have an image texture,
	// this shader will not be used, but still be written
	yafray::paramMap_t params;
	list<yafray::paramMap_t> lparams;
	for (map<string, Material*>::const_iterator blendmat=used_materials.begin();
		blendmat!=used_materials.end();++blendmat) 
	{
		Material* matr = blendmat->second;
		// mapper(s)
		for (int m=0;m<MAX_MTEX;m++) 
		{
			if (matr->septex & (1<<m)) continue;// all active channels
			// ignore null mtex
			MTex* mtex = matr->mtex[m];
			if (mtex==NULL) continue;
			// ignore null tex
			Tex* tex = mtex->tex;
			if (tex==NULL) continue;

			map<string, MTex*>::const_iterator mtexL = used_textures.find(string(tex->id.name));
			if (mtexL!=used_textures.end()) 
			{
				params.clear();	//!!!
				lparams.clear();
				char temp[32];
				sprintf(temp, "_map%d", m);
				params["type"] = yafray::parameter_t("blendermapper");
				params["name"] = yafray::parameter_t(blendmat->first + string(temp));
				if ((mtex->texco & TEXCO_OBJECT) || (mtex->texco & TEXCO_REFL) || (mtex->texco & TEXCO_NORM))
				{
					// For object, reflection & normal mapping, add the object matrix to the modulator,
					// as in LF script, use camera matrix if no object specified.
					// In this case this means the inverse of that matrix
					float texmat[4][4], itexmat[4][4];
					if ((mtex->texco & TEXCO_OBJECT) && (mtex->object))
						MTC_Mat4CpyMat4(texmat, mtex->object->obmat);
					else	// also for refl. map
						MTC_Mat4CpyMat4(texmat, maincam_obj->obmat);
					MTC_Mat4Invert(itexmat, texmat);
#define flp yafray::parameter_t
					params["m00"]=flp(itexmat[0][0]);  params["m01"]=flp(itexmat[1][0]);
					params["m02"]=flp(itexmat[2][0]);  params["m03"]=flp(itexmat[3][0]);
					params["m10"]=flp(itexmat[0][1]);  params["m11"]=flp(itexmat[1][1]);
					params["m12"]=flp(itexmat[2][1]);  params["m13"]=flp(itexmat[3][1]);
					params["m20"]=flp(itexmat[0][2]);  params["m21"]=flp(itexmat[1][2]);
					params["m22"]=flp(itexmat[2][2]);  params["m23"]=flp(itexmat[3][2]);
					params["m30"]=flp(itexmat[0][3]);  params["m31"]=flp(itexmat[1][3]);
					params["m32"]=flp(itexmat[2][3]);  params["m33"]=flp(itexmat[3][3]);
#undef flp
				}
				// use image name instead of texname when texture is image
				if ((tex->type==TEX_IMAGE) && tex->ima)
					params["input"] = yafray::parameter_t(tex->ima->id.name);
				else if ((tex->flag & TEX_COLORBAND) & (tex->coba!=NULL))
					params["input"] = yafray::parameter_t(mtexL->first + "_coba");
				else
					params["input"] = yafray::parameter_t(mtexL->first);

				// texture size
				params["sizex"] = yafray::parameter_t(mtex->size[0]);
				params["sizey"] = yafray::parameter_t(mtex->size[1]);
				params["sizez"] = yafray::parameter_t(mtex->size[2]);

				// texture offset
				params["ofsx"] = yafray::parameter_t(mtex->ofs[0]);
				params["ofsy"] = yafray::parameter_t(mtex->ofs[1]);
				params["ofsz"] = yafray::parameter_t(mtex->ofs[2]);

				// texture coordinates, have to disable 'sticky' in Blender
				if (mtex->texco & TEXCO_UV)
					params["texco"] = yafray::parameter_t("uv");
				else if ((mtex->texco & TEXCO_GLOB) || (mtex->texco & TEXCO_OBJECT))
					// object mode is also set as global, but the object matrix 
					// was specified above with <modulator..>
					params["texco"] = yafray::parameter_t("global");
				else if ((mtex->texco & TEXCO_ORCO) || (mtex->texco & TEXCO_STRAND))
					// orco flag now used for 'strand'-mapping as well, see mesh code
					params["texco"] = yafray::parameter_t("orco");
				else if (mtex->texco & TEXCO_WINDOW)
					params["texco"] = yafray::parameter_t("window");
				else if (mtex->texco & TEXCO_NORM)
					params["texco"] = yafray::parameter_t("normal");
				else if (mtex->texco & TEXCO_REFL)
					params["texco"] = yafray::parameter_t("reflect");

				// texture projection axes, both image & procedural
				string proj = "nxyz";		// 'n' for 'none'
				params["proj_x"] = yafray::parameter_t(string(1,proj[mtex->projx]));
				params["proj_y"] = yafray::parameter_t(string(1,proj[mtex->projy]));
				params["proj_z"] = yafray::parameter_t(string(1,proj[mtex->projz]));

				// texture mapping parameters only relevant to image type
				if (tex->type==TEX_IMAGE) 
				{
					if (mtex->mapping==MTEX_FLAT)
						params["mapping"] = yafray::parameter_t("flat");
					else if (mtex->mapping==MTEX_CUBE)
						params["mapping"] = yafray::parameter_t("cube");
					else if (mtex->mapping==MTEX_TUBE)
						params["mapping"] = yafray::parameter_t("tube");
					else if (mtex->mapping==MTEX_SPHERE)
						params["mapping"] = yafray::parameter_t("sphere");

					// repeat
					params["xrepeat"] = yafray::parameter_t(tex->xrepeat);
					params["yrepeat"] = yafray::parameter_t(tex->yrepeat);

					// clipping
					if (tex->extend==TEX_EXTEND)
						params["clipping"] = yafray::parameter_t("extend");
					else if (tex->extend==TEX_CLIP)
						params["clipping"] = yafray::parameter_t("clip");
					else if (tex->extend==TEX_CLIPCUBE)
						params["clipping"] = yafray::parameter_t("clipcube");
					else if (tex->extend==TEX_CHECKER) {
						params["clipping"] = yafray::parameter_t("checker");
						string ts = "";
						if (tex->flag & TEX_CHECKER_ODD) ts += "odd";
						if (tex->flag & TEX_CHECKER_EVEN) ts += " even";
						params["checker_mode"] = yafray::parameter_t(ts);
						params["checker_dist"] = yafray::parameter_t(tex->checkerdist);
					}
					else
						params["clipping"] = yafray::parameter_t("repeat");

					// crop min/max
					params["cropmin_x"] = yafray::parameter_t(tex->cropxmin);
					params["cropmin_y"] = yafray::parameter_t(tex->cropymin);
					params["cropmax_x"] = yafray::parameter_t(tex->cropxmax);
					params["cropmax_y"] = yafray::parameter_t(tex->cropymax);

					// rot90 flag
					if (tex->imaflag & TEX_IMAROT) 
						params["rot90"] = yafray::parameter_t("on");
					else
						params["rot90"] = yafray::parameter_t("off");
				}
				yafrayGate->addShader(params, lparams);
			}
		}

		// shader + modulators
		writeShader(blendmat->first, matr);

	}

		// write the mappers & shaders for the TexFace case
	if (!imagetex.empty()) {
		// Yafray doesn't have per-face-textures, only per-face-shaders,
		// so create as many mappers/shaders as the images used by the object
		params.clear();
		lparams.clear();
		int snum = 0;
		for (map<Image*, set<Material*> >::const_iterator imgtex=imagetex.begin();
				imgtex!=imagetex.end();++imgtex)
		{

			for (set<Material*>::const_iterator imgmat=imgtex->second.begin();
					imgmat!=imgtex->second.end();++imgmat)
			{
				Material* matr = *imgmat;
				// mapper
				params["type"] = yafray::parameter_t("blendermapper");
				char temp[32];
				sprintf(temp, "_ftmap%d", snum);
				params["name"] = yafray::parameter_t(string(matr->id.name) + string(temp));
				params["input"] = yafray::parameter_t(imgtex->first->id.name);
				// all yafray default settings, except for texco, so no need to set others
				params["texco"] = yafray::parameter_t("uv");
				yafrayGate->addShader(params, lparams);

				// shader, remember name, used later when writing per-face-shaders
				sprintf(temp, "_ftsha%d", snum);
				string shader_name = string(matr->id.name) + string(temp);
				imgtex_shader[string(matr->id.name) + string(imgtex->first->id.name)] = shader_name;

				sprintf(temp, "_ftmap%d", snum++);
				string facetexname = string(matr->id.name) + string(temp);
				writeShader(shader_name, matr, facetexname);
			}

		}
	}

}

void yafrayPluginRender_t::genUVcoords(vector<yafray::GFLOAT> &uvcoords, ObjectRen *obr, VlakRen *vlr, MTFace* uvc, bool comple)
{
	if (uvc) 
	{
		// tri uv split indices
		int ui1=0, ui2=1, ui3=2;
		if (vlr->flag & R_DIVIDE_24) {
			ui3++;
			if (vlr->flag & R_FACE_SPLIT) { ui1++;  ui2++; }
		}
		else if (vlr->flag & R_FACE_SPLIT) { ui2++;  ui3++; }
		if (comple) {
			ui1 = (ui1+2) & 3;
			ui2 = (ui2+2) & 3;
			ui3 = (ui3+2) & 3;
		}
		uvcoords.push_back(uvc->uv[ui1][0]);  uvcoords.push_back(1-uvc->uv[ui1][1]);
		uvcoords.push_back(uvc->uv[ui2][0]);  uvcoords.push_back(1-uvc->uv[ui2][1]);
		uvcoords.push_back(uvc->uv[ui3][0]);  uvcoords.push_back(1-uvc->uv[ui3][1]);
	}
	else
	{
		uvcoords.push_back(0);  uvcoords.push_back(0);
		uvcoords.push_back(0);  uvcoords.push_back(0);
		uvcoords.push_back(0);  uvcoords.push_back(0);
	}
}

void yafrayPluginRender_t::genVcol(vector<yafray::CFLOAT> &vcol, ObjectRen *obr, VlakRen *vlr, bool comple)
{
	MCol *mcol= RE_vlakren_get_mcol(obr, vlr, obr->actmcol, NULL, 0);

	if (mcol)
	{
		// tri vcol split indices
		int ui1=0, ui2=1, ui3=2;
		if (vlr->flag & R_DIVIDE_24) {
			ui3++;
			if (vlr->flag & R_FACE_SPLIT) { ui1++;  ui2++; }
		}
		else if (vlr->flag & R_FACE_SPLIT) { ui2++;  ui3++; }
		if (comple) {
			ui1 = (ui1+2) & 3;
			ui2 = (ui2+2) & 3;
			ui3 = (ui3+2) & 3;
		}
		unsigned char* pt = reinterpret_cast<unsigned char*>(&mcol[ui1]);
		vcol.push_back((float)pt[3]/255.f);  vcol.push_back((float)pt[2]/255.f);  vcol.push_back((float)pt[1]/255.f);
		pt = reinterpret_cast<unsigned char*>(&mcol[ui2]);
		vcol.push_back((float)pt[3]/255.f);  vcol.push_back((float)pt[2]/255.f);  vcol.push_back((float)pt[1]/255.f);
		pt = reinterpret_cast<unsigned char*>(&mcol[ui3]);
		vcol.push_back((float)pt[3]/255.f);  vcol.push_back((float)pt[2]/255.f);  vcol.push_back((float)pt[1]/255.f);
	}
	else
	{
		vcol.push_back(0);  vcol.push_back(0);  vcol.push_back(0);
		vcol.push_back(0);  vcol.push_back(0);  vcol.push_back(0);
		vcol.push_back(0);  vcol.push_back(0);  vcol.push_back(0);
	}
}

void yafrayPluginRender_t::genFace(vector<int> &faces,vector<string> &shaders,vector<int> &faceshader,
														vector<yafray::GFLOAT> &uvcoords,vector<yafray::CFLOAT> &vcol,
														map<VertRen*, int> &vert_idx,ObjectRen *obr,VlakRen *vlr,
														int has_orco,bool has_uv)
{
	Material* fmat = vlr->mat;
	bool EXPORT_VCOL = ((fmat->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))!=0);
	string fmatname(fmat->id.name);
	// use name in imgtex_shader list if 'TexFace' enabled for this face material
	if (fmat->mode & MA_FACETEXTURE) {
		MTFace* tface = RE_vlakren_get_tface(obr, vlr, obr->actmtface, NULL, 0);
		if (tface) {
			Image* fimg = (Image*)tface->tpage;
			if (fimg) fmatname = imgtex_shader[fmatname + string(fimg->id.name)];
		}
	}
	else if (fmatname.length()==0) fmatname = "blender_default";
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

	MTFace* uvc = RE_vlakren_get_tface(obr, vlr, obr->actmtface, NULL, 0); // possible uvcoords (v upside down)
	int idx1, idx2, idx3;

	idx1 = vert_idx.find(vlr->v1)->second;
	idx2 = vert_idx.find(vlr->v2)->second;
	idx3 = vert_idx.find(vlr->v3)->second;

	// make sure the indices point to the vertices when orco coords exported
	if (has_orco) { idx1*=2;  idx2*=2;  idx3*=2; }

	faces.push_back(idx1);  faces.push_back(idx2);  faces.push_back(idx3);

	if(has_uv) genUVcoords(uvcoords, obr, vlr, uvc);
	if (EXPORT_VCOL) genVcol(vcol, obr, vlr);
}

void yafrayPluginRender_t::genCompleFace(vector<int> &faces,/*vector<string> &shaders,*/vector<int> &faceshader,
														vector<yafray::GFLOAT> &uvcoords,vector<yafray::CFLOAT> &vcol,
														map<VertRen*, int> &vert_idx,ObjectRen *obr,VlakRen *vlr,
														int has_orco,bool has_uv)
{
	Material* fmat = vlr->mat;
	bool EXPORT_VCOL = ((fmat->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))!=0);

	faceshader.push_back(faceshader.back());
	MTFace* uvc = RE_vlakren_get_tface(obr, vlr, obr->actmtface, NULL, 0); // possible uvcoords (v upside down)
	int idx1, idx2, idx3;
	idx1 = vert_idx.find(vlr->v3)->second;
	idx2 = vert_idx.find(vlr->v4)->second;
	idx3 = vert_idx.find(vlr->v1)->second;

	// make sure the indices point to the vertices when orco coords exported
	if (has_orco) { idx1*=2;  idx2*=2;  idx3*=2; }

	faces.push_back(idx1);  faces.push_back(idx2);  faces.push_back(idx3);

	if (has_uv) genUVcoords(uvcoords, obr, vlr, uvc, true);
	if (EXPORT_VCOL) genVcol(vcol, obr, vlr, true);
}

void yafrayPluginRender_t::genVertices(vector<yafray::point3d_t> &verts, int &vidx,
																			 map<VertRen*, int> &vert_idx, ObjectRen *obr, VlakRen* vlr, int has_orco, Object* obj)
{
	VertRen* ver;
	float tvec[3];	// for back2world transform

	// for deformed objects, object->imat is no longer valid,
	// so have to create inverse render matrix ourselves here
	float mat[4][4], imat[4][4];
	MTC_Mat4MulMat4(mat, obj->obmat, re->viewmat);
	MTC_Mat4Invert(imat, mat);

	if (vert_idx.find(vlr->v1)==vert_idx.end()) 
	{
		vert_idx[vlr->v1] = vidx++;
		ver = vlr->v1;
		MTC_cp3Float(ver->co, tvec);
		MTC_Mat4MulVecfl(imat, tvec);
		verts.push_back(yafray::point3d_t(tvec[0], tvec[1], tvec[2]));
		// has_orco now an int, if 1 -> strand mapping, if 2 -> normal orco mapping
		if (has_orco==1)
			verts.push_back(yafray::point3d_t(ver->accum));
		else if (has_orco==2)
			verts.push_back(yafray::point3d_t(ver->orco[0], ver->orco[1], ver->orco[2]));
	}
	if (vert_idx.find(vlr->v2)==vert_idx.end()) 
	{
		vert_idx[vlr->v2] = vidx++;
		ver = vlr->v2;
		MTC_cp3Float(ver->co, tvec);
		MTC_Mat4MulVecfl(imat, tvec);
		verts.push_back(yafray::point3d_t(tvec[0], tvec[1], tvec[2]));
		// has_orco now an int, if 1 -> strand mapping, if 2 -> normal orco mapping
		if (has_orco==1)
			verts.push_back(yafray::point3d_t(ver->accum));
		else if (has_orco==2)
			verts.push_back(yafray::point3d_t(ver->orco[0], ver->orco[1], ver->orco[2]));
	}
	if (vert_idx.find(vlr->v3)==vert_idx.end()) 
	{
		vert_idx[vlr->v3] = vidx++;
		ver = vlr->v3;
		MTC_cp3Float(ver->co, tvec);
		MTC_Mat4MulVecfl(imat, tvec);
		verts.push_back(yafray::point3d_t(tvec[0], tvec[1], tvec[2]));
		// has_orco now an int, if 1 -> strand mapping, if 2 -> normal orco mapping
		if (has_orco==1)
			verts.push_back(yafray::point3d_t(ver->accum));
		else if (has_orco==2)
			verts.push_back(yafray::point3d_t(ver->orco[0], ver->orco[1], ver->orco[2]));
	}
	if ((vlr->v4) && (vert_idx.find(vlr->v4)==vert_idx.end())) 
	{
		vert_idx[vlr->v4] = vidx++;
		ver = vlr->v4;
		MTC_cp3Float(ver->co, tvec);
		MTC_Mat4MulVecfl(imat, tvec);
		verts.push_back(yafray::point3d_t(tvec[0], tvec[1], tvec[2]));
		// has_orco now an int, if 1 -> strand mapping, if 2 -> normal orco mapping
		if (has_orco==1)
			verts.push_back(yafray::point3d_t(ver->accum));
		else if (has_orco==2)
			verts.push_back(yafray::point3d_t(ver->orco[0], ver->orco[1], ver->orco[2]));
	}
}

void yafrayPluginRender_t::writeObject(Object* obj, ObjectRen *obr, const vector<VlakRen*> &VLR_list, const float obmat[4][4])
{
	float mtr[4*4];
	mtr[0*4+0]=obmat[0][0];  mtr[0*4+1]=obmat[1][0];  mtr[0*4+2]=obmat[2][0];  mtr[0*4+3]=obmat[3][0];
	mtr[1*4+0]=obmat[0][1];  mtr[1*4+1]=obmat[1][1];  mtr[1*4+2]=obmat[2][1];  mtr[1*4+3]=obmat[3][1];
	mtr[2*4+0]=obmat[0][2];  mtr[2*4+1]=obmat[1][2];  mtr[2*4+2]=obmat[2][2];  mtr[2*4+3]=obmat[3][2];
	mtr[3*4+0]=obmat[0][3];  mtr[3*4+1]=obmat[1][3];  mtr[3*4+2]=obmat[2][3];  mtr[3*4+3]=obmat[3][3];
	yafrayGate->transformPush(mtr);
	
	VlakRen* face0 = VLR_list[0];
	Material* face0mat = face0->mat;
	
	bool castShadows = face0mat->mode & MA_TRACEBLE;
	float caus_IOR=1.0;
	yafray::color_t caus_tcolor(0.0, 0.0, 0.0), caus_rcolor(0.0, 0.0, 0.0);
	bool caus = (((face0->mat->mode & MA_RAYTRANSP) | (face0->mat->mode & MA_RAYMIRROR))!=0);
	if (caus) {
		caus_IOR = face0mat->ang;
		float tr = 1.0-face0mat->alpha;
		caus_tcolor.set(face0mat->r*tr, face0mat->g*tr, face0mat->b*tr);
		tr = face0mat->ray_mirror;
		caus_rcolor.set(face0mat->mirr*tr, face0mat->mirg*tr, face0mat->mirb*tr);
	}

	// Export orco coords test.
	// Previously was done by checking orco pointer, however this can be non-null but still not initialized.
	// Test the rendermaterial texco flag instead.
	// update2: bug #3193 it seems it has changed again with the introduction of static 'hair' particles,
	// now it uses the vert pointer again as an extra test to make sure there are orco coords available
	int has_orco = 0;
	if (face0mat->texco & TEXCO_STRAND)
		has_orco = 1;
	else
		has_orco = (((face0mat->texco & TEXCO_ORCO)!=0) && (face0->v1->orco!=NULL)) ? 2 : 0;

	bool no_auto = true;	//in case non-mesh, or mesh has no autosmooth
	float sm_angle = 0.1f;
	if (obj->type==OB_MESH) 
	{
		Mesh* mesh = (Mesh*)obj->data;
		if (mesh->flag & ME_AUTOSMOOTH) {
			sm_angle = mesh->smoothresh;
			no_auto = false;
		}
	}
	// this for non-mesh as well
	if (no_auto) {
		// no per face smooth flag in yafray, if AutoSmooth not used, 
		// use smooth flag of the first face instead
		if (face0->flag & ME_SMOOTH) sm_angle=180;
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
		genVertices(verts, vidx, vert_idx, obr, vlr, has_orco, obj);
		if(RE_vlakren_get_tface(obr, vlr, obr->actmtface, NULL, 0)) has_uv=true;
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
		genFace(faces, shaders, faceshader, uvcoords, vcol, vert_idx, obr, vlr, has_orco, has_uv);
		if (vlr->v4) 
			genCompleFace(faces, faceshader, uvcoords, vcol, vert_idx, obr, vlr, has_orco, has_uv);
	}

	// using the ObjectRen database, contruct a new name if object has a parent.
	// This is done to prevent name clashes (group/library link related)
	string obname(obj->id.name);
	// previous implementation, keep around, in case this is still useful
	//if (obj->id.flag & (LIB_EXTERN|LIB_INDIRECT))obname = "lib_" + obname;
	ObjectRen *obren;
	for (obren = static_cast<ObjectRen*>(re->objecttable.first);
	     obren; obren=static_cast<ObjectRen*>(obren->next))
	{
		Object *db_ob = obren->ob, *db_par = obren->par;
		if (db_ob==obj)
			if ((db_ob!=NULL) && (db_par!=NULL)) {
				obname += "_" + string(db_par->id.name);
				break;
			}
	}

	yafrayGate->addObject_trimesh(obname, verts, faces, uvcoords, vcol,
			shaders, faceshader, sm_angle, castShadows, true, true, caus, has_orco,
			caus_rcolor, caus_tcolor, caus_IOR);
	yafrayGate->transformPop();
}


// write all objects
void yafrayPluginRender_t::writeAllObjects()
{
	// first all objects except dupliverts (and main instance object for dups)
	for (map<Object*, yafrayObjectRen >::const_iterator obi=all_objects.begin();
			obi!=all_objects.end(); ++obi)
	{
	  // skip main duplivert object if in dupliMtx_list, written later
		Object* obj = obi->first;
		if (dupliMtx_list.find(string(obj->id.name))!=dupliMtx_list.end()) continue;
		writeObject(obj, obi->second.obr, obi->second.faces, obj->obmat);
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
		writeObject(obj, all_objects[obj].obr, all_objects[obj].faces, obmat);

		// all others instances of first
		for (unsigned int curmtx=16;curmtx<dupMtx->second.size();curmtx+=16) 
		{	// number of 4x4 matrices
			// new mtx
			for (int i=0;i<4;i++)
				for (int j=0;j<4;j++)
					nmat[i][j] = dupMtx->second[curmtx+(i<<2)+j];

			MTC_Mat4MulMat4(cmat, imat, nmat);	// transform with respect to original = inverse_original * new

			float mtr[4*4];
			mtr[0*4+0]=cmat[0][0];  mtr[0*4+1]=cmat[1][0];  mtr[0*4+2]=cmat[2][0];  mtr[0*4+3]=cmat[3][0];
			mtr[1*4+0]=cmat[0][1];  mtr[1*4+1]=cmat[1][1];  mtr[1*4+2]=cmat[2][1];  mtr[1*4+3]=cmat[3][1];
			mtr[2*4+0]=cmat[0][2];  mtr[2*4+1]=cmat[1][2];  mtr[2*4+2]=cmat[2][2];  mtr[2*4+3]=cmat[3][2];
			mtr[3*4+0]=cmat[0][3];  mtr[3*4+1]=cmat[1][3];  mtr[3*4+2]=cmat[2][3];  mtr[3*4+3]=cmat[3][3];
			yafrayGate->transformPush(mtr);

			// new name from original
			string name=(obj->id.name);
			char temp[16];
			sprintf(temp,"_dup%d",(curmtx>>4));
			name+=temp;
			yafrayGate->addObject_reference(name,obj->id.name);
			yafrayGate->transformPop();
		}

	}
}

void yafrayPluginRender_t::writeAreaLamp(LampRen* lamp, int num, float iview[4][4])
{
	yafray::paramMap_t params;
	
	if (lamp->area_shape!=LA_AREA_SQUARE) return;
	float *a=lamp->area[0], *b=lamp->area[1], *c=lamp->area[2], *d=lamp->area[3];
	float power=lamp->energy;
	
	string md = "off";
	// if no GI used, the GIphotons flag can still be set, so only use when 'full' selected
	if ((re->r.GImethod==2) && (re->r.GIphotons)) { md="on";  power*=re->r.GIpower; }
	params["type"]=yafray::parameter_t("arealight");
	char temp[16];
	sprintf(temp,"LAMP%d",num+1);
	params["name"]=yafray::parameter_t(temp);
	params["dummy"]=yafray::parameter_t(md);
	params["power"]=yafray::parameter_t(power);
	// samples not used for GI with photons, can still be exported, is ignored
	int psm=0, sm = lamp->ray_totsamp;
	if (sm>=25) psm = sm/5;
	params["samples"]=yafray::parameter_t(sm);
	params["psamples"]=yafray::parameter_t(psm);
	
	// transform area lamp coords back to world
	float lpco[4][3];
	MTC_cp3Float(a, lpco[0]);
	MTC_Mat4MulVecfl(iview, lpco[0]);
	MTC_cp3Float(b, lpco[1]);
	MTC_Mat4MulVecfl(iview, lpco[1]);
	MTC_cp3Float(c, lpco[2]);
	MTC_Mat4MulVecfl(iview, lpco[2]);
	MTC_cp3Float(d, lpco[3]);
	MTC_Mat4MulVecfl(iview, lpco[3]);	
	params["a"] = yafray::parameter_t(yafray::point3d_t(lpco[0][0], lpco[0][1], lpco[0][2]));
	params["b"] = yafray::parameter_t(yafray::point3d_t(lpco[1][0], lpco[1][1], lpco[1][2]));
	params["c"] = yafray::parameter_t(yafray::point3d_t(lpco[2][0], lpco[2][1], lpco[2][2]));
	params["d"] = yafray::parameter_t(yafray::point3d_t(lpco[3][0], lpco[3][1], lpco[3][2]));
	
	params["color"]=yafray::parameter_t(yafray::color_t(lamp->r,lamp->g,lamp->b));
	yafrayGate->addLight(params);
}

void yafrayPluginRender_t::writeLamps()
{
	GroupObject *go;
	int i=0;
	
	// inver viewmatrix needed for back2world transform
	float iview[4][4];
	// re->viewinv != inv.re->viewmat because of possible ortho mode (see convertBlenderScene.c)
	// have to invert it here
	MTC_Mat4Invert(iview, re->viewmat);

	// all lamps
	for(go=(GroupObject *)re->lights.first; go; go= go->next, i++)
	{
		LampRen* lamp = (LampRen *)go->lampren;
		
		yafray::paramMap_t params;
		string type="";
		
		if (lamp->type==LA_AREA) { writeAreaLamp(lamp, i, iview);  continue; }
		
		// TODO: add decay setting in yafray
		bool is_softL=false, is_sphereL=false;
		if (lamp->type==LA_LOCAL) {
			if (lamp->mode & LA_YF_SOFT) {
				// shadowmapped omnidirectional light
				params["type"] = yafray::parameter_t("softlight");
				is_softL = true;
			}
			else if ((lamp->mode & LA_SHAD_RAY) && (lamp->YF_ltradius>0.0)) {
				// area sphere, only when ray shadows enabled and radius>0.0
				params["type"] = yafray::parameter_t("spherelight");
				is_sphereL = true;
			}
			else params["type"] = yafray::parameter_t("pointlight");
			params["glow_intensity"] = yafray::parameter_t(lamp->YF_glowint);
			params["glow_offset"] = yafray::parameter_t(lamp->YF_glowofs);
			params["glow_type"] = yafray::parameter_t(lamp->YF_glowtype);
		}
		else if (lamp->type==LA_SPOT)
			params["type"] = yafray::parameter_t("spotlight");
		else if ((lamp->type==LA_SUN) || (lamp->type==LA_HEMI))	// hemi exported as sun
			params["type"] = yafray::parameter_t("sunlight");
		else if (lamp->type==LA_YF_PHOTON)
			params["type"] = yafray::parameter_t("photonlight");
		else {
			// possibly unknown type, ignore
			cout << "Unknown Blender lamp type: " << lamp->type << endl;
			continue;
		}
		
		//no name available here, create one
		char temp[16];
		sprintf(temp,"LAMP%d",i+1);
		params["name"] = yafray::parameter_t(temp);

		// color already premultiplied by energy, so only need distance here
		float pwr = 1;	// default for sun/hemi, distance irrelevant
		if ((lamp->type!=LA_SUN) && (lamp->type!=LA_HEMI)) {
			if (lamp->mode & LA_SPHERE) {
				// best approx. as used in LFexport script (LF d.f.m. 4pi?)
				pwr = lamp->dist*(lamp->dist+1)*(0.25/M_PI);
				//decay = 2;
			}
			else {
				pwr = lamp->dist;
				//decay = 1;
			}
		}

		if (is_sphereL) {
			// 'dummy' mode for spherelight when used with gpm
			string md = "off";
			// if no GI used, the GIphotons flag can still be set, so only use when 'full' selected
			if ((re->r.GImethod==2) && (re->r.GIphotons)) { md="on";  pwr*=re->r.GIpower; }
			params["power"] = yafray::parameter_t(pwr);
			params["dummy"] = yafray::parameter_t(md);
		}
		else params["power"] = yafray::parameter_t(pwr);
		
		// cast_shadows flag not used with softlight, spherelight or photonlight
		if ((!is_softL) && (!is_sphereL) && (lamp->type!=LA_YF_PHOTON)) {
			string lpmode="off";
			// Blender hemilights exported as sunlights which might have shadow flag set
			// should have cast_shadows set to off (reported by varuag)
			if (lamp->type!=LA_HEMI) {
				if (re->r.mode & R_SHADOW) {
					// old bug was here since the yafray lamp settings panel was added,
					// blender spotlight shadbuf flag should be ignored, since it is not in the panel anymore
					if (lamp->mode & LA_SHAD_RAY) lpmode="on";
				}
			}
			params["cast_shadows"] = yafray::parameter_t(lpmode);
		}
		
		// spot specific stuff
		bool has_halo = ((lamp->type==LA_SPOT) && (lamp->mode & LA_HALO) && (lamp->haint>0.0));
		if (lamp->type==LA_SPOT) {
			// conversion already changed spotsize to cosine of half angle
			float ld = 1-lamp->spotsi;	//convert back to blender slider setting
			if (ld!=0) ld = 1.f/ld;
			params["size"] = yafray::parameter_t(acos(lamp->spotsi)*180.0/M_PI);
			params["blend"] = yafray::parameter_t(lamp->spotbl*ld);
			params["beam_falloff"] = yafray::parameter_t(2.0);
			// halo params
			if (has_halo) {
				params["halo"] = yafray::parameter_t("on");
				params["res"] = yafray::parameter_t(lamp->YF_bufsize);
				int hsmp = ((12-lamp->shadhalostep)*16)/12;
				hsmp = (hsmp+1)*16;	// makes range (16, 272) for halostep(12, 0), good enough?
				// halo 'samples' now 'stepsize'
				// convert from old integer samples value to some reasonable stepsize
				params["stepsize"] = yafray::parameter_t(1.0/sqrt((float)hsmp));
				params["shadow_samples"] = yafray::parameter_t(lamp->samp*lamp->samp);
				params["halo_blur"] = yafray::parameter_t(0.0);
				params["shadow_blur"] = yafray::parameter_t(lamp->soft*0.01f);
				params["fog_density"] = yafray::parameter_t(lamp->haint*0.2f);
			}
		}
		else if (is_softL) {
			// softlight
			params["res"] = yafray::parameter_t(lamp->YF_bufsize);
			params["radius"] = yafray::parameter_t(lamp->soft);
			params["bias"] = yafray::parameter_t(lamp->bias);
		}
		else if (is_sphereL) {
			// spherelight
			int psm=0, sm = lamp->ray_samp*lamp->ray_samp;
			if (sm>=25) psm = sm/5;
			params["radius"] = yafray::parameter_t(lamp->YF_ltradius);
			params["samples"] = yafray::parameter_t(sm);
			params["psamples"] = yafray::parameter_t(psm);
			params["qmc_method"] = yafray::parameter_t(1);
		}
		else if (lamp->type==LA_YF_PHOTON) {
			string qmc="off";
			if (lamp->YF_useqmc) qmc="on";
			params["photons"] = yafray::parameter_t(lamp->YF_numphotons);
			params["search"] = yafray::parameter_t(lamp->YF_numsearch);
			params["depth"] = yafray::parameter_t(lamp->YF_phdepth);
			params["use_QMC"] = yafray::parameter_t(qmc);
			params["angle"] = yafray::parameter_t(acos(lamp->spotsi)*180.0/M_PI);
			float cl = lamp->YF_causticblur/sqrt((float)lamp->YF_numsearch);
			params["fixedradius"] = yafray::parameter_t(lamp->YF_causticblur);
			params["cluster"] = yafray::parameter_t(cl);
		}

		// transform lamp co & vec back to world
		float lpco[3], lpvec[3];
		MTC_cp3Float(lamp->co, lpco);
		MTC_Mat4MulVecfl(iview, lpco);
		MTC_cp3Float(lamp->vec, lpvec);
		MTC_Mat4Mul3Vecfl(iview, lpvec);

		// position, (==-blendir for sun/hemi)
		if ((lamp->type==LA_SUN) || (lamp->type==LA_HEMI))
			params["from"] = yafray::parameter_t(yafray::point3d_t(-lpvec[0], -lpvec[1], -lpvec[2]));
		else
			params["from"] = yafray::parameter_t(yafray::point3d_t(lpco[0], lpco[1], lpco[2]));
		// 'to' for spot/photonlight, already calculated by Blender
		if ((lamp->type==LA_SPOT) || (lamp->type==LA_YF_PHOTON)) {
			params["to"] = yafray::parameter_t(yafray::point3d_t(lpco[0] + lpvec[0],
																													 lpco[1] + lpvec[1],
																													 lpco[2] + lpvec[2]));
			if (has_halo) params["fog"] = yafray::parameter_t(yafray::color_t(1.0, 1.0, 1.0));
		}
		
		// color
		// rgb in LampRen is premultiplied by energy, power is compensated for that above
		params["color"] = yafray::parameter_t(yafray::color_t(lamp->r, lamp->g, lamp->b));
		yafrayGate->addLight(params);
	}
}

// write main camera
void yafrayPluginRender_t::writeCamera()
{
	yafray::paramMap_t params;
	params["name"]=yafray::parameter_t("MAINCAM");
	if (re->r.mode & R_ORTHO)
		params["type"] = yafray::parameter_t("ortho");
	else
		params["type"] = yafray::parameter_t("perspective");
	
	params["resx"] = yafray::parameter_t(re->winx);
	params["resy"] = yafray::parameter_t(re->winy);

	float f_aspect = 1;
	if ((re->winx * re->r.xasp) <= (re->winy * re->r.yasp))
		f_aspect = float(re->winx * re->r.xasp) / float(re->winy * re->r.yasp);
	params["focal"] = yafray::parameter_t(mainCamLens/(f_aspect*32.f));
	// bug #4532, when field rendering is enabled, ycor is doubled
	if (re->r.mode & R_FIELDS)
		params["aspect_ratio"] = yafray::parameter_t(re->ycor * 0.5f);
	else
		params["aspect_ratio"] = yafray::parameter_t(re->ycor);

	// dof params, only valid for real camera
	float fdist = 1;	// only changes for ortho
	if (maincam_obj->type==OB_CAMERA) {
		Camera* cam = (Camera*)maincam_obj->data;
		if (re->r.mode & R_ORTHO) fdist = cam->ortho_scale*(mainCamLens/32.f);
		params["dof_distance"] = yafray::parameter_t(cam->YF_dofdist);
		params["aperture"] = yafray::parameter_t(cam->YF_aperture);
		if (cam->flag & CAM_YF_NO_QMC)
			params["use_qmc"] = yafray::parameter_t("off");
		else
			params["use_qmc"] = yafray::parameter_t("on");
		// bokeh params
		string st = "disk1";
		if (cam->YF_bkhtype==1)
			st = "disk2";
		else if (cam->YF_bkhtype==2)
			st = "triangle";
		else if (cam->YF_bkhtype==3)
			st = "square";
		else if (cam->YF_bkhtype==4)
			st = "pentagon";
		else if (cam->YF_bkhtype==5)
			st = "hexagon";
		else if (cam->YF_bkhtype==6)
			st = "ring";
		params["bokeh_type"] = yafray::parameter_t(st);
		st = "uniform";
		if (cam->YF_bkhbias==1)
			st = "center";
		else if (cam->YF_bkhbias==2)
			st = "edge";
		params["bokeh_bias"] = yafray::parameter_t(st);
		params["bokeh_rotation"] = yafray::parameter_t(cam->YF_bkhrot);
	}

	params["from"]=yafray::parameter_t(
			yafray::point3d_t(maincam_obj->obmat[3][0], maincam_obj->obmat[3][1], maincam_obj->obmat[3][2]));
	params["to"]=yafray::parameter_t(
			yafray::point3d_t(maincam_obj->obmat[3][0] - fdist * re->viewmat[0][2],
												maincam_obj->obmat[3][1] - fdist * re->viewmat[1][2],
												maincam_obj->obmat[3][2] - fdist * re->viewmat[2][2]));
	params["up"]=yafray::parameter_t(
			yafray::point3d_t(maincam_obj->obmat[3][0] + re->viewmat[0][1],
												maincam_obj->obmat[3][1] + re->viewmat[1][1],
												maincam_obj->obmat[3][2] + re->viewmat[2][1]));

	yafrayGate->addCamera(params);
}

void yafrayPluginRender_t::writeHemilight()
{
	yafray::paramMap_t params;
	World *world = G.scene->world;
	bool fromAO = false;
	if (re->r.GIquality==6){
		// use Blender AO params is possible
		if (world==NULL) return;
		if ((world->mode & WO_AMB_OCC)==0) {
			// no AO, use default GIquality
			cout << "[Warning]: Can't use AO parameters\nNo ambient occlusion enabled, using default values instead" << endl;
		}
		else fromAO = true;
	}
	if (re->r.GIcache) {
		params["type"] = yafray::parameter_t("pathlight");
		params["name"] = yafray::parameter_t("path_LT");
		params["power"] = yafray::parameter_t(re->r.GIpower);
		params["mode"] = yafray::parameter_t("occlusion");
		params["ignore_bumpnormals"] = yafray::parameter_t(re->r.YF_nobump ? "on" : "off");
		if (fromAO) {
			// for AO, with cache, using range of 32*1 to 32*16 seems good enough
			params["samples"] = yafray::parameter_t(32*world->aosamp);
			params["maxdistance"] = yafray::parameter_t(world->aodist);
		}
		else {
			switch (re->r.GIquality)
			{
				case 1 : params["samples"] = yafray::parameter_t(128);  break;
				case 2 : params["samples"] = yafray::parameter_t(256);  break;
				case 3 : params["samples"] = yafray::parameter_t(512);  break;
				case 4 : params["samples"] = yafray::parameter_t(1024); break;
				case 5 : params["samples"] = yafray::parameter_t(2048); break;
				default: params["samples"] = yafray::parameter_t(256);
			}
		}
		params["cache"] = yafray::parameter_t("on");
		params["use_QMC"] = yafray::parameter_t("on");
		params["threshold"] = yafray::parameter_t(re->r.GIrefinement);
		params["cache_size"] = yafray::parameter_t((2.0/float(re->winx))*re->r.GIpixelspersample);
		params["shadow_threshold"] = yafray::parameter_t(1.0 - re->r.GIshadowquality);
		params["grid"] = yafray::parameter_t(82);
		params["search"] = yafray::parameter_t(35);
	}
	else {
		params["type"] = yafray::parameter_t("hemilight");
		params["name"] = yafray::parameter_t("hemi_LT");
		params["power"] = yafray::parameter_t(re->r.GIpower);
		if (fromAO) {
			// use minimum of 4 samples for lowest sample setting, single sample way too noisy
			params["samples"] = yafray::parameter_t(3 + world->aosamp*world->aosamp);
			params["maxdistance"] = yafray::parameter_t(world->aodist);
			params["use_QMC"] = yafray::parameter_t((world->aomode & WO_AORNDSMP) ? "off" : "on");
		}
		else {
			switch (re->r.GIquality)
			{
				case 1 :
				case 2 : params["samples"]=yafray::parameter_t(16);  break;
				case 3 : params["samples"]=yafray::parameter_t(36);  break;
				case 4 : params["samples"]=yafray::parameter_t(64);  break;
				case 5 : params["samples"]=yafray::parameter_t(128);  break;
				default: params["samples"]=yafray::parameter_t(25);
			}
		}
	}
	yafrayGate->addLight(params);
}

void yafrayPluginRender_t::writePathlight()
{
	if (re->r.GIphotons)
	{
		yafray::paramMap_t params;
		params["type"] = yafray::parameter_t("globalphotonlight");
		params["name"] = yafray::parameter_t("gpm");
		params["photons"] = yafray::parameter_t(re->r.GIphotoncount);
		params["radius"] = yafray::parameter_t(re->r.GIphotonradius);
		params["depth"] = yafray::parameter_t(((re->r.GIdepth>2) ? (re->r.GIdepth-1) : 1));
		params["caus_depth"] = yafray::parameter_t(re->r.GIcausdepth);
		params["search"] = yafray::parameter_t(re->r.GImixphotons);
		yafrayGate->addLight(params);
	}
	yafray::paramMap_t params;
	params["type"] = yafray::parameter_t("pathlight");
	params["name"] = yafray::parameter_t("path_LT");
	params["power"] = yafray::parameter_t(re->r.GIindirpower);
	params["depth"] = yafray::parameter_t(((re->r.GIphotons) ? 1 : re->r.GIdepth));
	params["caus_depth"] = yafray::parameter_t(re->r.GIcausdepth);
	if (re->r.GIdirect && re->r.GIphotons) params["direct"] = yafray::parameter_t("on");
	if (re->r.GIcache && !(re->r.GIdirect && re->r.GIphotons))
	{
		switch (re->r.GIquality)
		{
			case 1 : params["samples"] = yafray::parameter_t(128);  break;
			case 2 : params["samples"] = yafray::parameter_t(256);  break;
			case 3 : params["samples"] = yafray::parameter_t(512);  break;
			case 4 : params["samples"] = yafray::parameter_t(1024); break;
			case 5 : params["samples"] = yafray::parameter_t(2048); break;
			default: params["samples"] = yafray::parameter_t(256);
		}
		params["cache"] = yafray::parameter_t("on");
		params["use_QMC"] = yafray::parameter_t("on");
		params["threshold"] = yafray::parameter_t(re->r.GIrefinement);
		params["cache_size"] = yafray::parameter_t((2.0/float(re->recty))*re->r.GIpixelspersample);
		params["shadow_threshold"] = yafray::parameter_t(1.0 - re->r.GIshadowquality);
		params["grid"] = yafray::parameter_t(82);
		params["search"] = yafray::parameter_t(35);
		params["ignore_bumpnormals"] = yafray::parameter_t(re->r.YF_nobump ? "on" : "off");
	}
	else
	{
		switch (re->r.GIquality)
		{
			case 1 : params["samples"] = yafray::parameter_t(16);  break;
			case 2 : params["samples"] = yafray::parameter_t(36);  break;
			case 3 : params["samples"] = yafray::parameter_t(64);  break;
			case 4 : params["samples"] = yafray::parameter_t(128); break;
			case 5 : params["samples"] = yafray::parameter_t(256); break;
			default: params["samples"] = yafray::parameter_t(25);
		}
	}
	yafrayGate->addLight(params);
}

bool yafrayPluginRender_t::writeWorld()
{
	World *world = G.scene->world;
	if (re->r.GIquality!=0) {
		if (re->r.GImethod==1) {
			if (world==NULL) cout << "WARNING: need world background for skydome!\n";
			writeHemilight();
		}
		else if (re->r.GImethod==2) writePathlight();
	}
	if (world==NULL) return false;
	
	yafray::paramMap_t params;
	for (int i=0;i<MAX_MTEX;i++) {
		MTex* wtex = world->mtex[i];
		if (!wtex) continue;
		Image* wimg = wtex->tex->ima;
		// now always exports if image used as world texture (and 'Hori' mapping enabled)
		if ((wtex->tex->type==TEX_IMAGE) && (wimg!=NULL) && (wtex->mapto & WOMAP_HORIZ)) {
			string wt_path = wimg->name;
			adjustPath(wt_path);
			params["type"] = yafray::parameter_t("image");
			params["name"] = yafray::parameter_t("world_background");
			// exposure_adjust not restricted to integer range anymore
			params["exposure_adjust"] = yafray::parameter_t(wtex->tex->bright-1.f);
			if (wtex->texco & TEXCO_ANGMAP)
				params["mapping"] = yafray::parameter_t("probe");
			else if (wtex->texco & TEXCO_H_SPHEREMAP)	// in yafray full sphere
				params["mapping"] = yafray::parameter_t("sphere");
			else	// assume 'tube' for anything else
				params["mapping"] = yafray::parameter_t("tube");
			params["filename"] = yafray::parameter_t(wt_path);
			params["interpolate"] = yafray::parameter_t((wtex->tex->imaflag & TEX_INTERPOL) ? "bilinear" : "none");
			if (wtex->tex->filtersize>1.f) params["prefilter"] = yafray::parameter_t("on");
			yafrayGate->addBackground(params);
			return true;
		}
	}

	params.clear();
	params["type"] = yafray::parameter_t("constant");
	params["name"] = yafray::parameter_t("world_background");
	// if no GI used, the GIpower parameter is not always initialized, so in that case ignore it
	// (have to change method to init yafray vars in Blender)
	float bg_mult = (re->r.GImethod==0) ? 1 : re->r.GIpower;
	params["color"]=yafray::parameter_t(yafray::color_t(world->horr * bg_mult,
																											world->horg * bg_mult,
																											world->horb * bg_mult));
	yafrayGate->addBackground(params);
	return true;
}

bool blenderYafrayOutput_t::putPixel(int x, int y, const yafray::color_t &c,
		yafray::CFLOAT alpha, yafray::PFLOAT depth)
{
	// XXX how to get the image from Blender and write to it. This call doesn't allow to change buffer rects
	RenderResult rres;
	RE_GetResultImage(re, &rres);
	// rres.rectx, rres.recty is width/height
	// rres.rectf is float buffer, scanlines starting in bottom
	// rres.rectz is zbuffer, available when associated pass is set

	const unsigned int maxy = rres.recty-1;

	if (re->r.mode & R_BORDER) {
		// border render, blender renderwin is size of border region,
		// but yafray returns coords relative to full resolution
		x -= int(re->r.border.xmin * re->winx);
		y -= int((1.f-re->r.border.ymax) * re->winy);
		if ((x >= 0) && (x < re->rectx) && (y >= 0) && (y < re->recty))
		{
			const unsigned int px = rres.rectx*(maxy - y);
			// rgba
			float* fpt = rres.rectf + ((px + x) << 2);
			*fpt++ = c.R;
			*fpt++ = c.G;
			*fpt++ = c.B;
			*fpt = alpha;
			// depth values
			if (rres.rectz) rres.rectz[px + x] = depth;
			// to simplify things a bit, just do complete redraw here...
			out++;
			if ((out==4096) || ((x+y*re->rectx) == ((re->rectx-1)+(re->recty-1)*re->rectx))) {
				re->result->renlay = render_get_active_layer(re, re->result);
				re->display_draw(re->result, NULL);
				out = 0;
			}
		}
		if (re->test_break()) return false;
		return true;
	}

	const unsigned int px = (maxy - y)*rres.rectx;

	// rgba
	float* fpt = rres.rectf + ((px + x) << 2);
	*fpt++ = c.R;
	*fpt++ = c.G;
	*fpt++ = c.B;
	*fpt = alpha;

	// depth values
	if (rres.rectz) rres.rectz[px + x] = depth;
	
	// attempt to optimize drawing, by only drawing the tile currently rendered by yafray,
	// and not the entire display every time (blender has to to do float->char conversion),
	// but since the tile is not actually known, it has to be calculated from the coords.
	// not sure if it really makes all that much difference at all... unless rendering really large pictures
	// (renderwin.c also had to be adapted for this)
	// tile start & end coords
	const int txs = x & 0xffffffc0, tys = y & 0xffffffc0;
	int txe = txs + 63, tye = tys + 63;
	// tile border clip
	if (txe >= rres.rectx) txe = rres.rectx-1;
	if (tye >= rres.recty) tye = maxy;
	// draw tile if last pixel reached
	if ((y*rres.rectx + x) == (tye*rres.rectx + txe)) {
		re->result->renlay = render_get_active_layer(re, re->result);
		// note: ymin/ymax swapped here, img. upside down!
		rcti rt = {txs, txe+1, maxy-tye, ((tys==0) ? maxy : (rres.recty-tys))}; // !!! tys can be zero
		re->display_draw(re->result, &rt);
	}

	if (re->test_break()) return false;
	return true;
}
