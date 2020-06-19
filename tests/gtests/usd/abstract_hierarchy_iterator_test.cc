/*
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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#include "IO_abstract_hierarchy_iterator.h"
#include "blenloader/blendfile_loading_base_test.h"

extern "C" {
#include "BLI_math.h"
#include "DEG_depsgraph.h"
#include "DNA_object_types.h"
}

#include <map>
#include <set>

/* Mapping from ID.name to set of export hierarchy path. Duplicated objects can be exported
 * multiple times with different export paths, hence the set. */
typedef std::map<std::string, std::set<std::string>> used_writers;

using namespace blender::io;

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

void debug_print_writers(const char *label, const used_writers &writers_map)
{
  printf("%s:\n", label);
  for (auto idname_writers : writers_map) {
    printf("    %s:\n", idname_writers.first.c_str());
    for (const std::string &export_path : idname_writers.second) {
      printf("      - %s\n", export_path.c_str());
    }
  }
}

class TestingHierarchyIterator : public AbstractHierarchyIterator {
 public: /* Public so that the test cases can directly inspect the created writers. */
  used_writers transform_writers;
  used_writers data_writers;
  used_writers hair_writers;
  used_writers particle_writers;

 public:
  explicit TestingHierarchyIterator(Depsgraph *depsgraph) : AbstractHierarchyIterator(depsgraph)
  {
  }
  virtual ~TestingHierarchyIterator()
  {
  }

 protected:
  AbstractHierarchyWriter *create_transform_writer(const HierarchyContext *context) override
  {
    return new TestHierarchyWriter("transform", transform_writers);
  }
  AbstractHierarchyWriter *create_data_writer(const HierarchyContext *context) override
  {
    return new TestHierarchyWriter("data", data_writers);
  }
  AbstractHierarchyWriter *create_hair_writer(const HierarchyContext *context) override
  {
    return new TestHierarchyWriter("hair", hair_writers);
  }
  AbstractHierarchyWriter *create_particle_writer(const HierarchyContext *context) override
  {
    return new TestHierarchyWriter("particle", particle_writers);
  }

  void delete_object_writer(AbstractHierarchyWriter *writer) override
  {
    delete writer;
  }
};

class USDHierarchyIteratorTest : public BlendfileLoadingBaseTest {
 protected:
  TestingHierarchyIterator *iterator;

  virtual void SetUp()
  {
    BlendfileLoadingBaseTest::SetUp();
    iterator = nullptr;
  }

  virtual void TearDown()
  {
    iterator_free();
    BlendfileLoadingBaseTest::TearDown();
  }

  /* Create a test iterator. */
  void iterator_create()
  {
    iterator = new TestingHierarchyIterator(depsgraph);
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

TEST_F(USDHierarchyIteratorTest, ExportHierarchyTest)
{
  /* Load the test blend file. */
  if (!blendfile_load("usd/usd_hierarchy_export_test.blend")) {
    return;
  }
  depsgraph_create(DAG_EVAL_RENDER);
  iterator_create();

  iterator->iterate_and_write();

  // Mapping from object name to set of export paths.
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

  // The scene has no hair or particle systems.
  EXPECT_EQ(0, iterator->hair_writers.size());
  EXPECT_EQ(0, iterator->particle_writers.size());

  // On the second iteration, everything should be written as well.
  // This tests the default value of iterator->export_subset_.
  iterator->transform_writers.clear();
  iterator->data_writers.clear();
  iterator->iterate_and_write();
  EXPECT_EQ(expected_transforms, iterator->transform_writers);
  EXPECT_EQ(expected_data, iterator->data_writers);
}

TEST_F(USDHierarchyIteratorTest, ExportSubsetTest)
{
  // The scene has no hair or particle systems, and this is already covered by ExportHierarchyTest,
  // so not included here. Update this test when hair & particle systems are included.

  /* Load the test blend file. */
  if (!blendfile_load("usd/usd_hierarchy_export_test.blend")) {
    return;
  }
  depsgraph_create(DAG_EVAL_RENDER);
  iterator_create();

  // Mapping from object name to set of export paths.
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

  // Even when only asking an export of transforms, on the first frame everything should be
  // exported.
  {
    ExportSubset export_subset = {0};
    export_subset.transforms = true;
    export_subset.shapes = false;
    iterator->set_export_subset(export_subset);
  }
  iterator->iterate_and_write();
  EXPECT_EQ(expected_transforms, iterator->transform_writers);
  EXPECT_EQ(expected_data, iterator->data_writers);

  // Clear data to prepare for the next iteration.
  iterator->transform_writers.clear();
  iterator->data_writers.clear();

  // Second iteration, should only write transforms now.
  iterator->iterate_and_write();
  EXPECT_EQ(expected_transforms, iterator->transform_writers);
  EXPECT_EQ(0, iterator->data_writers.size());

  // Clear data to prepare for the next iteration.
  iterator->transform_writers.clear();
  iterator->data_writers.clear();

  // Third iteration, should only write data now.
  {
    ExportSubset export_subset = {0};
    export_subset.transforms = false;
    export_subset.shapes = true;
    iterator->set_export_subset(export_subset);
  }
  iterator->iterate_and_write();
  EXPECT_EQ(0, iterator->transform_writers.size());
  EXPECT_EQ(expected_data, iterator->data_writers);

  // Clear data to prepare for the next iteration.
  iterator->transform_writers.clear();
  iterator->data_writers.clear();

  // Fourth iteration, should export everything now.
  {
    ExportSubset export_subset = {0};
    export_subset.transforms = true;
    export_subset.shapes = true;
    iterator->set_export_subset(export_subset);
  }
  iterator->iterate_and_write();
  EXPECT_EQ(expected_transforms, iterator->transform_writers);
  EXPECT_EQ(expected_data, iterator->data_writers);
}
