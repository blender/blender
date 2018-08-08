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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_codegen.c
 *  \ingroup gpu
 *
 * Convert material node-trees to GLSL.
 */

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "BLI_blenlib.h"
#include "BLI_hash_mm2a.h"
#include "BLI_link_utils.h"
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "PIL_time.h"

#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniformbuffer.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "gpu_codegen.h"

#include <string.h>
#include <stdarg.h>

extern char datatoc_gpu_shader_material_glsl[];
extern char datatoc_gpu_shader_vertex_glsl[];
extern char datatoc_gpu_shader_vertex_world_glsl[];
extern char datatoc_gpu_shader_geometry_glsl[];

static char *glsl_material_library = NULL;

/* -------------------- GPUPass Cache ------------------ */
/**
 * Internal shader cache: This prevent the shader recompilation / stall when
 * using undo/redo AND also allows for GPUPass reuse if the Shader code is the
 * same for 2 different Materials. Unused GPUPasses are free by Garbage collection.
 **/

/* Only use one linklist that contains the GPUPasses grouped by hash. */
static GPUPass *pass_cache = NULL;
static SpinLock pass_cache_spin;

static uint32_t gpu_pass_hash(const char *frag_gen, const char *defs, GPUVertexAttribs *attribs)
{
	BLI_HashMurmur2A hm2a;
	BLI_hash_mm2a_init(&hm2a, 0);
	BLI_hash_mm2a_add(&hm2a, (unsigned char *)frag_gen, strlen(frag_gen));
	if (attribs) {
		for (int att_idx = 0; att_idx < attribs->totlayer; att_idx++) {
			char *name = attribs->layer[att_idx].name;
			BLI_hash_mm2a_add(&hm2a, (unsigned char *)name, strlen(name));
		}
	}
	if (defs)
		BLI_hash_mm2a_add(&hm2a, (unsigned char *)defs, strlen(defs));

	return BLI_hash_mm2a_end(&hm2a);
}

/* Search by hash only. Return first pass with the same hash.
 * There is hash collision if (pass->next && pass->next->hash == hash) */
static GPUPass *gpu_pass_cache_lookup(uint32_t hash)
{
	BLI_spin_lock(&pass_cache_spin);
	/* Could be optimized with a Lookup table. */
	for (GPUPass *pass = pass_cache; pass; pass = pass->next) {
		if (pass->hash == hash) {
			BLI_spin_unlock(&pass_cache_spin);
			return pass;
		}
	}
	BLI_spin_unlock(&pass_cache_spin);
	return NULL;
}

/* Check all possible passes with the same hash. */
static GPUPass *gpu_pass_cache_resolve_collision(
        GPUPass *pass, const char *vert, const char *geom, const char *frag, const char *defs, uint32_t hash)
{
	BLI_spin_lock(&pass_cache_spin);
	/* Collision, need to strcmp the whole shader. */
	for (; pass && (pass->hash == hash); pass = pass->next) {
		if ((defs != NULL) && (strcmp(pass->defines, defs) != 0)) { /* Pass */ }
		else if ((geom != NULL) && (strcmp(pass->geometrycode, geom) != 0)) { /* Pass */ }
		else if ((strcmp(pass->fragmentcode, frag) == 0) &&
		         (strcmp(pass->vertexcode, vert) == 0))
		{
			BLI_spin_unlock(&pass_cache_spin);
			return pass;
		}
	}
	BLI_spin_unlock(&pass_cache_spin);
	return NULL;
}

/* -------------------- GPU Codegen ------------------ */

/* type definitions and constants */

enum {
	MAX_FUNCTION_NAME = 64
};
enum {
	MAX_PARAMETER = 32
};

typedef enum {
	FUNCTION_QUAL_IN,
	FUNCTION_QUAL_OUT,
	FUNCTION_QUAL_INOUT
} GPUFunctionQual;

typedef struct GPUFunction {
	char name[MAX_FUNCTION_NAME];
	GPUType paramtype[MAX_PARAMETER];
	GPUFunctionQual paramqual[MAX_PARAMETER];
	int totparam;
} GPUFunction;

/* Indices match the GPUType enum */
static const char *GPU_DATATYPE_STR[17] = {
	"", "float", "vec2", "vec3", "vec4",
	NULL, NULL, NULL, NULL, "mat3", NULL, NULL, NULL, NULL, NULL, NULL, "mat4"
};

/* GLSL code parsing for finding function definitions.
 * These are stored in a hash for lookup when creating a material. */

static GHash *FUNCTION_HASH = NULL;
#if 0
static char *FUNCTION_PROTOTYPES = NULL;
static GPUShader *FUNCTION_LIB = NULL;
#endif

static int gpu_str_prefix(const char *str, const char *prefix)
{
	while (*str && *prefix) {
		if (*str != *prefix)
			return 0;

		str++;
		prefix++;
	}

	return (*prefix == '\0');
}

static char *gpu_str_skip_token(char *str, char *token, int max)
{
	int len = 0;

	/* skip a variable/function name */
	while (*str) {
		if (ELEM(*str, ' ', '(', ')', ',', '\t', '\n', '\r'))
			break;
		else {
			if (token && len < max - 1) {
				*token = *str;
				token++;
				len++;
			}
			str++;
		}
	}

	if (token)
		*token = '\0';

	/* skip the next special characters:
	 * note the missing ')' */
	while (*str) {
		if (ELEM(*str, ' ', '(', ',', '\t', '\n', '\r'))
			str++;
		else
			break;
	}

	return str;
}

static void gpu_parse_functions_string(GHash *hash, char *code)
{
	GPUFunction *function;
	GPUType type;
	GPUFunctionQual qual;
	int i;

	while ((code = strstr(code, "void "))) {
		function = MEM_callocN(sizeof(GPUFunction), "GPUFunction");

		code = gpu_str_skip_token(code, NULL, 0);
		code = gpu_str_skip_token(code, function->name, MAX_FUNCTION_NAME);

		/* get parameters */
		while (*code && *code != ')') {
			/* test if it's an input or output */
			qual = FUNCTION_QUAL_IN;
			if (gpu_str_prefix(code, "out "))
				qual = FUNCTION_QUAL_OUT;
			if (gpu_str_prefix(code, "inout "))
				qual = FUNCTION_QUAL_INOUT;
			if ((qual != FUNCTION_QUAL_IN) || gpu_str_prefix(code, "in "))
				code = gpu_str_skip_token(code, NULL, 0);

			/* test for type */
			type = GPU_NONE;
			for (i = 1; i < ARRAY_SIZE(GPU_DATATYPE_STR); i++) {
				if (GPU_DATATYPE_STR[i] && gpu_str_prefix(code, GPU_DATATYPE_STR[i])) {
					type = i;
					break;
				}
			}

			if (!type && gpu_str_prefix(code, "samplerCube")) {
				type = GPU_TEXCUBE;
			}
			if (!type && gpu_str_prefix(code, "sampler2DShadow")) {
				type = GPU_SHADOW2D;
			}
			if (!type && gpu_str_prefix(code, "sampler2D")) {
				type = GPU_TEX2D;
			}
			if (!type && gpu_str_prefix(code, "sampler3D")) {
				type = GPU_TEX3D;
			}

			if (!type && gpu_str_prefix(code, "Closure")) {
				type = GPU_CLOSURE;
			}

			if (type) {
				/* add parameter */
				code = gpu_str_skip_token(code, NULL, 0);
				code = gpu_str_skip_token(code, NULL, 0);
				function->paramqual[function->totparam] = qual;
				function->paramtype[function->totparam] = type;
				function->totparam++;
			}
			else {
				fprintf(stderr, "GPU invalid function parameter in %s.\n", function->name);
				break;
			}
		}

		if (function->name[0] == '\0' || function->totparam == 0) {
			fprintf(stderr, "GPU functions parse error.\n");
			MEM_freeN(function);
			break;
		}

		BLI_ghash_insert(hash, function->name, function);
	}
}

#if 0
static char *gpu_generate_function_prototyps(GHash *hash)
{
	DynStr *ds = BLI_dynstr_new();
	GHashIterator *ghi;
	GPUFunction *function;
	char *name, *prototypes;
	int a;

	/* automatically generate function prototypes to add to the top of the
	 * generated code, to avoid have to add the actual code & recompile all */
	ghi = BLI_ghashIterator_new(hash);

	for (; !BLI_ghashIterator_done(ghi); BLI_ghashIterator_step(ghi)) {
		name = BLI_ghashIterator_getValue(ghi);
		function = BLI_ghashIterator_getValue(ghi);

		BLI_dynstr_appendf(ds, "void %s(", name);
		for (a = 0; a < function->totparam; a++) {
			if (function->paramqual[a] == FUNCTION_QUAL_OUT)
				BLI_dynstr_append(ds, "out ");
			else if (function->paramqual[a] == FUNCTION_QUAL_INOUT)
				BLI_dynstr_append(ds, "inout ");

			if (function->paramtype[a] == GPU_TEX2D)
				BLI_dynstr_append(ds, "sampler2D");
			else if (function->paramtype[a] == GPU_SHADOW2D)
				BLI_dynstr_append(ds, "sampler2DShadow");
			else
				BLI_dynstr_append(ds, GPU_DATATYPE_STR[function->paramtype[a]]);
#  if 0
			BLI_dynstr_appendf(ds, " param%d", a);
#  endif

			if (a != function->totparam - 1)
				BLI_dynstr_append(ds, ", ");
		}
		BLI_dynstr_append(ds, ");\n");
	}

	BLI_dynstr_append(ds, "\n");

	prototypes = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	return prototypes;
}
#endif

static GPUFunction *gpu_lookup_function(const char *name)
{
	if (!FUNCTION_HASH) {
		FUNCTION_HASH = BLI_ghash_str_new("GPU_lookup_function gh");
		gpu_parse_functions_string(FUNCTION_HASH, glsl_material_library);
	}

	return BLI_ghash_lookup(FUNCTION_HASH, (const void *)name);
}

void gpu_codegen_init(void)
{
	GPU_code_generate_glsl_lib();
}

void gpu_codegen_exit(void)
{
	extern Material defmaterial; /* render module abuse... */

	if (defmaterial.gpumaterial.first)
		GPU_material_free(&defmaterial.gpumaterial);

	if (FUNCTION_HASH) {
		BLI_ghash_free(FUNCTION_HASH, NULL, MEM_freeN);
		FUNCTION_HASH = NULL;
	}

	GPU_shader_free_builtin_shaders();

	if (glsl_material_library) {
		MEM_freeN(glsl_material_library);
		glsl_material_library = NULL;
	}

#if 0
	if (FUNCTION_PROTOTYPES) {
		MEM_freeN(FUNCTION_PROTOTYPES);
		FUNCTION_PROTOTYPES = NULL;
	}
	if (FUNCTION_LIB) {
		GPU_shader_free(FUNCTION_LIB);
		FUNCTION_LIB = NULL;
	}
#endif
}

/* GLSL code generation */

static void codegen_convert_datatype(DynStr *ds, int from, int to, const char *tmp, int id)
{
	char name[1024];

	BLI_snprintf(name, sizeof(name), "%s%d", tmp, id);

	if (from == to) {
		BLI_dynstr_append(ds, name);
	}
	else if (to == GPU_FLOAT) {
		if (from == GPU_VEC4)
			BLI_dynstr_appendf(ds, "convert_rgba_to_float(%s)", name);
		else if (from == GPU_VEC3)
			BLI_dynstr_appendf(ds, "(%s.r + %s.g + %s.b) / 3.0", name, name, name);
		else if (from == GPU_VEC2)
			BLI_dynstr_appendf(ds, "%s.r", name);
	}
	else if (to == GPU_VEC2) {
		if (from == GPU_VEC4)
			BLI_dynstr_appendf(ds, "vec2((%s.r + %s.g + %s.b) / 3.0, %s.a)", name, name, name, name);
		else if (from == GPU_VEC3)
			BLI_dynstr_appendf(ds, "vec2((%s.r + %s.g + %s.b) / 3.0, 1.0)", name, name, name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_appendf(ds, "vec2(%s, 1.0)", name);
	}
	else if (to == GPU_VEC3) {
		if (from == GPU_VEC4)
			BLI_dynstr_appendf(ds, "%s.rgb", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_appendf(ds, "vec3(%s.r, %s.r, %s.r)", name, name, name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_appendf(ds, "vec3(%s, %s, %s)", name, name, name);
	}
	else if (to == GPU_VEC4) {
		if (from == GPU_VEC3)
			BLI_dynstr_appendf(ds, "vec4(%s, 1.0)", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_appendf(ds, "vec4(%s.r, %s.r, %s.r, %s.g)", name, name, name, name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_appendf(ds, "vec4(%s, %s, %s, 1.0)", name, name, name);
	}
	else if (to == GPU_CLOSURE) {
		if (from == GPU_VEC4)
			BLI_dynstr_appendf(ds, "closure_emission(%s.rgb)", name);
		else if (from == GPU_VEC3)
			BLI_dynstr_appendf(ds, "closure_emission(%s.rgb)", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_appendf(ds, "closure_emission(%s.rrr)", name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_appendf(ds, "closure_emission(vec3(%s, %s, %s))", name, name, name);
	}
	else {
		BLI_dynstr_append(ds, name);
	}
}

static void codegen_print_datatype(DynStr *ds, const GPUType type, float *data)
{
	int i;

	BLI_dynstr_appendf(ds, "%s(", GPU_DATATYPE_STR[type]);

	for (i = 0; i < type; i++) {
		BLI_dynstr_appendf(ds, "%.12f", data[i]);
		if (i == type - 1)
			BLI_dynstr_append(ds, ")");
		else
			BLI_dynstr_append(ds, ", ");
	}
}

static int codegen_input_has_texture(GPUInput *input)
{
	if (input->link)
		return 0;
	else if (input->ima || input->prv)
		return 1;
	else
		return input->tex != NULL;
}

const char *GPU_builtin_name(GPUBuiltin builtin)
{
	if (builtin == GPU_VIEW_MATRIX)
		return "unfviewmat";
	else if (builtin == GPU_OBJECT_MATRIX)
		return "unfobmat";
	else if (builtin == GPU_INVERSE_VIEW_MATRIX)
		return "unfinvviewmat";
	else if (builtin == GPU_INVERSE_OBJECT_MATRIX)
		return "unfinvobmat";
	else if (builtin == GPU_LOC_TO_VIEW_MATRIX)
		return "unflocaltoviewmat";
	else if (builtin == GPU_INVERSE_LOC_TO_VIEW_MATRIX)
		return "unfinvlocaltoviewmat";
	else if (builtin == GPU_VIEW_POSITION)
		return "varposition";
	else if (builtin == GPU_VIEW_NORMAL)
		return "varnormal";
	else if (builtin == GPU_OBCOLOR)
		return "unfobcolor";
	else if (builtin == GPU_AUTO_BUMPSCALE)
		return "unfobautobumpscale";
	else if (builtin == GPU_CAMERA_TEXCO_FACTORS)
		return "unfcameratexfactors";
	else if (builtin == GPU_PARTICLE_SCALAR_PROPS)
		return "unfparticlescalarprops";
	else if (builtin == GPU_PARTICLE_LOCATION)
		return "unfparticleco";
	else if (builtin == GPU_PARTICLE_VELOCITY)
		return "unfparticlevel";
	else if (builtin == GPU_PARTICLE_ANG_VELOCITY)
		return "unfparticleangvel";
	else if (builtin == GPU_OBJECT_INFO)
		return "unfobjectinfo";
	else if (builtin == GPU_VOLUME_DENSITY)
		return "sampdensity";
	else if (builtin == GPU_VOLUME_FLAME)
		return "sampflame";
	else if (builtin == GPU_VOLUME_TEMPERATURE)
		return "unftemperature";
	else
		return "";
}

/* assign only one texid per buffer to avoid sampling the same texture twice */
static void codegen_set_texid(GHash *bindhash, GPUInput *input, int *texid, void *key)
{
	if (BLI_ghash_haskey(bindhash, key)) {
		/* Reuse existing texid */
		input->texid = GET_INT_FROM_POINTER(BLI_ghash_lookup(bindhash, key));
	}
	else {
		/* Allocate new texid */
		input->texid = *texid;
		(*texid)++;
		input->bindtex = true;
		BLI_ghash_insert(bindhash, key, SET_INT_IN_POINTER(input->texid));
	}
}

static void codegen_set_unique_ids(ListBase *nodes)
{
	GHash *bindhash, *definehash;
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;
	int id = 1, texid = 0;

	bindhash = BLI_ghash_ptr_new("codegen_set_unique_ids1 gh");
	definehash = BLI_ghash_ptr_new("codegen_set_unique_ids2 gh");

	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			/* set id for unique names of uniform variables */
			input->id = id++;
			input->bindtex = false;
			input->definetex = false;

			/* set texid used for settings texture slot with multitexture */
			if (codegen_input_has_texture(input) &&
			    ((input->source == GPU_SOURCE_TEX) || (input->source == GPU_SOURCE_TEX_PIXEL)))
			{
				/* assign only one texid per buffer to avoid sampling
				 * the same texture twice */
				if (input->link) {
					/* input is texture from buffer */
					codegen_set_texid(bindhash, input, &texid, input->link);
				}
				else if (input->ima) {
					/* input is texture from image */
					codegen_set_texid(bindhash, input, &texid, input->ima);
				}
				else if (input->prv) {
					/* input is texture from preview render */
					codegen_set_texid(bindhash, input, &texid, input->prv);
				}
				else if (input->tex) {
					/* input is user created texture, check tex pointer */
					codegen_set_texid(bindhash, input, &texid, input->tex);
				}

				/* make sure this pixel is defined exactly once */
				if (input->source == GPU_SOURCE_TEX_PIXEL) {
					if (input->ima) {
						if (!BLI_ghash_haskey(definehash, input->ima)) {
							input->definetex = true;
							BLI_ghash_insert(definehash, input->ima, SET_INT_IN_POINTER(input->texid));
						}
					}
					else {
						if (!BLI_ghash_haskey(definehash, input->link)) {
							input->definetex = true;
							BLI_ghash_insert(definehash, input->link, SET_INT_IN_POINTER(input->texid));
						}
					}
				}
			}
		}

		for (output = node->outputs.first; output; output = output->next)
			/* set id for unique names of tmp variables storing output */
			output->id = id++;
	}

	BLI_ghash_free(bindhash, NULL, NULL);
	BLI_ghash_free(definehash, NULL, NULL);
}

/**
 * It will create an UBO for GPUMaterial if there is any GPU_DYNAMIC_UBO.
 */
static int codegen_process_uniforms_functions(GPUMaterial *material, DynStr *ds, ListBase *nodes)
{
	GPUNode *node;
	GPUInput *input;
	const char *name;
	int builtins = 0;
	ListBase ubo_inputs = {NULL, NULL};

	/* print uniforms */
	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			if ((input->source == GPU_SOURCE_TEX) || (input->source == GPU_SOURCE_TEX_PIXEL)) {
				/* create exactly one sampler for each texture */
				if (codegen_input_has_texture(input) && input->bindtex) {
					BLI_dynstr_appendf(
					        ds, "uniform %s samp%d;\n",
					        (input->textype == GPU_TEX2D) ? "sampler2D" :
					        (input->textype == GPU_TEXCUBE) ? "samplerCube" : "sampler2DShadow",
					        input->texid);
				}
			}
			else if (input->source == GPU_SOURCE_BUILTIN) {
				/* only define each builtin uniform/varying once */
				if (!(builtins & input->builtin)) {
					builtins |= input->builtin;
					name = GPU_builtin_name(input->builtin);

					if (gpu_str_prefix(name, "samp")) {
						if ((input->builtin == GPU_VOLUME_DENSITY) ||
						    (input->builtin == GPU_VOLUME_FLAME))
						{
							BLI_dynstr_appendf(ds, "uniform sampler3D %s;\n", name);
						}
					}
					else if (gpu_str_prefix(name, "unf")) {
						BLI_dynstr_appendf(
						        ds, "uniform %s %s;\n",
						        GPU_DATATYPE_STR[input->type], name);
					}
					else {
						BLI_dynstr_appendf(
						        ds, "%s %s %s;\n",
						        GLEW_VERSION_3_0 ? "in" : "varying",
						        GPU_DATATYPE_STR[input->type], name);
					}
				}
			}
			else if (input->source == GPU_SOURCE_STRUCT) {
				/* Add other struct here if needed. */
				BLI_dynstr_appendf(ds, "Closure strct%d = CLOSURE_DEFAULT;\n", input->id);
			}
			else if (input->source == GPU_SOURCE_VEC_UNIFORM) {
				if (input->dynamictype == GPU_DYNAMIC_UBO) {
					if (!input->link) {
						/* We handle the UBOuniforms separately. */
						BLI_addtail(&ubo_inputs, BLI_genericNodeN(input));
					}
				}
				else if (input->dynamicvec) {
					/* only create uniforms for dynamic vectors */
					BLI_dynstr_appendf(
					        ds, "uniform %s unf%d;\n",
					        GPU_DATATYPE_STR[input->type], input->id);
				}
				else {
					BLI_dynstr_appendf(
					        ds, "const %s cons%d = ",
					        GPU_DATATYPE_STR[input->type], input->id);
					codegen_print_datatype(ds, input->type, input->vec);
					BLI_dynstr_append(ds, ";\n");
				}
			}
			else if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
#ifdef WITH_OPENSUBDIV
				bool skip_opensubdiv = input->attribtype == CD_TANGENT;
				if (skip_opensubdiv) {
					BLI_dynstr_appendf(ds, "#ifndef USE_OPENSUBDIV\n");
				}
#endif
				BLI_dynstr_appendf(
				        ds, "%s %s var%d;\n",
				        GLEW_VERSION_3_0 ? "in" : "varying",
				        GPU_DATATYPE_STR[input->type], input->attribid);
#ifdef WITH_OPENSUBDIV
				if (skip_opensubdiv) {
					BLI_dynstr_appendf(ds, "#endif\n");
				}
#endif
			}
		}
	}

	/* Handle the UBO block separately. */
	if ((material != NULL) && !BLI_listbase_is_empty(&ubo_inputs)) {
		GPU_material_uniform_buffer_create(material, &ubo_inputs);

		/* Inputs are sorted */
		BLI_dynstr_appendf(ds, "\nlayout (std140) uniform %s {\n", GPU_UBO_BLOCK_NAME);

		for (LinkData *link = ubo_inputs.first; link; link = link->next) {
			input = link->data;
			BLI_dynstr_appendf(
			        ds, "\t%s unf%d;\n",
			        GPU_DATATYPE_STR[input->type], input->id);
		}
		BLI_dynstr_append(ds, "};\n");
		BLI_freelistN(&ubo_inputs);
	}

	BLI_dynstr_append(ds, "\n");

	return builtins;
}

static void codegen_declare_tmps(DynStr *ds, ListBase *nodes)
{
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;

	for (node = nodes->first; node; node = node->next) {
		/* load pixels from textures */
		for (input = node->inputs.first; input; input = input->next) {
			if (input->source == GPU_SOURCE_TEX_PIXEL) {
				if (codegen_input_has_texture(input) && input->definetex) {
					BLI_dynstr_appendf(
					        ds, "\tvec4 tex%d = texture2D(", input->texid);
					BLI_dynstr_appendf(
					        ds, "samp%d, gl_TexCoord[%d].st);\n",
					        input->texid, input->texid);
				}
			}
		}

		/* declare temporary variables for node output storage */
		for (output = node->outputs.first; output; output = output->next) {
			if (output->type == GPU_CLOSURE) {
				BLI_dynstr_appendf(
				        ds, "\tClosure tmp%d;\n", output->id);
			}
			else {
				BLI_dynstr_appendf(
				        ds, "\t%s tmp%d;\n",
				        GPU_DATATYPE_STR[output->type], output->id);
			}
		}
	}

	BLI_dynstr_append(ds, "\n");
}

static void codegen_call_functions(DynStr *ds, ListBase *nodes, GPUOutput *finaloutput)
{
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;

	for (node = nodes->first; node; node = node->next) {
		BLI_dynstr_appendf(ds, "\t%s(", node->name);

		for (input = node->inputs.first; input; input = input->next) {
			if (input->source == GPU_SOURCE_TEX) {
				BLI_dynstr_appendf(ds, "samp%d", input->texid);
				if (input->link)
					BLI_dynstr_appendf(ds, ", gl_TexCoord[%d].st", input->texid);
			}
			else if (input->source == GPU_SOURCE_TEX_PIXEL) {
				codegen_convert_datatype(
				        ds, input->link->output->type, input->type,
				        "tmp", input->link->output->id);
			}
			else if (input->source == GPU_SOURCE_BUILTIN) {
				if (input->builtin == GPU_INVERSE_VIEW_MATRIX)
					BLI_dynstr_append(ds, "viewinv");
				else if (input->builtin == GPU_VIEW_MATRIX)
					BLI_dynstr_append(ds, "viewmat");
				else if (input->builtin == GPU_CAMERA_TEXCO_FACTORS)
					BLI_dynstr_append(ds, "camtexfac");
				else if (input->builtin == GPU_OBJECT_MATRIX)
					BLI_dynstr_append(ds, "objmat");
				else if (input->builtin == GPU_INVERSE_OBJECT_MATRIX)
					BLI_dynstr_append(ds, "objinv");
				else if (input->builtin == GPU_VIEW_POSITION)
					BLI_dynstr_append(ds, "viewposition");
				else if (input->builtin == GPU_VIEW_NORMAL)
					BLI_dynstr_append(ds, "facingnormal");
				else
					BLI_dynstr_append(ds, GPU_builtin_name(input->builtin));
			}
			else if (input->source == GPU_SOURCE_STRUCT) {
				BLI_dynstr_appendf(ds, "strct%d", input->id);
			}
			else if (input->source == GPU_SOURCE_VEC_UNIFORM) {
				if (input->dynamicvec)
					BLI_dynstr_appendf(ds, "unf%d", input->id);
				else
					BLI_dynstr_appendf(ds, "cons%d", input->id);
			}
			else if (input->source == GPU_SOURCE_ATTRIB) {
				BLI_dynstr_appendf(ds, "var%d", input->attribid);
			}
			else if (input->source == GPU_SOURCE_OPENGL_BUILTIN) {
				if (input->oglbuiltin == GPU_MATCAP_NORMAL)
					BLI_dynstr_append(ds, "gl_SecondaryColor");
				else if (input->oglbuiltin == GPU_COLOR)
					BLI_dynstr_append(ds, "gl_Color");
			}

			BLI_dynstr_append(ds, ", ");
		}

		for (output = node->outputs.first; output; output = output->next) {
			BLI_dynstr_appendf(ds, "tmp%d", output->id);
			if (output->next)
				BLI_dynstr_append(ds, ", ");
		}

		BLI_dynstr_append(ds, ");\n");
	}

	BLI_dynstr_appendf(ds, "\n\treturn tmp%d", finaloutput->id);
	BLI_dynstr_append(ds, ";\n");
}

static char *code_generate_fragment(GPUMaterial *material, ListBase *nodes, GPUOutput *output)
{
	DynStr *ds = BLI_dynstr_new();
	char *code;
	int builtins;

#ifdef WITH_OPENSUBDIV
	GPUNode *node;
	GPUInput *input;
#endif


#if 0
	BLI_dynstr_append(ds, FUNCTION_PROTOTYPES);
#endif

	codegen_set_unique_ids(nodes);
	builtins = codegen_process_uniforms_functions(material, ds, nodes);

#if 0
	if (G.debug & G_DEBUG)
		BLI_dynstr_appendf(ds, "/* %s */\n", name);
#endif

	BLI_dynstr_append(ds, "Closure nodetree_exec(void)\n{\n");

	if (builtins & GPU_VIEW_MATRIX)
		BLI_dynstr_append(ds, "\tmat4 viewmat = ViewMatrix;\n");
	if (builtins & GPU_CAMERA_TEXCO_FACTORS)
		BLI_dynstr_append(ds, "\tvec4 camtexfac = CameraTexCoFactors;\n");
	if (builtins & GPU_OBJECT_MATRIX)
		BLI_dynstr_append(ds, "\tmat4 objmat = ModelMatrix;\n");
	if (builtins & GPU_INVERSE_OBJECT_MATRIX)
		BLI_dynstr_append(ds, "\tmat4 objinv = ModelMatrixInverse;\n");
	if (builtins & GPU_INVERSE_VIEW_MATRIX)
		BLI_dynstr_append(ds, "\tmat4 viewinv = ViewMatrixInverse;\n");
	if (builtins & GPU_VIEW_NORMAL)
		BLI_dynstr_append(ds, "\tvec3 facingnormal = gl_FrontFacing? viewNormal: -viewNormal;\n");
	if (builtins & GPU_VIEW_POSITION)
		BLI_dynstr_append(ds, "\tvec3 viewposition = viewPosition;\n");

	/* Calculate tangent space. */
#ifdef WITH_OPENSUBDIV
	{
		bool has_tangent = false;
		for (node = nodes->first; node; node = node->next) {
			for (input = node->inputs.first; input; input = input->next) {
				if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
					if (input->attribtype == CD_TANGENT) {
						BLI_dynstr_appendf(
						        ds, "#ifdef USE_OPENSUBDIV\n");
						BLI_dynstr_appendf(
						        ds, "\t%s var%d;\n",
						        GPU_DATATYPE_STR[input->type],
						        input->attribid);
						if (has_tangent == false) {
							BLI_dynstr_appendf(ds, "\tvec3 Q1 = dFdx(inpt.v.position.xyz);\n");
							BLI_dynstr_appendf(ds, "\tvec3 Q2 = dFdy(inpt.v.position.xyz);\n");
							BLI_dynstr_appendf(ds, "\tvec2 st1 = dFdx(inpt.v.uv);\n");
							BLI_dynstr_appendf(ds, "\tvec2 st2 = dFdy(inpt.v.uv);\n");
							BLI_dynstr_appendf(ds, "\tvec3 T = normalize(Q1 * st2.t - Q2 * st1.t);\n");
						}
						BLI_dynstr_appendf(ds, "\tvar%d = vec4(T, 1.0);\n", input->attribid);
						BLI_dynstr_appendf(ds, "#endif\n");
					}
				}
			}
		}
	}
#endif

	codegen_declare_tmps(ds, nodes);
	codegen_call_functions(ds, nodes, output);

	BLI_dynstr_append(ds, "}\n");

	/* XXX This cannot go into gpu_shader_material.glsl because main() would be parsed and generate error */
	/* Old glsl mode compat. */
	BLI_dynstr_append(ds, "#ifndef NODETREE_EXEC\n");
	BLI_dynstr_append(ds, "out vec4 fragColor;\n");
	BLI_dynstr_append(ds, "void main()\n");
	BLI_dynstr_append(ds, "{\n");
	BLI_dynstr_append(ds, "\tClosure cl = nodetree_exec();\n");
	BLI_dynstr_append(ds, "\tfragColor = vec4(cl.radiance, cl.opacity);\n");
	BLI_dynstr_append(ds, "}\n");
	BLI_dynstr_append(ds, "#endif\n\n");

	/* create shader */
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

#if 0
	if (G.debug & G_DEBUG) printf("%s\n", code);
#endif

	return code;
}

static const char *attrib_prefix_get(CustomDataType type)
{
	switch (type) {
		case CD_ORCO:           return "orco";
		case CD_MTFACE:         return "u";
		case CD_TANGENT:        return "t";
		case CD_MCOL:           return "c";
		case CD_AUTO_FROM_NAME: return "a";
		default: BLI_assert(false && "GPUVertAttr Prefix type not found : This should not happen!"); return "";
	}
}

static char *code_generate_vertex(ListBase *nodes, const char *vert_code, bool use_geom)
{
	DynStr *ds = BLI_dynstr_new();
	GPUNode *node;
	GPUInput *input;
	char *code;

	/* Hairs uv and col attribs are passed by bufferTextures. */
	BLI_dynstr_append(
	        ds,
	        "#ifdef HAIR_SHADER\n"
	        "#define DEFINE_ATTRIB(type, attr) uniform samplerBuffer attr\n"
	        "#else\n"
	        "#define DEFINE_ATTRIB(type, attr) in type attr\n"
	        "#endif\n"
	);

	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
				/* XXX FIXME : see notes in mesh_render_data_create() */
				/* NOTE : Replicate changes to mesh_render_data_create() in draw_cache_impl_mesh.c */
				if (input->attribtype == CD_ORCO) {
					/* orco is computed from local positions, see bellow */
					BLI_dynstr_appendf(ds, "uniform vec3 OrcoTexCoFactors[2];\n");
				}
				else if (input->attribname[0] == '\0') {
					BLI_dynstr_appendf(ds, "DEFINE_ATTRIB(%s, %s);\n", GPU_DATATYPE_STR[input->type], attrib_prefix_get(input->attribtype));
					BLI_dynstr_appendf(ds, "#define att%d %s\n", input->attribid, attrib_prefix_get(input->attribtype));
				}
				else {
					unsigned int hash = BLI_ghashutil_strhash_p(input->attribname);
					BLI_dynstr_appendf(
					        ds, "DEFINE_ATTRIB(%s, %s%u);\n",
					        GPU_DATATYPE_STR[input->type], attrib_prefix_get(input->attribtype), hash);
					BLI_dynstr_appendf(
					        ds, "#define att%d %s%u\n",
					        input->attribid, attrib_prefix_get(input->attribtype), hash);
					/* Auto attrib can be vertex color byte buffer.
					 * We need to know and convert them to linear space in VS. */
					if (!use_geom && input->attribtype == CD_AUTO_FROM_NAME) {
						BLI_dynstr_appendf(ds, "uniform bool ba%u;\n", hash);
						BLI_dynstr_appendf(ds, "#define att%d_is_srgb ba%u\n", input->attribid, hash);
					}
				}
				BLI_dynstr_appendf(
				        ds, "out %s var%d%s;\n",
				        GPU_DATATYPE_STR[input->type], input->attribid, use_geom ? "g" : "");
			}
		}
	}

	BLI_dynstr_append(ds, "\n");

	BLI_dynstr_append(
	        ds,
	        "#define ATTRIB\n"
	        "uniform mat3 NormalMatrix;\n"
	        "uniform mat4 ModelMatrixInverse;\n"
	        "vec3 srgb_to_linear_attrib(vec3 c) {\n"
	        "\tc = max(c, vec3(0.0));\n"
	        "\tvec3 c1 = c * (1.0 / 12.92);\n"
	        "\tvec3 c2 = pow((c + 0.055) * (1.0 / 1.055), vec3(2.4));\n"
	        "\treturn mix(c1, c2, step(vec3(0.04045), c));\n"
	        "}\n\n"
	);

	/* Prototype because defined later. */
	BLI_dynstr_append(
	        ds,
	        "vec2 hair_get_customdata_vec2(const samplerBuffer);\n"
	        "vec3 hair_get_customdata_vec3(const samplerBuffer);\n"
	        "vec4 hair_get_customdata_vec4(const samplerBuffer);\n"
	        "vec3 hair_get_strand_pos(void);\n"
	        "\n"
	);

	BLI_dynstr_append(ds, "void pass_attrib(in vec3 position) {\n");

	BLI_dynstr_append(ds, "#ifdef HAIR_SHADER\n");

	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
				if (input->attribtype == CD_TANGENT) {
					/* Not supported by hairs */
					BLI_dynstr_appendf(
					        ds, "\tvar%d%s = vec4(0.0);\n",
					        input->attribid, use_geom ? "g" : "");
				}
				else if (input->attribtype == CD_ORCO) {
					BLI_dynstr_appendf(
					        ds, "\tvar%d%s = OrcoTexCoFactors[0] + (ModelMatrixInverse * vec4(hair_get_strand_pos(), 1.0)).xyz * OrcoTexCoFactors[1];\n",
					        input->attribid, use_geom ? "g" : "");
				}
				else {
					BLI_dynstr_appendf(
					        ds, "\tvar%d%s = hair_get_customdata_%s(att%d);\n",
					        input->attribid, use_geom ? "g" : "", GPU_DATATYPE_STR[input->type], input->attribid);
				}
			}
		}
	}

	BLI_dynstr_append(ds, "#else /* MESH_SHADER */\n");

	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
				if (input->attribtype == CD_TANGENT) { /* silly exception */
					BLI_dynstr_appendf(
					        ds, "\tvar%d%s.xyz = normalize(NormalMatrix * att%d.xyz);\n",
					        input->attribid, use_geom ? "g" : "", input->attribid);
					BLI_dynstr_appendf(
					        ds, "\tvar%d%s.w = att%d.w;\n",
					        input->attribid, use_geom ? "g" : "", input->attribid);
				}
				else if (input->attribtype == CD_ORCO) {
					BLI_dynstr_appendf(
					        ds, "\tvar%d%s = OrcoTexCoFactors[0] + position * OrcoTexCoFactors[1];\n",
					        input->attribid, use_geom ? "g" : "");
				}
				else if (input->attribtype == CD_MCOL) {
					BLI_dynstr_appendf(
					        ds, "\tvar%d%s = srgb_to_linear_attrib(att%d);\n",
					        input->attribid, use_geom ? "g" : "", input->attribid);
				}
				else if (input->attribtype == CD_AUTO_FROM_NAME) {
					BLI_dynstr_appendf(
					        ds, "\tvar%d%s = (att%d_is_srgb) ? srgb_to_linear_attrib(att%d) : att%d;\n",
					        input->attribid, use_geom ? "g" : "",
					        input->attribid, input->attribid, input->attribid);
				}
				else {
					BLI_dynstr_appendf(
					        ds, "\tvar%d%s = att%d;\n",
					        input->attribid, use_geom ? "g" : "", input->attribid);
				}
			}
		}
	}
	BLI_dynstr_append(ds, "#endif /* HAIR_SHADER */\n");

	BLI_dynstr_append(ds, "}\n");

	BLI_dynstr_append(ds, vert_code);

	code = BLI_dynstr_get_cstring(ds);

	BLI_dynstr_free(ds);

#if 0
	if (G.debug & G_DEBUG) printf("%s\n", code);
#endif

	return code;
}

static char *code_generate_geometry(ListBase *nodes, const char *geom_code)
{
	DynStr *ds = BLI_dynstr_new();
	GPUNode *node;
	GPUInput *input;
	char *code;

	/* Create prototype because attributes cannot be declared before layout. */
	BLI_dynstr_appendf(ds, "void pass_attrib(in int vert);\n");
	BLI_dynstr_append(ds, "#define ATTRIB\n");

	BLI_dynstr_append(ds, geom_code);

	/* Generate varying declarations. */
	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
				BLI_dynstr_appendf(
				        ds, "in %s var%dg[];\n",
				        GPU_DATATYPE_STR[input->type],
				        input->attribid);
				BLI_dynstr_appendf(
				        ds, "out %s var%d;\n",
				        GPU_DATATYPE_STR[input->type],
				        input->attribid);
			}
		}
	}

	/* Generate varying assignments. */
	BLI_dynstr_appendf(ds, "void pass_attrib(in int vert) {\n");
	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
				/* TODO let shader choose what to do depending on what the attrib is. */
				BLI_dynstr_appendf(ds, "\tvar%d = var%dg[vert];\n", input->attribid, input->attribid);
			}
		}
	}
	BLI_dynstr_append(ds, "}\n");

	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	return code;
}

void GPU_code_generate_glsl_lib(void)
{
	DynStr *ds;

	/* only initialize the library once */
	if (glsl_material_library)
		return;

	ds = BLI_dynstr_new();

	BLI_dynstr_append(ds, datatoc_gpu_shader_material_glsl);


	glsl_material_library = BLI_dynstr_get_cstring(ds);

	BLI_dynstr_free(ds);
}


/* GPU pass binding/unbinding */

GPUShader *GPU_pass_shader_get(GPUPass *pass)
{
	return pass->shader;
}

void GPU_nodes_extract_dynamic_inputs(GPUShader *shader, ListBase *inputs, ListBase *nodes)
{
	GPUNode *node;
	GPUInput *next, *input;
	int extract, z;

	BLI_listbase_clear(inputs);

	if (!shader)
		return;

	GPU_shader_bind(shader);

	for (node = nodes->first; node; node = node->next) {
		z = 0;
		for (input = node->inputs.first; input; input = next, z++) {
			next = input->next;

			/* attributes don't need to be bound, they already have
			 * an id that the drawing functions will use */
			if (input->source == GPU_SOURCE_ATTRIB) {
				continue;
			}

			if (input->source == GPU_SOURCE_BUILTIN ||
			    input->source == GPU_SOURCE_OPENGL_BUILTIN)
			{
				continue;
			}

			if (input->ima || input->tex || input->prv)
				BLI_snprintf(input->shadername, sizeof(input->shadername), "samp%d", input->texid);
			else
				BLI_snprintf(input->shadername, sizeof(input->shadername), "unf%d", input->id);

			/* pass non-dynamic uniforms to opengl */
			extract = 0;

			if (input->ima || input->tex || input->prv) {
				if (input->bindtex)
					extract = 1;
			}
			else if (input->dynamictype == GPU_DYNAMIC_UBO) {
				/* Don't extract UBOs */
			}
			else if (input->dynamicvec) {
				extract = 1;
			}

			if (extract)
				input->shaderloc = GPU_shader_get_uniform(shader, input->shadername);

			/* extract nodes */
			if (extract) {
				BLI_remlink(&node->inputs, input);
				BLI_addtail(inputs, input);
			}
		}
	}

	GPU_shader_unbind();
}

/* Node Link Functions */

static GPUNodeLink *GPU_node_link_create(void)
{
	GPUNodeLink *link = MEM_callocN(sizeof(GPUNodeLink), "GPUNodeLink");
	link->type = GPU_NONE;
	link->users++;

	return link;
}

static void gpu_node_link_free(GPUNodeLink *link)
{
	link->users--;

	if (link->users < 0)
		fprintf(stderr, "GPU_node_link_free: negative refcount\n");

	if (link->users == 0) {
		if (link->output)
			link->output->link = NULL;
		MEM_freeN(link);
	}
}

/* Node Functions */

static GPUNode *GPU_node_begin(const char *name)
{
	GPUNode *node = MEM_callocN(sizeof(GPUNode), "GPUNode");

	node->name = name;

	return node;
}

static void gpu_node_input_link(GPUNode *node, GPUNodeLink *link, const GPUType type)
{
	GPUInput *input;
	GPUNode *outnode;
	const char *name;

	if (link->output) {
		outnode = link->output->node;
		name = outnode->name;
		input = outnode->inputs.first;

		if ((STREQ(name, "set_value") || STREQ(name, "set_rgb") || STREQ(name, "set_rgba")) &&
		    (input->type == type))
		{
			input = MEM_dupallocN(outnode->inputs.first);
			input->type = type;
			if (input->link)
				input->link->users++;
			BLI_addtail(&node->inputs, input);
			return;
		}
	}

	input = MEM_callocN(sizeof(GPUInput), "GPUInput");
	input->node = node;

	if (link->builtin) {
		/* builtin uniform */
		input->type = type;
		input->source = GPU_SOURCE_BUILTIN;
		input->builtin = link->builtin;

		MEM_freeN(link);
	}
	else if (link->oglbuiltin) {
		/* builtin uniform */
		input->type = type;
		input->source = GPU_SOURCE_OPENGL_BUILTIN;
		input->oglbuiltin = link->oglbuiltin;

		MEM_freeN(link);
	}
	else if (link->output) {
		/* link to a node output */
		input->type = type;
		input->source = GPU_SOURCE_TEX_PIXEL;
		input->link = link;
		link->users++;
	}
	else if (link->dynamictex) {
		/* dynamic texture, GPUTexture is updated/deleted externally */
		input->type = type;
		input->source = GPU_SOURCE_TEX;

		input->tex = link->dynamictex;
		input->textarget = GL_TEXTURE_2D;
		input->textype = type;
		input->dynamictex = true;
		input->dynamicdata = link->ptr2;
		MEM_freeN(link);
	}
	else if (link->texture) {
		/* small texture created on the fly, like for colorbands */
		input->type = GPU_VEC4;
		input->source = GPU_SOURCE_TEX;
		input->textype = type;

#if 0
		input->tex = GPU_texture_create_2D(link->texturesize, link->texturesize, link->ptr2, NULL);
#endif
		input->tex = GPU_texture_create_2D(link->texturesize, 1, GPU_RGBA8, link->ptr1, NULL);
		input->textarget = GL_TEXTURE_2D;

		MEM_freeN(link->ptr1);
		MEM_freeN(link);
	}
	else if (link->image) {
		/* blender image */
		input->type = GPU_VEC4;
		input->source = GPU_SOURCE_TEX;

		if (link->image == GPU_NODE_LINK_IMAGE_PREVIEW) {
			input->prv = link->ptr1;
			input->textarget = GL_TEXTURE_2D;
			input->textype = GPU_TEX2D;
		}
		else if (link->image == GPU_NODE_LINK_IMAGE_BLENDER) {
			input->ima = link->ptr1;
			input->iuser = link->ptr2;
			input->image_isdata = link->image_isdata;
			input->textarget = GL_TEXTURE_2D;
			input->textype = GPU_TEX2D;
		}
		else if (link->image == GPU_NODE_LINK_IMAGE_CUBE_MAP) {
			input->ima = link->ptr1;
			input->iuser = link->ptr2;
			input->image_isdata = link->image_isdata;
			input->textarget = GL_TEXTURE_CUBE_MAP;
			input->textype = GPU_TEXCUBE;
		}
		MEM_freeN(link);
	}
	else if (link->attribtype) {
		/* vertex attribute */
		input->type = type;
		input->source = GPU_SOURCE_ATTRIB;

		input->attribtype = link->attribtype;
		BLI_strncpy(input->attribname, link->attribname, sizeof(input->attribname));
		MEM_freeN(link);
	}
	else if (type == GPU_CLOSURE) {
		input->type = type;
		input->source = GPU_SOURCE_STRUCT;

		MEM_freeN(link);
	}
	else {
		/* uniform vector */
		input->type = type;
		input->source = GPU_SOURCE_VEC_UNIFORM;

		memcpy(input->vec, link->ptr1, type * sizeof(float));
		if (link->dynamic) {
			input->dynamicvec = link->ptr1;
			input->dynamictype = link->dynamictype;
			input->dynamicdata = link->ptr2;
		}

		MEM_freeN(link);
	}

	BLI_addtail(&node->inputs, input);
}


static const char *gpu_uniform_set_function_from_type(eNodeSocketDatatype type)
{
	switch (type) {
		case SOCK_FLOAT:
			return "set_value";
		case SOCK_VECTOR:
			return "set_rgb";
		case SOCK_RGBA:
			return "set_rgba";
		default:
			BLI_assert(!"No gpu function for non-supported eNodeSocketDatatype");
			return NULL;
	}
}

/**
 * Link stack uniform buffer.
 * This is called for the input/output sockets that are note connected.
 */
static GPUNodeLink *gpu_uniformbuffer_link(
        GPUMaterial *mat, bNode *node, GPUNodeStack *stack, const int index, const eNodeSocketInOut in_out)
{
	bNodeSocket *socket;

	/* Some nodes can have been create on the fly and does
	 * not have an original to point to. (i.e. the bump from
	 * ntree_shader_relink_displacement). In this case just
	 * revert to static constant folding. */
	if (node->original == NULL) {
		return NULL;
	}

	if (in_out == SOCK_IN) {
		socket = BLI_findlink(&node->original->inputs, index);
	}
	else {
		socket = BLI_findlink(&node->original->outputs, index);
	}

	BLI_assert(socket != NULL);
	BLI_assert(socket->in_out == in_out);

	if ((socket->flag & SOCK_HIDE_VALUE) == 0) {
		GPUNodeLink *link;
		switch (socket->type) {
			case SOCK_FLOAT:
			{
				bNodeSocketValueFloat *socket_data = socket->default_value;
				link = GPU_uniform_buffer(&socket_data->value, GPU_FLOAT);
				break;
			}
			case SOCK_VECTOR:
			{
				bNodeSocketValueRGBA *socket_data = socket->default_value;
				link = GPU_uniform_buffer(socket_data->value, GPU_VEC3);
				break;
			}
			case SOCK_RGBA:
			{
				bNodeSocketValueRGBA *socket_data = socket->default_value;
				link = GPU_uniform_buffer(socket_data->value, GPU_VEC4);
				break;
			}
			default:
				return NULL;
				break;
		}

		if (in_out == SOCK_IN) {
			GPU_link(mat, gpu_uniform_set_function_from_type(socket->type), link, &stack->link);
		}
		return link;
	}
	return NULL;
}

static void gpu_node_input_socket(GPUMaterial *material, bNode *bnode, GPUNode *node, GPUNodeStack *sock, const int index)
{
	if (sock->link) {
		gpu_node_input_link(node, sock->link, sock->type);
	}
	else if ((material != NULL) && (gpu_uniformbuffer_link(material, bnode, sock, index, SOCK_IN) != NULL)) {
		gpu_node_input_link(node, sock->link, sock->type);
	}
	else {
		GPUNodeLink *link = GPU_node_link_create();
		link->ptr1 = sock->vec;
		gpu_node_input_link(node, link, sock->type);
	}
}

static void gpu_node_output(GPUNode *node, const GPUType type, GPUNodeLink **link)
{
	GPUOutput *output = MEM_callocN(sizeof(GPUOutput), "GPUOutput");

	output->type = type;
	output->node = node;

	if (link) {
		*link = output->link = GPU_node_link_create();
		output->link->type = type;
		output->link->output = output;

		/* note: the caller owns the reference to the link, GPUOutput
		 * merely points to it, and if the node is destroyed it will
		 * set that pointer to NULL */
	}

	BLI_addtail(&node->outputs, output);
}

void GPU_inputs_free(ListBase *inputs)
{
	GPUInput *input;

	for (input = inputs->first; input; input = input->next) {
		if (input->link)
			gpu_node_link_free(input->link);
		else if (input->tex && !input->dynamictex)
			GPU_texture_free(input->tex);
	}

	BLI_freelistN(inputs);
}

static void gpu_node_free(GPUNode *node)
{
	GPUOutput *output;

	GPU_inputs_free(&node->inputs);

	for (output = node->outputs.first; output; output = output->next)
		if (output->link) {
			output->link->output = NULL;
			gpu_node_link_free(output->link);
		}

	BLI_freelistN(&node->outputs);
	MEM_freeN(node);
}

static void gpu_nodes_free(ListBase *nodes)
{
	GPUNode *node;

	while ((node = BLI_pophead(nodes))) {
		gpu_node_free(node);
	}
}

/* vertex attributes */

void GPU_nodes_get_vertex_attributes(ListBase *nodes, GPUVertexAttribs *attribs)
{
	GPUNode *node;
	GPUInput *input;
	int a;

	/* convert attributes requested by node inputs to an array of layers,
	 * checking for duplicates and assigning id's starting from zero. */

	memset(attribs, 0, sizeof(*attribs));

	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			if (input->source == GPU_SOURCE_ATTRIB) {
				for (a = 0; a < attribs->totlayer; a++) {
					if (attribs->layer[a].type == input->attribtype &&
					    STREQ(attribs->layer[a].name, input->attribname))
					{
						break;
					}
				}

				if (a < GPU_MAX_ATTRIB) {
					if (a == attribs->totlayer) {
						input->attribid = attribs->totlayer++;
						input->attribfirst = 1;

						attribs->layer[a].type = input->attribtype;
						attribs->layer[a].attribid = input->attribid;
						BLI_strncpy(
						        attribs->layer[a].name, input->attribname,
						        sizeof(attribs->layer[a].name));
					}
					else {
						input->attribid = attribs->layer[a].attribid;
					}
				}
			}
		}
	}
}

/* varargs linking  */

GPUNodeLink *GPU_attribute(const CustomDataType type, const char *name)
{
	GPUNodeLink *link = GPU_node_link_create();

	/* Fall back to the UV layer, which matches old behavior. */
	if (type == CD_AUTO_FROM_NAME && name[0] == '\0') {
		link->attribtype = CD_MTFACE;
	}
	else {
		link->attribtype = type;
	}

	link->attribname = name;

	return link;
}

GPUNodeLink *GPU_uniform(float *num)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->ptr1 = num;
	link->ptr2 = NULL;

	return link;
}

GPUNodeLink *GPU_dynamic_uniform(float *num, GPUDynamicType dynamictype, void *data)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->ptr1 = num;
	link->ptr2 = data;
	link->dynamic = true;
	link->dynamictype = dynamictype;


	return link;
}

/**
 * Add uniform to UBO struct of GPUMaterial.
 */
GPUNodeLink *GPU_uniform_buffer(float *num, GPUType gputype)
{
	GPUNodeLink *link = GPU_node_link_create();
	link->ptr1 = num;
	link->ptr2 = NULL;
	link->dynamic = true;
	link->dynamictype = GPU_DYNAMIC_UBO;
	link->type = gputype;

	return link;
}

GPUNodeLink *GPU_image(Image *ima, ImageUser *iuser, bool is_data)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->image = GPU_NODE_LINK_IMAGE_BLENDER;
	link->ptr1 = ima;
	link->ptr2 = iuser;
	link->image_isdata = is_data;

	return link;
}

GPUNodeLink *GPU_cube_map(Image *ima, ImageUser *iuser, bool is_data)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->image = GPU_NODE_LINK_IMAGE_CUBE_MAP;
	link->ptr1 = ima;
	link->ptr2 = iuser;
	link->image_isdata = is_data;

	return link;
}

GPUNodeLink *GPU_image_preview(PreviewImage *prv)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->image = GPU_NODE_LINK_IMAGE_PREVIEW;
	link->ptr1 = prv;

	return link;
}


GPUNodeLink *GPU_texture(int size, float *pixels)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->texture = true;
	link->texturesize = size;
	link->ptr1 = pixels;

	return link;
}

GPUNodeLink *GPU_dynamic_texture(GPUTexture *tex, GPUDynamicType dynamictype, void *data)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->dynamic = true;
	link->dynamictex = tex;
	link->dynamictype = dynamictype;
	link->ptr2 = data;

	return link;
}

GPUNodeLink *GPU_builtin(GPUBuiltin builtin)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->builtin = builtin;

	return link;
}

GPUNodeLink *GPU_opengl_builtin(GPUOpenGLBuiltin builtin)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->oglbuiltin = builtin;

	return link;
}

bool GPU_link(GPUMaterial *mat, const char *name, ...)
{
	GPUNode *node;
	GPUFunction *function;
	GPUNodeLink *link, **linkptr;
	va_list params;
	int i;

	function = gpu_lookup_function(name);
	if (!function) {
		fprintf(stderr, "GPU failed to find function %s\n", name);
		return false;
	}

	node = GPU_node_begin(name);

	va_start(params, name);
	for (i = 0; i < function->totparam; i++) {
		if (function->paramqual[i] != FUNCTION_QUAL_IN) {
			linkptr = va_arg(params, GPUNodeLink **);
			gpu_node_output(node, function->paramtype[i], linkptr);
		}
		else {
			link = va_arg(params, GPUNodeLink *);
			gpu_node_input_link(node, link, function->paramtype[i]);
		}
	}
	va_end(params);

	gpu_material_add_node(mat, node);

	return true;
}

bool GPU_stack_link(GPUMaterial *material, bNode *bnode, const char *name, GPUNodeStack *in, GPUNodeStack *out, ...)
{
	GPUNode *node;
	GPUFunction *function;
	GPUNodeLink *link, **linkptr;
	va_list params;
	int i, totin, totout;

	function = gpu_lookup_function(name);
	if (!function) {
		fprintf(stderr, "GPU failed to find function %s\n", name);
		return false;
	}

	node = GPU_node_begin(name);
	totin = 0;
	totout = 0;

	if (in) {
		for (i = 0; !in[i].end; i++) {
			if (in[i].type != GPU_NONE) {
				gpu_node_input_socket(material, bnode, node, &in[i], i);
				totin++;
			}
		}
	}

	if (out) {
		for (i = 0; !out[i].end; i++) {
			if (out[i].type != GPU_NONE) {
				gpu_node_output(node, out[i].type, &out[i].link);
				totout++;
			}
		}
	}

	va_start(params, out);
	for (i = 0; i < function->totparam; i++) {
		if (function->paramqual[i] != FUNCTION_QUAL_IN) {
			if (totout == 0) {
				linkptr = va_arg(params, GPUNodeLink **);
				gpu_node_output(node, function->paramtype[i], linkptr);
			}
			else
				totout--;
		}
		else {
			if (totin == 0) {
				link = va_arg(params, GPUNodeLink *);
				if (link->socket)
					gpu_node_input_socket(NULL, NULL, node, link->socket, -1);
				else
					gpu_node_input_link(node, link, function->paramtype[i]);
			}
			else
				totin--;
		}
	}
	va_end(params);

	gpu_material_add_node(material, node);

	return true;
}

int GPU_link_changed(GPUNodeLink *link)
{
	GPUNode *node;
	GPUInput *input;
	const char *name;

	if (link->output) {
		node = link->output->node;
		name = node->name;

		if (STREQ(name, "set_value") || STREQ(name, "set_rgb")) {
			input = node->inputs.first;
			return (input->link != NULL);
		}

		return 1;
	}
	else
		return 0;
}

GPUNodeLink *GPU_uniformbuffer_link_out(GPUMaterial *mat, bNode *node, GPUNodeStack *stack, const int index)
{
	return gpu_uniformbuffer_link(mat, node, stack, index, SOCK_OUT);
}

/* Pass create/free */

static void gpu_nodes_tag(GPUNodeLink *link)
{
	GPUNode *node;
	GPUInput *input;

	if (!link->output)
		return;

	node = link->output->node;
	if (node->tag)
		return;

	node->tag = true;
	for (input = node->inputs.first; input; input = input->next)
		if (input->link)
			gpu_nodes_tag(input->link);
}

void GPU_nodes_prune(ListBase *nodes, GPUNodeLink *outlink)
{
	GPUNode *node, *next;

	for (node = nodes->first; node; node = node->next)
		node->tag = false;

	gpu_nodes_tag(outlink);

	for (node = nodes->first; node; node = next) {
		next = node->next;

		if (!node->tag) {
			BLI_remlink(nodes, node);
			gpu_node_free(node);
		}
	}
}

static bool gpu_pass_is_valid(GPUPass *pass)
{
	/* Shader is not null if compilation is successful. */
	return (pass->compiled == false || pass->shader != NULL);
}

GPUPass *GPU_generate_pass_new(
        GPUMaterial *material,
        GPUNodeLink *frag_outlink,
        struct GPUVertexAttribs *attribs,
        ListBase *nodes,
        const char *vert_code,
        const char *geom_code,
        const char *frag_lib,
        const char *defines)
{
	char *vertexcode, *geometrycode, *fragmentcode;
	GPUPass *pass = NULL, *pass_hash = NULL;

	/* prune unused nodes */
	GPU_nodes_prune(nodes, frag_outlink);

	GPU_nodes_get_vertex_attributes(nodes, attribs);

	/* generate code */
	char *fragmentgen = code_generate_fragment(material, nodes, frag_outlink->output);

	/* Cache lookup: Reuse shaders already compiled */
	uint32_t hash = gpu_pass_hash(fragmentgen, defines, attribs);
	pass_hash = gpu_pass_cache_lookup(hash);

	if (pass_hash && (pass_hash->next == NULL || pass_hash->next->hash != hash)) {
		/* No collision, just return the pass. */
		MEM_freeN(fragmentgen);
		if (!gpu_pass_is_valid(pass_hash)) {
			/* Shader has already been created but failed to compile. */
			return NULL;
		}
		pass_hash->refcount += 1;
		return pass_hash;
	}

	/* Either the shader is not compiled or there is a hash collision...
	 * continue generating the shader strings. */
	char *tmp = BLI_strdupcat(frag_lib, glsl_material_library);

	vertexcode = code_generate_vertex(nodes, vert_code, (geom_code != NULL));
	geometrycode = (geom_code) ? code_generate_geometry(nodes, geom_code) : NULL;
	fragmentcode = BLI_strdupcat(tmp, fragmentgen);

	MEM_freeN(fragmentgen);
	MEM_freeN(tmp);

	if (pass_hash) {
		/* Cache lookup: Reuse shaders already compiled */
		pass = gpu_pass_cache_resolve_collision(pass_hash, vertexcode, geometrycode, fragmentcode, defines, hash);
	}

	if (pass) {
		/* Cache hit. Reuse the same GPUPass and GPUShader. */
		if (!gpu_pass_is_valid(pass)) {
			/* Shader has already been created but failed to compile. */
			return NULL;
		}

		MEM_SAFE_FREE(vertexcode);
		MEM_SAFE_FREE(fragmentcode);
		MEM_SAFE_FREE(geometrycode);

		pass->refcount += 1;
	}
	else {
		/* We still create a pass even if shader compilation
		 * fails to avoid trying to compile again and again. */
		pass = MEM_callocN(sizeof(GPUPass), "GPUPass");
		pass->shader = NULL;
		pass->refcount = 1;
		pass->hash = hash;
		pass->vertexcode = vertexcode;
		pass->fragmentcode = fragmentcode;
		pass->geometrycode = geometrycode;
		pass->defines = (defines) ? BLI_strdup(defines) : NULL;
		pass->compiled = false;

		BLI_spin_lock(&pass_cache_spin);
		if (pass_hash != NULL) {
			/* Add after the first pass having the same hash. */
			pass->next = pass_hash->next;
			pass_hash->next = pass;
		}
		else {
			/* No other pass have same hash, just prepend to the list. */
			BLI_LINKS_PREPEND(pass_cache, pass);
		}
		BLI_spin_unlock(&pass_cache_spin);
	}

	return pass;
}

void GPU_pass_compile(GPUPass *pass, const char *shname)
{
	if (!pass->compiled) {
		pass->shader = GPU_shader_create(
		        pass->vertexcode,
		        pass->fragmentcode,
		        pass->geometrycode,
		        NULL,
		        pass->defines,
		        shname);
		pass->compiled = true;
	}
}

void GPU_pass_release(GPUPass *pass)
{
	BLI_assert(pass->refcount > 0);
	pass->refcount--;
}

static void gpu_pass_free(GPUPass *pass)
{
	BLI_assert(pass->refcount == 0);
	if (pass->shader) {
		GPU_shader_free(pass->shader);
	}
	MEM_SAFE_FREE(pass->fragmentcode);
	MEM_SAFE_FREE(pass->geometrycode);
	MEM_SAFE_FREE(pass->vertexcode);
	MEM_SAFE_FREE(pass->defines);
	MEM_freeN(pass);
}

void GPU_pass_free_nodes(ListBase *nodes)
{
	gpu_nodes_free(nodes);
}

void GPU_pass_cache_garbage_collect(void)
{
	static int lasttime = 0;
	const int shadercollectrate = 60; /* hardcoded for now. */
	int ctime = (int)PIL_check_seconds_timer();

	if (ctime < shadercollectrate + lasttime)
		return;

	lasttime = ctime;

	BLI_spin_lock(&pass_cache_spin);
	GPUPass *next, **prev_pass = &pass_cache;
	for (GPUPass *pass = pass_cache; pass; pass = next) {
		next = pass->next;
		if (pass->refcount == 0) {
			/* Remove from list */
			*prev_pass = next;
			gpu_pass_free(pass);
		}
		else {
			prev_pass = &pass->next;
		}
	}
	BLI_spin_unlock(&pass_cache_spin);
}

void GPU_pass_cache_init(void)
{
	BLI_spin_init(&pass_cache_spin);
}

void GPU_pass_cache_free(void)
{
	BLI_spin_lock(&pass_cache_spin);
	while (pass_cache) {
		GPUPass *next = pass_cache->next;
		gpu_pass_free(pass_cache);
		pass_cache = next;
	}
	BLI_spin_unlock(&pass_cache_spin);

	BLI_spin_end(&pass_cache_spin);
}
