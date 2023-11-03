#include "BLI_vector.hh"
#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_path.hh"
#include "RNA_types.hh"

namespace blender::animrig {

Vector<float> ANIM_setting_get_rna_values(PointerRNA *ptr, PropertyRNA *prop)
{
  Vector<float> values;
  if (RNA_property_array_check(prop)) {
    const int length = RNA_property_array_length(ptr, prop);

    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN: {
        bool *tmp_bool = static_cast<bool *>(MEM_malloc_arrayN(length, sizeof(bool), __func__));
        RNA_property_boolean_get_array(ptr, prop, tmp_bool);
        for (int i = 0; i < length; i++) {
          values.append(float(tmp_bool[i]));
        }
        MEM_freeN(tmp_bool);
        break;
      }
      case PROP_INT: {
        int *tmp_int = static_cast<int *>(MEM_malloc_arrayN(length, sizeof(int), __func__));
        RNA_property_int_get_array(ptr, prop, tmp_int);
        for (int i = 0; i < length; i++) {
          values.append(float(tmp_int[i]));
        }
        MEM_freeN(tmp_int);
        break;
      }
      case PROP_FLOAT: {
        values.reinitialize(length);
        RNA_property_float_get_array(ptr, prop, &values[0]);
        break;
      }
      default:
        values.reinitialize(length);
        break;
    }
  }
  else {
    switch (RNA_property_type(prop)) {
      case PROP_BOOLEAN:
        values.append(float(RNA_property_boolean_get(ptr, prop)));
        break;
      case PROP_INT:
        values.append(float(RNA_property_int_get(ptr, prop)));
        break;
      case PROP_FLOAT:
        values.append(RNA_property_float_get(ptr, prop));
        break;
      case PROP_ENUM:
        values.append(float(RNA_property_enum_get(ptr, prop)));
        break;
      default:
        values.append(0.0f);
    }
  }

  return values;
}
}  // namespace blender::animrig
