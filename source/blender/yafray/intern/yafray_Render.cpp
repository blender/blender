//----------------------------------------------------------------------------------------------------
// YafRay XML export
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
}

bool yafrayRender_t::exportScene()
{

  // get camera first, no checking should be necessary, all done by Blender
	maincam_obj = G.scene->camera;

	// use fixed lens for objects functioning as temporary camera (ctrl-0)
	mainCamLens = 35.0;
	if (maincam_obj->type==OB_CAMERA) mainCamLens=((Camera*)maincam_obj->data)->lens;

	maxraydepth = 5;	// will be set to maximum depth used in blender materials

	// recreate the scene as object data, as well as sorting the material & textures, ignoring duplicates
	if (!getAllMatTexObs())
	{
		// error found, clear for next call
		clearAll();
		return false;
	}

	if(!initExport())
	{
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

	if(!finishExport())
	{
		G.afbreek = 1;	//stop render and anim if doing so
		return false;
	}
	else return true;
}


// find object by name in global scene (+'OB'!)
Object* yafrayRender_t::findObject(const char* name)
{
	Base* bs = (Base*)G.scene->base.first;
	while (bs) {
	  Object* obj = bs->object;
		if (!strcmp(name, obj->id.name)) return obj;
		bs = bs->next;
	}
	return NULL;
}

// gets all unique face materials & textures,
// and sorts the facelist rejecting anything that is not a quad or tri,
// as well as associating them again with the original Object.
bool yafrayRender_t::getAllMatTexObs()
{

	VlakRen* vlr;

	for (int i=0;i<R.totvlak;i++) {

		if ((i & 255)==0) vlr=R.blovl[i>>8]; else vlr++;

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
				used_materials[matr->id.name] = matr; // <-- full name to avoid name collision in yafray
				//used_materials[matr->id.name+2] = matr;	// skip 'MA' id
			// textures, all active channels
			for (int m=0;m<8;m++) {
				if (matr->septex & (1<<m)) continue;	// only active channels
				MTex* mx = matr->mtex[m];
				// if no mtex, ignore
				if (mx==NULL) continue;
				// if no tex, ignore
				Tex* tx = mx->tex;
				if (tx==NULL) continue;
				short txtp = tx->type;
				// if texture type not available in yafray, ignore
				if ((txtp!=TEX_STUCCI) &&
						(txtp!=TEX_CLOUDS) &&
						(txtp!=TEX_WOOD) &&
						(txtp!=TEX_MARBLE) &&
						(txtp!=TEX_IMAGE)) continue;
				// in the case of an image texture, check that there is an actual image, otherwise ignore
				if ((txtp & TEX_IMAGE) && (!tx->ima)) continue;
				used_textures[tx->id.name] = make_pair(matr, mx); // <-- full name to avoid name collision in yafray
				//used_textures[tx->id.name+2] = make_pair(matr, mx);
			}
		}

		// make list of faces per object, ignore <3 vert faces, duplicate vertex sorting done later
		// make sure null object pointers are ignored
		if (vlr->ob) {
			int nv = 0;	// number of vertices
			if (vlr->v4) nv=4; else if (vlr->v3) nv=3;
			if (nv) all_objects[vlr->ob].push_back(vlr);
		}
		//else cout << "WARNING: VlakRen struct with null obj.ptr!\n";

	}

	// in case dupliMtx_list not empty, make sure that there is at least one source object
	// in all_objects with the name given in dupliMtx_list
	if (!dupliMtx_list.empty()) {

		for (map<Object*, vector<VlakRen*> >::const_iterator obn=all_objects.begin();
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



void yafrayRender_t::addDupliMtx(Object* obj)
{
	for (int i=0;i<4;i++)
		for (int j=0;j<4;j++)
			dupliMtx_list[string(obj->id.name)].push_back(obj->obmat[i][j]);
}


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
				dupliMtx_list[string(orgob->id.name)].push_back(obj->obmat[i][j]);
		return true;
	}
	// object not known yet
	objectData[obj->data] = obj;
	return false;
}
