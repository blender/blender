/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_blender_project.hh"
#include "BKE_gtest_base.hh"
#include "BKE_main.hh"

#include "testing/testing.h"

namespace blender::bke::tests {

class BlenderProjectTest : public bke::BlenderGTestBase {
 public:
  Main *bmain;

  void SetUp() override
  {
    bmain = BKE_main_new();
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(BlenderProjectTest, blender_project_init_clear_test)
{
  EXPECT_EQ(true, BKE_blender_project_init("My Project", "/path/to/my/project"));

  BKE_blender_project_read_callback(bmain, [](const BlenderProject *project) {
    ASSERT_NE(project, nullptr);
    EXPECT_EQ(project->get_name(), "My Project");
    EXPECT_EQ(project->get_root_path(), "/path/to/my/project");
  });

  BKE_blender_project_clear();

  BKE_blender_project_read_callback(
      bmain, [](const BlenderProject *project) { ASSERT_EQ(project, nullptr); });
}

TEST_F(BlenderProjectTest, blender_project_replace_test)
{
  EXPECT_EQ(true, BKE_blender_project_init("My Project", "/path/to/my/project"));

  BKE_blender_project_read_callback(bmain, [](const BlenderProject *project) {
    ASSERT_NE(project, nullptr);
    EXPECT_EQ(project->get_name(), "My Project");
    EXPECT_EQ(project->get_root_path(), "/path/to/my/project");
  });

  /* Invalid: empty root. Should leave existing project in place. */
  EXPECT_EQ(false, BKE_blender_project_init("My Project", ""));

  BKE_blender_project_read_callback(bmain, [](const BlenderProject *project) {
    ASSERT_NE(project, nullptr);
    EXPECT_EQ(project->get_name(), "My Project");
    EXPECT_EQ(project->get_root_path(), "/path/to/my/project");
  });

  /* Invalid: empty name. Should leave existing project in place. */
  EXPECT_EQ(false, BKE_blender_project_init("", "/path/to/my/project"));

  BKE_blender_project_read_callback(bmain, [](const BlenderProject *project) {
    ASSERT_NE(project, nullptr);
    EXPECT_EQ(project->get_name(), "My Project");
    EXPECT_EQ(project->get_root_path(), "/path/to/my/project");
  });

  /* Valid. Should clear and replace the existing project. */
  EXPECT_EQ(true, BKE_blender_project_init("My Next Project", "/path/to/way/cooler/project"));

  BKE_blender_project_read_callback(bmain, [](const BlenderProject *project) {
    ASSERT_NE(project, nullptr);
    EXPECT_EQ(project->get_name(), "My Next Project");
    EXPECT_EQ(project->get_root_path(), "/path/to/way/cooler/project");
  });
}

/* Test get/set for the project's name and root path. */
TEST_F(BlenderProjectTest, blender_project_name_and_path_test)
{
  BKE_blender_project_init("My Project", "/path/to/my/project");

  BKE_blender_project_write_callback(bmain, [](BlenderProject *project) {
    ASSERT_NE(project, nullptr);
    EXPECT_EQ(project->get_name(), "My Project");
    EXPECT_EQ(project->get_root_path(), "/path/to/my/project");

    project->set_name("My Next Project");
    EXPECT_EQ(project->get_name(), "My Next Project");

    project->set_root_path("/path/to/way/cooler/project");
    EXPECT_EQ(project->get_root_path(), "/path/to/way/cooler/project");
  });
}

/* Check the basics of creating new variables. */
TEST_F(BlenderProjectTest, blender_project_variable_new_test)
{
  BKE_blender_project_init("My Project", "/path/to/my/project");

  BKE_blender_project_write_callback(bmain, [](BlenderProject *project) {
    ASSERT_NE(project, nullptr);

    ProjectVariable *var_str = project->new_variable("MyStringVar", ProjectVariableType::STRING);
    ASSERT_NE(var_str, nullptr);
    EXPECT_EQ(var_str->name_get(), "MyStringVar");
    EXPECT_EQ(var_str->type_get(), ProjectVariableType::STRING);
    EXPECT_EQ(var_str->string_subtype_get(), ProjectVariableStringSubtype::NONE);
    EXPECT_EQ(var_str->value_string_get(), "");
    EXPECT_EQ(var_str->description_get(), "");

    ProjectVariable *var_int = project->new_variable("MyIntVar", ProjectVariableType::INT);
    ASSERT_NE(var_int, nullptr);
    EXPECT_EQ(var_int->name_get(), "MyIntVar");
    EXPECT_EQ(var_int->type_get(), ProjectVariableType::INT);
    EXPECT_EQ(var_int->value_int_get(), 0);
    EXPECT_EQ(var_int->description_get(), "");

    ProjectVariable *var_float = project->new_variable("MyFloatVar", ProjectVariableType::FLOAT);
    ASSERT_NE(var_float, nullptr);
    EXPECT_EQ(var_float->name_get(), "MyFloatVar");
    EXPECT_EQ(var_float->type_get(), ProjectVariableType::FLOAT);
    EXPECT_EQ(var_float->value_float_get(), 0.0);
    EXPECT_EQ(var_float->description_get(), "");
  });
}

/* Test how variable names are handled when creating new variables.
 *
 * - Duplicate names should be suffixed with `_001`, etc.
 * - Invalid names should be modified to become valid. */
TEST_F(BlenderProjectTest, blender_project_variable_new_names_test)
{
  BKE_blender_project_init("My Project", "/path/to/my/project");

  BKE_blender_project_write_callback(bmain, [](BlenderProject *project) {
    ASSERT_NE(project, nullptr);

    ProjectVariable *var0 = project->new_variable("MyVar", ProjectVariableType::STRING);
    ProjectVariable *var1 = project->new_variable("MyVar", ProjectVariableType::STRING);
    ProjectVariable *var2 = project->new_variable("MyVar_001", ProjectVariableType::STRING);
    ProjectVariable *var3 = project->new_variable("MyVar_003", ProjectVariableType::STRING);
    ProjectVariable *var4 = project->new_variable("MyOtherVar", ProjectVariableType::STRING);
    ProjectVariable *var5 = project->new_variable("MyOtherVar", ProjectVariableType::STRING);
    ProjectVariable *var6 = project->new_variable("MyOtherVar_001", ProjectVariableType::STRING);
    ProjectVariable *var7 = project->new_variable("MyOtherVar_003", ProjectVariableType::STRING);
    ProjectVariable *var8 = project->new_variable("Invalid name.with-punctuation_and@ユニコード",
                                                  ProjectVariableType::STRING);
    ASSERT_NE(var0, nullptr);
    ASSERT_NE(var1, nullptr);
    ASSERT_NE(var2, nullptr);
    ASSERT_NE(var3, nullptr);
    ASSERT_NE(var4, nullptr);
    ASSERT_NE(var5, nullptr);
    ASSERT_NE(var6, nullptr);
    ASSERT_NE(var7, nullptr);
    ASSERT_NE(var8, nullptr);

    EXPECT_EQ(var0->name_get(), "MyVar");
    EXPECT_EQ(var1->name_get(), "MyVar_001");
    EXPECT_EQ(var2->name_get(), "MyVar_002");
    EXPECT_EQ(var3->name_get(), "MyVar_003");
    EXPECT_EQ(var4->name_get(), "MyOtherVar");
    EXPECT_EQ(var5->name_get(), "MyOtherVar_001");
    EXPECT_EQ(var6->name_get(), "MyOtherVar_002");
    EXPECT_EQ(var7->name_get(), "MyOtherVar_003");
    EXPECT_EQ(var8->name_get(), "Invalid_name_with_punctuation_and________________");
  });
}

/* Test how variable names are handled when renaming.
 *
 * - Duplicate names should be suffixed with `_001`, etc.
 * - Invalid names should be modified to become valid. */
TEST_F(BlenderProjectTest, blender_project_variable_rename_test)
{
  BKE_blender_project_init("My Project", "/path/to/my/project");

  BKE_blender_project_write_callback(bmain, [](BlenderProject *project) {
    ASSERT_NE(project, nullptr);

    ProjectVariable *var0 = project->new_variable("MyVar0", ProjectVariableType::STRING);
    ProjectVariable *var1 = project->new_variable("MyVar1", ProjectVariableType::STRING);
    ProjectVariable *var2 = project->new_variable("MyVar2", ProjectVariableType::STRING);
    ASSERT_NE(var0, nullptr);
    ASSERT_NE(var1, nullptr);
    ASSERT_NE(var2, nullptr);

    ASSERT_EQ(project->find_variable_index(var0), 0);
    ASSERT_EQ(project->find_variable_index(var1), 1);
    ASSERT_EQ(project->find_variable_index(var2), 2);

    /* Rename to a duplicate name. */
    project->rename_variable(1, "MyVar0");
    EXPECT_EQ(var1->name_get(), "MyVar0_001");
    project->rename_variable(2, "MyVar0");
    EXPECT_EQ(var2->name_get(), "MyVar0_002");

    /* Rename to same as current name. */
    project->rename_variable(0, "MyVar0");
    EXPECT_EQ(var0->name_get(), "MyVar0");
    project->rename_variable(1, "MyVar0_001");
    EXPECT_EQ(var1->name_get(), "MyVar0_001");
    project->rename_variable(2, "MyVar0_002");
    EXPECT_EQ(var2->name_get(), "MyVar0_002");

    /* Rename to invalid name. */
    project->rename_variable(0, "Invalid name.with-punctuation_and@ユニコード");
    EXPECT_EQ(var0->name_get(), "Invalid_name_with_punctuation_and________________");

    /* Rename to duplicate invalid name. */
    project->rename_variable(1, "Invalid name.with-punctuation_and@ユニコード");
    EXPECT_EQ(var1->name_get(), "Invalid_name_with_punctuation_and_________________001");

    /* Rename to empty (invalid) name. */
    project->rename_variable(0, "");
    EXPECT_EQ(var0->name_get(), "_");

    /* Rename to duplicate empty (invalid) name. */
    project->rename_variable(1, "");
    EXPECT_EQ(var1->name_get(), "__001");
  });
}

/* Test sub-type get/set on project variables. */
TEST_F(BlenderProjectTest, blender_project_variable_subtype_test)
{
  BKE_blender_project_init("My Project", "/path/to/my/project");

  BKE_blender_project_write_callback(bmain, [](BlenderProject *project) {
    ASSERT_NE(project, nullptr);

    ProjectVariable *var = project->new_variable("MyVar", ProjectVariableType::STRING);
    ASSERT_NE(var, nullptr);

    var->string_subtype_set(ProjectVariableStringSubtype::FILEPATH);
    EXPECT_EQ(var->string_subtype_get(), ProjectVariableStringSubtype::FILEPATH);
    var->string_subtype_set(ProjectVariableStringSubtype::NONE);
    EXPECT_EQ(var->string_subtype_get(), ProjectVariableStringSubtype::NONE);
  });
}

/* Test value get/set on project variables. */
TEST_F(BlenderProjectTest, blender_project_variable_value_test)
{
  BKE_blender_project_init("My Project", "/path/to/my/project");

  BKE_blender_project_write_callback(bmain, [](BlenderProject *project) {
    ASSERT_NE(project, nullptr);

    ProjectVariable *var_str = project->new_variable("MyStringVar", ProjectVariableType::STRING);
    ProjectVariable *var_int = project->new_variable("MyIntVar", ProjectVariableType::INT);
    ProjectVariable *var_float = project->new_variable("MyFloatVar", ProjectVariableType::FLOAT);
    ASSERT_NE(var_str, nullptr);
    ASSERT_NE(var_int, nullptr);
    ASSERT_NE(var_float, nullptr);

    var_str->value_set("Hello Suzanne!");
    EXPECT_EQ(var_str->value_string_get(), "Hello Suzanne!");
    var_str->value_set("Hello Suzaaaaaaaaaaaaan!");
    EXPECT_EQ(var_str->value_string_get(), "Hello Suzaaaaaaaaaaaaan!");
    var_str->value_set("Hello!");
    EXPECT_EQ(var_str->value_string_get(), "Hello!");
    var_str->value_set("");
    EXPECT_EQ(var_str->value_string_get(), "");

    var_int->value_set(42);
    EXPECT_EQ(var_int->value_int_get(), 42);
    var_int->value_set(7);
    EXPECT_EQ(var_int->value_int_get(), 7);

    var_float->value_set(3.5f);
    EXPECT_EQ(var_float->value_float_get(), 3.5f);
    var_float->value_set(-1.5f);
    EXPECT_EQ(var_float->value_float_get(), -1.5f);
  });
}

/* Test description get/set on project variables. */
TEST_F(BlenderProjectTest, blender_project_variable_description_test)
{
  BKE_blender_project_init("My Project", "/path/to/my/project");

  BKE_blender_project_write_callback(bmain, [](BlenderProject *project) {
    ASSERT_NE(project, nullptr);

    ProjectVariable *var = project->new_variable("MyVar", ProjectVariableType::STRING);
    ASSERT_NE(var, nullptr);

    var->description_set("I'm a variable!");
    EXPECT_EQ(var->description_get(), "I'm a variable!");
    var->description_set("No I'm not!  I'm a *project* variable!");
    EXPECT_EQ(var->description_get(), "No I'm not!  I'm a *project* variable!");
    var->description_set("");
    EXPECT_EQ(var->description_get(), "");
  });
}

/* Test moving project variables. */
TEST_F(BlenderProjectTest, blender_project_variable_move_test)
{
  BKE_blender_project_init("My Project", "/path/to/my/project");

  BKE_blender_project_write_callback(bmain, [](BlenderProject *project) {
    ASSERT_NE(project, nullptr);

    ProjectVariable *var_a = project->new_variable("MyVarA", ProjectVariableType::STRING);
    ProjectVariable *var_b = project->new_variable("MyVarB", ProjectVariableType::STRING);
    ProjectVariable *var_c = project->new_variable("MyVarC", ProjectVariableType::STRING);
    ProjectVariable *var_d = project->new_variable("MyVarD", ProjectVariableType::STRING);
    ASSERT_NE(var_a, nullptr);
    ASSERT_NE(var_b, nullptr);
    ASSERT_NE(var_c, nullptr);
    ASSERT_NE(var_d, nullptr);

    EXPECT_EQ(project->find_variable_index(var_a), 0);
    EXPECT_EQ(project->find_variable_index(var_b), 1);
    EXPECT_EQ(project->find_variable_index(var_c), 2);
    EXPECT_EQ(project->find_variable_index(var_d), 3);

    project->move_variable(0, 1);
    EXPECT_EQ(project->find_variable_index(var_b), 0);
    EXPECT_EQ(project->find_variable_index(var_a), 1);
    EXPECT_EQ(project->find_variable_index(var_c), 2);
    EXPECT_EQ(project->find_variable_index(var_d), 3);

    project->move_variable(2, 3);
    EXPECT_EQ(project->find_variable_index(var_b), 0);
    EXPECT_EQ(project->find_variable_index(var_a), 1);
    EXPECT_EQ(project->find_variable_index(var_d), 2);
    EXPECT_EQ(project->find_variable_index(var_c), 3);

    project->move_variable(0, 3);
    EXPECT_EQ(project->find_variable_index(var_a), 0);
    EXPECT_EQ(project->find_variable_index(var_d), 1);
    EXPECT_EQ(project->find_variable_index(var_c), 2);
    EXPECT_EQ(project->find_variable_index(var_b), 3);

    project->move_variable(2, 0);
    EXPECT_EQ(project->find_variable_index(var_c), 0);
    EXPECT_EQ(project->find_variable_index(var_a), 1);
    EXPECT_EQ(project->find_variable_index(var_d), 2);
    EXPECT_EQ(project->find_variable_index(var_b), 3);
  });
}

/* Test removing project variables. */
TEST_F(BlenderProjectTest, blender_project_variable_remove_test)
{
  BKE_blender_project_init("My Project", "/path/to/my/project");

  BKE_blender_project_write_callback(bmain, [](BlenderProject *project) {
    ASSERT_NE(project, nullptr);

    ProjectVariable *var_a = project->new_variable("MyVarA", ProjectVariableType::STRING);
    ProjectVariable *var_b = project->new_variable("MyVarB", ProjectVariableType::STRING);
    ProjectVariable *var_c = project->new_variable("MyVarC", ProjectVariableType::STRING);
    ProjectVariable *var_d = project->new_variable("MyVarD", ProjectVariableType::STRING);
    ASSERT_NE(var_a, nullptr);
    ASSERT_NE(var_b, nullptr);
    ASSERT_NE(var_c, nullptr);
    ASSERT_NE(var_d, nullptr);

    EXPECT_EQ(project->variables.size(), 4);
    EXPECT_EQ(project->find_variable_index(var_a), 0);
    EXPECT_EQ(project->find_variable_index(var_b), 1);
    EXPECT_EQ(project->find_variable_index(var_c), 2);
    EXPECT_EQ(project->find_variable_index(var_d), 3);

    project->remove_variable(var_c);
    EXPECT_EQ(project->variables.size(), 3);
    EXPECT_EQ(project->find_variable_index(var_a), 0);
    EXPECT_EQ(project->find_variable_index(var_b), 1);
    EXPECT_EQ(project->find_variable_index(var_d), 2);

    project->remove_variable(var_a);
    EXPECT_EQ(project->variables.size(), 2);
    EXPECT_EQ(project->find_variable_index(var_b), 0);
    EXPECT_EQ(project->find_variable_index(var_d), 1);

    project->remove_variable(var_d);
    EXPECT_EQ(project->variables.size(), 1);
    EXPECT_EQ(project->find_variable_index(var_b), 0);

    project->remove_variable(var_b);
    EXPECT_EQ(project->variables.size(), 0);
  });
}

}  // namespace blender::bke::tests
