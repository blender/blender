//----------------------------------------------------------------------------------------------------
// YafRay export
//
// For anyone else looking at this, this was designed for a tabspacing of 2 (YafRay/Jandro standard :)
//----------------------------------------------------------------------------------------------------

#include "yafray_Render.h"

#include <math.h>

using namespace std;

void yafrayRender_t::clearAll()
{
	all_objects.clear();
	used_materials.clear();
	used_textures.clear();
	dupliMtx_list.clear();
	dup_srcob.clear();
	objectData.clear();
	imagetex.clear();
	imgtex_shader.clear();
}

bool yafrayRender_t::exportScene(Render* re)
{
	this->re = re;

  // get camera first, no checking should be necessary, all done by Blender
	maincam_obj = G.scene->camera;

	// use fixed lens for objects functioning as temporary camera (ctrl-0)
	mainCamLens = 35.0;
	if (maincam_obj->type==OB_CAMERA) mainCamLens=((Camera*)maincam_obj->data)->lens;

	// recreate the scene as object data, as well as sorting the material & textures, ignoring duplicates
	if (!getAllMatTexObs())
	{
		// error found, clear for next call
		clearAll();
		return false;
	}

	if (!initExport())
	{
		G.afbreek = 1;
		clearAll();
		return false;
	}

	// start actual data export
	writeTextures();
	writeMaterialsAndModulators();
	writeAllObjects();
	writeLamps();
	hasworld = writeWorld();
	writeCamera();
	writeRender();
	
	// clear for next call, before render to free some memory
	clearAll();

	if (!finishExport())
	{
		G.afbreek = 1;	//stop render and anim if doing so
		return false;
	}
	else return true;
}

// gets all unique face materials & textures,
// and sorts the facelist rejecting anything that is not a quad or tri,
// as well as associating them again with the original Object.
bool yafrayRender_t::getAllMatTexObs()
{
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	VlakRen* vlr;
	float mat[4][4];

	// convert blender object instances to dupli matrices
	for(obi=(ObjectInstanceRen*)re->instancetable.first; obi; obi=obi->next) {
		if(obi->flag & R_DUPLI_TRANSFORMED) {
			// compute object matrix with dupli transform, need to transform
			// obi->mat out of view space for it
			MTC_Mat4MulSerie(mat, re->viewinv, obi->mat, re->viewmat, obi->obr->ob->obmat, 0, 0, 0, 0);
			addDupliMtx(obi->obr->ob, mat);
		}
	}

	// Blender does not include object which have total 0 alpha materials,
	// however, the objects might have been included in the dupliMtx list,
	// so this will cause a 'Duplilist non-empty...'  error after going through the renderlist.
	// To solve this, keep track of all render objects included sofar,
	// and remove from dupliMtx_list if object not found.
	// This should also help to solve some other yet undetected 'dupli..' errors,
	// but on the other hand that could also hide the real problem of course...
	map<string, Object*> renderobs;

	for(obr=(ObjectRen*)re->objecttable.first; obr; obr=obr->next) {
		for (int i=0; i < obr->totvlak; i++) {

			if ((i & 255)==0) vlr=obr->vlaknodes[i>>8].vlak; else vlr++;

			// ---- The materials & textures
			// in this case, probably every face has a material assigned, which can be the default material,
			// so checking that this is !0 is probably not necessary, but just in case...
			Material* matr = vlr->mat;
			if (matr) {
				// The default assigned material seems to be nameless, no MA id, an empty string.
				// Since this name is needed in yafray, make it 'blender_default'
				if (strlen(matr->id.name)==0)
					used_materials["blender_default"] = matr;
				else
					used_materials[matr->id.name] = matr;
				// textures, all active channels
				for (int m=0;m<MAX_MTEX;m++) {
					if (matr->septex & (1<<m)) continue;	// only active channels
					MTex* mx = matr->mtex[m];
					// if no mtex, ignore
					if (mx==NULL) continue;
					// if no tex, ignore
					Tex* tx = mx->tex;
					if (tx==NULL) continue;
					short txtp = tx->type;
					// if texture type not available in yafray, ignore
					if ((txtp==0) ||
							(txtp==TEX_MAGIC) ||
							(txtp==TEX_PLUGIN) ||
							(txtp==TEX_ENVMAP)) continue;
					// if texture is stucci, only export if 'nor' enabled
					if ((txtp==TEX_STUCCI) && !((mx->mapto & MAP_NORM) || (mx->maptoneg & MAP_NORM))) continue;
					// In the case of an image texture, check that there is an actual image, otherwise ignore.
					// Stupid error was here (...if (txtp & TEX_IMAGE)...),
					// which happened to work sofar, but not anymore with the extended texture support..
					if ((txtp==TEX_IMAGE) && (!tx->ima)) continue;
					used_textures[tx->id.name] = mx;
				}
			}

			// Make list of faces per object, ignore <3 vert faces, duplicate vertex sorting done later.
			// ignore null object pointers.
			// Also make list of facetexture images (material 'TexFace').
			if (obr->ob) {
				int nv = 0;	// number of vertices
				MTFace *tface;

				if (vlr->v4) nv=4; else if (vlr->v3) nv=3;
				if (nv) {
					renderobs[obr->ob->id.name] = obr->ob;
					all_objects[obr->ob].obr= obr;
					all_objects[obr->ob].faces.push_back(vlr);

					tface= RE_vlakren_get_tface(obr, vlr, obr->actmtface, NULL, 0);
					if (tface && tface->tpage) {
						Material* fmat = vlr->mat;

						// only save if TexFace enabled
						if(fmat && (fmat->mode & MA_FACETEXTURE))
							imagetex[tface->tpage].insert(fmat);
					}
				}
			}

		}
	}

	// now remove any objects from dupliMtx_list which are not in the renderlist
	for (map<string, vector<float> >::iterator dL=dupliMtx_list.begin();
				dL!=dupliMtx_list.end();)
	{
		string ro_name = dL->first;
		if (renderobs.find(ro_name)==renderobs.end()) {
			cout << "Object " << ro_name << " not in renderlist, removing from dupliMtx_list" << endl;
			dL->second.clear();
			++dL;
			dupliMtx_list.erase(ro_name);
		}
		else
			++dL;
	}

	// in case dupliMtx_list not empty, make sure that there is at least one source object
	// in all_objects with the name given in dupliMtx_list
	if (!dupliMtx_list.empty()) {

		for (map<Object*, yafrayObjectRen>::const_iterator obn=all_objects.begin();
			obn!=all_objects.end();++obn)
		{
			Object* obj = obn->first;
			string obname = obj->id.name;
			if (dupliMtx_list.find(obname)!=dupliMtx_list.end()) dup_srcob[obname] = obj;
		}

		// if the name reference list is empty, return now, something was seriously wrong
		if (dup_srcob.empty()) {
			// error() doesn't work to well, when switching from Blender to console at least, so use stdout instead
			cout << "ERROR: Duplilist non_empty, but no srcobs\n";
			return false;
		}
	}

	return true;
}

void yafrayRender_t::addDupliMtx(Object* obj, float mat[][4])
{
	for (int i=0;i<4;i++)
		for (int j=0;j<4;j++)
			dupliMtx_list[obj->id.name].push_back(mat[i][j]);
}


#if 0

bool yafrayRender_t::objectKnownData(Object* obj)
{
	// if object data already known, no need to include in renderlist, otherwise save object datapointer
	if (objectData.find(obj->data)!=objectData.end()) {
		Object* orgob = objectData[obj->data];
		// first save original object matrix in dupliMtx_list, if not added yet
		if (dupliMtx_list.find(orgob->id.name)==dupliMtx_list.end()) {
			cout << "Added original matrix\n";
			addDupliMtx(orgob);
		}
		// then save matrix of linked object in dupliMtx_list, using name of ORIGINAL object
		for (int i=0;i<4;i++)
			for (int j=0;j<4;j++)
				dupliMtx_list[orgob->id.name].push_back(obj->obmat[i][j]);
		return true;
	}
	// object not known yet
	objectData[obj->data] = obj;
	return false;
}
#endif

