#include"export_File.h"

#include <math.h>

using namespace std;

static string command_path = "";

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
		cout << "No export directory set in user defaults!\n";
		dir_failed = true;
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

	//if( (G.scene->world!=NULL) && (G.scene->world->GIquality>1) && ! G.scene->world->cache )
	if ((R.r.GImethod!=0) && (R.r.GIquality>1) && (!R.r.GIcache))
		ostr << "\tAA_passes=\"5\" AA_minsamples=\"5\" " << endl;
	else if ((R.r.mode & R_OSA) && (R.r.osa)) {
		int passes=(R.r.osa%4)==0 ? R.r.osa/4 : 1;
		int minsamples=(R.r.osa%4)==0 ? 4 : R.r.osa;
		ostr << "\tAA_passes=\"" << passes << "\" AA_minsamples=\"" << minsamples << "\"";
	}
	else ostr << "\tAA_passes=\"0\" AA_minsamples=\"1\"";

	ostr << "\n";

	if (hasworld) ostr << "\tbackground_name=\"world_background\"\n";

	ostr << "\tAA_pixelwidth=\"2\" AA_threshold=\"0.05\" bias=\""<<R.r.YF_raybias<<"\" >\n";

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


void yafrayFileRender_t::writeTextures()
{
	for (map<string, pair<Material*, MTex*> >::const_iterator blendtex=used_textures.begin();
						blendtex!=used_textures.end();++blendtex) {
		//Material* matr = blendtex->second.first;
		MTex* mtex = blendtex->second.second;
		Tex* tex = mtex->tex;
		switch (tex->type) {
			case TEX_STUCCI:
				// stucci is clouds as bump, but could be added to yafray to handle both wall in/out as well.
				// noisedepth must be at least 1 in yafray
			case TEX_CLOUDS: {
				ostr.str("");
				ostr << "<shader type=\"clouds\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t<attributes>\n";
				ostr << "\t\t<depth value=\"" << tex->noisedepth+1 << "\" />\n";
				ostr << "\t</attributes>\n";
				ostr << "</shader >\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_WOOD: {
				ostr.str("");
				ostr << "<shader type=\"wood\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t\t<attributes>\n";
				ostr << "\t\t<depth value=\"" << tex->noisedepth+1 << "\" />\n";
				ostr << "\t\t<turbulence value=\"" << tex->turbul << "\" />\n";
				ostr << "\t\t<ringscale_x value=\"" << mtex->size[0] << "\" />\n";
				ostr << "\t\t<ringscale_y value=\"" << mtex->size[1] << "\" />\n";
				string ts = "on";
				if (tex->noisetype==TEX_NOISESOFT) ts = "off";
				ostr << "\t\t<hard value=\"" << ts << "\" />\n";
				ostr << "\t</attributes>\n";
				ostr << "</shader>\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_MARBLE: {
				ostr.str("");
				ostr << "<shader type=\"marble\" name=\"" << blendtex->first << "\" >\n";
				ostr << "\t<attributes>\n";
				ostr << "\t\t<depth value=\"" << tex->noisedepth+1 << "\" />\n";
				ostr << "\t\t<turbulence value=\"" << tex->turbul << "\" />\n";
				string ts = "on";
				if (tex->noisetype==TEX_NOISESOFT) ts = "off";
				ostr << "\t\t<hard value=\"" << ts << "\" />\n";
				ts = "1";
				if (tex->stype==1) ts="5"; else if (tex->stype==2) ts="10";
				ostr << "\t\t<sharpness value=\"" << ts << "\" />\n";
				ostr << "\t</attributes>\n";
				ostr << "</shader>\n\n";
				xmlfile << ostr.str();
				break;
			}
			case TEX_IMAGE: {
				Image* ima = tex->ima;
				if (ima) {
					ostr.str("");
					ostr << "<shader type=\"image\" name=\"" << blendtex->first << "\" >\n";
					ostr << "\t<attributes>\n";
					// image->name is full path
					string texpath = ima->name;
#ifdef WIN32
					// add drive char if not there
					addDrive(texpath);
#endif
					ostr << "\t\t<filename value=\"" << texpath << "\" />\n";
					ostr << "\t</attributes>\n";
					ostr << "</shader>\n\n";
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
}


// write all materials & modulators
void yafrayFileRender_t::writeMaterialsAndModulators()
{
	for (map<string, Material*>::const_iterator blendmat=used_materials.begin();
		blendmat!=used_materials.end();++blendmat) {

		Material* matr = blendmat->second;

		// blendermappers
		for (int m=0;m<8;m++) {

			if (matr->septex & (1<<m)) continue;// all active channels

			// ignore null mtex
			MTex* mtex = matr->mtex[m];
			if (mtex==NULL) continue;
			// ignore null tex
			Tex* tex = mtex->tex;
			if (tex==NULL) continue;

			//map<string, pair<Material*, MTex*> >::const_iterator mtexL = used_textures.find(string(tex->id.name+2));
			// now included the full name
			map<string, pair<Material*, MTex*> >::const_iterator mtexL = used_textures.find(string(tex->id.name));
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
					ostr << "\n           m00=\"" << itexmat[0][0] << "\" m01=\"" << itexmat[1][0]
							 <<           "\" m02=\"" << itexmat[2][0] << "\" m03=\"" << itexmat[3][0] << "\"\n\t";
					ostr <<   "           m10=\"" << itexmat[0][1] << "\" m11=\"" << itexmat[1][1]
							 <<           "\" m12=\"" << itexmat[2][1] << "\" m13=\"" << itexmat[3][1] << "\"\n\t";
					ostr <<   "           m20=\"" << itexmat[0][2] << "\" m21=\"" << itexmat[1][2]
							 <<           "\" m22=\"" << itexmat[2][2] << "\" m23=\"" << itexmat[3][2] << "\"\n\t";
					ostr <<   "           m30=\"" << itexmat[0][3] << "\" m31=\"" << itexmat[1][3]
							 <<           "\" m32=\"" << itexmat[2][3] << "\" m33=\"" << itexmat[3][3] << "\">\n";
				}
				else ostr << ">\n";
				ostr << "\t<attributes>\n";

				if ((tex->flag & TEX_COLORBAND) & (tex->coba!=NULL))
					ostr << "\t\t<input value=\"" << mtexL->first + "_coba" << "\" />\n";
				else
					ostr << "\t\t<input value=\"" << mtexL->first << "\" />\n";

				// size, if the texturetype is clouds/marble/wood, also take noisesize into account
				float sc = 1;
				if ((tex->type==TEX_CLOUDS) || (tex->type==TEX_MARBLE) || (tex->type==TEX_WOOD)) {
					sc = tex->noisesize;
					if (sc!=0) sc = 1.f/sc;

				}
				// texture size
				ostr << "\t\t<sizex value=\"" << mtex->size[0]*sc << "\" />\n";
				ostr << "\t\t<sizey value=\"" << mtex->size[1]*sc << "\" />\n";
				ostr << "\t\t<sizez value=\"" << mtex->size[2]*sc << "\" />\n";

				// texture offset
				ostr << "\t\t<ofsx value=\"" << mtex->ofs[0] << "\" />\n";
				ostr << "\t\t<ofsy value=\"" << mtex->ofs[1] << "\" />\n";
				ostr << "\t\t<ofsz value=\"" << mtex->ofs[2] << "\" />\n";

				// texture coordinates, have to disable 'sticky' in Blender
				if ((mtex->texco & TEXCO_UV) || (matr->mode & MA_FACETEXTURE))
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

					// texture projection axes
					string proj = "nxyz";		// 'n' for 'none'
					ostr << "\t\t<proj_x value=\"" << proj[mtex->projx] << "\" />\n";
					ostr << "\t\t<proj_y value=\"" << proj[mtex->projy] << "\" />\n";
					ostr << "\t\t<proj_z value=\"" << proj[mtex->projz] << "\" />\n";

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

		// blendershaders + modulators
		ostr.str("");
		ostr << "<shader type=\"blendershader\" name=\"" << blendmat->first << "\" >\n";
		ostr << "\t<attributes>\n";
		float diff=matr->alpha;
		ostr << "\t\t<color r=\"" << matr->r*diff << "\" g=\"" << matr->g*diff << "\" b=\"" << matr->b*diff << "\" />\n";
		ostr << "\t\t<specular_color r=\"" << matr->specr << "\" g=\"" << matr->specg << "\" b=\"" << matr->specb<< "\" />\n";
		ostr << "\t\t<mirror_color r=\"" << matr->mirr << "\" g=\"" << matr->mirg << "\" b=\"" << matr->mirb << "\" />\n";
		ostr << "\t\t<diffuse_reflect value=\"" << matr->ref << "\" />\n";
		ostr << "\t\t<specular_amount value=\"" << matr->spec << "\" />\n";
		ostr << "\t\t<hard value=\"" << matr->har << "\" />\n";
		ostr << "\t\t<alpha value=\"" << matr->alpha << "\" />\n";
		ostr << "\t\t<emit value=\"" << (matr->emit * R.r.GIpower) << "\" />\n";

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
		for (int m2=0;m2<8;m2++) {

			if (matr->septex & (1<<m2)) continue;// all active channels

			// ignore null mtex
			MTex* mtex = matr->mtex[m2];
			if (mtex==NULL) continue;

			// ignore null tex
			Tex* tex = mtex->tex;
			if (tex==NULL) continue;

			//map<string, pair<Material*, MTex*> >::const_iterator mtexL = used_textures.find(string(tex->id.name+2));
			map<string, pair<Material*, MTex*> >::const_iterator mtexL = used_textures.find(string(tex->id.name));
			if (mtexL!=used_textures.end()) {

				ostr.str("");
				ostr << "\t<modulator>\n";
				ostr << "\t\t<input value=\"" << blendmat->first + "_map" << m2 << "\" />\n";

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
					// for yafray, bump factor is negated (unless negative option of 'Nor', is not affected by 'Neg')
					// scaled down quite a bit for yafray when image type, otherwise used directly
					float nf = -mtex->norfac;
					if (mtex->maptoneg & MAP_NORM) nf *= -1.f;
					if (tex->type==TEX_IMAGE) nf *= 2e-3f;
					ostr << "\t\t<normal value=\"" << nf << "\" />\n";

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

				ostr << "\t</modulator>\n";
				xmlfile << ostr.str();

			}
		}
		xmlfile << "</shader>\n\n";
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
	ostr << "<object name=\"" << obj->id.name+2 << "\"";
	// yafray still needs default shader name in object def.,
	// since we write a shader with every face, simply use the material of the first face
	// if this is an empty string, assume default mat
	char* matname = VLR_list[0]->mat->id.name;
	bool shadow=VLR_list[0]->mat->mode & MA_TRACEBLE;
	ostr <<" shadow=\""<< (shadow ? "on" : "off" )<<"\" ";
	if (VLR_list[0]->mat->mode & MA_RAYTRANSP) 
		ostr << "caus_IOR=\"" << VLR_list[0]->mat->ang << "\" ";
	if (strlen(matname)==0) matname = "blender_default"; 
	//else matname+=2;	//skip MA id
	ostr << " shader_name=\"" << matname << "\" >\n";
	ostr << "\t<attributes>\n";
	if (VLR_list[0]->mat->mode & MA_RAYTRANSP) 
	{
			float tr=1.0-VLR_list[0]->mat->alpha;
			ostr << "\t\t<caus_tcolor r=\"" << VLR_list[0]->mat->r * tr 
					 << "\" g=\"" << VLR_list[0]->mat->g * tr 
					 << "\" b=\"" << VLR_list[0]->mat->b * tr << "\" />\n";
	}
	ostr << "\t</attributes>\n";
	xmlfile << ostr.str();

	// if any face in the Blender mesh uses an orco texture, every face has orco coords,
	// so only need to check the first facevtx.orco in the list if they need to be exported
	bool EXPORT_ORCO = (VLR_list[0]->v1->orco!=NULL);

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
		if (VLR_list[0]->flag & ME_SMOOTH)
			xmlfile << "\t<mesh autosmooth=\"90\" has_orco=\"" << has_orco << "\" >\n";
		else
			xmlfile << "\t<mesh autosmooth=\"0.1\" has_orco=\"" << has_orco << "\" >\n";	//0 shows artefacts
	}

	// now all vertices
	map<VertRen*, int> vert_idx;	// for removing duplicate verts and creating an index list
	int vidx = 0;	// vertex index counter

	xmlfile << "\t\t<points>\n";
	for (vector<VlakRen*>::const_iterator fci=VLR_list.begin();
				fci!=VLR_list.end();++fci)
	{
		VlakRen* vlr = *fci;
		VertRen* ver;
		float* orco;
		ostr.str("");
		if (vert_idx.find(vlr->v1)==vert_idx.end()) {
			vert_idx[vlr->v1] = vidx++;
			ver = vlr->v1;
			ostr << "\t\t\t<p x=\"" << ver->co[0]
								 << "\" y=\"" << ver->co[1]
								 << "\" z=\"" << ver->co[2] << "\" />\n";
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
			ostr << "\t\t\t<p x=\"" << ver->co[0]
								 << "\" y=\"" << ver->co[1]
								 << "\" z=\"" << ver->co[2] << "\" />\n";
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
			ostr << "\t\t\t<p x=\"" << ver->co[0]
								 << "\" y=\"" << ver->co[1]
								 << "\" z=\"" << ver->co[2] << "\" />\n";
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
			ostr << "\t\t\t<p x=\"" << ver->co[0]
								 << "\" y=\"" << ver->co[1]
								 << "\" z=\"" << ver->co[2] << "\" />\n";
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
		char* fmatname = fmat->id.name;
		if (strlen(fmatname)==0) fmatname = "blender_default"; 
		//else fmatname+=2;	//skip MA id
		TFace* uvc = vlr->tface;	// possible uvcoords (v upside down)
		int idx1, idx2, idx3;

		idx1 = vert_idx.find(vlr->v1)->second;
		idx2 = vert_idx.find(vlr->v2)->second;
		idx3 = vert_idx.find(vlr->v3)->second;

		// make sure the indices point to the vertices when orco coords exported
		if (EXPORT_ORCO) { idx1*=2;  idx2*=2;  idx3*=2; }

		ostr.str("");
		ostr << "\t\t\t<f a=\"" << idx1 << "\" b=\"" << idx2 << "\" c=\"" << idx3 << "\"";

		if (uvc) {
			// use correct uv coords for this triangle
			if (vlr->flag & R_FACE_SPLIT) {
				ostr << " u_a=\"" << uvc->uv[0][0] << "\" v_a=\"" << 1-uvc->uv[0][1] << "\""
						 << " u_b=\"" << uvc->uv[2][0] << "\" v_b=\"" << 1-uvc->uv[2][1] << "\""
						 << " u_c=\"" << uvc->uv[3][0] << "\" v_c=\"" << 1-uvc->uv[3][1] << "\"";
			}
			else {
				ostr << " u_a=\"" << uvc->uv[0][0] << "\" v_a=\"" << 1-uvc->uv[0][1] << "\""
						 << " u_b=\"" << uvc->uv[1][0] << "\" v_b=\"" << 1-uvc->uv[1][1] << "\""
						 << " u_c=\"" << uvc->uv[2][0] << "\" v_c=\"" << 1-uvc->uv[2][1] << "\"";
			}
		}

		// since Blender seems to need vcols when uvs are used, for yafray only export when the material actually uses vcols
		if ((EXPORT_VCOL) && (vlr->vcol)) {
			// vertex colors
			float vr, vg, vb;
			vr = ((vlr->vcol[0] >> 24) & 255)/255.0;
			vg = ((vlr->vcol[0] >> 16) & 255)/255.0;
			vb = ((vlr->vcol[0] >> 8) & 255)/255.0;
			ostr << " vcol_a_r=\"" << vr << "\" vcol_a_g=\"" << vg << "\" vcol_a_b=\"" << vb << "\"";
			vr = ((vlr->vcol[1] >> 24) & 255)/255.0;
			vg = ((vlr->vcol[1] >> 16) & 255)/255.0;
			vb = ((vlr->vcol[1] >> 8) & 255)/255.0;
			ostr << " vcol_b_r=\"" << vr << "\" vcol_b_g=\"" << vg << "\" vcol_b_b=\"" << vb << "\"";
			vr = ((vlr->vcol[2] >> 24) & 255)/255.0;
			vg = ((vlr->vcol[2] >> 16) & 255)/255.0;
			vb = ((vlr->vcol[2] >> 8) & 255)/255.0;
			ostr << " vcol_c_r=\"" << vr << "\" vcol_c_g=\"" << vg << "\" vcol_c_b=\"" << vb << "\"";
		}
		ostr << " shader_name=\"" << fmatname << "\" />\n";

		if (vlr->v4) {

			idx1 = vert_idx.find(vlr->v3)->second;
			idx2 = vert_idx.find(vlr->v4)->second;
			idx3 = vert_idx.find(vlr->v1)->second;

			// make sure the indices point to the vertices when orco coords exported
			if (EXPORT_ORCO) { idx1*=2;  idx2*=2;  idx3*=2; }

			ostr << "\t\t\t<f a=\"" << idx1 << "\" b=\"" << idx2 << "\" c=\"" << idx3 << "\"";

			if (uvc) {
				ostr << " u_a=\"" << uvc->uv[2][0] << "\" v_a=\"" << 1-uvc->uv[2][1] << "\""
						 << " u_b=\"" << uvc->uv[3][0] << "\" v_b=\"" << 1-uvc->uv[3][1] << "\""
						 << " u_c=\"" << uvc->uv[0][0] << "\" v_c=\"" << 1-uvc->uv[0][1] << "\"";
			}
			if ((EXPORT_VCOL) && (vlr->vcol)) {
				// vertex colors
				float vr, vg, vb;
				vr = ((vlr->vcol[2] >> 24) & 255)/255.0;
				vg = ((vlr->vcol[2] >> 16) & 255)/255.0;
				vb = ((vlr->vcol[2] >> 8) & 255)/255.0;
				ostr << " vcol_a_r=\"" << vr << "\" vcol_a_g=\"" << vg << "\" vcol_a_b=\"" << vb << "\"";
				vr = ((vlr->vcol[3] >> 24) & 255)/255.0;
				vg = ((vlr->vcol[3] >> 16) & 255)/255.0;
				vb = ((vlr->vcol[3] >> 8) & 255)/255.0;
				ostr << " vcol_b_r=\"" << vr << "\" vcol_b_g=\"" << vg << "\" vcol_b_b=\"" << vb << "\"";
				vr = ((vlr->vcol[0] >> 24) & 255)/255.0;
				vg = ((vlr->vcol[0] >> 16) & 255)/255.0;
				vb = ((vlr->vcol[0] >> 8) & 255)/255.0;
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
			ostr << "<object name=\"" << obj->id.name+2 << "_dup" << (curmtx>>4) << "\" original=\"" << obj->id.name+2 << "\" >\n";
			xmlfile << ostr.str();
			xmlfile << "\t<attributes>\n\t</attributes>\n\t<null/>\n</object>\n</transform>\n\n";

		}

	}

}

void yafrayFileRender_t::writeAreaLamp(LampRen* lamp, int num)
{
	if (lamp->area_shape!=LA_AREA_SQUARE) return;
	float *a=lamp->area[0], *b=lamp->area[1], *c=lamp->area[2], *d=lamp->area[3];
	float power=lamp->energy;
	
	ostr.str("");
	string md = "off";
	if (R.r.GIphotons) {md = "on";power*=R.r.GIpower;}
	ostr << "<light type=\"arealight\" name=\"LAMP" << num+1 << "\" dummy=\""<< md << "\" power=\"" << power << "\" ";
	if (!R.r.GIphotons) {
		int psm=0, sm = lamp->ray_totsamp;
		if (sm>=64) psm = sm/4;
		ostr << "samples=\"" << sm << "\" psamples=\"" << psm << "\" ";
	}
	ostr << ">\n";
	ostr << "\t<a x=\""<< a[0] <<"\" y=\""<< a[1] <<"\" z=\"" << a[2] <<"\" />\n";
	ostr << "\t<b x=\""<< b[0] <<"\" y=\""<< b[1] <<"\" z=\"" << b[2] <<"\" />\n";
	ostr << "\t<c x=\""<< c[0] <<"\" y=\""<< c[1] <<"\" z=\"" << c[2] <<"\" />\n";
	ostr << "\t<d x=\""<< d[0] <<"\" y=\""<< d[1] <<"\" z=\"" << d[2] <<"\" />\n";
	ostr << "\t<color r=\"" << lamp->r << "\" g=\"" << lamp->g << "\" b=\"" << lamp->b << "\" />\n";
	ostr << "</light>\n\n";
	xmlfile << ostr.str();
}

void yafrayFileRender_t::writeLamps()
{
	// all lamps
	for (int i=0;i<R.totlamp;i++)
	{
		ostr.str("");
		LampRen* lamp = R.la[i];
		if (lamp->type==LA_AREA) { writeAreaLamp(lamp, i);  continue; }
		// TODO: add decay setting in yafray
		ostr << "<light type=\"";
		if (lamp->type==LA_LOCAL)
			ostr << "pointlight";
		else if (lamp->type==LA_SPOT)
			ostr << "spotlight";
		else if ((lamp->type==LA_SUN) || (lamp->type==LA_HEMI))	// for now, hemi same as sun
			ostr << "sunlight";
		else {
			// possibly unknown type, ignore
			cout << "Unknown Blender lamp type: " << lamp->type << endl;
			continue;
		}
		ostr << "\" name=\"LAMP" << i+1;	//no name available here, create one
		// color already premultiplied by energy, so only need distance here
		float pwr;
		if (lamp->mode & LA_SPHERE) {
			// best approx. as used in LFexport script (LF d.f.m. 4pi?)
			pwr = lamp->dist*(lamp->dist+1)*(0.25/M_PI);
			//decay = 2;
		}
		else {
			if ((lamp->type==LA_LOCAL) || (lamp->type==LA_SPOT)) {
				pwr = lamp->dist;
				//decay = 1;
			}
			else pwr = 1;	// sun/hemi distance irrelevent.
		}
		ostr << "\" power=\"" << pwr;
		string lpmode="off";
		// shadows only when Blender has shadow button enabled, only spots use LA_SHAD flag
		if (R.r.mode & R_SHADOW)
			if (((lamp->type==LA_SPOT) && (lamp->mode & LA_SHAD)) || (lamp->mode & LA_SHAD_RAY)) lpmode="on";;
		ostr << "\" cast_shadows=\"" << lpmode << "\"";
		// spot specific stuff
		if (lamp->type==LA_SPOT) {
			// conversion already changed spotsize to cosine of half angle
			float ld = 1-lamp->spotsi;	//convert back to blender slider setting
			if (ld!=0) ld = 1.f/ld;
			ostr << " size=\"" << acos(lamp->spotsi)*180.0/M_PI << "\""
					<< " blend=\"" << lamp->spotbl*ld << "\""
					<< " beam_falloff=\"2\"";	// no Blender equivalent (yet)
		}
		ostr << " >\n";
		// position
		ostr << "\t<from x=\"" << lamp->co[0] << "\" y=\"" << lamp->co[1] << "\" z=\"" << lamp->co[2] << "\" />\n";
		// 'to' for spot, already calculated by Blender
		if (lamp->type==LA_SPOT)
			ostr << "\t<to x=\"" << lamp->co[0]+lamp->vec[0]
							<< "\" y=\"" << lamp->co[1]+lamp->vec[1]
							<< "\" z=\"" << lamp->co[2]+lamp->vec[2]
							<< "\" />\n";
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
	ostr << "<camera name=\"MAINCAM\"";

	// render resolution including the percentage buttons (aleady calculated in initrender for R renderdata)
	int xres = R.r.xsch;
	int yres = R.r.ysch;
	ostr << " resx=\"" << xres << "\" resy=\"" << yres;

	// aspectratio can be set in Blender as well using aspX & aspY, need an extra param. for yafray cam.
	float aspect = 1;
	if (R.r.xsch < R.r.ysch) aspect = float(R.r.xsch)/float(R.r.ysch);

	ostr << "\" focal=\"" << mainCamLens/(aspect*32.0) << "\" >\n";
	xmlfile << ostr.str();

	// from, to, up vectors
	// comment in MTC_matrixops.h not correct, copy is arg2->arg1
	float camtx[4][4];
	MTC_Mat4CpyMat4(camtx, maincam_obj->obmat);
	MTC_normalise3DF(camtx[1]);	//up
	MTC_normalise3DF(camtx[2]);	//dir
	ostr.str("");
	ostr << "\t<from x=\"" << camtx[3][0] << "\""
							<< " y=\"" << camtx[3][1] << "\""
							<< " z=\"" << camtx[3][2] << "\" />\n";
	Object* dofob = findObject("OBFOCUS");
	if (dofob) {
		// dof empty found, modify lookat point accordingly
		// location from matrix, in case animated
		float fdx = dofob->obmat[3][0] - camtx[3][0];
		float fdy = dofob->obmat[3][1] - camtx[3][1];
		float fdz = dofob->obmat[3][2] - camtx[3][2];
		float fdist = sqrt(fdx*fdx + fdy*fdy + fdz*fdz);
		cout << "FOCUS object found, distance is: " << fdist << endl;
		ostr << "\t<to x=\"" << camtx[3][0] - fdist*camtx[2][0]
						<< "\" y=\"" << camtx[3][1] - fdist*camtx[2][1]
						<< "\" z=\"" << camtx[3][2] - fdist*camtx[2][2] << "\" />\n";
	}
	else {
		ostr << "\t<to x=\"" << camtx[3][0] - camtx[2][0]
						<< "\" y=\"" << camtx[3][1] - camtx[2][1]
						<< "\" z=\"" << camtx[3][2] - camtx[2][2] << "\" />\n";
	}
	ostr << "\t<up x=\"" << camtx[3][0] + camtx[1][0]
					<< "\" y=\"" << camtx[3][1] + camtx[1][1]
					<< "\" z=\"" << camtx[3][2] + camtx[1][2] << "\" />\n";
	xmlfile << ostr.str();
	xmlfile << "</camera>\n\n";
}

void yafrayFileRender_t::writeHemilight()
{
	ostr.str("");
	ostr << "<light type=\"hemilight\" name=\"hemi_LT\" power=\"1.0\" ";
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
	if(R.r.GIphotons)
	{
		ostr << "<light type=\"globalphotonlight\" name=\"gpm\" photons=\""<<R.r.GIphotoncount<<"\""<<endl;
		ostr << "\tradius=\"" <<R.r.GIphotonradius << "\" depth=\""<< ((R.r.GIdepth>2) ? (R.r.GIdepth-1) : 1)
				 << "\" caus_depth=\""<<R.r.GIcausdepth<< "\" search=\""<< R.r.GImixphotons<<"\" >"<<endl;
		ostr << "</light>"<<endl;
	}
	ostr << "<light type=\"pathlight\" name=\"path_LT\" power=\"1.0\" ";
	ostr << " depth=\"" <<((R.r.GIphotons) ? 1 : R.r.GIdepth)<< "\" caus_depth=\"" << R.r.GIcausdepth <<"\"\n";
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
		float aspect = 1;
		if (R.r.xsch < R.r.ysch) aspect = float(R.r.xsch)/float(R.r.ysch);
		float sbase = 2.0*atan(0.5/(mainCamLens/(aspect*32.0)))/float(R.r.xsch);
		ostr << " cache=\"on\" use_QMC=\"on\" threshold=\"" <<R.r.GIrefinement<<"\""<<endl;
		ostr << " cache_size=\"" << sbase*R.r.GIpixelspersample << "\" shadow_threshold=\"" <<
			1.0 - R.r.GIshadowquality << "\" grid=\"82\" search=\"35\" gradient=\"" <<
			((R.r.GIgradient)? "on" : "off") << "\" >\n";
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
		if(world->mtex[i] != NULL){
			if(world->mtex[i]->tex->type == TEX_IMAGE && world->mtex[i]->tex->ima != NULL){
				
				for(j=0;j<160;j++){
					if(world->mtex[i]->tex->ima->name[j] == '\0' && j > 3){
						if(
							(world->mtex[i]->tex->ima->name[j-3] == 'h' || world->mtex[i]->tex->ima->name[j-3] == 'H' ) &&
							(world->mtex[i]->tex->ima->name[j-2] == 'd' || world->mtex[i]->tex->ima->name[j-2] == 'D' ) &&
							(world->mtex[i]->tex->ima->name[j-1] == 'r' || world->mtex[i]->tex->ima->name[j-1] == 'R' )
							){
								ostr.str("");
								ostr << "<background type=\"HDRI\" name=\"world_background\" ";
								ostr << "exposure_adjust = \"";
								ostr << (world->mtex[i]->tex->bright-1) << "\"";									
								ostr << " mapping = \"probe\" ";
								ostr << ">\n";
								ostr << "<filename value=\"" << world->mtex[i]->tex->ima->name << "\"/>\n";
								ostr << "</background>\n\n";
								xmlfile << ostr.str();
								return true;						
						}
					}
				}

				ostr.str("");
				ostr << "<background type=\"image\" name=\"world_background\" power=\"";
				ostr << world->mtex[i]->tex->bright << "\">\n";
				ostr << "<filename value=\"" << world->mtex[i]->tex->ima->name << "\"/>\n";
				ostr << "</background>\n\n";
				xmlfile << ostr.str();
				return true;
			}
		}
	}

	ostr.str("");
	ostr << "<background type=\"constant\" name=\"world_background\" >\n";
	// if no GI used, the GIpower parameter is not always initialized, so in that case ignore it  (have to change method to init yafray vars in Blender)
	float bg_mult;
	if (R.r.GImethod==0) bg_mult=1; else bg_mult=R.r.GIpower;
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


