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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/view_map/ViewMapIO.cpp
 *  \ingroup freestyle
 *  \brief Functions to manage I/O for the view map
 *  \author Emmanuel Turquin
 *  \date 09/01/2003
 */

#include <limits.h>

#include "ViewMapIO.h"

#ifdef IRIX
#  define WRITE(n) Internal::write<sizeof((n))>(out, (const char *)(&(n)))
#  define READ(n) Internal::read<sizeof((n))>(in, (char *)(&(n)))
#else
#  define WRITE(n) out.write((const char *)(&(n)), sizeof((n)))
#  define READ(n) in.read((char *)(&(n)), sizeof((n)))
#endif

#define WRITE_IF_NON_NULL(ptr)  \
	if (ptr) {                  \
		WRITE((ptr)->userdata); \
	}                           \
	else {                      \
		WRITE(ZERO);            \
	} (void)0

#define READ_IF_NON_NULL(ptr, array) \
	READ(tmp);                       \
	if (tmp) {                       \
		(ptr) = (array)[tmp];        \
	}                                \
	else {                           \
		(ptr) = NULL;                \
	} (void)0

namespace Freestyle {

namespace ViewMapIO {

namespace Internal {

static ViewMap *g_vm;

//////////////////// 'load' Functions ////////////////////

inline int load(istream& in, Vec3r& v)
{
	if (Options::getFlags() & Options::FLOAT_VECTORS) {
		float tmp;
		READ(tmp);
		v[0] = tmp;
		READ(tmp);
		v[1] = tmp;
		READ(tmp);
		v[2] = tmp;
	}
	else {
		Vec3r::value_type tmp;
		READ(tmp);
		v[0] = tmp;
		READ(tmp);
		v[1] = tmp;
		READ(tmp);
		v[2] = tmp;
	}
	return 0;
}

inline int load(istream& in, Polygon3r& p)
{
	unsigned tmp;

	// Id
	READ(tmp);
	p.setId(tmp);

	// vertices (List)
	vector<Vec3r> tmp_vec;
	Vec3r v;
	READ(tmp);
	for (unsigned int i = 0; i < tmp; i++) {
		load(in, v);
		tmp_vec.push_back(v);
	}
	p.setVertices(tmp_vec);

	// min & max
	// Already computed (in the SetVertices() method)

	return 0;
}

inline int load(istream& in, FrsMaterial& m)
{
	float tmp_array[4];
	int i;

	// Diffuse
	for (i = 0; i < 4; i++)
		READ(tmp_array[i]);
	m.setDiffuse(tmp_array[0], tmp_array[1], tmp_array[2], tmp_array[3]);

	// Specular
	for (i = 0; i < 4; i++)
		READ(tmp_array[i]);
	m.setSpecular(tmp_array[0], tmp_array[1], tmp_array[2], tmp_array[3]);

	// Ambient
	for (i = 0; i < 4; i++)
		READ(tmp_array[i]);
	m.setAmbient(tmp_array[0], tmp_array[1], tmp_array[2], tmp_array[3]);

	// Emission
	for (i = 0; i < 4; i++)
		READ(tmp_array[i]);
	m.setEmission(tmp_array[0], tmp_array[1], tmp_array[2], tmp_array[3]);

	// Shininess
	READ(tmp_array[0]);
	m.setShininess(tmp_array[0]);

	return 0;
}

static int load(istream& in, ViewShape *vs)
{
	if (!vs || !vs->sshape())
		return 1;

	// SShape

	// -> Id
	Id::id_type id1, id2;
	READ(id1);
	READ(id2);
	vs->sshape()->setId(Id(id1, id2));

	// -> Importance
	float importance;
	READ(importance);
	vs->sshape()->setImportance(importance);

	// -> BBox
	//    Not necessary (only used during view map computatiom)

	unsigned i, size, tmp;

	// -> Material
	READ(size);
	vector<FrsMaterial> frs_materials;
	FrsMaterial m;
	for (i = 0; i < size; ++i) {
		load(in, m);
		frs_materials.push_back(m);
	}
	vs->sshape()->setFrsMaterials(frs_materials);

	// -> VerticesList (List)
	READ(size);
	for (i = 0; i < size; i++) {
		SVertex *sv;
		READ_IF_NON_NULL(sv, g_vm->SVertices());
		vs->sshape()->AddNewVertex(sv);
	}

	// -> Chains (List)
	READ(size);
	for (i = 0; i < size; i++) {
		FEdge *fe;
		READ_IF_NON_NULL(fe, g_vm->FEdges());
		vs->sshape()->AddChain(fe);
	}

	// -> EdgesList (List)
	READ(size);
	for (i = 0; i < size; i++) {
		FEdge *fe;
		READ_IF_NON_NULL(fe, g_vm->FEdges());
		vs->sshape()->AddEdge(fe);
	}

	// ViewEdges (List)
	READ(size);
	for (i = 0; i < size; i++) {
		ViewEdge *ve;
		READ_IF_NON_NULL(ve, g_vm->ViewEdges());
		vs->AddEdge(ve);
	}

	// ViewVertices (List)
	READ(size);
	for (i = 0; i < size; i++) {
		ViewVertex *vv;
		READ_IF_NON_NULL(vv, g_vm->ViewVertices());
		vs->AddVertex(vv);
	}

	return 0;
}


static int load(istream& in, FEdge *fe)
{
	if (!fe)
		return 1;

	bool b;

	FEdgeSmooth *fesmooth = NULL;
	FEdgeSharp *fesharp = NULL;
	if (fe->isSmooth()) {
		fesmooth = dynamic_cast<FEdgeSmooth*>(fe);
	}
	else {
		fesharp = dynamic_cast<FEdgeSharp*>(fe);
	}

	// Id
	Id::id_type id1, id2;
	READ(id1);
	READ(id2);
	fe->setId(Id(id1, id2));

	// Nature
	Nature::EdgeNature nature;
	READ(nature);
	fe->setNature(nature);

#if 0 // hasVisibilityPoint
	bool b;
	READ(b);
	fe->setHasVisibilityPoint(b);
#endif

	Vec3r v;
	unsigned int matindex;

#if 0
	// VisibilityPointA
	load(in, v);
	fe->setVisibilityPointA(v);

	// VisibilityPointB
	load(in, v);
	fe->setVisibilityPointB(v);
#endif

	if (fe->isSmooth()) {
		// Normal
		load(in, v);
		fesmooth->setNormal(v);

		// Material
		READ(matindex);
		fesmooth->setFrsMaterialIndex(matindex);
	}
	else {
		// aNormal
		load(in, v);
		fesharp->setNormalA(v);

		// bNormal
		load(in, v);
		fesharp->setNormalB(v);

		// Materials
		READ(matindex);
		fesharp->setaFrsMaterialIndex(matindex);
		READ(matindex);
		fesharp->setbFrsMaterialIndex(matindex);
	}

	unsigned tmp;

	// VertexA
	SVertex *sva;
	READ_IF_NON_NULL(sva, g_vm->SVertices());
	fe->setVertexA(sva);

	// VertexB
	SVertex *svb;
	READ_IF_NON_NULL(svb, g_vm->SVertices());
	fe->setVertexB(svb);

	// NextEdge
	FEdge *nfe;
	READ_IF_NON_NULL(nfe, g_vm->FEdges());
	fe->setNextEdge(nfe);

	// PreviousEdge
	FEdge *pfe;
	READ_IF_NON_NULL(pfe, g_vm->FEdges());
	fe->setPreviousEdge(pfe);

	// ViewEdge
	ViewEdge *ve;
	READ_IF_NON_NULL(ve, g_vm->ViewEdges());
	fe->setViewEdge(ve);

	// Face
	// Not necessary (only used during view map computatiom)

	Polygon3r p;

	// aFace
	load(in, p);
	fe->setaFace(p);

	// occludeeEmpty
	READ(b);
	fe->setOccludeeEmpty(b);

	// occludeeIntersection
	load(in, v);
	fe->setOccludeeIntersection(v);

	return 0;
}

static int load(istream& in, SVertex *sv)
{
	if (!sv)
		return 1;

	// Id
	Id::id_type id1, id2;
	READ(id1);
	READ(id2);
	sv->setId(Id(id1, id2));

	Vec3r v;

	// Point3D
	load(in, v);
	sv->setPoint3D(v);

	// Point2D
	load(in, v);
	sv->setPoint2D(v);

	unsigned tmp;

	// Shape
	ViewShape *vs;
	READ_IF_NON_NULL(vs, g_vm->ViewShapes());
	sv->setShape(vs->sshape());

	// pViewVertex
	ViewVertex *vv;
	READ_IF_NON_NULL(vv, g_vm->ViewVertices());
	sv->setViewVertex(vv);

	unsigned i, size;

	// Normals (List)
	READ(size);
	for (i = 0; i < size; i++) {
		load(in, v);
		sv->AddNormal(v);
	}

	// FEdges (List)
	READ(size);
	FEdge *fe;
	for (i = 0; i < size; i++) {
		READ_IF_NON_NULL(fe, g_vm->FEdges());
		sv->AddFEdge(fe);
	}

	return 0;
}


static int load(istream& in, ViewEdge *ve)
{
	if (!ve)
		return 1;

	unsigned tmp;

	// Id
	Id::id_type id1, id2;
	READ(id1);
	READ(id2);
	ve->setId(Id(id1, id2));

	// Nature
	Nature::EdgeNature nature;
	READ(nature);
	ve->setNature(nature);

	// QI
	READ(tmp);
	ve->setQI(tmp);

	// Shape
	ViewShape *vs;
	READ_IF_NON_NULL(vs, g_vm->ViewShapes());
	ve->setShape(vs);

	// aShape
	ViewShape *avs;
	READ_IF_NON_NULL(avs, g_vm->ViewShapes());
	ve->setaShape(avs);

	// FEdgeA
	FEdge *fea;
	READ_IF_NON_NULL(fea, g_vm->FEdges());
	ve->setFEdgeA(fea);

	// FEdgeB
	FEdge *feb;
	READ_IF_NON_NULL(feb, g_vm->FEdges());
	ve->setFEdgeB(feb);

	// A
	ViewVertex *vva;
	READ_IF_NON_NULL(vva, g_vm->ViewVertices());
	ve->setA(vva);

	// B
	ViewVertex *vvb;
	READ_IF_NON_NULL(vvb, g_vm->ViewVertices());
	ve->setB(vvb);

	// Occluders (List)
	if (!(Options::getFlags() & Options::NO_OCCLUDERS)) {
		unsigned size;
		READ(size);
		ViewShape *vso;
		for (unsigned int i = 0; i < size; i++) {
			READ_IF_NON_NULL(vso, g_vm->ViewShapes());
			ve->AddOccluder(vso);
		}
	}

	return 0;
}


static int load(istream& in, ViewVertex *vv)
{
	if (!vv)
		return 1;

	unsigned tmp;
	bool b;

	// Nature
	Nature::VertexNature nature;
	READ(nature);
	vv->setNature(nature);

	if (vv->getNature() & Nature::T_VERTEX) {
		TVertex *tv = dynamic_cast<TVertex*>(vv);

		// Id
		Id::id_type id1, id2;
		READ(id1);
		READ(id2);
		tv->setId(Id(id1, id2));

		// FrontSVertex
		SVertex *fsv;
		READ_IF_NON_NULL(fsv, g_vm->SVertices());
		tv->setFrontSVertex(fsv);

		// BackSVertex
		SVertex *bsv;
		READ_IF_NON_NULL(bsv, g_vm->SVertices());
		tv->setBackSVertex(bsv);

		// FrontEdgeA
		ViewEdge *fea;
		READ_IF_NON_NULL(fea, g_vm->ViewEdges());
		READ(b);
		tv->setFrontEdgeA(fea, b);

		// FrontEdgeB
		ViewEdge *feb;
		READ_IF_NON_NULL(feb, g_vm->ViewEdges());
		READ(b);
		tv->setFrontEdgeB(feb, b);

		// BackEdgeA
		ViewEdge *bea;
		READ_IF_NON_NULL(bea, g_vm->ViewEdges());
		READ(b);
		tv->setBackEdgeA(bea, b);

		// BackEdgeB
		ViewEdge *beb;
		READ_IF_NON_NULL(beb, g_vm->ViewEdges());
		READ(b);
		tv->setBackEdgeB(beb, b);
	}
	else if (vv->getNature() & Nature::NON_T_VERTEX) {
		NonTVertex *ntv = dynamic_cast<NonTVertex*>(vv);

		// SVertex
		SVertex *sv;
		READ_IF_NON_NULL(sv, g_vm->SVertices());
		ntv->setSVertex(sv);

		// ViewEdges (List)
		unsigned size;
		READ(size);
		ViewEdge *ve;
		for (unsigned int i = 0; i < size; i++) {
			READ_IF_NON_NULL(ve, g_vm->ViewEdges());
			READ(b);
			ntv->AddViewEdge(ve, b);
		}
	}

	return 0;
}

//////////////////// 'save' Functions ////////////////////

inline int save(ostream& out, const Vec3r& v)
{
	if (Options::getFlags() & Options::FLOAT_VECTORS) {
		float tmp;

		tmp = v[0];
		WRITE(tmp);
		tmp = v[1];
		WRITE(tmp);
		tmp = v[2];
		WRITE(tmp);
	}
	else {
		Vec3r::value_type tmp;

		tmp = v[0];
		WRITE(tmp);
		tmp = v[1];
		WRITE(tmp);
		tmp = v[2];
		WRITE(tmp);
	}
	return 0;
}


inline int save(ostream& out, const Polygon3r& p)
{
	unsigned tmp;

	// Id
	tmp = p.getId();
	WRITE(tmp);

	// vertices (List)
	tmp = p.getVertices().size();
	WRITE(tmp);
	for (vector<Vec3r>::const_iterator i = p.getVertices().begin(); i != p.getVertices().end(); i++) {
		save(out, *i);
	}

	// min & max
	// Do not need to be saved

	return 0;
}

inline int save(ostream& out, const FrsMaterial& m)
{
	unsigned i;

	// Diffuse
	for (i = 0; i < 4; i++)
		WRITE(m.diffuse()[i]);

	// Specular
	for (i = 0; i < 4; i++)
		WRITE(m.specular()[i]);

	// Ambient
	for (i = 0; i < 4; i++)
		WRITE(m.ambient()[i]);

	// Emission
	for (i = 0; i < 4; i++)
		WRITE(m.emission()[i]);

	// Shininess
	float shininess = m.shininess();
	WRITE(shininess);

	return 0;
}

static int save(ostream& out, ViewShape *vs)
{
	if (!vs || !vs->sshape()) {
		cerr << "Warning: null ViewShape" << endl;
		return 1;
	}

	unsigned tmp;

	// SShape

	// -> Id
	Id::id_type id = vs->sshape()->getId().getFirst();
	WRITE(id);
	id = vs->sshape()->getId().getSecond();
	WRITE(id);

	// -> Importance
	float importance = vs->sshape()->importance();
	WRITE(importance);

	// -> BBox
	//    Not necessary (only used during view map computatiom)

	// -> Material
	unsigned int size = vs->sshape()->frs_materials().size();
	WRITE(size);
	for (unsigned int i = 0; i < size; ++i)
		save(out, vs->sshape()->frs_material(i));

	// -> VerticesList (List)
	tmp = vs->sshape()->getVertexList().size();
	WRITE(tmp);
	for (vector<SVertex*>::const_iterator i1 = vs->sshape()->getVertexList().begin();
	     i1 != vs->sshape()->getVertexList().end();
	     i1++)
	{
		WRITE_IF_NON_NULL(*i1);
	}

	// -> Chains (List)
	tmp = vs->sshape()->getChains().size();
	WRITE(tmp);
	for (vector<FEdge*>::const_iterator i2 = vs->sshape()->getChains().begin();
	     i2 != vs->sshape()->getChains().end();
	     i2++)
	{
		WRITE_IF_NON_NULL(*i2);
	}

	// -> EdgesList (List)
	tmp = vs->sshape()->getEdgeList().size();
	WRITE(tmp);
	for (vector<FEdge*>::const_iterator i3 = vs->sshape()->getEdgeList().begin();
	     i3 != vs->sshape()->getEdgeList().end();
	     i3++)
	{
		WRITE_IF_NON_NULL(*i3);
	}

	// ViewEdges (List)
	tmp = vs->edges().size();
	WRITE(tmp);
	for (vector<ViewEdge*>::const_iterator i4 = vs->edges().begin(); i4 != vs->edges().end(); i4++) {
		WRITE_IF_NON_NULL(*i4);
	}

	// ViewVertices (List)
	tmp = vs->vertices().size();
	WRITE(tmp);
	for (vector<ViewVertex*>::const_iterator i5 = vs->vertices().begin(); i5 != vs->vertices().end(); i5++) {
		WRITE_IF_NON_NULL(*i5);
	}

	return 0;
}


static int save(ostream& out, FEdge *fe)
{
	if (!fe) {
		cerr << "Warning: null FEdge" << endl;
		return 1;
	}

	FEdgeSmooth *fesmooth = dynamic_cast<FEdgeSmooth*>(fe);
	FEdgeSharp *fesharp =  dynamic_cast<FEdgeSharp*>(fe);

	// Id
	Id::id_type id = fe->getId().getFirst();
	WRITE(id);
	id = fe->getId().getSecond();
	WRITE(id);

	// Nature
	Nature::EdgeNature nature = fe->getNature();
	WRITE(nature);

	bool b;

#if 0
	// hasVisibilityPoint
	b = fe->hasVisibilityPoint();
	WRITE(b);

	// VisibilityPointA
	save(out, fe->visibilityPointA());

	// VisibilityPointB
	save(out, fe->visibilityPointB());
#endif

	unsigned index;
	if (fe->isSmooth()) {
		// normal
		save(out, fesmooth->normal());
		// material
		index = fesmooth->frs_materialIndex();
		WRITE(index);
	}
	else {
		// aNormal
		save(out, fesharp->normalA());
		// bNormal
		save(out, fesharp->normalB());
		// aMaterial
		index = fesharp->aFrsMaterialIndex();
		WRITE(index);
		// bMaterial
		index = fesharp->bFrsMaterialIndex();
		WRITE(index);
	}

	// VertexA
	WRITE_IF_NON_NULL(fe->vertexA());

	// VertexB
	WRITE_IF_NON_NULL(fe->vertexB());

	// NextEdge
	WRITE_IF_NON_NULL(fe->nextEdge());

	// PreviousEdge
	WRITE_IF_NON_NULL(fe->previousEdge());

	// ViewEdge
	WRITE_IF_NON_NULL(fe->viewedge());

	// Face
	// Not necessary (only used during view map computatiom)

	// aFace
	save(out, (Polygon3r&)fe->aFace());

	// occludeeEmpty
	b = fe->getOccludeeEmpty();
	WRITE(b);

	// occludeeIntersection
	save(out, fe->getOccludeeIntersection());

	return 0;
}

static int save(ostream& out, SVertex *sv)
{
	if (!sv) {
		cerr << "Warning: null SVertex" << endl;
		return 1;
	}

	unsigned tmp;

	// Id
	Id::id_type id = sv->getId().getFirst();
	WRITE(id);
	id = sv->getId().getSecond();
	WRITE(id);

	Vec3r v;

	// Point3D
	v = sv->point3D();
	save(out, sv->point3D());

	// Point2D
	v = sv->point2D();
	save(out, v);

	// Shape
	WRITE_IF_NON_NULL(sv->shape());

	// pViewVertex
	WRITE_IF_NON_NULL(sv->viewvertex());

	// Normals (List)
	// Note: the 'size()' method of a set doesn't seem to return the actual size of the given set, so we have to
	// hack it...
	set<Vec3r>::const_iterator i;
	for (i = sv->normals().begin(), tmp = 0; i != sv->normals().end(); i++, tmp++);
	WRITE(tmp);
	for (i = sv->normals().begin(); i != sv->normals().end(); i++)
		save(out, *i);

	// FEdges (List)
	tmp = sv->fedges().size();
	WRITE(tmp);
	for (vector<FEdge*>::const_iterator j = sv->fedges_begin(); j != sv->fedges_end(); j++) {
		WRITE_IF_NON_NULL(*j);
	}

	return 0;
}


static int save(ostream& out, ViewEdge *ve)
{
	if (!ve) {
		cerr << "Warning: null ViewEdge" << endl;
		return 1;
	}

	unsigned tmp;

	// Id
	Id::id_type id = ve->getId().getFirst();
	WRITE(id);
	id = ve->getId().getSecond();
	WRITE(id);

	// Nature
	Nature::EdgeNature nature = ve->getNature();
	WRITE(nature);

	// QI
	unsigned qi = ve->qi();
	WRITE(qi);

	// Shape
	WRITE_IF_NON_NULL(ve->shape());

	// aShape
	WRITE_IF_NON_NULL(ve->aShape());

	// FEdgeA
	WRITE_IF_NON_NULL(ve->fedgeA());

	// FEdgeB
	WRITE_IF_NON_NULL(ve->fedgeB());

	// A
	WRITE_IF_NON_NULL(ve->A());

	// B
	WRITE_IF_NON_NULL(ve->B());

	// Occluders (List)
	if (!(Options::getFlags() & Options::NO_OCCLUDERS)) {
		tmp = ve->occluders().size();
		WRITE(tmp);
		for (vector<ViewShape*>::const_iterator i = ve->occluders().begin(); i != ve->occluders().end(); i++) {
			WRITE_IF_NON_NULL((*i));
		}
	}

	return 0;
}


static int save(ostream& out, ViewVertex *vv)
{
	if (!vv) {
		cerr << "Warning: null ViewVertex" << endl;
		return 1;
	}

	// Nature
	Nature::VertexNature nature = vv->getNature();
	WRITE(nature);

	if (vv->getNature() & Nature::T_VERTEX) {
		TVertex *tv = dynamic_cast<TVertex*>(vv);

		// Id
		Id::id_type id = tv->getId().getFirst();
		WRITE(id);
		id = tv->getId().getSecond();
		WRITE(id);

		// FrontSVertex
		WRITE_IF_NON_NULL(tv->frontSVertex());

		// BackSVertex
		WRITE_IF_NON_NULL(tv->backSVertex());

		// FrontEdgeA
		WRITE_IF_NON_NULL(tv->frontEdgeA().first);
		WRITE(tv->frontEdgeA().second);

		// FrontEdgeB
		WRITE_IF_NON_NULL(tv->frontEdgeB().first);
		WRITE(tv->frontEdgeB().second);

		// BackEdgeA
		WRITE_IF_NON_NULL(tv->backEdgeA().first);
		WRITE(tv->backEdgeA().second);

		// BackEdgeB
		WRITE_IF_NON_NULL(tv->backEdgeB().first);
		WRITE(tv->backEdgeB().second);
	}
	else if (vv->getNature() & Nature::NON_T_VERTEX) {
		NonTVertex *ntv = dynamic_cast<NonTVertex*>(vv);

		// SVertex
		WRITE_IF_NON_NULL(ntv->svertex());

		// ViewEdges (List)
		unsigned size = ntv->viewedges().size();
		WRITE(size);
		vector<ViewVertex::directedViewEdge>::const_iterator i = ntv->viewedges().begin();
		for (; i != ntv->viewedges().end(); i++) {
			WRITE_IF_NON_NULL(i->first);
			WRITE(i->second);
		}
	}
	else {
		cerr << "Warning: unexpected ViewVertex nature" << endl;
		return 1;
	}

	return 0;
}

} // End of namespace Internal


//////////////////// "Public" 'load' and 'save' functions ////////////////////

#define SET_PROGRESS(n)       \
	if (pb) {                 \
		pb->setProgress((n)); \
	} (void)0

int load(istream& in, ViewMap *vm, ProgressBar *pb)
{
	if (!vm)
		return 1;

	//soc unused - unsigned tmp;
	int err = 0;
	Internal::g_vm = vm;

	// Management of the progress bar (if present)
	if (pb) {
		pb->reset();
		pb->setLabelText("Loading View Map...");
		pb->setTotalSteps(6);
		pb->setProgress(0);
	}

	// Read and set the options
	unsigned char flags;
	READ(flags);
	Options::setFlags(flags);

	// Read the size of the five ViewMap's lists (with some extra information for the ViewVertices)
	// and instantiate them (with default costructors)
	unsigned vs_s, fe_s, fe_rle1, fe_rle2, sv_s, ve_s, vv_s, vv_rle1, vv_rle2;
	READ(vs_s);
	READ(fe_s);

	if (fe_s) {
		bool b;
		READ(b);
		for (READ(fe_rle1), fe_rle2 = 0; fe_rle1 <= fe_s; fe_rle2 = fe_rle1, READ(fe_rle1)) {
			if (b) {
				for (unsigned int i = fe_rle2; i < fe_rle1; i++) {
					FEdgeSmooth *fes = new FEdgeSmooth;
					vm->AddFEdge(fes);
				}
				b = !b;
			}
			else if (!b) {
				for (unsigned int i = fe_rle2; i < fe_rle1; i++) {
					FEdgeSharp *fes = new FEdgeSharp;
					vm->AddFEdge(fes);
				}
				b = !b;
			}
		}
	}

	READ(sv_s);
	READ(ve_s);
	READ(vv_s);

	if (vv_s) {
		Nature::VertexNature nature;
		READ(nature);
		for (READ(vv_rle1), vv_rle2 = 0; vv_rle1 <= vv_s; vv_rle2 = vv_rle1, READ(vv_rle1)) {
			if (nature & Nature::T_VERTEX) {
				for (unsigned int i = vv_rle2; i < vv_rle1; i++) {
					TVertex *tv = new TVertex();
					vm->AddViewVertex(tv);
				}
				nature = Nature::NON_T_VERTEX;
			}
			else if (nature & Nature::NON_T_VERTEX) {
				for (unsigned int i = vv_rle2; i < vv_rle1; i++) {
					NonTVertex *ntv = new NonTVertex();
					vm->AddViewVertex(ntv);
				}
				nature = Nature::T_VERTEX;
			}
		}
	}

	for (unsigned int i0 = 0; i0 < vs_s; i0++) {
		SShape *ss = new SShape();
		ViewShape *vs = new ViewShape();
		vs->setSShape(ss);
		ss->setViewShape(vs);
		vm->AddViewShape(vs);
	}
#if 0
	for (unsigned int i1 = 0; i1 < fe_s; i1++) {
		FEdge *fe = new FEdge();
		vm->AddFEdge(fe);
	}
#endif
	for (unsigned int i2 = 0; i2 < sv_s; i2++) {
		SVertex *sv = new SVertex();
		vm->AddSVertex(sv);
	}
	for (unsigned int i3 = 0; i3 < ve_s; i3++) {
		ViewEdge *ve = new ViewEdge();
		vm->AddViewEdge(ve);
	}

	// Read the values for all the objects created above
	SET_PROGRESS(1);
	for (vector<ViewShape*>::const_iterator i4 = vm->ViewShapes().begin(); i4 != vm->ViewShapes().end(); i4++)
		err += Internal::load(in, *i4);
	SET_PROGRESS(2);
	for (vector<FEdge*>::const_iterator i5 = vm->FEdges().begin(); i5 != vm->FEdges().end(); i5++)
		err += Internal::load(in, *i5);
	SET_PROGRESS(3);
	for (vector<SVertex*>::const_iterator i6 = vm->SVertices().begin(); i6 != vm->SVertices().end(); i6++)
		err += Internal::load(in, *i6);
	SET_PROGRESS(4);
	for (vector<ViewEdge*>::const_iterator i7 = vm->ViewEdges().begin(); i7 != vm->ViewEdges().end(); i7++)
		err += Internal::load(in, *i7);
	SET_PROGRESS(5);
	for (vector<ViewVertex*>::const_iterator i8 = vm->ViewVertices().begin(); i8 != vm->ViewVertices().end(); i8++)
		err += Internal::load(in, *i8);
	SET_PROGRESS(6);

	// Read the shape id to index mapping
	unsigned map_s;
	READ(map_s);
	unsigned id, index;
	for (unsigned int i4 = 0; i4 < map_s; ++i4) {
		READ(id);
		READ(index);
		vm->shapeIdToIndexMap()[id] = index;
	}

	return err;
}


int save(ostream& out, ViewMap *vm, ProgressBar *pb)
{
	if (!vm)
		return 1;

	int err = 0;

	// Management of the progress bar (if present)
	if (pb) {
		pb->reset();
		pb->setLabelText("Saving View Map...");
		pb->setTotalSteps(6);
		pb->setProgress(0);
	}

	// For every object, initialize its userdata member to its index in the ViewMap list
	for (unsigned int i0 = 0; i0 < vm->ViewShapes().size(); i0++) {
		vm->ViewShapes()[i0]->userdata = POINTER_FROM_UINT(i0);
		vm->ViewShapes()[i0]->sshape()->userdata = POINTER_FROM_UINT(i0);
	}
	for (unsigned int i1 = 0; i1 < vm->FEdges().size(); i1++)
		vm->FEdges()[i1]->userdata = POINTER_FROM_UINT(i1);
	for (unsigned int i2 = 0; i2 < vm->SVertices().size(); i2++)
		vm->SVertices()[i2]->userdata = POINTER_FROM_UINT(i2);
	for (unsigned int i3 = 0; i3 < vm->ViewEdges().size(); i3++)
		vm->ViewEdges()[i3]->userdata = POINTER_FROM_UINT(i3);
	for (unsigned int i4 = 0; i4 < vm->ViewVertices().size(); i4++)
		vm->ViewVertices()[i4]->userdata = POINTER_FROM_UINT(i4);

	// Write the current options
	unsigned char flags = Options::getFlags();
	WRITE(flags);

	// Write the size of the five lists (with some extra information for the ViewVertices)
	unsigned size;
	size = vm->ViewShapes().size();
	WRITE(size);
	size = vm->FEdges().size();
	WRITE(size);
	if (size) {
		bool b = vm->FEdges()[0]->isSmooth();
		WRITE(b);
		for (unsigned int i = 0; i < size; i++) {
			while (i < size && (vm->FEdges()[i]->isSmooth() == b))
				i++;
			if (i < size) {
				WRITE(i);
				b = !b;
			}
		}
		WRITE(size);
		size++;
		WRITE(size);
	}
	size = vm->SVertices().size();
	WRITE(size);
	size = vm->ViewEdges().size();
	WRITE(size);
	size = vm->ViewVertices().size();
	WRITE(size);
	if (size) {
		Nature::VertexNature nature = vm->ViewVertices()[0]->getNature();
		WRITE(nature);
		nature &= ~Nature::VIEW_VERTEX;
		for (unsigned int i = 0; i < size; i++) {
			while (i < size && (vm->ViewVertices()[i]->getNature() & nature))
				i++;
			if (i < size) {
				WRITE(i);
				nature = vm->ViewVertices()[i]->getNature() & ~Nature::VIEW_VERTEX;
			}
		}
		WRITE(size);
		size++;
		WRITE(size);
	}

	// Write all the elts of the ViewShapes List
	SET_PROGRESS(1);
	for (vector<ViewShape*>::const_iterator i5 = vm->ViewShapes().begin(); i5 != vm->ViewShapes().end(); i5++)
		err += Internal::save(out, *i5);
	SET_PROGRESS(2);
	for (vector<FEdge*>::const_iterator i6 = vm->FEdges().begin(); i6 != vm->FEdges().end(); i6++)
		err += Internal::save(out, *i6);
	SET_PROGRESS(3);
	for (vector<SVertex*>::const_iterator i7 = vm->SVertices().begin(); i7 != vm->SVertices().end(); i7++)
		err += Internal::save(out, *i7);
	SET_PROGRESS(4);
	for (vector<ViewEdge*>::const_iterator i8 = vm->ViewEdges().begin(); i8 != vm->ViewEdges().end(); i8++)
		err += Internal::save(out, *i8);
	SET_PROGRESS(5);
	for (vector<ViewVertex*>::const_iterator i9 = vm->ViewVertices().begin(); i9 != vm->ViewVertices().end(); i9++)
		err += Internal::save(out, *i9);

	// Write the shape id to index mapping
	size = vm->shapeIdToIndexMap().size();
	WRITE(size);
	unsigned int id, index;
	for (ViewMap::id_to_index_map::iterator mit = vm->shapeIdToIndexMap().begin(),
	                                        mitend = vm->shapeIdToIndexMap().end();
	     mit != mitend;
	     ++mit)
	{
		id = mit->first;
		index = mit->second;
		WRITE(id);
		WRITE(index);
	}

	// Reset 'userdata' members
	for (vector<ViewShape*>::const_iterator j0 = vm->ViewShapes().begin(); j0 != vm->ViewShapes().end(); j0++) {
		(*j0)->userdata = NULL;
		(*j0)->sshape()->userdata = NULL;
	}
	for (vector<FEdge*>::const_iterator j1 = vm->FEdges().begin(); j1 != vm->FEdges().end(); j1++)
		(*j1)->userdata = NULL;
	for (vector<SVertex*>::const_iterator j2 = vm->SVertices().begin(); j2 != vm->SVertices().end(); j2++)
		(*j2)->userdata = NULL;
	for (vector<ViewEdge*>::const_iterator j3 = vm->ViewEdges().begin(); j3 != vm->ViewEdges().end(); j3++)
		(*j3)->userdata = NULL;
	for (vector<ViewVertex*>::const_iterator j4 = vm->ViewVertices().begin(); j4 != vm->ViewVertices().end(); j4++)
		(*j4)->userdata = NULL;
	SET_PROGRESS(6);

	return err;
}


//////////////////// Options ////////////////////

namespace Options {

namespace Internal {

static unsigned char g_flags = 0;
static string g_models_path;

} // End of namespace Internal

void setFlags(const unsigned char flags)
{
	Internal::g_flags = flags;
}

void addFlags(const unsigned char flags)
{
	Internal::g_flags |= flags;
}

void rmFlags(const unsigned char flags)
{
	Internal::g_flags &= ~flags;
}

unsigned char getFlags()
{
	return Internal::g_flags;
}

void setModelsPath(const string& path)
{
	Internal::g_models_path = path;
}

string getModelsPath()
{
	return Internal::g_models_path;
}

} // End of namepace Options

} // End of namespace ViewMapIO

} /* namespace Freestyle */
