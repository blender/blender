#include "export_File.h"

#include <math.h>

using namespace std;

static string command_path = "";

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

static string unixYafrayPath()
{
	static char *alternative[]=
	{
		"/usr/local/bin/",
		"/usr/bin/",
		"/bin/",
		NULL
	};

	for(int i=0;alternative[i]!=NULL;++i)
	{
		string fp=string(alternative[i])+"yafray";
		struct stat st;
		if(stat(fp.c_str(),&st)<0) continue;
		if(st.st_mode&S_IXOTH) return alternative[i];
	}
	return "";
}

#endif

bool yafrayFileRender_t::initExport()
{
	xmlpath = "";
	bool dir_failed = false;
	// try the user setting setting first, export dir must be set and exist
	if (strlen(U.yfexportdir)==0) 
	{
		cout << "No export directory set in user defaults!" << endl;
		char* temp = getenv("TEMP");
		// if no envar, use /tmp
		xmlpath = temp ? temp : "/tmp";
		cout << "Will try TEMP instead: " << xmlpath << endl;
		// no fail here, but might fail when opening file...
	}
	else 
	{
		// check if it exists
		if (!BLI_exists(U.yfexportdir)) {
			cout << "YafRay temporary xml export directory:\n" << U.yfexportdir << "\ndoes not exist!\n";
#ifdef WIN32
			// try to create it
			cout << "Trying to create...\n";
			if (createDir(U.yfexportdir)==0) dir_failed=true; else dir_failed=false;
#else
			dir_failed = true;
#endif
		}
		xmlpath = U.yfexportdir;
#ifdef WIN32
		// have to add drive char here too, in case win user still wants to set path him/herself
		addDrive(xmlpath);
#endif
	}

#ifdef WIN32
	// for windows try to get the path to the yafray binary from the registry, only done once
	if (command_path=="") 
	{
		char path[FILE_MAXDIR+FILE_MAXFILE];
		string yafray_path = find_path();
		if (yafray_path=="") 
		{
			// error already printed in find_path()
			clearAll();
			return false;
		}
		GetShortPathName((LPCTSTR)(yafray_path.c_str()), path, FILE_MAXDIR+FILE_MAXFILE);
		command_path = string(path) + "\\";
		cout << "Yafray found at : " << command_path << endl;
	}
	// if no export dir set, or could not create, try to create one in the yafray dir, unless it already exists
	if (dir_failed) 
	{
		string ybdir = command_path + "YBtest";
		if (createDir(const_cast<char*>(ybdir.c_str()))==0) dir_failed=true; else dir_failed=false;
		xmlpath = ybdir;
	}
#else
	if (command_path=="")
	{
		command_path = unixYafrayPath();
		if (command_path.size()) cout << "Yafray found at : " << command_path << endl;
	}
#endif

	// for all
	if (dir_failed) return false;

#ifdef WIN32
	string DLM = "\\";
#else
	string DLM = "/";
#endif
	// remove trailing slash if needed
	if (xmlpath.find_last_of(DLM)!=(xmlpath.length()-1)) xmlpath += DLM;

	imgout = xmlpath + "YBtest.tga";
	xmlpath += "YBtest.xml";
	xmlfile.open(xmlpath.c_str());
	if (xmlfile.fail()) 
	{
		cout << "Could not open file\n";
		return false;
	}
	ostr << setiosflags(ios::showpoint | ios::fixed);
	xmlfile << "<scene>\n\n";
	return true;
}

bool yafrayFileRender_t::writeRender()
{
	// finally export render block
	ostr.str("");
	ostr << "<render camera_name=\"MAINCAM\"\n";
	ostr << "\traydepth=\"" << R.r.YF_raydepth << "\" gamma=\"" << R.r.YF_gamma << "\" exposure=\"" << R.r.YF_exposure << "\"\n";

	if(R.r.YF_AA) {
		ostr << "\tAA_passes=\"" << R.r.YF_AApasses << "\" AA_minsamples=\"" << R.r.YF_AAsamples << "\"\n";
		ostr << "\tAA_pixelwidth=\"" << R.r.YF_AApixelsize << "\" AA_threshold=\"" << R.r.YF_AAthreshold << "\"\n";
	}
	else {
		if ((R.r.GImethod!=0) && (R.r.GIquality>1) && (!R.r.GIcache))
			ostr << "\tAA_passes=\"5\" AA_minsamples=\"5\"\n";
		else if ((R.r.mode & R_OSA) && (R.r.osa)) {
			int passes=(R.r.osa%4)==0 ? R.r.osa/4 : 1;
			int minsamples=(R.r.osa%4)==0 ? 4 : R.r.osa;
			ostr << "\tAA_passes=\"" << passes << "\" AA_minsamples=\"" << minsamples << "\"\n";
		}
		else ostr << "\tAA_passes=\"0\" AA_minsamples=\"1\"\n";
		ostr << "\tAA_pixelwidth=\"1.5\" AA_threshold=\"0.05\" bias=\"" << R.r.YF_raybias << "\"\n";
	}

	if (hasworld) ostr << "\tbackground_name=\"world_background\"\n";
 
	// alpha channel render when RGBA button enabled
	if (R.r.planes==R_PLANES32) ostr << "\n\tsave_alpha=\"on\"";
	ostr << " >\n";

	ostr << "\t<outfile value=\"" << imgout << "\" />\n";

	ostr << "</render>\n\n";
	xmlfile << ostr.str();
	return true;
}

bool yafrayFileRender_t::finishExport()
{
	xmlfile << "</scene>\n";
	xmlfile.close();

	// file exported, now render
	if (executeYafray(xmlpath))
		displayImage();
	else 
	{
		cout << "Could not execute yafray. Is it in path?" << endl;
		return false;
	}
	return true;
}

// displays the image rendered with xml export
// Now loads rendered image into blender renderbuf.
void yafrayFileRender_t::displayImage()
{
	// although it is possible to load the image using blender,
	// maybe it is best to just do a read here, for now the yafray output is always a raw tga anyway

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
	BLI_convertstringcode(cpath, G.sce, 0);
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

void yafrayFileRender_t::writeTextures()
{
	// used to keep track of images already written
	// (to avoid duplicates if also in imagetex for material TexFace texture)
	set<Image*> dupimg;
	
	for (map<string, MTex*>::const_iterator blendtex=used_textures.begin();
						blendtex!=used_textures.end();++blendtex) {
		MTex* mtex = blendtex->second;
		Tex* tex = mtex->tex;

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
				ostr.str("");
				ostr << "<shader type=\"clouds\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t<attributes>\n";
				ostr << "\t\t<size value=\"" << nsz << "\" />\n";
				ostr << "\t\t<hard value=\"" << hardnoise << "\" />\n";
				if (tex->type==TEX_STUCCI) {
					if (tex->stype==1)
						ts = "positive";
					else if (tex->stype==2)
						ts = "negative";
					else ts = "none";
					ostr << "\t\t<bias value=\"" << ts << "\" />\n";
					ostr << "\t\t<depth value=\"0\" />\n";	// for stucci always 0
				}
				else ostr << "\t\t<depth value=\"" << tex->noisedepth << "\" />\n";
				ostr << "\t\t<color_type value=\"" << tex->stype << "\" />\n";
				ostr << "\t\t<noise_type value=\"" << ntype << "\" />\n";
				ostr << "\t</attributes>\n</shader >\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_WOOD: {
				ostr.str("");
				ostr << "<shader type=\"wood\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t\t<attributes>\n";
				// blender does not use depth value for wood, always 0
				ostr << "\t\t<depth value=\"0\" />\n";
				float turb = (tex->stype<2) ? 0.0 : tex->turbul;
				ostr << "\t\t<turbulence value=\"" << turb << "\" />\n";
				ostr << "\t\t<size value=\"" << nsz << "\" />\n";
				ostr << "\t\t<hard value=\"" << hardnoise << "\" />\n";
				ts = (tex->stype & 1) ? "rings" : "bands";	//stype 1&3 ringtype
				ostr << "\t\t<wood_type value=\"" << ts << "\" />\n";
				ostr << "\t\t<noise_type value=\"" << ntype << "\" />\n";
				ostr << "\t</attributes>\n</shader>\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_MARBLE: {
				ostr.str("");
				ostr << "<shader type=\"marble\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t<attributes>\n";
				ostr << "\t\t<depth value=\"" << tex->noisedepth << "\" />\n";
				ostr << "\t\t<turbulence value=\"" << tex->turbul << "\" />\n";
				ostr << "\t\t<size value=\"" << nsz << "\" />\n";
				ostr << "\t\t<hard value=\"" << hardnoise << "\" />\n";
				ostr << "\t\t<sharpness value=\"" << (float)(1<<tex->stype) << "\" />\n";
				ostr << "\t\t<noise_type value=\"" << ntype << "\" />\n";
				ostr << "\t</attributes>\n</shader>\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_VORONOI: {
				ostr.str("");
				ostr << "<shader type=\"voronoi\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t<attributes>\n";
				ts = "int";
				if (tex->vn_coltype==1)
					ts = "col1";
				else if (tex->vn_coltype==2)
					ts = "col2";
				else if (tex->vn_coltype==3)
					ts = "col3";
				ostr << "\t\t<color_type value=\"" << ts << "\" />\n";
				ostr << "\t\t<weight1 value=\"" << tex->vn_w1 << "\" />\n";
				ostr << "\t\t<weight2 value=\"" << tex->vn_w2 << "\" />\n";
				ostr << "\t\t<weight3 value=\"" << tex->vn_w3 << "\" />\n";
				ostr << "\t\t<weight4 value=\"" << tex->vn_w4 << "\" />\n";
				ostr << "\t\t<mk_exponent value=\"" << tex->vn_mexp << "\" />\n";
				ostr << "\t\t<intensity value=\"" << tex->ns_outscale << "\" />\n";
				ostr << "\t\t<size value=\"" << nsz << "\" />\n";
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
				ostr << "\t\t<distance_metric value=\"" << ts << "\" />\n";
				ostr << "\t</attributes>\n</shader>\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_MUSGRAVE: {
				ostr.str("");
				ostr << "<shader type=\"musgrave\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t<attributes>\n";
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
				ostr << "\t\t<musgrave_type value=\"" << ts << "\" />\n";
				ostr << "\t\t<noise_type value=\"" << ntype << "\" />\n";
				ostr << "\t\t<H value=\"" << tex->mg_H << "\" />\n";
				ostr << "\t\t<lacunarity value=\"" << tex->mg_lacunarity << "\" />\n";
				ostr << "\t\t<octaves value=\"" << tex->mg_octaves << "\" />\n";
				if ((tex->stype==TEX_HTERRAIN) || (tex->stype==TEX_RIDGEDMF) || (tex->stype==TEX_HYBRIDMF)) {
					ostr << "\t\t<offset value=\"" << tex->mg_offset << "\" />\n";
					if ((tex->stype==TEX_RIDGEDMF) || (tex->stype==TEX_HYBRIDMF))
						ostr << "\t\t<gain value=\"" << tex->mg_gain << "\" />\n";
				}
				ostr << "\t\t<size value=\"" << nsz << "\" />\n";
				ostr << "\t\t<intensity value=\"" << tex->ns_outscale << "\" />\n";
				ostr << "\t</attributes>\n</shader>\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_DISTNOISE: {
				ostr.str("");
				ostr << "<shader type=\"distorted_noise\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t<attributes>\n";
				ostr << "\t\t<distort value=\"" << tex->dist_amount << "\" />\n";
				ostr << "\t\t<size value=\"" << nsz << "\" />\n";
				ostr << "\t\t<noise_type1 value=\"" << ntype << "\" />\n";
				ostr << "\t\t<noise_type2 value=\"" << noise2string(tex->noisebasis2) << "\" />\n";
				ostr << "\t</attributes>\n</shader>\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_BLEND: {
				ostr.str("");
				ostr << "<shader type=\"gradient\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t<attributes>\n";
				switch (tex->stype) {
					case 1:  ts="quadratic"; break;
					case 2:  ts="cubic";     break;
					case 3:  ts="diagonal";  break;
					case 4:  ts="sphere";    break;
					case 5:  ts="halo";      break;
					default:
					case 0:  ts="linear";    break;
				}
				ostr << "\t\t<gradient_type value=\"" << ts << "\" />\n";
				if (tex->flag & TEX_FLIPBLEND) ts="on"; else ts="off";
				ostr << "\t\t<flip_xy value=\"" << ts << "\" />\n";
				ostr << "\t</attributes>\n</shader>\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_NOISE: {
				ostr.str("");
				ostr << "<shader type=\"random_noise\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t<attributes>\n";
				ostr << "\t\t<depth value=\"" << tex->noisedepth << "\" />\n";
				ostr << "\t</attributes>\n</shader>\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_IMAGE: {
				Image* ima = tex->ima;
				if (ima) {
					// remember image to avoid duplicates later if also in imagetex
					// (formerly done by removing from imagetex, but need image/material link)
					dupimg.insert(ima);
					ostr.str("");
					// use image name instead of texname here
					ostr << "<shader type=\"image\" name=\"" << ima->id.name << "\" >\n";
					ostr << "\t<attributes>\n";
					string texpath(ima->name);
					adjustPath(texpath);
					ostr << "\t\t<filename value=\"" << texpath << "\" />\n";
					ostr << "\t</attributes>\n</shader>\n\n";
					xmlfile << ostr.str();
				}
				break;
			}
			default:
				cout << "Unsupported texture type\n";
		}

		// colorbands
		if (tex->flag & TEX_COLORBAND) {
			ColorBand* cb = tex->coba;
			if (cb) {
				ostr.str("");
				ostr << "<shader type=\"colorband\" name=\"" << blendtex->first + "_coba" << "\" >\n";
				ostr << "\t<attributes>\n";
				ostr << "\t\t<input value=\"" << blendtex->first << "\" />\n";
				ostr << "\t</attributes>\n";
				for (int i=0;i<cb->tot;i++) {
					ostr << "\t<modulator value=\"" << cb->data[i].pos << "\" >\n";
					ostr << "\t\t<color r=\"" << cb->data[i].r << "\"" <<
														" g=\"" << cb->data[i].g << "\"" <<
														" b=\"" << cb->data[i].b << "\"" <<
														" a=\"" << cb->data[i].a << "\" />\n";
					ostr << "\t</modulator>\n";
				}
				ostr << "</shader>\n\n";
				xmlfile << ostr.str();
			}
		}

	}
	
	// If used, textures for the material 'TexFace' case
	if (!imagetex.empty()) {
		for (map<Image*, set<Material*> >::const_iterator imgtex=imagetex.begin();
					imgtex!=imagetex.end();++imgtex)
		{
			// skip if already written above
			Image* ima = imgtex->first;
			if (dupimg.find(ima)==dupimg.end()) {
				ostr.str("");
				ostr << "<shader type=\"image\" name=\"" << ima->id.name << "\" >\n";
				ostr << "\t<attributes>\n";
				string texpath(ima->name);
				adjustPath(texpath);
				ostr << "\t\t<filename value=\"" << texpath << "\" />\n";
				ostr << "\t</attributes>\n</shader>\n\n";
				xmlfile << ostr.str();
			}
		}
	}

}

void yafrayFileRender_t::writeShader(const string &shader_name, Material* matr, const string &facetexname)
{
	ostr.str("");
	ostr << "<shader type=\"blendershader\" name=\"" << shader_name << "\" >\n";
	ostr << "\t<attributes>\n";
	float diff = matr->alpha;
	ostr << "\t\t<color r=\"" << matr->r*diff << "\" g=\"" << matr->g*diff << "\" b=\"" << matr->b*diff << "\" />\n";
	ostr << "\t\t<specular_color r=\"" << matr->specr << "\" g=\"" << matr->specg << "\" b=\"" << matr->specb << "\" />\n";
	ostr << "\t\t<mirror_color r=\"" << matr->mirr << "\" g=\"" << matr->mirg << "\" b=\"" << matr->mirb << "\" />\n";
	ostr << "\t\t<diffuse_reflect value=\"" << matr->ref << "\" />\n";
	ostr << "\t\t<specular_amount value=\"" << matr->spec << "\" />\n";
	ostr << "\t\t<hard value=\"" << matr->har << "\" />\n";
	ostr << "\t\t<alpha value=\"" << matr->alpha << "\" />\n";
	// if no GI used, the GIpower parameter is not always initialized, so in that case ignore it
	float bg_mult = (R.r.GImethod==0) ? 1 : R.r.GIpower;
	ostr << "\t\t<emit value=\"" << (matr->emit * bg_mult) << "\" />\n";

	// reflection/refraction
	if ( (matr->mode & MA_RAYMIRROR) || (matr->mode & MA_RAYTRANSP) )
		ostr << "\t\t<IOR value=\"" << matr->ang << "\" />\n";
	if (matr->mode & MA_RAYMIRROR) {
		float rf = matr->ray_mirror;
		// blender uses mir color for reflection as well
		ostr << "\t\t<reflected r=\"" << matr->mirr << "\" g=\"" << matr->mirg << "\" b=\"" << matr->mirb << "\" />\n";
		ostr << "\t\t<min_refle value=\""<< rf << "\" />\n";
		if (matr->ray_depth>maxraydepth) maxraydepth = matr->ray_depth;
	}
	if (matr->mode & MA_RAYTRANSP) 
	{
		float tr=1.0-matr->alpha;
		ostr << "\t\t<transmitted r=\"" << matr->r * tr << "\" g=\"" << matr->g * tr << "\" b=\"" << matr->b * tr << "\" />\n";
		// tir on by default
		ostr << "\t\t<tir value=\"on\" />\n";
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
	if (Mmode!="") ostr << "\t\t<matmodes value=\"" << Mmode << "\" />\n";
	ostr << "\t</attributes>\n";
	xmlfile << ostr.str();
	
	// modulators
	// first modulator is the texture of the face, if used (TexFace mode)
	if (facetexname.length()!=0) {
			ostr.str("");
			ostr << "\t<modulator>\n";
			ostr << "\t\t<input value=\"" << facetexname << "\" />\n";
			ostr << "\t\t<color value=\"1\" />\n";
			ostr << "\t</modulator>\n";
			xmlfile << ostr.str();
	}

	for (int m2=0;m2<MAX_MTEX;m2++) {

		if (matr->septex & (1<<m2)) continue;// all active channels

		// ignore null mtex
		MTex* mtex = matr->mtex[m2];
		if (mtex==NULL) continue;

		// ignore null tex
		Tex* tex = mtex->tex;
		if (tex==NULL) continue;

		map<string, MTex*>::const_iterator mtexL = used_textures.find(string(tex->id.name));
		if (mtexL!=used_textures.end()) {

			ostr.str("");
			ostr << "\t<modulator>\n";
			// when no facetex used, shader_name is created from original material name
			if (facetexname.length()!=0)
				ostr << "\t\t<input value=\"" << matr->id.name << "_map" << m2 << "\" />\n";
			else
				ostr << "\t\t<input value=\"" << shader_name << "_map" << m2 << "\" />\n";

			// blendtype
			string ts = "mix";
			if (mtex->blendtype==MTEX_MUL) ts="mul";
			else if (mtex->blendtype==MTEX_ADD) ts="add";
			else if (mtex->blendtype==MTEX_SUB) ts="sub";
			ostr << "\t\t<mode value=\"" << ts << "\" />\n";

			// texture color (for use with MUL and/or no_rgb etc..)
			ostr << "\t\t<texcol r=\"" << mtex->r << "\" g=\"" << mtex->g << "\" b=\"" << mtex->b << "\" />\n";

			// texture contrast, brightness & color adjustment
			ostr << "\t\t<filtercolor r=\"" << tex->rfac << "\" g=\"" << tex->gfac << "\" b=\"" << tex->bfac << "\" />\n";
			ostr << "\t\t<contrast value=\"" << tex->contrast << "\" />\n";
			ostr << "\t\t<brightness value=\"" << tex->bright << "\" />\n";

			// all texture flags now are switches, having the value 1 or -1 (negative option)
			// the negative option only used for the intensity modulation options.

			// material (diffuse) color, amount controlled by colfac (see below)
			if (mtex->mapto & MAP_COL)
				ostr << "\t\t<color value=\"1\" />\n";

			// bumpmapping
			if ((mtex->mapto & MAP_NORM) || (mtex->maptoneg & MAP_NORM)) {
				// for yafray, bump factor is negated (unless tex is stucci, not affected by 'Neg')
				// scaled down quite a bit
				float nf = mtex->norfac;
				if (tex->type!=TEX_STUCCI) nf *= -1.f;
				if (mtex->maptoneg & MAP_NORM) nf *= -1.f;
				ostr << "\t\t<normal value=\"" << (nf/60.f) << "\" />\n";

			}

			// all blender texture modulation as switches, either 1 or -1 (negative state of button)
			// Csp, specular color modulation
			if (mtex->mapto & MAP_COLSPEC)
				ostr << "\t\t<colspec value=\"1\" />\n";

			// CMir, mirror color  modulation
			if (mtex->mapto & MAP_COLMIR)
				ostr << "\t\t<colmir value=\"1\" />\n";

			// Ref, diffuse reflection amount  modulation
			if ((mtex->mapto & MAP_REF) || (mtex->maptoneg & MAP_REF)) {
				int t = 1;
				if (mtex->maptoneg & MAP_REF) t = -1;
				ostr << "\t\t<difref value=\"" << t << "\" />\n";
			}

			// Spec, specular amount mod
			if ((mtex->mapto & MAP_SPEC) || (mtex->maptoneg & MAP_SPEC)) {
				int t = 1;
				if (mtex->maptoneg & MAP_SPEC) t = -1;
				ostr << "\t\t<specular value=\"" << t << "\" />\n";
			}

			// hardness modulation
			if ((mtex->mapto & MAP_HAR) || (mtex->maptoneg & MAP_HAR)) {
				int t = 1;
				if (mtex->maptoneg & MAP_HAR) t = -1;
				ostr << "\t\t<hard value=\"" << t << "\" />\n";
			}

			// alpha modulation
			if ((mtex->mapto & MAP_ALPHA) || (mtex->maptoneg & MAP_ALPHA)) {
				int t = 1;
				if (mtex->maptoneg & MAP_ALPHA) t = -1;
				ostr << "\t\t<alpha value=\"" << t << "\" />\n";

			}

			// emit modulation
			if ((mtex->mapto & MAP_EMIT) || (mtex->maptoneg & MAP_EMIT)) {
				int t = 1;
				if (mtex->maptoneg & MAP_EMIT) t = -1;
				ostr << "\t\t<emit value=\"" << t << "\" />\n";
			}

			// texture flag, combination of strings
			if (mtex->texflag & (MTEX_RGBTOINT | MTEX_STENCIL | MTEX_NEGATIVE)) {
				ts = "";
				if (mtex->texflag & MTEX_RGBTOINT) ts += "no_rgb ";
				if (mtex->texflag & MTEX_STENCIL) ts += "stencil ";
				if (mtex->texflag & MTEX_NEGATIVE) ts += "negative";
				ostr << "\t\t<texflag value=\"" << ts << "\" />\n";
			}

			// colfac, controls amount of color modulation
			ostr << "\t\t<colfac value=\"" << mtex->colfac << "\" />\n";

			// def_var
			ostr << "\t\t<def_var value=\"" << mtex->def_var << "\" />\n";

			//varfac
			ostr << "\t\t<varfac value=\"" << mtex->varfac << "\" />\n";

			if ((tex->imaflag & (TEX_CALCALPHA | TEX_USEALPHA)) || (tex->flag & TEX_NEGALPHA)) {
				ts = "";
				if (tex->imaflag & TEX_CALCALPHA) ts += "calc_alpha ";
				if (tex->imaflag & TEX_USEALPHA) ts += "use_alpha ";
				if (tex->flag & TEX_NEGALPHA) ts += "neg_alpha";
				ostr << "\t\t<alpha_flag value=\"" << ts << "\" />\n";
			}

			// image as normalmap flag
			if (tex->imaflag & TEX_NORMALMAP) ostr << "\t\t<normalmap value=\"on\" />\n";

			ostr << "\t</modulator>\n";
			xmlfile << ostr.str();

		}
	}
	xmlfile << "</shader>\n\n";

}


// write all materials & modulators
void yafrayFileRender_t::writeMaterialsAndModulators()
{
	// shaders/mappers for regular texture (or non-texture) mode
	// In case material has texface mode, and all faces have an image texture,
	// this shader will not be used, but still be written
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
			if (mtexL!=used_textures.end()) {
				ostr.str("");
				ostr << "<shader type=\"blendermapper\" name=\"" << blendmat->first + "_map" << m <<"\"";
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
					ostr << "\n\t\tm00=\"" << itexmat[0][0] << "\" m01=\"" << itexmat[1][0]
							<< "\" m02=\"" << itexmat[2][0] << "\" m03=\"" << itexmat[3][0] << "\"\n";
					ostr << "\t\tm10=\"" << itexmat[0][1] << "\" m11=\"" << itexmat[1][1]
							<< "\" m12=\"" << itexmat[2][1] << "\" m13=\"" << itexmat[3][1] << "\"\n";
					ostr << "\t\tm20=\"" << itexmat[0][2] << "\" m21=\"" << itexmat[1][2]
							<< "\" m22=\"" << itexmat[2][2] << "\" m23=\"" << itexmat[3][2] << "\"\n";
					ostr << "\t\tm30=\"" << itexmat[0][3] << "\" m31=\"" << itexmat[1][3]
							<< "\" m32=\"" << itexmat[2][3] << "\" m33=\"" << itexmat[3][3] << "\">\n";
				}
				else ostr << ">\n";
				ostr << "\t<attributes>\n";

				// use image name instead of texname when texture is image
				if ((tex->type==TEX_IMAGE) && tex->ima)
					ostr << "\t\t<input value=\"" << tex->ima->id.name << "\" />\n";
				else if ((tex->flag & TEX_COLORBAND) & (tex->coba!=NULL))
					ostr << "\t\t<input value=\"" << mtexL->first + "_coba" << "\" />\n";
				else
					ostr << "\t\t<input value=\"" << mtexL->first << "\" />\n";

				// texture size
				ostr << "\t\t<sizex value=\"" << mtex->size[0] << "\" />\n";
				ostr << "\t\t<sizey value=\"" << mtex->size[1] << "\" />\n";
				ostr << "\t\t<sizez value=\"" << mtex->size[2] << "\" />\n";

				// texture offset
				ostr << "\t\t<ofsx value=\"" << mtex->ofs[0] << "\" />\n";
				ostr << "\t\t<ofsy value=\"" << mtex->ofs[1] << "\" />\n";
				ostr << "\t\t<ofsz value=\"" << mtex->ofs[2] << "\" />\n";

				// texture coordinates, have to disable 'sticky' in Blender
				if (mtex->texco & TEXCO_UV)
					ostr << "\t\t<texco value=\"uv\" />\n";
				else if ((mtex->texco & TEXCO_GLOB) || (mtex->texco & TEXCO_OBJECT))
					// object mode is also set as global, but the object matrix was specified above with <modulator..>
					ostr << "\t\t<texco value=\"global\" />\n";
				else if (mtex->texco & TEXCO_ORCO)
					ostr << "\t\t<texco value=\"orco\" />\n";
				else if (mtex->texco & TEXCO_WINDOW)
					ostr << "\t\t<texco value=\"window\" />\n";
				else if (mtex->texco & TEXCO_NORM)
					ostr << "\t\t<texco value=\"normal\" />\n";
				else if (mtex->texco & TEXCO_REFL)
					ostr << "\t\t<texco value=\"reflect\" />\n";

				// texture projection axes, both image & procedural
				string proj = "nxyz";		// 'n' for 'none'
				ostr << "\t\t<proj_x value=\"" << proj[mtex->projx] << "\" />\n";
				ostr << "\t\t<proj_y value=\"" << proj[mtex->projy] << "\" />\n";
				ostr << "\t\t<proj_z value=\"" << proj[mtex->projz] << "\" />\n";

				// texture mapping parameters only relevant to image type
				if (tex->type==TEX_IMAGE) {
					if (mtex->mapping==MTEX_FLAT)
						ostr << "\t\t<mapping value=\"flat\" />\n";
					else if (mtex->mapping==MTEX_CUBE)
						ostr << "\t\t<mapping value=\"cube\" />\n";
					else if (mtex->mapping==MTEX_TUBE)
						ostr << "\t\t<mapping value=\"tube\" />\n";
					else if (mtex->mapping==MTEX_SPHERE)
						ostr << "\t\t<mapping value=\"sphere\" />\n";

					// repeat
					ostr << "\t\t<xrepeat value=\"" << tex->xrepeat << "\" />\n";
					ostr << "\t\t<yrepeat value=\"" << tex->yrepeat << "\" />\n";

					// clipping
					if (tex->extend==TEX_EXTEND)
						ostr << "\t\t<clipping value=\"extend\" />\n";
					else if (tex->extend==TEX_CLIP)
						ostr << "\t\t<clipping value=\"clip\" />\n";
					else if (tex->extend==TEX_CLIPCUBE)
						ostr << "\t\t<clipping value=\"clipcube\" />\n";
					else
						ostr << "\t\t<clipping value=\"repeat\" />\n";

					// crop min/max
					ostr << "\t\t<cropmin_x value=\"" << tex->cropxmin << "\" />\n";
					ostr << "\t\t<cropmin_y value=\"" << tex->cropymin << "\" />\n";
					ostr << "\t\t<cropmax_x value=\"" << tex->cropxmax << "\" />\n";
					ostr << "\t\t<cropmax_y value=\"" << tex->cropymax << "\" />\n";

					// rot90 flag
					string ts = "off";
					if (tex->imaflag & TEX_IMAROT) ts = "on";
					ostr << "\t\t<rot90 value=\"" << ts << "\" />\n";
				}

				ostr << "\t</attributes>\n";
				ostr << "</shader>\n\n";

				xmlfile << ostr.str();
			}
		}

		// shader + modulators
		writeShader(blendmat->first, matr);

	}

	// write the mappers & shaders for the TexFace case
	if (!imagetex.empty()) {
		// Yafray doesn't have per-face-textures, only per-face-shaders,
		// so create as many mappers/shaders as the images used by the object
		int snum = 0;
		for (map<Image*, set<Material*> >::const_iterator imgtex=imagetex.begin();
				imgtex!=imagetex.end();++imgtex)
		{

			for (set<Material*>::const_iterator imgmat=imgtex->second.begin();
					imgmat!=imgtex->second.end();++imgmat)
			{
				Material* matr = *imgmat;
				// mapper
				ostr.str("");
				ostr << "<shader type=\"blendermapper\" name=\"" << string(matr->id.name) + "_ftmap" << snum << "\" >\n";
				ostr << "\t<attributes>\n";
				ostr << "\t\t<input value=\"" << imgtex->first->id.name << "\" />\n";
				// all yafray default settings, except for texco, so no need to set others
				ostr << "\t\t<texco value=\"uv\" />\n";
				ostr << "\t</attributes>\n";
				ostr << "</shader>\n\n";
				xmlfile << ostr.str();

				// shader, remember name, used later when writing per-face-shaders
				ostr.str("");
				ostr << matr->id.name <<  "_ftsha" << snum;
				string shader_name = ostr.str();
				imgtex_shader[string(matr->id.name) + string(imgtex->first->id.name)] = shader_name;

				ostr.str("");
				ostr << matr->id.name << "_ftmap" << snum++;
				writeShader(shader_name, matr, ostr.str());
			}

		}
	}

}


void yafrayFileRender_t::writeObject(Object* obj, const vector<VlakRen*> &VLR_list, const float obmat[4][4])
{
	ostr.str("");
	// transform first (not necessarily actual obj->obmat, can be duplivert see below)
	ostr << "<transform m00=\"" << obmat[0][0] << "\" m01=\"" << obmat[1][0]
			 <<         "\" m02=\"" << obmat[2][0] << "\" m03=\"" << obmat[3][0] << "\"\n";
	ostr << "           m10=\"" << obmat[0][1] << "\" m11=\"" << obmat[1][1]
			 <<         "\" m12=\"" << obmat[2][1] << "\" m13=\"" << obmat[3][1] << "\"\n";
	ostr << "           m20=\"" << obmat[0][2] << "\" m21=\"" << obmat[1][2]
			 <<         "\" m22=\"" << obmat[2][2] << "\" m23=\"" << obmat[3][2] << "\"\n";
	ostr << "           m30=\"" << obmat[0][3] << "\" m31=\"" << obmat[1][3]
			 <<         "\" m32=\"" << obmat[2][3] << "\" m33=\"" << obmat[3][3] << "\">\n";
	xmlfile << ostr.str();

	ostr.str("");
	ostr << "<object name=\"" << obj->id.name << "\"";
	// Yafray still needs default shader name in object def.,
	// since we write a shader with every face, simply use the material of the first face.
	// If this is an empty string, assume default material.
	VlakRen* face0 = VLR_list[0];
	Material* face0mat = face0->mat;
	string matname(face0mat->id.name);
	// use name in imgtex_shader list if 'TexFace' enabled for this material
	if (face0mat->mode & MA_FACETEXTURE) {
		TFace* tface = face0->tface;
		if (tface) {
			Image* fimg = (Image*)tface->tpage;
			if (fimg) matname = imgtex_shader[string(face0mat->id.name) + string(fimg->id.name)];
		}
	}
	bool shadow = face0mat->mode & MA_TRACEBLE;
	ostr <<" shadow=\""<< (shadow ? "on" : "off" ) << "\" ";
	bool caus = (((face0mat->mode & MA_RAYTRANSP) | (face0->mat->mode & MA_RAYMIRROR))!=0);
	if (caus) ostr << "caus_IOR=\"" << face0mat->ang << "\"";
	if (matname.length()==0) matname = "blender_default";
	ostr << " shader_name=\"" << matname << "\" >\n";
	ostr << "\t<attributes>\n";
	if (caus)
	{
			float tr = 1.0-face0mat->alpha;
			ostr << "\t\t<caus_tcolor r=\"" << face0mat->r*tr
					 << "\" g=\"" << face0mat->g*tr
					 << "\" b=\"" << face0mat->b*tr << "\" />\n";
			tr = face0mat->ray_mirror;
			ostr << "\t\t<caus_rcolor r=\"" << face0mat->mirr*tr
					 << "\" g=\"" << face0mat->mirg*tr
					 << "\" b=\"" << face0mat->mirb*tr << "\" />\n";
	}
	ostr << "\t</attributes>\n";
	xmlfile << ostr.str();

	// Export orco coords test.
	// Previously was done by checking orco pointer, however this can be non-null but still not initialized.
	// Test the rendermaterial texco flag instead.
	bool EXPORT_ORCO = ((face0mat->texco & TEXCO_ORCO)!=0);

	string has_orco = "off";
	if (EXPORT_ORCO) has_orco = "on";

	// smooth shading if enabled
	bool no_auto = true;	//in case non-mesh, or mesh has no autosmooth
	if (obj->type==OB_MESH) {
		Mesh* mesh = (Mesh*)obj->data;
		if (mesh->flag & ME_AUTOSMOOTH) {
			no_auto = false;
			ostr.str("");
			ostr << "\t<mesh autosmooth=\"" << mesh->smoothresh << "\" has_orco=\"" << has_orco << "\" >\n";
			xmlfile << ostr.str();
		}
	}
	// this for non-mesh as well
	if (no_auto) {
		// If AutoSmooth not used, since yafray currently cannot specify if a face is smooth
		// or flat shaded, the smooth flag of the first face is used to determine
		// the shading for the whole mesh
		if (face0->flag & ME_SMOOTH)
			xmlfile << "\t<mesh autosmooth=\"90\" has_orco=\"" << has_orco << "\" >\n";
		else
			xmlfile << "\t<mesh autosmooth=\"0.1\" has_orco=\"" << has_orco << "\" >\n";	//0 shows artefacts
	}

	// now all vertices
	map<VertRen*, int> vert_idx;	// for removing duplicate verts and creating an index list
	int vidx = 0;	// vertex index counter

	// vertices, transformed back to world
	xmlfile << "\t\t<points>\n";

	// for deformed objects, object->imat is no longer valid,
	// so have to create inverse render matrix ourselves here
	float mat[4][4], imat[4][4];
	MTC_Mat4MulMat4(mat, obj->obmat, R.viewmat);
	MTC_Mat4Invert(imat, mat);

	for (vector<VlakRen*>::const_iterator fci=VLR_list.begin();
				fci!=VLR_list.end();++fci)
	{
		VlakRen* vlr = *fci;
		VertRen* ver;
		float* orco;
		float tvec[3];
		ostr.str("");
		if (vert_idx.find(vlr->v1)==vert_idx.end()) {
			vert_idx[vlr->v1] = vidx++;
			ver = vlr->v1;
			MTC_cp3Float(ver->co, tvec);
			MTC_Mat4MulVecfl(imat, tvec);
			ostr << "\t\t\t<p x=\"" << tvec[0]
								 << "\" y=\"" << tvec[1]
								 << "\" z=\"" << tvec[2] << "\" />\n";
			if (EXPORT_ORCO) {
				orco = ver->orco;
				ostr << "\t\t\t<p x=\"" << orco[0]
									 << "\" y=\"" << orco[1]
									 << "\" z=\"" << orco[2] << "\" />\n";
			}
		}
		if (vert_idx.find(vlr->v2)==vert_idx.end()) {
			vert_idx[vlr->v2] = vidx++;
			ver = vlr->v2;
			MTC_cp3Float(ver->co, tvec);
			MTC_Mat4MulVecfl(imat, tvec);
			ostr << "\t\t\t<p x=\"" << tvec[0]
								 << "\" y=\"" << tvec[1]
								 << "\" z=\"" << tvec[2] << "\" />\n";
			if (EXPORT_ORCO) {
				orco = ver->orco;
				ostr << "\t\t\t<p x=\"" << orco[0]
									 << "\" y=\"" << orco[1]
									 << "\" z=\"" << orco[2] << "\" />\n";
			}
		}
		if (vert_idx.find(vlr->v3)==vert_idx.end()) {
			vert_idx[vlr->v3] = vidx++;
			ver = vlr->v3;
			MTC_cp3Float(ver->co, tvec);
			MTC_Mat4MulVecfl(imat, tvec);
			ostr << "\t\t\t<p x=\"" << tvec[0]
								 << "\" y=\"" << tvec[1]
								 << "\" z=\"" << tvec[2] << "\" />\n";
			if (EXPORT_ORCO) {
				orco = ver->orco;
				ostr << "\t\t\t<p x=\"" << orco[0]
									 << "\" y=\"" << orco[1]
									 << "\" z=\"" << orco[2] << "\" />\n";
			}
		}
		if ((vlr->v4) && (vert_idx.find(vlr->v4)==vert_idx.end())) {
			vert_idx[vlr->v4] = vidx++;
			ver = vlr->v4;
			MTC_cp3Float(ver->co, tvec);
			MTC_Mat4MulVecfl(imat, tvec);
			ostr << "\t\t\t<p x=\"" << tvec[0]
								 << "\" y=\"" << tvec[1]
								 << "\" z=\"" << tvec[2] << "\" />\n";
			if (EXPORT_ORCO) {
				orco = ver->orco;
				ostr << "\t\t\t<p x=\"" << orco[0]
									 << "\" y=\"" << orco[1]
									 << "\" z=\"" << orco[2] << "\" />\n";
			}
		}
		xmlfile << ostr.str();
	}
	xmlfile << "\t\t</points>\n";

	// all faces using the index list created above
	xmlfile << "\t\t<faces>\n";
	for (vector<VlakRen*>::const_iterator fci2=VLR_list.begin();
				fci2!=VLR_list.end();++fci2)
	{
		VlakRen* vlr = *fci2;
		Material* fmat = vlr->mat;
		bool EXPORT_VCOL = ((fmat->mode & (MA_VERTEXCOL|MA_VERTEXCOLP))!=0);
		string fmatname(fmat->id.name);
		// use name in imgtex_shader list if 'TexFace' enabled for this face material
		if (fmat->mode & MA_FACETEXTURE) {
			TFace* tface = vlr->tface;
			if (tface) {
				Image* fimg = (Image*)tface->tpage;
				if (fimg) fmatname = imgtex_shader[fmatname + string(fimg->id.name)];
			}
		}
		else if (fmatname.length()==0) fmatname = "blender_default";
	
		int idx1 = vert_idx.find(vlr->v1)->second;
		int idx2 = vert_idx.find(vlr->v2)->second;
		int idx3 = vert_idx.find(vlr->v3)->second;
		// make sure the indices point to the vertices when orco coords exported
		if (EXPORT_ORCO) { idx1*=2;  idx2*=2;  idx3*=2; }

		ostr.str("");
		ostr << "\t\t\t<f a=\"" << idx1 << "\" b=\"" << idx2 << "\" c=\"" << idx3 << "\"";

		// triangle uv and vcol indices
		int ui1=0, ui2=1, ui3=2;
		if (vlr->flag & R_DIVIDE_24) {
			ui3++;
			if (vlr->flag & R_FACE_SPLIT) { ui1++;  ui2++; }
		}
		else if (vlr->flag & R_FACE_SPLIT) { ui2++;  ui3++; }

		TFace* uvc = vlr->tface;	// possible uvcoords (v upside down)
		if (uvc) {
			ostr << " u_a=\"" << uvc->uv[ui1][0] << "\" v_a=\"" << 1-uvc->uv[ui1][1] << "\""
					 << " u_b=\"" << uvc->uv[ui2][0] << "\" v_b=\"" << 1-uvc->uv[ui2][1] << "\""
					 << " u_c=\"" << uvc->uv[ui3][0] << "\" v_c=\"" << 1-uvc->uv[ui3][1] << "\"";
		}

		// since Blender seems to need vcols when uvs are used, for yafray only export when the material actually uses vcols
		if ((EXPORT_VCOL) && (vlr->vcol)) {
			// vertex colors
			unsigned char* pt = reinterpret_cast<unsigned char*>(&vlr->vcol[ui1]);
			ostr << " vcol_a_r=\"" << (float)pt[3]/255.f << "\" vcol_a_g=\"" << (float)pt[2]/255.f
					 << "\" vcol_a_b=\"" << (float)pt[1]/255.f << "\"";
			pt = reinterpret_cast<unsigned char*>(&vlr->vcol[ui2]);
			ostr << " vcol_b_r=\"" << (float)pt[3]/255.f << "\" vcol_b_g=\"" << (float)pt[2]/255.f
					 << "\" vcol_b_b=\"" << (float)pt[1]/255.f << "\"";
			pt = reinterpret_cast<unsigned char*>(&vlr->vcol[ui3]);
			ostr << " vcol_c_r=\"" << (float)pt[3]/255.f << "\" vcol_c_g=\"" << (float)pt[2]/255.f
					 << "\" vcol_c_b=\"" << (float)pt[1]/255.f << "\"";
		}
		ostr << " shader_name=\"" << fmatname << "\" />\n";

		if (vlr->v4) {

			idx1 = vert_idx.find(vlr->v3)->second;
			idx2 = vert_idx.find(vlr->v4)->second;
			idx3 = vert_idx.find(vlr->v1)->second;

			// make sure the indices point to the vertices when orco coords exported
			if (EXPORT_ORCO) { idx1*=2;  idx2*=2;  idx3*=2; }

			ostr << "\t\t\t<f a=\"" << idx1 << "\" b=\"" << idx2 << "\" c=\"" << idx3 << "\"";

			// increment uv & vcol indices
			ui1 = (ui1+2) & 3;
			ui2 = (ui2+2) & 3;
			ui3 = (ui3+2) & 3;

			if (uvc) {
				ostr << " u_a=\"" << uvc->uv[ui1][0] << "\" v_a=\"" << 1-uvc->uv[ui1][1] << "\""
						 << " u_b=\"" << uvc->uv[ui2][0] << "\" v_b=\"" << 1-uvc->uv[ui2][1] << "\""
						 << " u_c=\"" << uvc->uv[ui3][0] << "\" v_c=\"" << 1-uvc->uv[ui3][1] << "\"";
			}
			if ((EXPORT_VCOL) && (vlr->vcol)) {
				// vertex colors
				float vr, vg, vb;
				vr = ((vlr->vcol[ui1] >> 24) & 255)/255.0;
				vg = ((vlr->vcol[ui1] >> 16) & 255)/255.0;
				vb = ((vlr->vcol[ui1] >> 8) & 255)/255.0;
				ostr << " vcol_a_r=\"" << vr << "\" vcol_a_g=\"" << vg << "\" vcol_a_b=\"" << vb << "\"";
				vr = ((vlr->vcol[ui2] >> 24) & 255)/255.0;
				vg = ((vlr->vcol[ui2] >> 16) & 255)/255.0;
				vb = ((vlr->vcol[ui2] >> 8) & 255)/255.0;
				ostr << " vcol_b_r=\"" << vr << "\" vcol_b_g=\"" << vg << "\" vcol_b_b=\"" << vb << "\"";
				vr = ((vlr->vcol[ui3] >> 24) & 255)/255.0;
				vg = ((vlr->vcol[ui3] >> 16) & 255)/255.0;
				vb = ((vlr->vcol[ui3] >> 8) & 255)/255.0;
				ostr << " vcol_c_r=\"" << vr << "\" vcol_c_g=\"" << vg << "\" vcol_c_b=\"" << vb << "\"";
			}
			ostr << " shader_name=\"" << fmatname << "\" />\n";

		}
		xmlfile << ostr.str();
	}
	xmlfile << "\t\t</faces>\n\t</mesh>\n</object>\n</transform>\n\n";
}


// write all objects
void yafrayFileRender_t::writeAllObjects()
{

	// first all objects except dupliverts (and main instance object for dups)
	for (map<Object*, vector<VlakRen*> >::const_iterator obi=all_objects.begin();
			obi!=all_objects.end(); ++obi)
	{
		// skip main duplivert object if in dupliMtx_list, written later
		Object* obj = obi->first;
		if (dupliMtx_list.find(string(obj->id.name))!=dupliMtx_list.end()) continue;
		writeObject(obj, obi->second, obj->obmat);
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
		for (unsigned int curmtx=16;curmtx<dupMtx->second.size();curmtx+=16) {	// number of 4x4 matrices

			// new mtx
			for (int i=0;i<4;i++)
				for (int j=0;j<4;j++)
					nmat[i][j] = dupMtx->second[curmtx+(i<<2)+j];

			MTC_Mat4MulMat4(cmat, imat, nmat);	// transform with respect to original = inverse_original * new

			ostr.str("");
			// yafray matrix = transpose of Blender
			ostr << "<transform m00=\"" << cmat[0][0] << "\" m01=\"" << cmat[1][0]
					 <<         "\" m02=\"" << cmat[2][0] << "\" m03=\"" << cmat[3][0] << "\"\n";
			ostr << "           m10=\"" << cmat[0][1] << "\" m11=\"" << cmat[1][1]
					 <<         "\" m12=\"" << cmat[2][1] << "\" m13=\"" << cmat[3][1] << "\"\n";
			ostr << "           m20=\"" << cmat[0][2] << "\" m21=\"" << cmat[1][2]
					 <<         "\" m22=\"" << cmat[2][2] << "\" m23=\"" << cmat[3][2] << "\"\n";
			ostr << "           m30=\"" << cmat[0][3] << "\" m31=\"" << cmat[1][3]
					 <<         "\" m32=\"" << cmat[2][3] << "\" m33=\"" << cmat[3][3] << "\">\n";
			xmlfile << ostr.str();

			// new name from original
			ostr.str("");
			ostr << "<object name=\"" << obj->id.name << "_dup" << (curmtx>>4) << "\" original=\"" << obj->id.name << "\" >\n";
			xmlfile << ostr.str();
			xmlfile << "\t<attributes>\n\t</attributes>\n\t<null/>\n</object>\n</transform>\n\n";

		}

	}

}

void yafrayFileRender_t::writeAreaLamp(LampRen* lamp, int num, float iview[4][4])
{
	if (lamp->area_shape!=LA_AREA_SQUARE) return;
	float *a=lamp->area[0], *b=lamp->area[1], *c=lamp->area[2], *d=lamp->area[3];
	float power=lamp->energy;
	
	ostr.str("");
	string md = "off";
	// if no GI used, the GIphotons flag can still be set, so only use when 'full' selected
	if ((R.r.GImethod==2) && (R.r.GIphotons)) { md="on";  power*=R.r.GIpower; }
	ostr << "<light type=\"arealight\" name=\"LAMP" << num+1 << "\" dummy=\""<< md << "\" power=\"" << power << "\" ";
	// samples not used for GI with photons, can still be exported, is ignored
	int psm=0, sm = lamp->ray_totsamp;
	if (sm>=25) psm = sm/5;
	ostr << "samples=\"" << sm << "\" psamples=\"" << psm << "\" ";
	ostr << ">\n";
	
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
	ostr << "\t<a x=\""<< lpco[0][0] <<"\" y=\""<< lpco[0][1] <<"\" z=\"" << lpco[0][2] <<"\" />\n";
	ostr << "\t<b x=\""<< lpco[1][0] <<"\" y=\""<< lpco[1][1] <<"\" z=\"" << lpco[1][2] <<"\" />\n";
	ostr << "\t<c x=\""<< lpco[2][0] <<"\" y=\""<< lpco[2][1] <<"\" z=\"" << lpco[2][2] <<"\" />\n";
	ostr << "\t<d x=\""<< lpco[3][0] <<"\" y=\""<< lpco[3][1] <<"\" z=\"" << lpco[3][2] <<"\" />\n";

	ostr << "\t<color r=\"" << lamp->r << "\" g=\"" << lamp->g << "\" b=\"" << lamp->b << "\" />\n";
	ostr << "</light>\n\n";
	xmlfile << ostr.str();
}

void yafrayFileRender_t::writeLamps()
{
	// inverse viewmatrix needed for back2world transform
	float iview[4][4];
	// R.viewinv != inv.R.viewmat because of possible ortho mode (see convertBlenderScene.c)
	// have to invert it here
	MTC_Mat4Invert(iview, R.viewmat);
	
	// all lamps
	for (int i=0;i<R.totlamp;i++)
	{
		ostr.str("");
		LampRen* lamp = R.la[i];
		
		if (lamp->type==LA_AREA) { writeAreaLamp(lamp, i, iview);  continue; }
		
		// TODO: add decay setting in yafray
		ostr << "<light type=\"";
		bool is_softL=false, is_sphereL=false;
		if (lamp->type==LA_LOCAL) {
			if (lamp->mode & LA_YF_SOFT) {
				// shadowmapped omnidirectional light
				ostr << "softlight";
				is_softL = true;
			}
			else if ((lamp->mode & LA_SHAD_RAY) && (lamp->YF_ltradius>0.0)) {
				// area sphere, only when ray shadows enabled and radius>0.0
				ostr << "spherelight";
				is_sphereL = true;
			}
			else ostr << "pointlight";
		}
		else if (lamp->type==LA_SPOT)
			ostr << "spotlight";
		else if ((lamp->type==LA_SUN) || (lamp->type==LA_HEMI))	// hemi exported as sun
			ostr << "sunlight";
		else if (lamp->type==LA_YF_PHOTON)
			ostr << "photonlight";
		else {
			// possibly unknown type, ignore
			cout << "Unknown Blender lamp type: " << lamp->type << endl;
			continue;
		}
		
		//no name available here, create one
		ostr << "\" name=\"LAMP" << i+1;
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
			if ((R.r.GImethod==2) && (R.r.GIphotons)) { md="on";  pwr*=R.r.GIpower; }
			ostr << "\" power=\"" <<  pwr << "\" dummy=\"" << md << "\"";
		}
		else ostr << "\" power=\"" << pwr << "\"";
		
		// cast_shadows flag not used with softlight, spherelight or photonlight
		if ((!is_softL) && (!is_sphereL) && (lamp->type!=LA_YF_PHOTON)) {
			string lpmode="off";
			// Shadows only when Blender has shadow button enabled, only spots use LA_SHAD flag.
			// Also blender hemilights exported as sunlights which might have shadow flag set
			// should have cast_shadows set to off (reported by varuag)
			if (lamp->type!=LA_HEMI) {
				if (R.r.mode & R_SHADOW)
					if (((lamp->type==LA_SPOT) && (lamp->mode & LA_SHAD)) || (lamp->mode & LA_SHAD_RAY)) lpmode="on";
			}
			ostr << " cast_shadows=\"" << lpmode << "\"";
		}

		
		// spot specific stuff
		bool has_halo = ((lamp->type==LA_SPOT) && (lamp->mode & LA_HALO) && (lamp->haint>0.0));
		if (lamp->type==LA_SPOT) {
			// conversion already changed spotsize to cosine of half angle
			float ld = 1-lamp->spotsi;	//convert back to blender slider setting
			if (ld!=0) ld = 1.f/ld;
			ostr	<< " size=\"" << acos(lamp->spotsi)*180.0/M_PI << "\""
						<< " blend=\"" << lamp->spotbl*ld << "\""
						<< " beam_falloff=\"2\"";	// no Blender equivalent (yet)
			// halo params
			if (has_halo) {
				ostr << "\n\thalo=\"on\" " << "res=\"" << lamp->YF_bufsize << "\"\n";
				int hsmp = ((12-lamp->shadhalostep)*16)/12;
				hsmp = (hsmp+1)*16;	// makes range (16, 272) for halostep(12, 0), good enough?
				ostr << "\tsamples=\"" << hsmp <<  "\" shadow_samples=\"" << (lamp->samp*lamp->samp) << "\"\n";
				ostr << "\thalo_blur=\"0\" shadow_blur=\"" << (lamp->soft*0.01f) << "\"\n";
				ostr << "\tfog_density=\"" << (lamp->haint*0.2f) << "\"";
			}
		}
		else if (is_softL) {
			// softlight
			ostr	<< " res=\"" << lamp->YF_bufsize << "\""
						<< " radius=\"" << lamp->soft << "\""
						<< " bias=\"" << lamp->bias << "\"";
		}
		else if (is_sphereL) {
			// spherelight
			int psm=0, sm = lamp->ray_samp*lamp->ray_samp;
			if (sm>=25) psm = sm/5;
			ostr	<< " radius=\"" << lamp->YF_ltradius << "\""
						<< " samples=\"" << sm << "\""
						<< " psamples=\"" << psm << "\""
						<< " qmc_method=\"1\"";
		}
		else if (lamp->type==LA_YF_PHOTON) {
			string qmc="off";
			if (lamp->YF_useqmc) qmc="on";
			ostr	<< "\n\tphotons=\"" << lamp->YF_numphotons << "\""
						<< " search=\"" << lamp->YF_numsearch << "\""
						<< " depth=\"" << lamp->YF_phdepth << "\""
						<< " use_QMC=\"" << qmc << "\""
						<< " angle=\"" << acos(lamp->spotsi)*180.0/M_PI << "\"";
			float cl = lamp->YF_causticblur/sqrt((float)lamp->YF_numsearch);
			ostr	<< "\n\tfixedradius=\"" << lamp->YF_causticblur << "\" cluster=\"" << cl << "\"";
		}
		ostr << " >\n";

		// transform lamp co & vec back to world
		float lpco[3], lpvec[3];
		MTC_cp3Float(lamp->co, lpco);
		MTC_Mat4MulVecfl(iview, lpco);
		MTC_cp3Float(lamp->vec, lpvec);
		MTC_Mat4Mul3Vecfl(iview, lpvec);

		// position, (==-blendir for sun/hemi)
		if ((lamp->type==LA_SUN) || (lamp->type==LA_HEMI))
			ostr << "\t<from x=\"" << -lpvec[0] << "\" y=\"" << -lpvec[1] << "\" z=\"" << -lpvec[2] << "\" />\n";
		else
			ostr << "\t<from x=\"" << lpco[0] << "\" y=\"" << lpco[1] << "\" z=\"" << lpco[2] << "\" />\n";		
		// 'to' for spot/photonlight, already calculated by Blender
		if ((lamp->type==LA_SPOT) || (lamp->type==LA_YF_PHOTON)) {
			ostr << "\t<to x=\"" << lpco[0] + lpvec[0]
							<< "\" y=\"" << lpco[1] + lpvec[1]
							<< "\" z=\"" << lpco[2] + lpvec[2] << "\" />\n";
			if (has_halo) ostr << "\t<fog r=\"1\" g=\"1\" b=\"1\" />\n";
		}

		// color
		// rgb in LampRen is premultiplied by energy, power is compensated for that above
		ostr << "\t<color r=\"" << lamp->r << "\" g=\"" << lamp->g << "\" b=\"" << lamp->b << "\" />\n";
		ostr << "</light>\n\n";
		xmlfile << ostr.str();
	}
}

// write main camera
void yafrayFileRender_t::writeCamera()
{
	// here Global used again
	ostr.str("");
	ostr << "<camera name=\"MAINCAM\" ";
	if (R.r.mode & R_ORTHO)
		ostr << "type=\"ortho\"";
	else	
		ostr << "type=\"perspective\"";

	// render resolution including the percentage buttons (aleady calculated in initrender for R renderdata)
	ostr << " resx=\"" << R.r.xsch << "\" resy=\"" << R.r.ysch << "\"";

	float f_aspect = 1;
	if ((R.r.xsch*R.r.xasp)<=(R.r.ysch*R.r.yasp)) f_aspect = float(R.r.xsch*R.r.xasp)/float(R.r.ysch*R.r.yasp);
	ostr << "\n\tfocal=\"" << mainCamLens/(f_aspect*32.f);
	ostr << "\" aspect_ratio=\"" << R.ycor << "\"";

	// dof params, only valid for real camera
	float fdist = 1;	// only changes for ortho
	if (maincam_obj->type==OB_CAMERA) {
		Camera* cam = (Camera*)maincam_obj->data;
		if (R.r.mode & R_ORTHO) fdist = cam->ortho_scale*(mainCamLens/32.f);
		ostr << "\n\tdof_distance=\"" << cam->YF_dofdist << "\"";
		ostr << " aperture=\"" << cam->YF_aperture << "\"";
		string st = "on";
		if (cam->flag & CAM_YF_NO_QMC) st = "off";
		ostr << " use_qmc=\"" << st << "\"";
		// bokeh params
		st = "disk1";
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
		ostr << "\n\tbokeh_type=\"" << st << "\"";
		st = "uniform";
		if (cam->YF_bkhbias==1)
			st = "center";
		else if (cam->YF_bkhbias==2)
			st = "edge";
		ostr << " bokeh_bias=\"" << st << "\"";
		ostr << " bokeh_rotation=\"" << cam->YF_bkhrot << "\"";
	}

	ostr << " >\n";
	xmlfile << ostr.str();

	ostr.str("");
	ostr << "\t<from x=\"" << maincam_obj->obmat[3][0] << "\""
							<< " y=\"" << maincam_obj->obmat[3][1] << "\""
							<< " z=\"" << maincam_obj->obmat[3][2] << "\" />\n";
	ostr << "\t<to x=\"" << maincam_obj->obmat[3][0] - fdist * R.viewmat[0][2]
					<< "\" y=\"" << maincam_obj->obmat[3][1] - fdist * R.viewmat[1][2]
					<< "\" z=\"" << maincam_obj->obmat[3][2] - fdist * R.viewmat[2][2] << "\" />\n";
	ostr << "\t<up x=\"" << maincam_obj->obmat[3][0] + R.viewmat[0][1]
					<< "\" y=\"" << maincam_obj->obmat[3][1] + R.viewmat[1][1]
					<< "\" z=\"" << maincam_obj->obmat[3][2] + R.viewmat[2][1] << "\" />\n";
	xmlfile << ostr.str();

	xmlfile << "</camera>\n\n";
}

void yafrayFileRender_t::writeHemilight()
{
	ostr.str("");
	ostr << "<light type=\"hemilight\" name=\"hemi_LT\" power=\"" << R.r.GIpower << "\"";
	switch (R.r.GIquality)
	{
		case 1 :
		case 2 : ostr << " samples=\"16\" >\n";  break;
		case 3 : ostr << " samples=\"36\" >\n";  break;
		case 4 : ostr << " samples=\"64\" >\n";  break;
		case 5 : ostr << " samples=\"128\" >\n";  break;
		default: ostr << " samples=\"25\" >\n";
	}
	ostr << "</light>\n\n";
	xmlfile << ostr.str();
}

void yafrayFileRender_t::writePathlight()
{
	ostr.str("");
	if (R.r.GIphotons)
	{
		ostr << "<light type=\"globalphotonlight\" name=\"gpm\" photons=\""<<R.r.GIphotoncount<<"\""<<endl;
		ostr << "\tradius=\"" <<R.r.GIphotonradius << "\" depth=\""<< ((R.r.GIdepth>2) ? (R.r.GIdepth-1) : 1)
				 << "\" caus_depth=\""<<R.r.GIcausdepth<< "\" search=\""<< R.r.GImixphotons<<"\" >"<<endl;
		ostr << "</light>"<<endl;
	}
	ostr << "<light type=\"pathlight\" name=\"path_LT\" power=\"" << R.r.GIindirpower << "\"";
	ostr << " depth=\"" << ((R.r.GIphotons) ? 1 : R.r.GIdepth) << "\" caus_depth=\"" << R.r.GIcausdepth <<"\"\n";
	if(R.r.GIdirect && R.r.GIphotons) ostr << "direct=\"on\"" << endl;
	if (R.r.GIcache && ! (R.r.GIdirect && R.r.GIphotons))
	{
		switch (R.r.GIquality)
		{
			case 1 : ostr << " samples=\"128\" \n";   break;
			case 2 : ostr << " samples=\"256\" \n";   break;
			case 3 : ostr << " samples=\"512\" \n";   break;
			case 4 : ostr << " samples=\"1024\" \n";  break;
			case 5 : ostr << " samples=\"2048\" \n";  break;
			default: ostr << " samples=\"512\" \n";
		}
		float sbase = 2.0/float(R.r.xsch);
		ostr << " cache=\"on\" use_QMC=\"on\" threshold=\"" << R.r.GIrefinement << "\"" << endl;
		ostr << " cache_size=\"" << sbase*R.r.GIpixelspersample << "\" shadow_threshold=\"" <<
			1.0 - R.r.GIshadowquality << "\" grid=\"82\" search=\"35\" >\n";
	}
	else
	{
		switch (R.r.GIquality)
		{
			case 1 : ostr << " samples=\"16\" >\n";   break;
			case 2 : ostr << " samples=\"36\" >\n";   break;
			case 3 : ostr << " samples=\"64\" >\n";   break;
			case 4 : ostr << " samples=\"128\" >\n";  break;
			case 5 : ostr << " samples=\"256\" >\n";  break;
			default: ostr << " samples=\"25\" >\n";
		}
	}
	ostr << "</light>\n\n";
	xmlfile << ostr.str();
}

bool yafrayFileRender_t::writeWorld()
{
	World *world = G.scene->world;
	if (R.r.GIquality!=0) {
		if (R.r.GImethod==1) {
			if (world==NULL) cout << "WARNING: need world background for skydome!\n";
			writeHemilight();
		}
		else if (R.r.GImethod==2) writePathlight();
	}

	if (world==NULL) return false;

	for (int i=0;i<MAX_MTEX;i++) {
		MTex* wtex = world->mtex[i];
		if (!wtex) continue;
		Image* wimg = wtex->tex->ima;
		if ((wtex->tex->type==TEX_IMAGE) && (wimg!=NULL)) {
			string wt_path = wimg->name;
			adjustPath(wt_path);
			if (BLI_testextensie(wimg->name, ".hdr")) {
				ostr.str("");
				ostr << "<background type=\"HDRI\" name=\"world_background\" ";
				// since exposure adjust is an integer, using the texbri slider isn't actually very useful here (result either -1/0/1)
				// GIpower could be used, but is only active for GI
				ostr << "exposure_adjust=\"" << int(world->mtex[i]->tex->bright-1) << "\" mapping=\"probe\" >\n";
				ostr << "\t<filename value=\"" << wt_path << "\" />\n";
				ostr << "</background>\n\n";
				xmlfile << ostr.str();
				return true;
			}
			else if (BLI_testextensie(wimg->name, ".jpg") || BLI_testextensie(wimg->name, ".jpeg") || BLI_testextensie(wimg->name, ".tga")) {
				ostr.str("");
				ostr << "<background type=\"image\" name=\"world_background\" >\n";
				/*
				// not yet in yafray, always assumes spheremap for now, not the same as in Blender,
				// which for some reason is scaled by 2 in Blender???
				if (wtex->texco & TEXCO_ANGMAP)
					ostr << " mapping=\"probe\" >\n";
				else
					ostr << " mapping=\"sphere\" >\n";
				*/
				ostr << "\t<filename value=\"" << wt_path << "\" />\n";
				ostr << "</background>\n\n";
				xmlfile << ostr.str();
				return true;
			}
		}
	}

	ostr.str("");
	ostr << "<background type=\"constant\" name=\"world_background\" >\n";
	// if no GI used, the GIpower parameter is not always initialized, so in that case ignore it
	// (have to change method to init yafray vars in Blender)
	float bg_mult = (R.r.GImethod==0) ? 1 : R.r.GIpower;
	ostr << "\t<color r=\"" << (world->horr * bg_mult) << 
								"\" g=\"" << (world->horg * bg_mult) << 
								"\" b=\"" << (world->horb * bg_mult) << "\" />\n";
	ostr << "</background>\n\n";
	xmlfile << ostr.str();

	return true;
}

bool yafrayFileRender_t::executeYafray(const string &xmlpath)
{
	char yfr[8];
	sprintf(yfr, "%d ", R.r.YF_numprocs);
	string command = command_path + "yafray -c " + yfr + "\"" + xmlpath + "\"";
#ifndef WIN32
	sigset_t yaf,old;
	sigemptyset(&yaf);
	sigaddset(&yaf, SIGVTALRM);
	sigprocmask(SIG_BLOCK, &yaf, &old);
	int ret=system(command.c_str());
	sigprocmask(SIG_SETMASK, &old, NULL);
	if (WIFEXITED(ret))
	{
		if (WEXITSTATUS(ret)) cout<<"Executed -"<<command<<"-"<<endl;
		switch (WEXITSTATUS(ret))
		{
			case 0: cout << "Yafray completed successfully\n";  return true;
			case 127: cout << "Yafray not found\n";  return false;
			case 126: cout << "Yafray: permission denied\n";  return false;
			default: cout << "Yafray exited with errors\n";  return false;
		}
	}
	else if (WIFSIGNALED(ret))
		cout << "Yafray crashed\n";
	else
		cout << "Unknown error\n";
	return false;
#else
	int ret=system(command.c_str());
	return ret==0;
#endif
	
}
