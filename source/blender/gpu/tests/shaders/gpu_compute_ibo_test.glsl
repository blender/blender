void main() {
  uint store_index = int(gl_GlobalInvocationID.x);
  out_indices[store_index] = store_index;
}