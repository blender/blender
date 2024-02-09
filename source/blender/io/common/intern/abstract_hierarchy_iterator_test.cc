/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "IO_abstract_hierarchy_iterator.h"

#include "tests/blendfile_loading_base_test.h"

#include "BKE_scene.h"
#include "BLI_path_util.h"
#include "BLO_readfile.hh"
#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DNA_object_types.h"

#include <map>
#include <set>

namespace blender::io {

namespace {

/* Mapping from ID.name to set of export hierarchy path. Duplicated objects can be exported
 * multiple times with different export paths, hence the set. */
using used_writers = std::map<std::string, std::set<std::string>>;

class TestHierarchyWriter : public AbstractHierarchyWriter {
 public:
  std::string writer_type;
  used_writers &writers_map;

  TestHierarchyWriter(const std::string &writer_type, used_writers &writers_map)
      : writer_type(writer_type), writers_map(writers_map)
  {
  }

  void write(HierarchyContext &context) override
  {
    const char *id_name = context.object->id.name;
    used_writers::mapped_type &writers = writers_map[id_name];

    if (writers.find(context.export_path) != writers.end()) {
      ADD_FAILURE() << "Unexpectedly found another " << writer_type << " writer for " << id_name
                    << " to export to " << context.export_path;
    }
    writers.insert(context.export_path);
  }
};

}  // namespace

class TestingHierarchyIterator : public AbstractHierarchyIterator {
 public: /* Public so that the test cases can directly inspect the created writers. */
  used_writers transform_writers;
  used_writers data_writers;
  used_writers hair_writers;
  used_writers particle_writers;

  explicit TestingHierarchyIterator(Main *bmain, Depsgraph *depsgraph)
      : AbstractHierarchyIterator(bmain, depsgraph)
  {
  }
  ~TestingHierarchyIterator() override
  {
    release_writers();
  }

 protected:
  AbstractHierarchyWriter *create_transform_writer(const HierarchyContext * /*context*/) override
  {
    return new TestHierarchyWriter("transform", transform_writers);
  }
  AbstractHierarchyWriter *create_data_writer(const HierarchyContext * /*context*/) override
  {
    return new TestHierarchyWriter("data", data_writers);
  }
  AbstractHierarchyWriter *create_hair_writer(const HierarchyContext * /*context*/) override
  {
    return new TestHierarchyWriter("hair", hair_writers);
  }
  AbstractHierarchyWriter *create_particle_writer(const HierarchyContext * /*context*/) override
  {
    return new TestHierarchyWriter("particle", particle_writers);
  }

  void release_writer(AbstractHierarchyWriter *writer) override
  {
    delete writer;
  }
};

class AbstractHierarchyIteratorTest : public BlendfileLoadingBaseTest {
 protected:
  TestingHierarchyIterator *iterator;

  void SetUp() override
  {
    BlendfileLoadingBaseTest::SetUp();
    iterator = nullptr;
  }

  void TearDown() override
  {
    iterator_free();
    BlendfileLoadingBaseTest::TearDown();
  }

  /* Create a test iterator. */
  void iterator_create()
  {
    iterator = new TestingHierarchyIterator(bfile->main, depsgraph);
  }
  /* Free the test iterator if it is not nullptr. */
  void iterator_free()
  {
    if (iterator == nullptr) {
      return;
    }
    delete iterator;
    iterator = nullptr;
  }
};

TEST_F(AbstractHierarchyIteratorTest, ExportHierarchyTest)
{
  /* Load the test blend file. */
  if (!blendfile_load("usd" SEP_STR "usd_hierarchy_export_test.blend")) {
    return;
  }
  depsgraph_create(DAG_EVAL_RENDER);
  iterator_create();

  iterator->iterate_and_write();

  /* Mapping from object name to set of export paths. */
  used_writers expected_transforms = {
      {"OBCamera", {"/Camera"}},
      {"OBDupli1", {"/Dupli1"}},
      {"OBDupli2", {"/ParentOfDupli2/Dupli2"}},
      {"OBGEO_Ear_L",
       {"/Dupli1/GEO_Head-0/GEO_Ear_L-1",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_L",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Ear_L-1"}},
      {"OBGEO_Ear_R",
       {"/Dupli1/GEO_Head-0/GEO_Ear_R-2",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_R",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Ear_R-2"}},
      {"OBGEO_Head",
       {"/Dupli1/GEO_Head-0",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head",
        "/ParentOfDupli2/Dupli2/GEO_Head-0"}},
      {"OBGEO_Nose",
       {"/Dupli1/GEO_Head-0/GEO_Nose-3",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Nose",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Nose-3"}},
      {"OBGround plane", {"/Ground plane"}},
      {"OBOutsideDupliGrandParent", {"/Ground plane/OutsideDupliGrandParent"}},
      {"OBOutsideDupliParent", {"/Ground plane/OutsideDupliGrandParent/OutsideDupliParent"}},
      {"OBParentOfDupli2", {"/ParentOfDupli2"}}};
  EXPECT_EQ(expected_transforms, iterator->transform_writers);

  used_writers expected_data = {
      {"OBCamera", {"/Camera/Camera"}},
      {"OBGEO_Ear_L",
       {"/Dupli1/GEO_Head-0/GEO_Ear_L-1/Ear",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_L/Ear",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Ear_L-1/Ear"}},
      {"OBGEO_Ear_R",
       {"/Dupli1/GEO_Head-0/GEO_Ear_R-2/Ear",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_R/Ear",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Ear_R-2/Ear"}},
      {"OBGEO_Head",
       {"/Dupli1/GEO_Head-0/Face",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/Face",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/Face"}},
      {"OBGEO_Nose",
       {"/Dupli1/GEO_Head-0/GEO_Nose-3/Nose",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Nose/Nose",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Nose-3/Nose"}},
      {"OBGround plane", {"/Ground plane/Plane"}},
      {"OBParentOfDupli2", {"/ParentOfDupli2/Icosphere"}},
  };

  EXPECT_EQ(expected_data, iterator->data_writers);

  /* The scene has no hair or particle systems. */
  EXPECT_EQ(0, iterator->hair_writers.size());
  EXPECT_EQ(0, iterator->particle_writers.size());

  /* On the second iteration, everything should be written as well.
   * This tests the default value of iterator->export_subset_. */
  iterator->transform_writers.clear();
  iterator->data_writers.clear();
  iterator->iterate_and_write();
  EXPECT_EQ(expected_transforms, iterator->transform_writers);
  EXPECT_EQ(expected_data, iterator->data_writers);
}

TEST_F(AbstractHierarchyIteratorTest, ExportSubsetTest)
{
  /* The scene has no hair or particle systems, and this is already covered by ExportHierarchyTest,
   * so not included here. Update this test when hair & particle systems are included. */

  /* Load the test blend file. */
  if (!blendfile_load("usd" SEP_STR "usd_hierarchy_export_test.blend")) {
    return;
  }
  depsgraph_create(DAG_EVAL_RENDER);
  iterator_create();

  /* Mapping from object name to set of export paths. */
  used_writers expected_transforms = {
      {"OBCamera", {"/Camera"}},
      {"OBDupli1", {"/Dupli1"}},
      {"OBDupli2", {"/ParentOfDupli2/Dupli2"}},
      {"OBGEO_Ear_L",
       {"/Dupli1/GEO_Head-0/GEO_Ear_L-1",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_L",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Ear_L-1"}},
      {"OBGEO_Ear_R",
       {"/Dupli1/GEO_Head-0/GEO_Ear_R-2",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_R",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Ear_R-2"}},
      {"OBGEO_Head",
       {"/Dupli1/GEO_Head-0",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head",
        "/ParentOfDupli2/Dupli2/GEO_Head-0"}},
      {"OBGEO_Nose",
       {"/Dupli1/GEO_Head-0/GEO_Nose-3",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Nose",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Nose-3"}},
      {"OBGround plane", {"/Ground plane"}},
      {"OBOutsideDupliGrandParent", {"/Ground plane/OutsideDupliGrandParent"}},
      {"OBOutsideDupliParent", {"/Ground plane/OutsideDupliGrandParent/OutsideDupliParent"}},
      {"OBParentOfDupli2", {"/ParentOfDupli2"}}};

  used_writers expected_data = {
      {"OBCamera", {"/Camera/Camera"}},
      {"OBGEO_Ear_L",
       {"/Dupli1/GEO_Head-0/GEO_Ear_L-1/Ear",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_L/Ear",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Ear_L-1/Ear"}},
      {"OBGEO_Ear_R",
       {"/Dupli1/GEO_Head-0/GEO_Ear_R-2/Ear",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Ear_R/Ear",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Ear_R-2/Ear"}},
      {"OBGEO_Head",
       {"/Dupli1/GEO_Head-0/Face",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/Face",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/Face"}},
      {"OBGEO_Nose",
       {"/Dupli1/GEO_Head-0/GEO_Nose-3/Nose",
        "/Ground plane/OutsideDupliGrandParent/OutsideDupliParent/GEO_Head/GEO_Nose/Nose",
        "/ParentOfDupli2/Dupli2/GEO_Head-0/GEO_Nose-3/Nose"}},
      {"OBGround plane", {"/Ground plane/Plane"}},
      {"OBParentOfDupli2", {"/ParentOfDupli2/Icosphere"}},
  };

  /* Even when only asking an export of transforms, on the first frame everything should be
   * exported. */
  {
    ExportSubset export_subset = {false};
    export_subset.transforms = true;
    export_subset.shapes = false;
    iterator->set_export_subset(export_subset);
  }
  iterator->iterate_and_write();
  EXPECT_EQ(expected_transforms, iterator->transform_writers);
  EXPECT_EQ(expected_data, iterator->data_writers);

  /* Clear data to prepare for the next iteration. */
  iterator->transform_writers.clear();
  iterator->data_writers.clear();

  /* Second iteration, should only write transforms now. */
  iterator->iterate_and_write();
  EXPECT_EQ(expected_transforms, iterator->transform_writers);
  EXPECT_EQ(0, iterator->data_writers.size());

  /* Clear data to prepare for the next iteration. */
  iterator->transform_writers.clear();
  iterator->data_writers.clear();

  /* Third iteration, should only write data now. */
  {
    ExportSubset export_subset = {false};
    export_subset.transforms = false;
    export_subset.shapes = true;
    iterator->set_export_subset(export_subset);
  }
  iterator->iterate_and_write();
  EXPECT_EQ(0, iterator->transform_writers.size());
  EXPECT_EQ(expected_data, iterator->data_writers);

  /* Clear data to prepare for the next iteration. */
  iterator->transform_writers.clear();
  iterator->data_writers.clear();

  /* Fourth iteration, should export everything now. */
  {
    ExportSubset export_subset = {false};
    export_subset.transforms = true;
    export_subset.shapes = true;
    iterator->set_export_subset(export_subset);
  }
  iterator->iterate_and_write();
  EXPECT_EQ(expected_transforms, iterator->transform_writers);
  EXPECT_EQ(expected_data, iterator->data_writers);
}

/* Test class that constructs a depsgraph in such a way that it includes invisible objects. */
class AbstractHierarchyIteratorInvisibleTest : public AbstractHierarchyIteratorTest {
 protected:
  void depsgraph_create(eEvaluationMode depsgraph_evaluation_mode) override
  {
    depsgraph = DEG_graph_new(
        bfile->main, bfile->curscene, bfile->cur_view_layer, depsgraph_evaluation_mode);
    DEG_graph_build_for_all_objects(depsgraph);
    BKE_scene_graph_update_tagged(depsgraph, bfile->main);
  }
};

TEST_F(AbstractHierarchyIteratorInvisibleTest, ExportInvisibleTest)
{
  if (!blendfile_load("alembic" SEP_STR "visibility.blend")) {
    return;
  }
  depsgraph_create(DAG_EVAL_RENDER);
  iterator_create();

  iterator->iterate_and_write();

  /* Mapping from object name to set of export paths. */
  used_writers expected_transforms = {{"OBInvisibleAnimatedCube", {"/InvisibleAnimatedCube"}},
                                      {"OBInvisibleCube", {"/InvisibleCube"}},
                                      {"OBVisibleCube", {"/VisibleCube"}}};
  EXPECT_EQ(expected_transforms, iterator->transform_writers);

  used_writers expected_data = {{"OBInvisibleAnimatedCube", {"/InvisibleAnimatedCube/Cube"}},
                                {"OBInvisibleCube", {"/InvisibleCube/Cube"}},
                                {"OBVisibleCube", {"/VisibleCube/Cube"}}};

  EXPECT_EQ(expected_data, iterator->data_writers);

  /* The scene has no hair or particle systems. */
  EXPECT_EQ(0, iterator->hair_writers.size());
  EXPECT_EQ(0, iterator->particle_writers.size());
}

}  // namespace blender::io
