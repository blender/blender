/* SPDX-FileCopyrightText: 2013 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_Debug.h"

extern "C" {
#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "BKE_appdir.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
}

#include "COM_ExecutionGroup.h"
#include "COM_ReadBufferOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_ViewerOperation.h"
#include "COM_WriteBufferOperation.h"

namespace blender::compositor {

int DebugInfo::file_index_ = 0;
DebugInfo::NodeNameMap DebugInfo::node_names_;
DebugInfo::OpNameMap DebugInfo::op_names_;
std::string DebugInfo::current_node_name_;
std::string DebugInfo::current_op_name_;
DebugInfo::GroupStateMap DebugInfo::group_states_;

static std::string operation_class_name(const NodeOperation *op)
{
  std::string full_name = typeid(*op).name();
  /* Remove name-spaces. */
  size_t pos = full_name.find_last_of(':');
  BLI_assert(pos != std::string::npos);
  return full_name.substr(pos + 1);
}

std::string DebugInfo::node_name(const Node *node)
{
  NodeNameMap::const_iterator it = node_names_.find(node);
  if (it != node_names_.end()) {
    return it->second;
  }
  return "";
}

std::string DebugInfo::operation_name(const NodeOperation *op)
{
  OpNameMap::const_iterator it = op_names_.find(op);
  if (it != op_names_.end()) {
    return it->second;
  }
  return "";
}

int DebugInfo::graphviz_operation(const ExecutionSystem *system,
                                  NodeOperation *operation,
                                  const ExecutionGroup *group,
                                  char *str,
                                  int maxlen)
{
  int len = 0;

  std::string fillcolor = "gainsboro";
  if (operation->get_flags().is_viewer_operation) {
    const ViewerOperation *viewer = (const ViewerOperation *)operation;
    if (viewer->is_active_viewer_output()) {
      fillcolor = "lightskyblue1";
    }
    else {
      fillcolor = "lightskyblue3";
    }
  }
  else if (operation->is_output_operation(system->get_context().is_rendering())) {
    fillcolor = "dodgerblue1";
  }
  else if (operation->get_flags().is_set_operation) {
    fillcolor = "khaki1";
  }
  else if (operation->get_flags().is_read_buffer_operation) {
    fillcolor = "darkolivegreen3";
  }
  else if (operation->get_flags().is_write_buffer_operation) {
    fillcolor = "darkorange";
  }

  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "// OPERATION: %p\r\n", operation);
  if (group) {
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\"O_%p_%p\"", operation, group);
  }
  else {
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\"O_%p\"", operation);
  }
  len += snprintf(str + len,
                  maxlen > len ? maxlen - len : 0,
                  " [fillcolor=%s,style=filled,shape=record,label=\"{",
                  fillcolor.c_str());

  int totinputs = operation->get_number_of_input_sockets();
  if (totinputs != 0) {
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "{");
    for (int k = 0; k < totinputs; k++) {
      NodeOperationInput *socket = operation->get_input_socket(k);
      if (k != 0) {
        len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "|");
      }
      len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "<IN_%p>", socket);
      switch (socket->get_data_type()) {
        case DataType::Value:
          len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Value");
          break;
        case DataType::Vector:
          len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Vector");
          break;
        case DataType::Color:
          len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Color");
          break;
      }
    }
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}");
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "|");
  }

  if (COM_GRAPHVIZ_SHOW_NODE_NAME) {
    std::string op_node_name = operation->get_name();
    if (!op_node_name.empty()) {
      len += snprintf(
          str + len, maxlen > len ? maxlen - len : 0, "%s\\n", (op_node_name + " Node").c_str());
    }
  }

  len += snprintf(str + len,
                  maxlen > len ? maxlen - len : 0,
                  "%s\\n",
                  operation_class_name(operation).c_str());

  len += snprintf(str + len,
                  maxlen > len ? maxlen - len : 0,
                  "#%d (%i,%i) (%u,%u)",
                  operation->get_id(),
                  operation->get_canvas().xmin,
                  operation->get_canvas().ymin,
                  operation->get_width(),
                  operation->get_height());

  int totoutputs = operation->get_number_of_output_sockets();
  if (totoutputs != 0) {
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "|");
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "{");
    for (int k = 0; k < totoutputs; k++) {
      NodeOperationOutput *socket = operation->get_output_socket(k);
      if (k != 0) {
        len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "|");
      }
      len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "<OUT_%p>", socket);
      switch (socket->get_data_type()) {
        case DataType::Value: {
          ConstantOperation *constant = operation->get_flags().is_constant_operation ?
                                            static_cast<ConstantOperation *>(operation) :
                                            nullptr;
          if (constant && constant->can_get_constant_elem()) {
            const float value = *constant->get_constant_elem();
            len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Value\\n%12.4g", value);
          }
          else {
            len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Value");
          }
          break;
        }
        case DataType::Vector: {
          len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Vector");
          break;
        }
        case DataType::Color: {
          len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "Color");
          break;
        }
      }
    }
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}");
  }
  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}\"]");
  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\r\n");

  return len;
}

int DebugInfo::graphviz_legend_color(const char *name, const char *color, char *str, int maxlen)
{
  int len = 0;
  len += snprintf(str + len,
                  maxlen > len ? maxlen - len : 0,
                  "<TR><TD>%s</TD><TD BGCOLOR=\"%s\"></TD></TR>\r\n",
                  name,
                  color);
  return len;
}

int DebugInfo::graphviz_legend_line(
    const char * /*name*/, const char * /*color*/, const char * /*style*/, char *str, int maxlen)
{
  /* XXX TODO */
  int len = 0;
  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\r\n");
  return len;
}

int DebugInfo::graphviz_legend_group(
    const char *name, const char *color, const char * /*style*/, char *str, int maxlen)
{
  int len = 0;
  len += snprintf(str + len,
                  maxlen > len ? maxlen - len : 0,
                  "<TR><TD>%s</TD><TD CELLPADDING=\"4\"><TABLE BORDER=\"1\" CELLBORDER=\"0\" "
                  "CELLSPACING=\"0\" CELLPADDING=\"0\"><TR><TD "
                  "BGCOLOR=\"%s\"></TD></TR></TABLE></TD></TR>\r\n",
                  name,
                  color);
  return len;
}

int DebugInfo::graphviz_legend(char *str, int maxlen, const bool has_execution_groups)
{
  int len = 0;

  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "{\r\n");
  if (has_execution_groups) {
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "rank = sink;\r\n");
  }
  len += snprintf(
      str + len, maxlen > len ? maxlen - len : 0, "Legend [shape=none, margin=0, label=<\r\n");

  len += snprintf(
      str + len,
      maxlen > len ? maxlen - len : 0,
      "  <TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\r\n");
  len += snprintf(str + len,
                  maxlen > len ? maxlen - len : 0,
                  "<TR><TD COLSPAN=\"2\"><B>Legend</B></TD></TR>\r\n");

  len += graphviz_legend_color(
      "NodeOperation", "gainsboro", str + len, maxlen > len ? maxlen - len : 0);
  len += graphviz_legend_color(
      "Output", "dodgerblue1", str + len, maxlen > len ? maxlen - len : 0);
  len += graphviz_legend_color(
      "Viewer", "lightskyblue3", str + len, maxlen > len ? maxlen - len : 0);
  len += graphviz_legend_color(
      "Active Viewer", "lightskyblue1", str + len, maxlen > len ? maxlen - len : 0);
  if (has_execution_groups) {
    len += graphviz_legend_color(
        "Write Buffer", "darkorange", str + len, maxlen > len ? maxlen - len : 0);
    len += graphviz_legend_color(
        "Read Buffer", "darkolivegreen3", str + len, maxlen > len ? maxlen - len : 0);
  }
  len += graphviz_legend_color(
      "Input Value", "khaki1", str + len, maxlen > len ? maxlen - len : 0);

  if (has_execution_groups) {
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "<TR><TD></TD></TR>\r\n");
    len += graphviz_legend_group(
        "Group Waiting", "white", "dashed", str + len, maxlen > len ? maxlen - len : 0);
    len += graphviz_legend_group(
        "Group Running", "firebrick1", "solid", str + len, maxlen > len ? maxlen - len : 0);
    len += graphviz_legend_group(
        "Group Finished", "chartreuse4", "solid", str + len, maxlen > len ? maxlen - len : 0);
  }

  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "</TABLE>\r\n");
  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, ">];\r\n");
  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}\r\n");

  return len;
}

bool DebugInfo::graphviz_system(const ExecutionSystem *system, char *str, int maxlen)
{
  char strbuf[64];
  int len = 0;

  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "digraph compositorexecution {\r\n");
  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "ranksep=1.5\r\n");
  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "rankdir=LR\r\n");
  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "splines=false\r\n");

  std::map<NodeOperation *, std::vector<std::string>> op_groups;
  int index = 0;
  for (const ExecutionGroup *group : system->groups_) {
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "// GROUP: %d\r\n", index);
    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "subgraph cluster_%d{\r\n", index);
    /* used as a check for executing group */
    if (group_states_[group] == EG_WAIT) {
      len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "style=dashed\r\n");
    }
    else if (group_states_[group] == EG_RUNNING) {
      len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "style=filled\r\n");
      len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "color=black\r\n");
      len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "fillcolor=firebrick1\r\n");
    }
    else if (group_states_[group] == EG_FINISHED) {
      len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "style=filled\r\n");
      len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "color=black\r\n");
      len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "fillcolor=chartreuse4\r\n");
    }

    for (NodeOperation *operation : group->operations_) {

      SNPRINTF(strbuf, "_%p", group);
      op_groups[operation].push_back(std::string(strbuf));

      len += graphviz_operation(
          system, operation, group, str + len, maxlen > len ? maxlen - len : 0);
    }

    len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}\r\n");
    index++;
  }

  /* operations not included in any group */
  for (NodeOperation *operation : system->operations_) {
    if (op_groups.find(operation) != op_groups.end()) {
      continue;
    }

    op_groups[operation].push_back(std::string(""));

    len += graphviz_operation(
        system, operation, nullptr, str + len, maxlen > len ? maxlen - len : 0);
  }

  for (NodeOperation *operation : system->operations_) {
    if (operation->get_flags().is_read_buffer_operation) {
      ReadBufferOperation *read = (ReadBufferOperation *)operation;
      WriteBufferOperation *write = read->get_memory_proxy()->get_write_buffer_operation();
      std::vector<std::string> &read_groups = op_groups[read];
      std::vector<std::string> &write_groups = op_groups[write];

      for (int k = 0; k < write_groups.size(); k++) {
        for (int l = 0; l < read_groups.size(); l++) {
          len += snprintf(str + len,
                          maxlen > len ? maxlen - len : 0,
                          "\"O_%p%s\" -> \"O_%p%s\" [style=dotted]\r\n",
                          write,
                          write_groups[k].c_str(),
                          read,
                          read_groups[l].c_str());
        }
      }
    }
  }

  for (NodeOperation *op : system->operations_) {
    for (NodeOperationInput &to : op->inputs_) {
      NodeOperationOutput *from = to.get_link();

      if (!from) {
        continue;
      }

      std::string color;
      switch (from->get_data_type()) {
        case DataType::Value:
          color = "gray";
          break;
        case DataType::Vector:
          color = "blue";
          break;
        case DataType::Color:
          color = "orange";
          break;
      }

      NodeOperation *to_op = &to.get_operation();
      NodeOperation *from_op = &from->get_operation();
      std::vector<std::string> &from_groups = op_groups[from_op];
      std::vector<std::string> &to_groups = op_groups[to_op];

      len += snprintf(str + len,
                      maxlen > len ? maxlen - len : 0,
                      "// CONNECTION: %p.%p -> %p.%p\r\n",
                      from_op,
                      from,
                      to_op,
                      &to);
      for (int k = 0; k < from_groups.size(); k++) {
        for (int l = 0; l < to_groups.size(); l++) {
          len += snprintf(str + len,
                          maxlen > len ? maxlen - len : 0,
                          R"("O_%p%s":"OUT_%p":e -> "O_%p%s":"IN_%p":w)",
                          from_op,
                          from_groups[k].c_str(),
                          from,
                          to_op,
                          to_groups[l].c_str(),
                          &to);
          len += snprintf(
              str + len, maxlen > len ? maxlen - len : 0, " [color=%s]", color.c_str());
          len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "\r\n");
        }
      }
    }
  }

  const bool has_execution_groups = system->get_context().get_execution_model() ==
                                        eExecutionModel::Tiled &&
                                    system->groups_.size() > 0;
  len += graphviz_legend(str + len, maxlen > len ? maxlen - len : 0, has_execution_groups);

  len += snprintf(str + len, maxlen > len ? maxlen - len : 0, "}\r\n");

  return (len < maxlen);
}

void DebugInfo::graphviz(const ExecutionSystem *system, StringRefNull name)
{
  if (!COM_EXPORT_GRAPHVIZ) {
    return;
  }
  const int max_textlength = 1000000;
  char *str = (char *)MEM_mallocN(max_textlength, __func__);
  if (graphviz_system(system, str, max_textlength - 1)) {
    char basename[FILE_MAX];
    char filepath[FILE_MAX];

    if (name.is_empty()) {
      SNPRINTF(basename, "compositor_%d.dot", file_index_);
    }
    else {
      STRNCPY(basename, (name + ".dot").c_str());
    }
    BLI_path_join(filepath, sizeof(filepath), BKE_tempdir_session(), basename);
    file_index_++;

    std::cout << "Writing compositor debug to: " << filepath << "\n";

    FILE *fp = BLI_fopen(filepath, "wb");
    fputs(str, fp);
    fclose(fp);
  }
  MEM_freeN(str);
}

static std::string get_operations_export_dir()
{
  return std::string(BKE_tempdir_session()) + "COM_operations" + SEP_STR;
}

void DebugInfo::export_operation(const NodeOperation *op, MemoryBuffer *render)
{
  const int width = render->get_width();
  const int height = render->get_height();
  const int num_channels = render->get_num_channels();

  ImBuf *ibuf = IMB_allocImBuf(width, height, 8 * num_channels, IB_rectfloat);
  MemoryBuffer mem_ibuf(ibuf->float_buffer.data, 4, width, height);
  mem_ibuf.copy_from(render, render->get_rect(), 0, num_channels, 0);

  const std::string file_name = operation_class_name(op) + "_" + std::to_string(op->get_id()) +
                                ".png";
  const std::string filepath = get_operations_export_dir() + file_name;
  BLI_file_ensure_parent_dir_exists(filepath.c_str());
  IMB_saveiff(ibuf, filepath.c_str(), ibuf->flags);
  IMB_freeImBuf(ibuf);
}

void DebugInfo::delete_operation_exports()
{
  const std::string dir = get_operations_export_dir();
  if (BLI_exists(dir.c_str())) {
    struct direntry *file_list;
    int file_list_num = BLI_filelist_dir_contents(dir.c_str(), &file_list);
    for (int i = 0; i < file_list_num; i++) {
      direntry *file = &file_list[i];
      const eFileAttributes file_attrs = BLI_file_attributes(file->path);
      if (file_attrs & FILE_ATTR_ANY_LINK) {
        continue;
      }

      if (BLI_is_file(file->path) && BLI_path_extension_check(file->path, ".png")) {
        BLI_delete(file->path, false, false);
      }
    }
    BLI_filelist_free(file_list, file_list_num);
  }
}

}  // namespace blender::compositor
