/** \file gameengine/Ketsji/BL_Material.cpp
 *  \ingroup ketsji
 */
// ------------------------------------
#include "BL_Material.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

MTex* getImageFromMaterial(Material *mat, int index)
{
	if (!mat) return 0;
	
	if (!(index >=0 && index < MAX_MTEX) ) return 0;
	
	MTex *m = mat->mtex[index];
	return m?m:0;
}

int getNumTexChannels( Material *mat )
{
	int count = -1;
	if (!mat) return -1;

	for (count =0; (count < 10) && mat->mtex[count] != 0; count++) {}
	return count;
}

BL_Material::BL_Material()
{
	Initialize();
}

void BL_Material::Initialize()
{
	m_mcol = 0xFFFFFFFFL;
	IdMode = 0;
	ras_mode = 0;
	glslmat = 0;
	tile = 0;
	matname = "NoMaterial";
	matcolor[0] = 0.5f;
	matcolor[1] = 0.5f;
	matcolor[2] = 0.5f;
	matcolor[3] = 0.5f;
	speccolor[0] = 1.f;
	speccolor[1] = 1.f;
	speccolor[2] = 1.f;
	alphablend = 0;
	hard = 50.f;
	spec_f = 0.5f;
	alpha = 1.f;
	emit = 0.f;
	material = 0;
	memset(&tface, 0, sizeof(tface));
	materialindex = 0;
	amb=0.5f;
	num_enabled = 0;
	num_users = 1;
	share = false;

	int i;

	for (i=0; i<MAXTEX; i++) // :(
	{
		mapping[i].mapping = 0;
		mapping[i].offsets[0] = 0.f;
		mapping[i].offsets[1] = 0.f;
		mapping[i].offsets[2] = 0.f;
		mapping[i].scale[0]   = 1.f;
		mapping[i].scale[1]   = 1.f;
		mapping[i].scale[2]   = 1.f;
		mapping[i].projplane[0] = PROJX;
		mapping[i].projplane[1] = PROJY;
		mapping[i].projplane[2] = PROJZ;
		mapping[i].objconame = "";
		mtexname[i] = "NULL";
		imageId[i]="NULL";
		flag[i] = 0;
		texname[i] = "NULL";
		tilexrep[i] = 1;
		tileyrep[i] = 1;
		color_blend[i] = 1.f;
		blend_mode[i]	= 0;
		img[i] = 0;
		cubemap[i] = 0;
	}
}

void BL_Material::SetUVLayerName(const STR_String& name)
{
	uvName = name;
}
void BL_Material::SetUVLayerName2(const STR_String& name)
{
	uv2Name = name;
}

void BL_Material::SetSharedMaterial(bool v)
{
	if ((v && num_users == -1) || num_users > 1 )
		share = true;
	else 
		share = false;
}

bool BL_Material::IsShared()
{
	return share;
}

void BL_Material::SetUsers(int num)
{
	num_users = num;
}

