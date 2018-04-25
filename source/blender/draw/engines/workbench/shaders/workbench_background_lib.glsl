vec3 background_color(WorldData world_data, float y) {
	return mix(world_data.background_color_low, world_data.background_color_high, y).xyz + bayer_dither_noise();
}
