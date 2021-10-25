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

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"

#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "gpu_codegen.h"

#include <string.h>
#include <stdarg.h>

extern char datatoc_gpu_shader_material_glsl[];
extern char datatoc_gpu_shader_vertex_glsl[];
extern char datatoc_gpu_shader_vertex_world_glsl[];
extern char datatoc_gpu_shader_geometry_glsl[];

static char *glsl_material_library = NULL;


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
	NULL, NULL, NULL, NULL, "mat3", NULL, NULL, NULL, NULL, NULL, NULL, "mat4",
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
			for (i = 1; i <= 16; i++) {
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
	else {
		if (from == GPU_VEC3)
			BLI_dynstr_appendf(ds, "vec4(%s, 1.0)", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_appendf(ds, "vec4(%s.r, %s.r, %s.r, %s.g)", name, name, name, name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_appendf(ds, "vec4(%s, %s, %s, 1.0)", name, name, name);
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

static int codegen_print_uniforms_functions(DynStr *ds, ListBase *nodes)
{
	GPUNode *node;
	GPUInput *input;
	const char *name;
	int builtins = 0;

	/* print uniforms */
	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			if ((input->source == GPU_SOURCE_TEX) || (input->source == GPU_SOURCE_TEX_PIXEL)) {
				/* create exactly one sampler for each texture */
				if (codegen_input_has_texture(input) && input->bindtex) {
					BLI_dynstr_appendf(ds, "uniform %s samp%d;\n",
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

					if (gpu_str_prefix(name, "unf")) {
						BLI_dynstr_appendf(ds, "uniform %s %s;\n",
							GPU_DATATYPE_STR[input->type], name);
					}
					else {
						BLI_dynstr_appendf(ds, "%s %s %s;\n",
							GLEW_VERSION_3_0 ? "in" : "varying",
							GPU_DATATYPE_STR[input->type], name);
					}
				}
			}
			else if (input->source == GPU_SOURCE_VEC_UNIFORM) {
				if (input->dynamicvec) {
					/* only create uniforms for dynamic vectors */
					BLI_dynstr_appendf(ds, "uniform %s unf%d;\n",
						GPU_DATATYPE_STR[input->type], input->id);
				}
				else {
					/* for others use const so the compiler can do folding */
					BLI_dynstr_appendf(ds, "const %s cons%d = ",
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
				BLI_dynstr_appendf(ds, "%s %s var%d;\n",
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
					BLI_dynstr_appendf(ds, "\tvec4 tex%d = texture2D(", input->texid);
					BLI_dynstr_appendf(ds, "samp%d, gl_TexCoord[%d].st);\n",
					                   input->texid, input->texid);
				}
			}
		}

		/* declare temporary variables for node output storage */
		for (output = node->outputs.first; output; output = output->next) {
			BLI_dynstr_appendf(ds, "\t%s tmp%d;\n",
			                   GPU_DATATYPE_STR[output->type], output->id);
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
				codegen_convert_datatype(ds, input->link->output->type, input->type,
					"tmp", input->link->output->id);
			}
			else if (input->source == GPU_SOURCE_BUILTIN) {
				if (input->builtin == GPU_VIEW_NORMAL)
					BLI_dynstr_append(ds, "facingnormal");
				else
					BLI_dynstr_append(ds, GPU_builtin_name(input->builtin));
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

	BLI_dynstr_append(ds, "\n\tgl_FragColor = ");
	codegen_convert_datatype(ds, finaloutput->type, GPU_VEC4, "tmp", finaloutput->id);
	BLI_dynstr_append(ds, ";\n");
}

static char *code_generate_fragment(ListBase *nodes, GPUOutput *output)
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
	builtins = codegen_print_uniforms_functions(ds, nodes);

#if 0
	if (G.debug & G_DEBUG)
		BLI_dynstr_appendf(ds, "/* %s */\n", name);
#endif

	BLI_dynstr_append(ds, "void main()\n{\n");

	if (builtins & GPU_VIEW_NORMAL)
		BLI_dynstr_append(ds, "\tvec3 facingnormal = gl_FrontFacing? varnormal: -varnormal;\n");

	/* Calculate tangent space. */
#ifdef WITH_OPENSUBDIV
	{
		bool has_tangent = false;
		for (node = nodes->first; node; node = node->next) {
			for (input = node->inputs.first; input; input = input->next) {
				if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
					if (input->attribtype == CD_TANGENT) {
						BLI_dynstr_appendf(ds, "#ifdef USE_OPENSUBDIV\n");
						BLI_dynstr_appendf(ds, "\t%s var%d;\n",
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

	/* create shader */
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

#if 0
	if (G.debug & G_DEBUG) printf("%s\n", code);
#endif

	return code;
}

static char *code_generate_vertex(ListBase *nodes, const GPUMatType type)
{
	DynStr *ds = BLI_dynstr_new();
	GPUNode *node;
	GPUInput *input;
	char *code;
	char *vertcode = NULL;
	
	for (node = nodes->first; node; node = node->next) {
		for (input = node->inputs.first; input; input = input->next) {
			if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
#ifdef WITH_OPENSUBDIV
				bool skip_opensubdiv = ELEM(input->attribtype, CD_MTFACE, CD_TANGENT);
				if (skip_opensubdiv) {
					BLI_dynstr_appendf(ds, "#ifndef USE_OPENSUBDIV\n");
				}
#endif
				BLI_dynstr_appendf(ds, "%s %s att%d;\n",
					GLEW_VERSION_3_0 ? "in" : "attribute",
					GPU_DATATYPE_STR[input->type], input->attribid);
				BLI_dynstr_appendf(ds, "uniform int att%d_info;\n",  input->attribid);
				BLI_dynstr_appendf(ds, "%s %s var%d;\n",
					GLEW_VERSION_3_0 ? "out" : "varying",
					GPU_DATATYPE_STR[input->type], input->attribid);
#ifdef WITH_OPENSUBDIV
				if (skip_opensubdiv) {
					BLI_dynstr_appendf(ds, "#endif\n");
				}
#endif
			}
		}
	}

	BLI_dynstr_append(ds, "\n");

	switch (type) {
		case GPU_MATERIAL_TYPE_MESH:
			vertcode = datatoc_gpu_shader_vertex_glsl;
			break;
		case GPU_MATERIAL_TYPE_WORLD:
			vertcode = datatoc_gpu_shader_vertex_world_glsl;
			break;
		default:
			fprintf(stderr, "invalid material type, set one after GPU_material_construct_begin\n");
			break;
	}

	BLI_dynstr_append(ds, vertcode);
	
	for (node = nodes->first; node; node = node->next)
		for (input = node->inputs.first; input; input = input->next)
			if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
				if (input->attribtype == CD_TANGENT) { /* silly exception */
#ifdef WITH_OPENSUBDIV
					BLI_dynstr_appendf(ds, "#ifndef USE_OPENSUBDIV\n");
#endif
					BLI_dynstr_appendf(
					        ds, "\tvar%d.xyz = normalize(gl_NormalMatrix * att%d.xyz);\n",
					        input->attribid, input->attribid);
					BLI_dynstr_appendf(
					        ds, "\tvar%d.w = att%d.w;\n",
					        input->attribid, input->attribid);
#ifdef WITH_OPENSUBDIV
					BLI_dynstr_appendf(ds, "#endif\n");
#endif
				}
				else {
#ifdef WITH_OPENSUBDIV
					bool is_mtface = input->attribtype == CD_MTFACE;
					if (is_mtface) {
						BLI_dynstr_appendf(ds, "#ifndef USE_OPENSUBDIV\n");
					}
#endif
					BLI_dynstr_appendf(ds, "\tset_var_from_attr(att%d, att%d_info, var%d);\n",
					                   input->attribid, input->attribid, input->attribid);
#ifdef WITH_OPENSUBDIV
					if (is_mtface) {
						BLI_dynstr_appendf(ds, "#endif\n");
					}
#endif
				}
			}
			/* unfortunately special handling is needed here because we abuse gl_Color/gl_SecondaryColor flat shading */
			else if (input->source == GPU_SOURCE_OPENGL_BUILTIN) {
				if (input->oglbuiltin == GPU_MATCAP_NORMAL) {
					/* remap to 0.0 - 1.0 range. This is done because OpenGL 2.0 clamps colors
					 * between shader stages and we want the full range of the normal */
					BLI_dynstr_appendf(ds, "\tvec3 matcapcol = vec3(0.5) * varnormal + vec3(0.5);\n");
					BLI_dynstr_appendf(ds, "\tgl_FrontSecondaryColor = vec4(matcapcol, 1.0);\n");
				}
				else if (input->oglbuiltin == GPU_COLOR) {
					BLI_dynstr_appendf(ds, "\tgl_FrontColor = gl_Color;\n");
				}
			}

	BLI_dynstr_append(ds, "}\n");

	code = BLI_dynstr_get_cstring(ds);

	BLI_dynstr_free(ds);

#if 0
	if (G.debug & G_DEBUG) printf("%s\n", code);
#endif

	return code;
}

static char *code_generate_geometry(ListBase *nodes, bool use_opensubdiv)
{
#ifdef WITH_OPENSUBDIV
	if (use_opensubdiv) {
		DynStr *ds = BLI_dynstr_new();
		GPUNode *node;
		GPUInput *input;
		char *code;

		/* Generate varying declarations. */
		for (node = nodes->first; node; node = node->next) {
			for (input = node->inputs.first; input; input = input->next) {
				if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
					if (input->attribtype == CD_MTFACE) {
						/* NOTE: For now we are using varying on purpose,
						 * otherwise we are not able to write to the varying.
						 */
						BLI_dynstr_appendf(ds, "%s %s var%d%s;\n",
						                   "varying",
						                   GPU_DATATYPE_STR[input->type],
						                   input->attribid,
						                   "");
						BLI_dynstr_appendf(ds, "uniform int fvar%d_offset;\n",
						                   input->attribid);
					}
				}
			}
		}

		BLI_dynstr_append(ds, datatoc_gpu_shader_geometry_glsl);

		/* Generate varying assignments. */
		for (node = nodes->first; node; node = node->next) {
			for (input = node->inputs.first; input; input = input->next) {
				if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
					if (input->attribtype == CD_MTFACE) {
						BLI_dynstr_appendf(
						        ds,
						        "\tINTERP_FACE_VARYING_ATT_2(var%d, "
						        "int(texelFetch(FVarDataOffsetBuffer, fvar%d_offset).r), st);\n",
						        input->attribid,
						        input->attribid);
					}
				}
			}
		}

		BLI_dynstr_append(ds, "}\n");
		code = BLI_dynstr_get_cstring(ds);
		BLI_dynstr_free(ds);

		//if (G.debug & G_DEBUG) printf("%s\n", code);

		return code;
	}
#else
	UNUSED_VARS(nodes, use_opensubdiv);
#endif
	return NULL;
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

GPUShader *GPU_pass_shader(GPUPass *pass)
{
	return pass->shader;
}

static void gpu_nodes_extract_dynamic_inputs(GPUPass *pass, ListBase *nodes)
{
	GPUShader *shader = pass->shader;
	GPUNode *node;
	GPUInput *next, *input;
	ListBase *inputs = &pass->inputs;
	int extract, z;

	memset(inputs, 0, sizeof(*inputs));

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
#ifdef WITH_OPENSUBDIV
				/* We do need mtface attributes for later, so we can
				 * update face-varuing variables offset in the texture
				 * buffer for proper sampling from the shader.
				 *
				 * We don't do anything about attribute itself, we
				 * only use it to learn which uniform name is to be
				 * updated.
				 *
				 * TODO(sergey): We can add ad extra uniform input
				 * for the offset, which will be purely internal and
				 * which would avoid having such an exceptions.
				 */
				if (input->attribtype != CD_MTFACE) {
					continue;
				}
#else
				continue;
#endif
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
			else if (input->dynamicvec)
				extract = 1;

			if (extract)
				input->shaderloc = GPU_shader_get_uniform(shader, input->shadername);

#ifdef WITH_OPENSUBDIV
			if (input->source == GPU_SOURCE_ATTRIB &&
			    input->attribtype == CD_MTFACE)
			{
				extract = 1;
			}
#endif

			/* extract nodes */
			if (extract) {
				BLI_remlink(&node->inputs, input);
				BLI_addtail(inputs, input);
			}
		}
	}

	GPU_shader_unbind();
}

void GPU_pass_bind(GPUPass *pass, double time, int mipmap)
{
	GPUInput *input;
	GPUShader *shader = pass->shader;
	ListBase *inputs = &pass->inputs;

	if (!shader)
		return;

	GPU_shader_bind(shader);

	/* create the textures */
	for (input = inputs->first; input; input = input->next) {
		if (input->ima)
			input->tex = GPU_texture_from_blender(input->ima, input->iuser, input->textarget, input->image_isdata, time, mipmap);
		else if (input->prv)
			input->tex = GPU_texture_from_preview(input->prv, mipmap);
	}

	/* bind the textures, in second loop so texture binding during
	 * create doesn't overwrite already bound textures */
	for (input = inputs->first; input; input = input->next) {
		if (input->tex && input->bindtex) {
			GPU_texture_bind(input->tex, input->texid);
			GPU_shader_uniform_texture(shader, input->shaderloc, input->tex);
		}
	}
}

void GPU_pass_update_uniforms(GPUPass *pass)
{
	GPUInput *input;
	GPUShader *shader = pass->shader;
	ListBase *inputs = &pass->inputs;

	if (!shader)
		return;

	/* pass dynamic inputs to opengl, others were removed */
	for (input = inputs->first; input; input = input->next) {
		if (!(input->ima || input->tex || input->prv)) {
			if (input->dynamictype == GPU_DYNAMIC_MAT_HARD) {
				// The hardness is actually a short pointer, so we convert it here
				float val = (float)(*(short *)input->dynamicvec);
				GPU_shader_uniform_vector(shader, input->shaderloc, 1, 1, &val);
			}
			else {
				GPU_shader_uniform_vector(shader, input->shaderloc, input->type, 1,
					input->dynamicvec);
			}
		}
	}
}

void GPU_pass_unbind(GPUPass *pass)
{
	GPUInput *input;
	GPUShader *shader = pass->shader;
	ListBase *inputs = &pass->inputs;

	if (!shader)
		return;

	for (input = inputs->first; input; input = input->next) {
		if (input->tex && input->bindtex)
			GPU_texture_unbind(input->tex);

		if (input->ima || input->prv)
			input->tex = NULL;
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

		if ((STREQ(name, "set_value") || STREQ(name, "set_rgb")) &&
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
		input->tex = GPU_texture_create_2D(link->texturesize, 1, link->ptr1, GPU_HDR_NONE, NULL);
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

static void gpu_node_input_socket(GPUNode *node, GPUNodeStack *sock)
{
	GPUNodeLink *link;

	if (sock->link) {
		gpu_node_input_link(node, sock->link, sock->type);
	}
	else {
		link = GPU_node_link_create();
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

static void gpu_inputs_free(ListBase *inputs)
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

	gpu_inputs_free(&node->inputs);

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

static void gpu_nodes_get_vertex_attributes(ListBase *nodes, GPUVertexAttribs *attribs)
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
						BLI_strncpy(attribs->layer[a].name, input->attribname,
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

static void gpu_nodes_get_builtin_flag(ListBase *nodes, int *builtin)
{
	GPUNode *node;
	GPUInput *input;
	
	*builtin = 0;

	for (node = nodes->first; node; node = node->next)
		for (input = node->inputs.first; input; input = input->next)
			if (input->source == GPU_SOURCE_BUILTIN)
				*builtin |= input->builtin;
}

/* varargs linking  */

GPUNodeLink *GPU_attribute(const CustomDataType type, const char *name)
{
	GPUNodeLink *link = GPU_node_link_create();

	link->attribtype = type;
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

bool GPU_stack_link(GPUMaterial *mat, const char *name, GPUNodeStack *in, GPUNodeStack *out, ...)
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
		for (i = 0; in[i].type != GPU_NONE; i++) {
			gpu_node_input_socket(node, &in[i]);
			totin++;
		}
	}
	
	if (out) {
		for (i = 0; out[i].type != GPU_NONE; i++) {
			gpu_node_output(node, out[i].type, &out[i].link);
			totout++;
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
					gpu_node_input_socket(node, link->socket);
				else
					gpu_node_input_link(node, link, function->paramtype[i]);
			}
			else
				totin--;
		}
	}
	va_end(params);

	gpu_material_add_node(mat, node);
	
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

static void gpu_nodes_prune(ListBase *nodes, GPUNodeLink *outlink)
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

GPUPass *GPU_generate_pass(
        ListBase *nodes, GPUNodeLink *outlink,
        GPUVertexAttribs *attribs, int *builtins,
        const GPUMatType type, const char *UNUSED(name),
        const bool use_opensubdiv,
        const bool use_new_shading)
{
	GPUShader *shader;
	GPUPass *pass;
	char *vertexcode, *geometrycode, *fragmentcode;

#if 0
	if (!FUNCTION_LIB) {
		GPU_nodes_free(nodes);
		return NULL;
	}
#endif

	/* prune unused nodes */
	gpu_nodes_prune(nodes, outlink);

	gpu_nodes_get_vertex_attributes(nodes, attribs);
	gpu_nodes_get_builtin_flag(nodes, builtins);

	/* generate code and compile with opengl */
	fragmentcode = code_generate_fragment(nodes, outlink->output);
	vertexcode = code_generate_vertex(nodes, type);
	geometrycode = code_generate_geometry(nodes, use_opensubdiv);

	int flags = GPU_SHADER_FLAGS_NONE;
	if (use_opensubdiv) {
		flags |= GPU_SHADER_FLAGS_SPECIAL_OPENSUBDIV;
	}
	if (use_new_shading) {
		flags |= GPU_SHADER_FLAGS_NEW_SHADING;
	}
	shader = GPU_shader_create_ex(vertexcode,
	                              fragmentcode,
	                              geometrycode,
	                              glsl_material_library,
	                              NULL,
	                              0,
	                              0,
	                              0,
	                              flags);

	/* failed? */
	if (!shader) {
		if (fragmentcode)
			MEM_freeN(fragmentcode);
		if (vertexcode)
			MEM_freeN(vertexcode);
		memset(attribs, 0, sizeof(*attribs));
		memset(builtins, 0, sizeof(*builtins));
		gpu_nodes_free(nodes);
		return NULL;
	}
	
	/* create pass */
	pass = MEM_callocN(sizeof(GPUPass), "GPUPass");

	pass->output = outlink->output;
	pass->shader = shader;
	pass->fragmentcode = fragmentcode;
	pass->geometrycode = geometrycode;
	pass->vertexcode = vertexcode;
	pass->libcode = glsl_material_library;

	/* extract dynamic inputs and throw away nodes */
	gpu_nodes_extract_dynamic_inputs(pass, nodes);
	gpu_nodes_free(nodes);

	return pass;
}

void GPU_pass_free(GPUPass *pass)
{
	GPU_shader_free(pass->shader);
	gpu_inputs_free(&pass->inputs);
	if (pass->fragmentcode)
		MEM_freeN(pass->fragmentcode);
	if (pass->geometrycode)
		MEM_freeN(pass->geometrycode);
	if (pass->vertexcode)
		MEM_freeN(pass->vertexcode);
	MEM_freeN(pass);
}

void GPU_pass_free_nodes(ListBase *nodes)
{
	gpu_nodes_free(nodes);
}

