/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_image_types.h"
#include "DNA_listBase.h"
#include "DNA_material_types.h"

#include "BLI_dynstr.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_heap.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "GPU_material.h"
#include "GPU_extensions.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "gpu_codegen.h"

#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#ifndef vsnprintf
#define vsnprintf _vsnprintf
#endif
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

extern char datatoc_gpu_shader_material_glsl[];
extern char datatoc_gpu_shader_vertex_glsl[];

/* structs and defines */

typedef enum GPUDataSource {
	GPU_SOURCE_VEC_UNIFORM,
	GPU_SOURCE_BUILTIN,
	GPU_SOURCE_TEX_PIXEL,
	GPU_SOURCE_TEX,
	GPU_SOURCE_ATTRIB
} GPUDataSource;

static char* GPU_DATATYPE_STR[17] = {"", "float", "vec2", "vec3", "vec4",
	0, 0, 0, 0, "mat3", 0, 0, 0, 0, 0, 0, "mat4"};

struct GPUNode {
	struct GPUNode *next, *prev;

	char *name;
	int tag;

	ListBase inputs;
	ListBase outputs;
};

struct GPUNodeLink {
	GPUNodeStack *socket;

	int attribtype;
	char *attribname;

	int image;

	int texture;
	int texturesize;

	void *ptr1, *ptr2;

	int dynamic;

	int type;
	int users;

	GPUTexture *dynamictex;

	GPUBuiltin builtin;

	struct GPUOutput *output;
};

typedef struct GPUOutput {
	struct GPUOutput *next, *prev;

	GPUNode *node;
	int type;				/* data type = length of vector/matrix */
	GPUNodeLink *link;		/* output link */
	int id;					/* unique id as created by code generator */
} GPUOutput;

typedef struct GPUInput {
	struct GPUInput *next, *prev;

	GPUNode *node;

	int type;				/* datatype */
	int source;				/* data source */

	int id;					/* unique id as created by code generator */
	int texid;				/* number for multitexture */
	int attribid;			/* id for vertex attributes */
	int bindtex;			/* input is responsible for binding the texture? */
	int definetex;			/* input is responsible for defining the pixel? */
	int textarget;			/* GL_TEXTURE_* */
	int textype;			/* datatype */

	struct Image *ima;		/* image */
	struct ImageUser *iuser;/* image user */
	float *dynamicvec;		/* vector data in case it is dynamic */
	GPUTexture *tex;		/* input texture, only set at runtime */
	int shaderloc;			/* id from opengl */
	char shadername[32];	/* name in shader */

	float vec[16];			/* vector data */
	GPUNodeLink *link;
	int dynamictex;			/* dynamic? */
	int attribtype;			/* attribute type */
	char attribname[32];	/* attribute name */
	int attribfirst;		/* this is the first one that is bound */
	GPUBuiltin builtin;		/* builtin uniform */
} GPUInput;

struct GPUPass {
	struct GPUPass *next, *prev;

	ListBase inputs;
	struct GPUOutput *output;
	struct GPUShader *shader;
};

/* Strings utility */

static void BLI_dynstr_printf(DynStr *dynstr, const char *format, ...)
{
	va_list args;
	int retval;
	char str[2048];

	va_start(args, format);
	retval = vsnprintf(str, sizeof(str), format, args);
	va_end(args);

	if (retval >= sizeof(str))
		fprintf(stderr, "BLI_dynstr_printf: limit exceeded\n");
	else
		BLI_dynstr_append(dynstr, str);
}

/* GLSL code parsing for finding function definitions.
 * These are stored in a hash for lookup when creating a material. */

static GHash *FUNCTION_HASH= NULL;
/*static char *FUNCTION_PROTOTYPES= NULL;
static GPUShader *FUNCTION_LIB= NULL;*/

static int gpu_str_prefix(char *str, char *prefix)
{
	while(*str && *prefix) {
		if(*str != *prefix)
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
	while(*str) {
		if(ELEM6(*str, ' ', '(', ')', ',', '\t', '\n'))
			break;
		else {
			if(token && len < max-1) {
				*token= *str;
				token++;
				len++;
			}
			str++;
		}
	}

	if(token)
		*token= '\0';

	/* skip the next special characters:
	 * note the missing ')' */
	while(*str) {
		if(ELEM5(*str, ' ', '(', ',', '\t', '\n'))
			str++;
		else
			break;
	}

	return str;
}

static void gpu_parse_functions_string(GHash *hash, char *code)
{
	GPUFunction *function;
	int i, type, qual;

	while((code = strstr(code, "void "))) {
		function = MEM_callocN(sizeof(GPUFunction), "GPUFunction");

		code = gpu_str_skip_token(code, NULL, 0);
		code = gpu_str_skip_token(code, function->name, MAX_FUNCTION_NAME);

		/* get parameters */
		while(*code && *code != ')') {
			/* test if it's an input or output */
			qual = FUNCTION_QUAL_IN;
			if(gpu_str_prefix(code, "out "))
				qual = FUNCTION_QUAL_OUT;
			if(gpu_str_prefix(code, "inout "))
				qual = FUNCTION_QUAL_INOUT;
			if((qual != FUNCTION_QUAL_IN) || gpu_str_prefix(code, "in "))
				code = gpu_str_skip_token(code, NULL, 0);

			/* test for type */
			type= 0;
			for(i=1; i<=16; i++) {
				if(GPU_DATATYPE_STR[i] && gpu_str_prefix(code, GPU_DATATYPE_STR[i])) {
					type= i;
					break;
				}
			}

			if(!type && gpu_str_prefix(code, "sampler2DShadow"))
				type= GPU_SHADOW2D;
			if(!type && gpu_str_prefix(code, "sampler1D"))
				type= GPU_TEX1D;
			if(!type && gpu_str_prefix(code, "sampler2D"))
				type= GPU_TEX2D;

			if(type) {
				/* add paramater */
				code = gpu_str_skip_token(code, NULL, 0);
				code = gpu_str_skip_token(code, NULL, 0);
				function->paramqual[function->totparam]= qual;
				function->paramtype[function->totparam]= type;
				function->totparam++;
			}
			else {
				fprintf(stderr, "GPU invalid function parameter in %s.\n", function->name);
				break;
			}
		}

		if(strlen(function->name) == 0 || function->totparam == 0) {
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

	for(; !BLI_ghashIterator_isDone(ghi); BLI_ghashIterator_step(ghi)) {
		name = BLI_ghashIterator_getValue(ghi);
		function = BLI_ghashIterator_getValue(ghi);

		BLI_dynstr_printf(ds, "void %s(", name);
		for(a=0; a<function->totparam; a++) {
			if(function->paramqual[a] == FUNCTION_QUAL_OUT)
				BLI_dynstr_append(ds, "out ");
			else if(function->paramqual[a] == FUNCTION_QUAL_INOUT)
				BLI_dynstr_append(ds, "inout ");

			if(function->paramtype[a] == GPU_TEX1D)
				BLI_dynstr_append(ds, "sampler1D");
			else if(function->paramtype[a] == GPU_TEX2D)
				BLI_dynstr_append(ds, "sampler2D");
			else if(function->paramtype[a] == GPU_SHADOW2D)
				BLI_dynstr_append(ds, "sampler2DShadow");
			else
				BLI_dynstr_append(ds, GPU_DATATYPE_STR[function->paramtype[a]]);
				
			//BLI_dynstr_printf(ds, " param%d", a);
			
			if(a != function->totparam-1)
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

GPUFunction *GPU_lookup_function(char *name)
{
	if(!FUNCTION_HASH) {
		FUNCTION_HASH = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp, "GPU_lookup_function gh");
		gpu_parse_functions_string(FUNCTION_HASH, datatoc_gpu_shader_material_glsl);
		/*FUNCTION_PROTOTYPES = gpu_generate_function_prototyps(FUNCTION_HASH);
		FUNCTION_LIB = GPU_shader_create_lib(datatoc_gpu_shader_material_glsl);*/
	}

	return (GPUFunction*)BLI_ghash_lookup(FUNCTION_HASH, name);
}

void GPU_extensions_exit(void)
{
	extern Material defmaterial;    // render module abuse...

	if(defmaterial.gpumaterial.first)
		GPU_material_free(&defmaterial);

	if(FUNCTION_HASH) {
		BLI_ghash_free(FUNCTION_HASH, NULL, (GHashValFreeFP)MEM_freeN);
		FUNCTION_HASH = NULL;
	}
	/*if(FUNCTION_PROTOTYPES) {
		MEM_freeN(FUNCTION_PROTOTYPES);
		FUNCTION_PROTOTYPES = NULL;
	}*/
	/*if(FUNCTION_LIB) {
		GPU_shader_free(FUNCTION_LIB);
		FUNCTION_LIB = NULL;
	}*/
}

/* GLSL code generation */

static void codegen_convert_datatype(DynStr *ds, int from, int to, char *tmp, int id)
{
	char name[1024];

	snprintf(name, sizeof(name), "%s%d", tmp, id);

	if (from == to) {
		BLI_dynstr_append(ds, name);
	}
	else if (to == GPU_FLOAT) {
		if (from == GPU_VEC4)
			BLI_dynstr_printf(ds, "dot(%s.rgb, vec3(0.35, 0.45, 0.2))", name);
		else if (from == GPU_VEC3)
			BLI_dynstr_printf(ds, "dot(%s, vec3(0.33))", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_printf(ds, "%s.r", name);
	}
	else if (to == GPU_VEC2) {
		if (from == GPU_VEC4)
			BLI_dynstr_printf(ds, "vec2(dot(%s.rgb, vec3(0.35, 0.45, 0.2)), %s.a)", name, name);
		else if (from == GPU_VEC3)
			BLI_dynstr_printf(ds, "vec2(dot(%s.rgb, vec3(0.33)), 1.0)", name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_printf(ds, "vec2(%s, 1.0)", name);
	}
	else if (to == GPU_VEC3) {
		if (from == GPU_VEC4)
			BLI_dynstr_printf(ds, "%s.rgb", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_printf(ds, "vec3(%s.r, %s.r, %s.r)", name, name, name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_printf(ds, "vec3(%s, %s, %s)", name, name, name);
	}
	else {
		if (from == GPU_VEC3)
			BLI_dynstr_printf(ds, "vec4(%s, 1.0)", name);
		else if (from == GPU_VEC2)
			BLI_dynstr_printf(ds, "vec4(%s.r, %s.r, %s.r, %s.g)", name, name, name, name);
		else if (from == GPU_FLOAT)
			BLI_dynstr_printf(ds, "vec4(%s, %s, %s, 1.0)", name, name, name);
	}
}

static void codegen_print_datatype(DynStr *ds, int type, float *data)
{
	int i;

	BLI_dynstr_printf(ds, "%s(", GPU_DATATYPE_STR[type]);

	for(i=0; i<type; i++) {
		BLI_dynstr_printf(ds, "%f", data[i]);
		if(i == type-1)
			BLI_dynstr_append(ds, ")");
		else
			BLI_dynstr_append(ds, ", ");
	}
}

static int codegen_input_has_texture(GPUInput *input)
{
	if (input->link)
		return 0;
	else if(input->ima)
		return 1;
	else
		return input->tex != 0;
}

char *GPU_builtin_name(GPUBuiltin builtin)
{
	if(builtin == GPU_VIEW_MATRIX)
		return "unfviewmat";
	else if(builtin == GPU_OBJECT_MATRIX)
		return "unfobmat";
	else if(builtin == GPU_INVERSE_VIEW_MATRIX)
		return "unfinvviewmat";
	else if(builtin == GPU_INVERSE_OBJECT_MATRIX)
		return "unfinvobmat";
	else if(builtin == GPU_VIEW_POSITION)
		return "varposition";
	else if(builtin == GPU_VIEW_NORMAL)
		return "varnormal";
	else if(builtin == GPU_OBCOLOR)
		return "unfobcolor";
	else
		return "";
}

static void codegen_set_unique_ids(ListBase *nodes)
{
	GHash *bindhash, *definehash;
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;
	int id = 1, texid = 0;

	bindhash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "codegen_set_unique_ids1 gh");
	definehash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "codegen_set_unique_ids2 gh");

	for (node=nodes->first; node; node=node->next) {
		for (input=node->inputs.first; input; input=input->next) {
			/* set id for unique names of uniform variables */
			input->id = id++;
			input->bindtex = 0;
			input->definetex = 0;

			/* set texid used for settings texture slot with multitexture */
			if (codegen_input_has_texture(input) &&
				((input->source == GPU_SOURCE_TEX) || (input->source == GPU_SOURCE_TEX_PIXEL))) {
				if (input->link) {
					/* input is texture from buffer, assign only one texid per
					   buffer to avoid sampling the same texture twice */
					if (!BLI_ghash_haskey(bindhash, input->link)) {
						input->texid = texid++;
						input->bindtex = 1;
						BLI_ghash_insert(bindhash, input->link, SET_INT_IN_POINTER(input->texid));
					}
					else
						input->texid = GET_INT_FROM_POINTER(BLI_ghash_lookup(bindhash, input->link));
				}
				else if(input->ima) {
					/* input is texture from image, assign only one texid per
					   buffer to avoid sampling the same texture twice */
					if (!BLI_ghash_haskey(bindhash, input->ima)) {
						input->texid = texid++;
						input->bindtex = 1;
						BLI_ghash_insert(bindhash, input->ima, SET_INT_IN_POINTER(input->texid));
					}
					else
						input->texid = GET_INT_FROM_POINTER(BLI_ghash_lookup(bindhash, input->ima));
				}
				else {
					if (!BLI_ghash_haskey(bindhash, input->tex)) {
						/* input is user created texture, check tex pointer */
						input->texid = texid++;
						input->bindtex = 1;
						BLI_ghash_insert(bindhash, input->tex, SET_INT_IN_POINTER(input->texid));
					}
					else
						input->texid = GET_INT_FROM_POINTER(BLI_ghash_lookup(bindhash, input->tex));
				}

				/* make sure this pixel is defined exactly once */
				if (input->source == GPU_SOURCE_TEX_PIXEL) {
					if(input->ima) {
						if (!BLI_ghash_haskey(definehash, input->ima)) {
							input->definetex = 1;
							BLI_ghash_insert(definehash, input->ima, SET_INT_IN_POINTER(input->texid));
						}
					}
					else {
						if (!BLI_ghash_haskey(definehash, input->link)) {
							input->definetex = 1;
							BLI_ghash_insert(definehash, input->link, SET_INT_IN_POINTER(input->texid));
						}
					}
				}
			}
		}

		for (output=node->outputs.first; output; output=output->next)
			/* set id for unique names of tmp variables storing output */
			output->id = id++;
	}

	BLI_ghash_free(bindhash, NULL, NULL);
	BLI_ghash_free(definehash, NULL, NULL);
}

static void codegen_print_uniforms_functions(DynStr *ds, ListBase *nodes)
{
	GPUNode *node;
	GPUInput *input;
	char *name;
	int builtins = 0;

	/* print uniforms */
	for (node=nodes->first; node; node=node->next) {
		for (input=node->inputs.first; input; input=input->next) {
			if ((input->source == GPU_SOURCE_TEX) || (input->source == GPU_SOURCE_TEX_PIXEL)) {
				/* create exactly one sampler for each texture */
				if (codegen_input_has_texture(input) && input->bindtex)
					BLI_dynstr_printf(ds, "uniform %s samp%d;\n",
						(input->textype == GPU_TEX1D)? "sampler1D":
						(input->textype == GPU_TEX2D)? "sampler2D": "sampler2DShadow",
						input->texid);
			}
			else if(input->source == GPU_SOURCE_BUILTIN) {
				/* only define each builting uniform/varying once */
				if(!(builtins & input->builtin)) {
					builtins |= input->builtin;
					name = GPU_builtin_name(input->builtin);

					if(gpu_str_prefix(name, "unf")) {
						BLI_dynstr_printf(ds, "uniform %s %s;\n",
							GPU_DATATYPE_STR[input->type], name);
					}
					else {
						BLI_dynstr_printf(ds, "varying %s %s;\n",
							GPU_DATATYPE_STR[input->type], name);
					}
				}
			}
			else if (input->source == GPU_SOURCE_VEC_UNIFORM) {
				if(input->dynamicvec) {
					/* only create uniforms for dynamic vectors */
					BLI_dynstr_printf(ds, "uniform %s unf%d;\n",
						GPU_DATATYPE_STR[input->type], input->id);
				}
				else {
					/* for others use const so the compiler can do folding */
					BLI_dynstr_printf(ds, "const %s cons%d = ",
						GPU_DATATYPE_STR[input->type], input->id);
					codegen_print_datatype(ds, input->type, input->vec);
					BLI_dynstr_append(ds, ";\n");
				}
			}
			else if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
				BLI_dynstr_printf(ds, "varying %s var%d;\n",
					GPU_DATATYPE_STR[input->type], input->attribid);
			}
		}
	}

	BLI_dynstr_append(ds, "\n");
}

static void codegen_declare_tmps(DynStr *ds, ListBase *nodes)
{
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;

	for (node=nodes->first; node; node=node->next) {
		/* load pixels from textures */
		for (input=node->inputs.first; input; input=input->next) {
			if (input->source == GPU_SOURCE_TEX_PIXEL) {
				if (codegen_input_has_texture(input) && input->definetex) {
					BLI_dynstr_printf(ds, "\tvec4 tex%d = texture2D(", input->texid);
					BLI_dynstr_printf(ds, "samp%d, gl_TexCoord[%d].st);\n",
						input->texid, input->texid);
				}
			}
		}

		/* declare temporary variables for node output storage */
		for (output=node->outputs.first; output; output=output->next)
			BLI_dynstr_printf(ds, "\t%s tmp%d;\n",
				GPU_DATATYPE_STR[output->type], output->id);
	}

	BLI_dynstr_append(ds, "\n");
}

static void codegen_call_functions(DynStr *ds, ListBase *nodes, GPUOutput *finaloutput)
{
	GPUNode *node;
	GPUInput *input;
	GPUOutput *output;

	for (node=nodes->first; node; node=node->next) {
		BLI_dynstr_printf(ds, "\t%s(", node->name);
		
		for (input=node->inputs.first; input; input=input->next) {
			if (input->source == GPU_SOURCE_TEX) {
				BLI_dynstr_printf(ds, "samp%d", input->texid);
				if (input->link)
					BLI_dynstr_printf(ds, ", gl_TexCoord[%d].st", input->texid);
			}
			else if (input->source == GPU_SOURCE_TEX_PIXEL) {
				if (input->link && input->link->output)
					codegen_convert_datatype(ds, input->link->output->type, input->type,
						"tmp", input->link->output->id);
				else
					codegen_convert_datatype(ds, input->link->output->type, input->type,
						"tex", input->texid);
			}
			else if(input->source == GPU_SOURCE_BUILTIN)
				BLI_dynstr_printf(ds, "%s", GPU_builtin_name(input->builtin));
			else if(input->source == GPU_SOURCE_VEC_UNIFORM) {
				if(input->dynamicvec)
					BLI_dynstr_printf(ds, "unf%d", input->id);
				else
					BLI_dynstr_printf(ds, "cons%d", input->id);
			}
			else if (input->source == GPU_SOURCE_ATTRIB)
				BLI_dynstr_printf(ds, "var%d", input->attribid);

			BLI_dynstr_append(ds, ", ");
		}

		for (output=node->outputs.first; output; output=output->next) {
			BLI_dynstr_printf(ds, "tmp%d", output->id);
			if (output->next)
				BLI_dynstr_append(ds, ", ");
		}

		BLI_dynstr_append(ds, ");\n");
	}

	BLI_dynstr_append(ds, "\n\tgl_FragColor = ");
	codegen_convert_datatype(ds, finaloutput->type, GPU_VEC4, "tmp", finaloutput->id);
	BLI_dynstr_append(ds, ";\n");
}

static char *code_generate_fragment(ListBase *nodes, GPUOutput *output, const char *name)
{
	DynStr *ds = BLI_dynstr_new();
	char *code;

	/*BLI_dynstr_append(ds, FUNCTION_PROTOTYPES);*/

	codegen_set_unique_ids(nodes);
	codegen_print_uniforms_functions(ds, nodes);

	//if(G.f & G_DEBUG)
	//	BLI_dynstr_printf(ds, "/* %s */\n", name);

	BLI_dynstr_append(ds, "void main(void)\n");
	BLI_dynstr_append(ds, "{\n");

	codegen_declare_tmps(ds, nodes);
	codegen_call_functions(ds, nodes, output);

	BLI_dynstr_append(ds, "}\n");

	/* create shader */
	code = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	//if(G.f & G_DEBUG) printf("%s\n", code);

	return code;
}

static char *code_generate_vertex(ListBase *nodes)
{
	DynStr *ds = BLI_dynstr_new();
	GPUNode *node;
	GPUInput *input;
	char *code;
	
	for (node=nodes->first; node; node=node->next) {
		for (input=node->inputs.first; input; input=input->next) {
			if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
				BLI_dynstr_printf(ds, "attribute %s att%d;\n",
					GPU_DATATYPE_STR[input->type], input->attribid);
				BLI_dynstr_printf(ds, "varying %s var%d;\n",
					GPU_DATATYPE_STR[input->type], input->attribid);
			}
		}
	}

	BLI_dynstr_append(ds, "\n");
	BLI_dynstr_append(ds, datatoc_gpu_shader_vertex_glsl);

	for (node=nodes->first; node; node=node->next)
		for (input=node->inputs.first; input; input=input->next)
			if (input->source == GPU_SOURCE_ATTRIB && input->attribfirst) {
				if(input->attribtype == CD_TANGENT) /* silly exception */
					BLI_dynstr_printf(ds, "\tvar%d = gl_NormalMatrix * ", input->attribid);
				else
					BLI_dynstr_printf(ds, "\tvar%d = ", input->attribid);

				BLI_dynstr_printf(ds, "att%d;\n", input->attribid);
			}

	BLI_dynstr_append(ds, "}\n\n");

	code = BLI_dynstr_get_cstring(ds);

	BLI_dynstr_free(ds);

	//if(G.f & G_DEBUG) printf("%s\n", code);

	return code;
}

/* GPU pass binding/unbinding */

GPUShader *GPU_pass_shader(GPUPass *pass)
{
	return pass->shader;
}

void GPU_nodes_extract_dynamic_inputs(GPUPass *pass, ListBase *nodes)
{
	GPUShader *shader = pass->shader;
	GPUNode *node;
	GPUInput *next, *input;
	ListBase *inputs = &pass->inputs;
	int extract, z;

	memset(inputs, 0, sizeof(*inputs));

	if(!shader)
		return;

	GPU_shader_bind(shader);

	for (node=nodes->first; node; node=node->next) {
		z = 0;
		for (input=node->inputs.first; input; input=next, z++) {
			next = input->next;

			/* attributes don't need to be bound, they already have
			 * an id that the drawing functions will use */
			if(input->source == GPU_SOURCE_ATTRIB ||
			   input->source == GPU_SOURCE_BUILTIN)
				continue;

			if (input->ima || input->tex)
				snprintf(input->shadername, sizeof(input->shadername), "samp%d", input->texid);
			else
				snprintf(input->shadername, sizeof(input->shadername), "unf%d", input->id);

			/* pass non-dynamic uniforms to opengl */
			extract = 0;

			if(input->ima || input->tex) {
				if (input->bindtex)
					extract = 1;
			}
			else if(input->dynamicvec)
				extract = 1;

			if(extract)
				input->shaderloc = GPU_shader_get_uniform(shader, input->shadername);

			/* extract nodes */
			if(extract) {
				BLI_remlink(&node->inputs, input);
				BLI_addtail(inputs, input);
			}
		}
	}

	GPU_shader_unbind(shader);
}

void GPU_pass_bind(GPUPass *pass, double time, int mipmap)
{
	GPUInput *input;
	GPUShader *shader = pass->shader;
	ListBase *inputs = &pass->inputs;

	if (!shader)
		return;

	GPU_shader_bind(shader);

	/* now bind the textures */
	for (input=inputs->first; input; input=input->next) {
		if (input->ima)
			input->tex = GPU_texture_from_blender(input->ima, input->iuser, time, mipmap);

		if(input->tex && input->bindtex) {
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
	for (input=inputs->first; input; input=input->next)
		if(!(input->ima || input->tex))
			GPU_shader_uniform_vector(shader, input->shaderloc, input->type, 1,
				input->dynamicvec);
}

void GPU_pass_unbind(GPUPass *pass)
{
	GPUInput *input;
	GPUShader *shader = pass->shader;
	ListBase *inputs = &pass->inputs;

	if (!shader)
		return;

	for (input=inputs->first; input; input=input->next) {
		if(input->tex && input->bindtex)
			GPU_texture_unbind(input->tex);

		if (input->ima)
			input->tex = 0;
	}
	
	GPU_shader_unbind(shader);
}

/* Node Link Functions */

GPUNodeLink *GPU_node_link_create(int type)
{
	GPUNodeLink *link = MEM_callocN(sizeof(GPUNodeLink), "GPUNodeLink");
	link->type = type;
	link->users++;

	return link;
}

void GPU_node_link_free(GPUNodeLink *link)
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

GPUNode *GPU_node_begin(char *name)
{
	GPUNode *node = MEM_callocN(sizeof(GPUNode), "GPUNode");

	node->name = name;

	return node;
}

void GPU_node_end(GPUNode *node)
{
	/* empty */
}

static void gpu_node_input_link(GPUNode *node, GPUNodeLink *link, int type)
{
	GPUInput *input;
	GPUNode *outnode;
	char *name;

	if(link->output) {
		outnode = link->output->node;
		name = outnode->name;

		if(strcmp(name, "set_value")==0 || strcmp(name, "set_rgb")==0) {
			input = MEM_dupallocN(outnode->inputs.first);
			input->type = type;
			if(input->link)
				input->link->users++;
			BLI_addtail(&node->inputs, input);
			return;
		}
	}
	
	input = MEM_callocN(sizeof(GPUInput), "GPUInput");
	input->node = node;

	if(link->builtin) {
		/* builtin uniform */
		input->type = type;
		input->source = GPU_SOURCE_BUILTIN;
		input->builtin = link->builtin;

		MEM_freeN(link);
	}
	else if(link->output) {
		/* link to a node output */
		input->type = type;
		input->source = GPU_SOURCE_TEX_PIXEL;
		input->link = link;
		link->users++;
	}
	else if(link->dynamictex) {
		/* dynamic texture, GPUTexture is updated/deleted externally */
		input->type = type;
		input->source = GPU_SOURCE_TEX;

		input->tex = link->dynamictex;
		input->textarget = GL_TEXTURE_2D;
		input->textype = type;
		input->dynamictex = 1;
		MEM_freeN(link);
	}
	else if(link->texture) {
		/* small texture created on the fly, like for colorbands */
		input->type = GPU_VEC4;
		input->source = GPU_SOURCE_TEX;
		input->textype = type;

		if (type == GPU_TEX1D) {
			input->tex = GPU_texture_create_1D(link->texturesize, link->ptr1);
			input->textarget = GL_TEXTURE_1D;
		}
		else {
			input->tex = GPU_texture_create_2D(link->texturesize, link->texturesize, link->ptr2);
			input->textarget = GL_TEXTURE_2D;
		}

		MEM_freeN(link->ptr1);
		MEM_freeN(link);
	}
	else if(link->image) {
		/* blender image */
		input->type = GPU_VEC4;
		input->source = GPU_SOURCE_TEX;

		input->ima = link->ptr1;
		input->iuser = link->ptr2;
		input->textarget = GL_TEXTURE_2D;
		input->textype = GPU_TEX2D;
		MEM_freeN(link);
	}
	else if(link->attribtype) {
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

		memcpy(input->vec, link->ptr1, type*sizeof(float));
		if(link->dynamic)
			input->dynamicvec= link->ptr1;
		MEM_freeN(link);
	}

	BLI_addtail(&node->inputs, input);
}

static void gpu_node_input_socket(GPUNode *node, GPUNodeStack *sock)
{
	GPUNodeLink *link;

	if(sock->link) {
		gpu_node_input_link(node, sock->link, sock->type);
	}
	else {
		 link = GPU_node_link_create(0);
		link->ptr1 = sock->vec;
		gpu_node_input_link(node, link, sock->type);
	}
}

void GPU_node_output(GPUNode *node, int type, char *name, GPUNodeLink **link)
{
	GPUOutput *output = MEM_callocN(sizeof(GPUOutput), "GPUOutput");

	output->type = type;
	output->node = node;

	if (link) {
		*link = output->link = GPU_node_link_create(type);
		output->link->output = output;

		/* note: the caller owns the reference to the linkfer, GPUOutput
		   merely points to it, and if the node is destroyed it will
		   set that pointer to NULL */
	}

	BLI_addtail(&node->outputs, output);
}

void GPU_inputs_free(ListBase *inputs)
{
	GPUInput *input;

	for(input=inputs->first; input; input=input->next) {
		if(input->link)
			GPU_node_link_free(input->link);
		else if(input->tex && !input->dynamictex)
			GPU_texture_free(input->tex);
	}

	BLI_freelistN(inputs);
}

void GPU_node_free(GPUNode *node)
{
	GPUOutput *output;

	GPU_inputs_free(&node->inputs);

	for (output=node->outputs.first; output; output=output->next)
		if (output->link) {
			output->link->output = NULL;
			GPU_node_link_free(output->link);
		}

	BLI_freelistN(&node->outputs);
	MEM_freeN(node);
}

void GPU_nodes_free(ListBase *nodes)
{
	GPUNode *node;

	while (nodes->first) {
		node = nodes->first;
		BLI_remlink(nodes, node);
		GPU_node_free(node);
	}
}

/* vertex attributes */

void gpu_nodes_get_vertex_attributes(ListBase *nodes, GPUVertexAttribs *attribs)
{
	GPUNode *node;
	GPUInput *input;
	int a;

	/* convert attributes requested by node inputs to an array of layers,
	 * checking for duplicates and assigning id's starting from zero. */

	memset(attribs, 0, sizeof(*attribs));

	for(node=nodes->first; node; node=node->next) {
		for(input=node->inputs.first; input; input=input->next) {
			if(input->source == GPU_SOURCE_ATTRIB) {
				for(a=0; a<attribs->totlayer; a++) {
					if(attribs->layer[a].type == input->attribtype &&
						strcmp(attribs->layer[a].name, input->attribname) == 0)
						break;
				}

				if(a == attribs->totlayer && a < GPU_MAX_ATTRIB) {
					input->attribid = attribs->totlayer++;
					input->attribfirst = 1;

					attribs->layer[a].type = input->attribtype;
					attribs->layer[a].glindex = input->attribid;
					BLI_strncpy(attribs->layer[a].name, input->attribname,
						sizeof(attribs->layer[a].name));
				}
				else
					input->attribid = attribs->layer[a].glindex;
			}
		}
	}
}

void gpu_nodes_get_builtin_flag(ListBase *nodes, int *builtin)
{
	GPUNode *node;
	GPUInput *input;
	
	*builtin= 0;

	for(node=nodes->first; node; node=node->next)
		for(input=node->inputs.first; input; input=input->next)
			if(input->source == GPU_SOURCE_BUILTIN)
				*builtin |= input->builtin;
}

/* varargs linking  */

GPUNodeLink *GPU_attribute(int type, char *name)
{
	GPUNodeLink *link = GPU_node_link_create(0);

	link->attribtype= type;
	link->attribname= name;

	return link;
}

GPUNodeLink *GPU_uniform(float *num)
{
	GPUNodeLink *link = GPU_node_link_create(0);

	link->ptr1= num;
	link->ptr2= NULL;

	return link;
}

GPUNodeLink *GPU_dynamic_uniform(float *num)
{
	GPUNodeLink *link = GPU_node_link_create(0);

	link->ptr1= num;
	link->ptr2= NULL;
	link->dynamic= 1;

	return link;
}

GPUNodeLink *GPU_image(Image *ima, ImageUser *iuser)
{
	GPUNodeLink *link = GPU_node_link_create(0);

	link->image= 1;
	link->ptr1= ima;
	link->ptr2= iuser;

	return link;
}

GPUNodeLink *GPU_texture(int size, float *pixels)
{
	GPUNodeLink *link = GPU_node_link_create(0);

	link->texture = 1;
	link->texturesize = size;
	link->ptr1= pixels;

	return link;
}

GPUNodeLink *GPU_dynamic_texture(GPUTexture *tex)
{
	GPUNodeLink *link = GPU_node_link_create(0);

	link->dynamic = 1;
	link->dynamictex = tex;

	return link;
}

GPUNodeLink *GPU_socket(GPUNodeStack *sock)
{
	GPUNodeLink *link = GPU_node_link_create(0);

	link->socket= sock;

	return link;
}

GPUNodeLink *GPU_builtin(GPUBuiltin builtin)
{
	GPUNodeLink *link = GPU_node_link_create(0);

	link->builtin= builtin;

	return link;
}

int GPU_link(GPUMaterial *mat, char *name, ...)
{
	GPUNode *node;
	GPUFunction *function;
	GPUNodeLink *link, **linkptr;
	va_list params;
	int i;

	function = GPU_lookup_function(name);
	if(!function) {
		fprintf(stderr, "GPU failed to find function %s\n", name);
		return 0;
	}

	node = GPU_node_begin(name);

	va_start(params, name);
	for(i=0; i<function->totparam; i++) {
		if(function->paramqual[i] != FUNCTION_QUAL_IN) {
			linkptr= va_arg(params, GPUNodeLink**);
			GPU_node_output(node, function->paramtype[i], "", linkptr);
		}
		else {
			link= va_arg(params, GPUNodeLink*);
			gpu_node_input_link(node, link, function->paramtype[i]);
		}
	}
	va_end(params);

	GPU_node_end(node);

	gpu_material_add_node(mat, node);

	return 1;
}

int GPU_stack_link(GPUMaterial *mat, char *name, GPUNodeStack *in, GPUNodeStack *out, ...)
{
	GPUNode *node;
	GPUFunction *function;
	GPUNodeLink *link, **linkptr;
	va_list params;
	int i, totin, totout;

	function = GPU_lookup_function(name);
	if(!function) {
		fprintf(stderr, "GPU failed to find function %s\n", name);
		return 0;
	}

	node = GPU_node_begin(name);
	totin = 0;
	totout = 0;

	if(in) {
		for(i = 0; in[i].type != GPU_NONE; i++) {
			gpu_node_input_socket(node, &in[i]);
			totin++;
		}
	}
	
	if(out) {
		for(i = 0; out[i].type != GPU_NONE; i++) {
			GPU_node_output(node, out[i].type, out[i].name, &out[i].link);
			totout++;
		}
	}

	va_start(params, out);
	for(i=0; i<function->totparam; i++) {
		if(function->paramqual[i] != FUNCTION_QUAL_IN) {
			if(totout == 0) {
				linkptr= va_arg(params, GPUNodeLink**);
				GPU_node_output(node, function->paramtype[i], "", linkptr);
			}
			else
				totout--;
		}
		else {
			if(totin == 0) {
				link= va_arg(params, GPUNodeLink*);
				if(link->socket)
					gpu_node_input_socket(node, link->socket);
				else
					gpu_node_input_link(node, link, function->paramtype[i]);
			}
			else
				totin--;
		}
	}
	va_end(params);

	GPU_node_end(node);

	gpu_material_add_node(mat, node);
	
	return 1;
}

int GPU_link_changed(GPUNodeLink *link)
{
	GPUNode *node;
	GPUInput *input;
	char *name;

	if(link->output) {
		node = link->output->node;
		name = node->name;

		if(strcmp(name, "set_value")==0 || strcmp(name, "set_rgb")==0) {
			input = node->inputs.first;
			return (input->link != NULL);
		}

		return 1;
	}
	else
		return 0;
}

/* Pass create/free */

void gpu_nodes_tag(GPUNodeLink *link)
{
	GPUNode *node;
	GPUInput *input;

	if(!link->output)
		return;

	node = link->output->node;
	if(node->tag)
		return;
	
	node->tag= 1;
	for(input=node->inputs.first; input; input=input->next)
		if(input->link)
			gpu_nodes_tag(input->link);
}

void gpu_nodes_prune(ListBase *nodes, GPUNodeLink *outlink)
{
	GPUNode *node, *next;

	for(node=nodes->first; node; node=node->next)
		node->tag= 0;

	gpu_nodes_tag(outlink);

	for(node=nodes->first; node; node=next) {
		next = node->next;

		if(!node->tag) {
			BLI_remlink(nodes, node);
			GPU_node_free(node);
		}
	}
}

GPUPass *GPU_generate_pass(ListBase *nodes, GPUNodeLink *outlink, GPUVertexAttribs *attribs, int *builtins, const char *name)
{
	GPUShader *shader;
	GPUPass *pass;
	char *vertexcode, *fragmentcode;

	/*if(!FUNCTION_LIB) {
		GPU_nodes_free(nodes);
		return NULL;
	}*/

	/* prune unused nodes */
	gpu_nodes_prune(nodes, outlink);

	gpu_nodes_get_vertex_attributes(nodes, attribs);
	gpu_nodes_get_builtin_flag(nodes, builtins);

	/* generate code and compile with opengl */
	fragmentcode = code_generate_fragment(nodes, outlink->output, name);
	vertexcode = code_generate_vertex(nodes);
	shader = GPU_shader_create(vertexcode, fragmentcode, datatoc_gpu_shader_material_glsl); /*FUNCTION_LIB);*/
	MEM_freeN(fragmentcode);
	MEM_freeN(vertexcode);

	/* failed? */
	if (!shader) {
		memset(attribs, 0, sizeof(*attribs));
		memset(builtins, 0, sizeof(*builtins));
		GPU_nodes_free(nodes);
		return NULL;
	}
	
	/* create pass */
	pass = MEM_callocN(sizeof(GPUPass), "GPUPass");

	pass->output = outlink->output;
	pass->shader = shader;

	/* extract dynamic inputs and throw away nodes */
	GPU_nodes_extract_dynamic_inputs(pass, nodes);
	GPU_nodes_free(nodes);

	return pass;
}

void GPU_pass_free(GPUPass *pass)
{
	GPU_shader_free(pass->shader);
	GPU_inputs_free(&pass->inputs);
	MEM_freeN(pass);
}

