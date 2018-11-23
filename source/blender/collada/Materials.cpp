/*
* ***** BEGIN GPL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* Contributor(s): Gaia Clary.
*
* ***** END GPL LICENSE BLOCK *****
*/

#include "Materials.h"

MaterialNode::MaterialNode(bContext *C, Material *ma, KeyImageMap &key_image_map) :
	mContext(C),
	effect(nullptr),
	material(ma),
	key_image_map(&key_image_map)
{
	ntree = prepare_material_nodetree();
	setShaderType();
}

MaterialNode::MaterialNode(bContext *C, COLLADAFW::EffectCommon *ef, Material *ma, UidImageMap &uid_image_map) :
	mContext(C),
	effect(ef),
	material(ma),
	uid_image_map(&uid_image_map)
{
	ntree = prepare_material_nodetree();
	setShaderType();

	std::map<std::string, bNode *> nmap;
#if 0
	nmap["main"] = add_node(C, ntree, SH_NODE_BSDF_PRINCIPLED, -300, 300);
	nmap["emission"] = add_node(C, ntree, SH_NODE_EMISSION, -300, 500, "emission");
	nmap["add"] = add_node(C, ntree, SH_NODE_ADD_SHADER, 100, 400);
	nmap["transparent"] = add_node(C, ntree, SH_NODE_BSDF_TRANSPARENT, 100, 200);
	nmap["mix"] = add_node(C, ntree, SH_NODE_MIX_SHADER, 400, 300, "transparency");
	nmap["out"] = add_node(C, ntree, SH_NODE_OUTPUT_MATERIAL, 600, 300);
	nmap["out"]->flag &= ~NODE_SELECT;

	add_link(ntree, nmap["emission"], 0, nmap["add"], 0);
	add_link(ntree, nmap["main"], 0, nmap["add"], 1);
	add_link(ntree, nmap["add"], 0, nmap["mix"], 1);
	add_link(ntree, nmap["transparent"], 0, nmap["mix"], 2);

	add_link(ntree, nmap["mix"], 0, nmap["out"], 0);
	// experimental, probably not used.
	make_group(C, ntree, nmap);
#else
	shader_node = add_node(SH_NODE_BSDF_PRINCIPLED, 0, 300, "");
	output_node = add_node(SH_NODE_OUTPUT_MATERIAL, 300, 300, "");
	add_link(shader_node, 0, output_node, 0);
#endif
}

void MaterialNode::setShaderType()
{
#if 0
	COLLADAFW::EffectCommon::ShaderType shader = ef->getShaderType();
	// Currently we only support PBR based shaders
	// TODO: simulate the effects with PBR

	// blinn
	if (shader == COLLADAFW::EffectCommon::SHADER_BLINN) {
		ma->spec_shader = MA_SPEC_BLINN;
		ma->spec = ef->getShininess().getFloatValue();
	}
	// phong
	else if (shader == COLLADAFW::EffectCommon::SHADER_PHONG) {
		ma->spec_shader = MA_SPEC_PHONG;
		ma->har = ef->getShininess().getFloatValue();
	}
	// lambert
	else if (shader == COLLADAFW::EffectCommon::SHADER_LAMBERT) {
		ma->diff_shader = MA_DIFF_LAMBERT;
	}
	// default - lambert
	else {
		ma->diff_shader = MA_DIFF_LAMBERT;
		fprintf(stderr, "Current shader type is not supported, default to lambert.\n");
	}
#endif
}

bNodeTree *MaterialNode::prepare_material_nodetree()
{
	if (material->nodetree == NULL) {
		material->nodetree = ntreeAddTree(NULL, "Shader Nodetree", "ShaderNodeTree");
		material->use_nodes = true;
	}
	return material->nodetree;
}

bNode *MaterialNode::add_node(int node_type, int locx, int locy, std::string label)
{
	bNode *node = nodeAddStaticNode(mContext, ntree, node_type);
	if (node) {
		if (label.length() > 0) {
			strcpy(node->label, label.c_str());
		}
		node->locx = locx;
		node->locy = locy;
		node->flag |= NODE_SELECT;
	}
	node_map[label] = node;
	return node;
}

void MaterialNode::add_link(bNode *from_node, int from_index, bNode *to_node, int to_index)
{
	bNodeSocket *from_socket = (bNodeSocket *)BLI_findlink(&from_node->outputs, from_index);
	bNodeSocket *to_socket = (bNodeSocket *)BLI_findlink(&to_node->inputs, to_index);

	nodeAddLink(ntree, from_node, from_socket, to_node, to_socket);
}

void MaterialNode::set_reflectivity(float val)
{
	material->metallic = val;
	bNodeSocket *socket = (bNodeSocket *)BLI_findlink(&shader_node->inputs, BC_PBR_METALLIC);
	*(float *)socket->default_value = val;
}

void MaterialNode::set_ior(float val)
{
	bNodeSocket *socket = (bNodeSocket *)BLI_findlink(&shader_node->inputs, BC_PBR_IOR);
	*(float *)socket->default_value = val;
}

void MaterialNode::set_diffuse(COLLADAFW::ColorOrTexture &cot, std::string label)
{
	int locy = -300 * (node_map.size()-2);
	if (cot.isColor()) {
		COLLADAFW::Color col = cot.getColor();
		material->r = col.getRed();
		material->g = col.getGreen();
		material->b = col.getBlue();

		bNodeSocket *socket = (bNodeSocket *)BLI_findlink(&shader_node->inputs, BC_PBR_DIFFUSE);
		float *fcol = (float *)socket->default_value;
		fcol[0] = col.getRed();
		fcol[1] = col.getGreen();
		fcol[2] = col.getBlue();
	}
	else if (cot.isTexture()) {
		bNode *texture_node = add_texture_node(cot, -300, locy, label);
		if (texture_node != NULL) {
			add_link(texture_node, 0, shader_node, 0);
		}
	}
}

Image *MaterialNode::get_diffuse_image()
{
	bNode *shader = ntreeFindType(ntree, SH_NODE_BSDF_PRINCIPLED);
	if (shader == nullptr) {
		return nullptr;
	}

	bNodeSocket *in_socket = (bNodeSocket *)BLI_findlink(&shader->inputs, BC_PBR_DIFFUSE);
	if (in_socket == nullptr) {
		return nullptr;
	}

	bNodeLink *link = in_socket->link;
	if (link == nullptr) {
		return nullptr;
	}

	bNode *texture = link->fromnode;
	if (texture == nullptr) {
		return nullptr;
	}

	if (texture->type != SH_NODE_TEX_IMAGE) {
		return nullptr;
	}

	Image *image = (Image *)texture->id;
	return image;

}

static bNodeSocket *set_color(bNode *node, COLLADAFW::Color col)
{
		bNodeSocket *socket = (bNodeSocket *)BLI_findlink(&node->outputs, 0);
		float *fcol = (float *)socket->default_value;
		fcol[0] = col.getRed();
		fcol[1] = col.getGreen();
		fcol[2] = col.getBlue();

		return socket;
}

void MaterialNode::set_ambient(COLLADAFW::ColorOrTexture &cot, std::string label)
{
	int locy = -300 * (node_map.size() - 2);
	if (cot.isColor()) {
		COLLADAFW::Color col = cot.getColor();
		bNode *node = add_node(SH_NODE_RGB, -300, locy, label);
		set_color(node, col);
		// TODO: Connect node
	}
	// texture
	else if (cot.isTexture()) {
		add_texture_node(cot, -300, locy, label);
		// TODO: Connect node
	}
}
void MaterialNode::set_reflective(COLLADAFW::ColorOrTexture &cot, std::string label)
{
	int locy = -300 * (node_map.size() - 2);
	if (cot.isColor()) {
		COLLADAFW::Color col = cot.getColor();
		bNode *node = add_node(SH_NODE_RGB, -300, locy, label);
		set_color(node, col);
		// TODO: Connect node
	}
	// texture
	else if (cot.isTexture()) {
		add_texture_node(cot, -300, locy, label);
		// TODO: Connect node
	}
}

void MaterialNode::set_emission(COLLADAFW::ColorOrTexture &cot, std::string label)
{
	int locy = -300 * (node_map.size() - 2);
	if (cot.isColor()) {
		COLLADAFW::Color col = cot.getColor();
		bNode *node = add_node(SH_NODE_RGB, -300, locy, label);
		set_color(node, col);
		// TODO: Connect node
	}
	// texture
	else if (cot.isTexture()) {
		add_texture_node(cot, -300, locy, label);
		// TODO: Connect node
	}
}

void MaterialNode::set_opacity(COLLADAFW::ColorOrTexture &cot, std::string label)
{
	if (effect == nullptr) {
		return;
	}

	int locy = -300 * (node_map.size() - 2);
	if (cot.isColor()) {
		COLLADAFW::Color col = effect->getTransparent().getColor();
		float alpha = effect->getTransparency().getFloatValue();

		if (col.isValid()) {
			alpha *= col.getAlpha(); // Assuming A_ONE opaque mode
		}
		if (col.isValid() || alpha < 1.0) {
			// not sure what to do here
		}

		bNode *node = add_node(SH_NODE_RGB, -300, locy, label);
		set_color(node, col);
		// TODO: Connect node
	}
	// texture
	else if (cot.isTexture()) {
		add_texture_node(cot, -300, locy, label);
		// TODO: Connect node
	}
}

void MaterialNode::set_specular(COLLADAFW::ColorOrTexture &cot, std::string label)
{
	int locy = -300 * (node_map.size() - 2);
	if (cot.isColor()) {
		COLLADAFW::Color col = cot.getColor();
		material->specr = col.getRed();
		material->specg = col.getGreen();
		material->specb = col.getBlue();

		bNode *node = add_node(SH_NODE_RGB, -300, locy, label);
		set_color(node, col);
		// TODO: Connect node
	}
	// texture
	else if (cot.isTexture()) {
		add_texture_node(cot, -300, locy, label);
		// TODO: Connect node
	}
}

bNode *MaterialNode::add_texture_node(COLLADAFW::ColorOrTexture &cot, int locx, int locy, std::string label)
{
	if (effect == nullptr) {
		return nullptr;
	}

	UidImageMap &image_map = *uid_image_map;

	COLLADAFW::Texture ctex = cot.getTexture();

	COLLADAFW::SamplerPointerArray& samp_array = effect->getSamplerPointerArray();
	COLLADAFW::Sampler *sampler = samp_array[ctex.getSamplerId()];

	const COLLADAFW::UniqueId& ima_uid = sampler->getSourceImage();

	if (image_map.find(ima_uid) == image_map.end()) {
		fprintf(stderr, "Couldn't find an image by UID.\n");
		return NULL;
	}

	Image *ima = image_map[ima_uid];
	bNode *texture_node = add_node(SH_NODE_TEX_IMAGE, locx, locy, label);
	texture_node->id = &ima->id;
	return texture_node;

}
