/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#include "camera.h"
#include "scene.h"

#include "blender_sync.h"
#include "blender_util.h"

CCL_NAMESPACE_BEGIN

/* Blender Camera Intermediate: we first convert both the offline and 3d view
 * render camera to this, and from there convert to our native camera format. */

struct BlenderCamera {
	float nearclip;
	float farclip;

	CameraType type;
	float ortho_scale;

	float lens;
	float shuttertime;

	float aperturesize;
	uint apertureblades;
	float aperturerotation;
	float focaldistance;

	float2 shift;
	float2 offset;
	float zoom;

	float2 pixelaspect;

	PanoramaType panorama_type;
	float fisheye_fov;
	float fisheye_lens;

	enum { AUTO, HORIZONTAL, VERTICAL } sensor_fit;
	float sensor_width;
	float sensor_height;

	int full_width;
	int full_height;

	BoundBox2D border;
	BoundBox2D pano_viewplane;

	Transform matrix;
};

static void blender_camera_init(BlenderCamera *bcam, BL::RenderSettings b_render, BL::Scene b_scene)
{
	memset(bcam, 0, sizeof(BlenderCamera));

	bcam->type = CAMERA_PERSPECTIVE;
	bcam->zoom = 1.0f;
	bcam->pixelaspect = make_float2(1.0f, 1.0f);
	bcam->sensor_width = 32.0f;
	bcam->sensor_height = 18.0f;
	bcam->sensor_fit = BlenderCamera::AUTO;
	bcam->shuttertime = 1.0f;
	bcam->border.right = 1.0f;
	bcam->border.top = 1.0f;
	bcam->pano_viewplane.right = 1.0f;
	bcam->pano_viewplane.top = 1.0f;

	/* render resolution */
	bcam->full_width = render_resolution_x(b_render);
	bcam->full_height = render_resolution_y(b_render);
}

static float blender_camera_focal_distance(BL::Object b_ob, BL::Camera b_camera)
{
	BL::Object b_dof_object = b_camera.dof_object();

	if(!b_dof_object)
		return b_camera.dof_distance();
	
	/* for dof object, return distance along camera Z direction */
	Transform obmat = transform_clear_scale(get_transform(b_ob.matrix_world()));
	Transform dofmat = get_transform(b_dof_object.matrix_world());
	Transform mat = transform_inverse(obmat) * dofmat;

	return fabsf(transform_get_column(&mat, 3).z);
}

static void blender_camera_from_object(BlenderCamera *bcam, BL::Object b_ob, bool skip_panorama = false)
{
	BL::ID b_ob_data = b_ob.data();

	if(b_ob_data.is_a(&RNA_Camera)) {
		BL::Camera b_camera(b_ob_data);
		PointerRNA ccamera = RNA_pointer_get(&b_camera.ptr, "cycles");

		bcam->nearclip = b_camera.clip_start();
		bcam->farclip = b_camera.clip_end();

		switch(b_camera.type())
		{
			case BL::Camera::type_ORTHO:
				bcam->type = CAMERA_ORTHOGRAPHIC;
				break;
			case BL::Camera::type_PANO:
				if(!skip_panorama)
					bcam->type = CAMERA_PANORAMA;
				else
					bcam->type = CAMERA_PERSPECTIVE;
				break;
			case BL::Camera::type_PERSP:
			default:
				bcam->type = CAMERA_PERSPECTIVE;
				break;
		}	

		switch(RNA_enum_get(&ccamera, "panorama_type"))
		{
			case 1:
				bcam->panorama_type = PANORAMA_FISHEYE_EQUIDISTANT;
				break;
			case 2:
				bcam->panorama_type = PANORAMA_FISHEYE_EQUISOLID;
				break;
			case 0:
			default:
				bcam->panorama_type = PANORAMA_EQUIRECTANGULAR;
				break;
		}	

		bcam->fisheye_fov = RNA_float_get(&ccamera, "fisheye_fov");
		bcam->fisheye_lens = RNA_float_get(&ccamera, "fisheye_lens");

		bcam->ortho_scale = b_camera.ortho_scale();

		bcam->lens = b_camera.lens();

		/* allow f/stop number to change aperture_size but still
		 * give manual control over aperture radius */
		int aperture_type = RNA_enum_get(&ccamera, "aperture_type");

		if(aperture_type == 1) {
			float fstop = RNA_float_get(&ccamera, "aperture_fstop");
			fstop = max(fstop, 1e-5f);

			if(bcam->type == CAMERA_ORTHOGRAPHIC)
				bcam->aperturesize = 1.0f/(2.0f*fstop);
			else
				bcam->aperturesize = (bcam->lens*1e-3f)/(2.0f*fstop);
		}
		else
			bcam->aperturesize = RNA_float_get(&ccamera, "aperture_size");

		bcam->apertureblades = RNA_int_get(&ccamera, "aperture_blades");
		bcam->aperturerotation = RNA_float_get(&ccamera, "aperture_rotation");
		bcam->focaldistance = blender_camera_focal_distance(b_ob, b_camera);

		bcam->shift.x = b_camera.shift_x();
		bcam->shift.y = b_camera.shift_y();

		bcam->sensor_width = b_camera.sensor_width();
		bcam->sensor_height = b_camera.sensor_height();

		if(b_camera.sensor_fit() == BL::Camera::sensor_fit_AUTO)
			bcam->sensor_fit = BlenderCamera::AUTO;
		else if(b_camera.sensor_fit() == BL::Camera::sensor_fit_HORIZONTAL)
			bcam->sensor_fit = BlenderCamera::HORIZONTAL;
		else
			bcam->sensor_fit = BlenderCamera::VERTICAL;
	}
	else {
		/* from lamp not implemented yet */
	}
}

static Transform blender_camera_matrix(const Transform& tfm, CameraType type)
{
	Transform result;

	if(type == CAMERA_PANORAMA) {
		/* make it so environment camera needs to be pointed in the direction
		 * of the positive x-axis to match an environment texture, this way
		 * it is looking at the center of the texture */
		result = tfm *
			make_transform( 0.0f, -1.0f, 0.0f, 0.0f,
			                0.0f,  0.0f, 1.0f, 0.0f,
			               -1.0f,  0.0f, 0.0f, 0.0f,
			                0.0f,  0.0f, 0.0f, 1.0f);
	}
	else {
		/* note the blender camera points along the negative z-axis */
		result = tfm * transform_scale(1.0f, 1.0f, -1.0f);
	}

	return transform_clear_scale(result);
}

static void blender_camera_viewplane(BlenderCamera *bcam, int width, int height,
	BoundBox2D *viewplane, float *aspectratio, float *sensor_size)
{
	/* dimensions */
	float xratio = width*bcam->pixelaspect.x;
	float yratio = height*bcam->pixelaspect.y;

	/* compute x/y aspect and ratio */
	float xaspect, yaspect;
	bool horizontal_fit;

	/* sensor fitting */
	if(bcam->sensor_fit == BlenderCamera::AUTO) {
		horizontal_fit = (xratio > yratio);
		*sensor_size = bcam->sensor_width;
	}
	else if(bcam->sensor_fit == BlenderCamera::HORIZONTAL) {
		horizontal_fit = true;
		*sensor_size = bcam->sensor_width;
	}
	else {
		horizontal_fit = false;
		*sensor_size = bcam->sensor_height;
	}

	if(horizontal_fit) {
		*aspectratio = xratio/yratio;
		xaspect = *aspectratio;
		yaspect = 1.0f;
	}
	else {
		*aspectratio = yratio/xratio;
		xaspect = 1.0f;
		yaspect = *aspectratio;
	}

	/* modify aspect for orthographic scale */
	if(bcam->type == CAMERA_ORTHOGRAPHIC) {
		xaspect = xaspect*bcam->ortho_scale/(*aspectratio*2.0f);
		yaspect = yaspect*bcam->ortho_scale/(*aspectratio*2.0f);
		*aspectratio = bcam->ortho_scale/2.0f;
	}

	if(bcam->type == CAMERA_PANORAMA) {
		/* set viewplane */
		*viewplane = bcam->pano_viewplane;
	}
	else {
		/* set viewplane */
		viewplane->left = -xaspect;
		viewplane->right = xaspect;
		viewplane->bottom = -yaspect;
		viewplane->top = yaspect;

		/* zoom for 3d camera view */
		*viewplane = (*viewplane) * bcam->zoom;

		/* modify viewplane with camera shift and 3d camera view offset */
		float dx = 2.0f*(*aspectratio*bcam->shift.x + bcam->offset.x*xaspect*2.0f);
		float dy = 2.0f*(*aspectratio*bcam->shift.y + bcam->offset.y*yaspect*2.0f);

		viewplane->left += dx;
		viewplane->right += dx;
		viewplane->bottom += dy;
		viewplane->top += dy;
	}
}

static void blender_camera_sync(Camera *cam, BlenderCamera *bcam, int width, int height)
{
	/* copy camera to compare later */
	Camera prevcam = *cam;
	float aspectratio, sensor_size;

	/* viewplane */
	blender_camera_viewplane(bcam, width, height,
		&cam->viewplane, &aspectratio, &sensor_size);

	/* panorama sensor */
	if (bcam->type == CAMERA_PANORAMA && bcam->panorama_type == PANORAMA_FISHEYE_EQUISOLID) {
		float fit_xratio = bcam->full_width*bcam->pixelaspect.x;
		float fit_yratio = bcam->full_height*bcam->pixelaspect.y;
		bool horizontal_fit;
		float sensor_size;

		if(bcam->sensor_fit == BlenderCamera::AUTO) {
			horizontal_fit = (fit_xratio > fit_yratio);
			sensor_size = bcam->sensor_width;
		}
		else if(bcam->sensor_fit == BlenderCamera::HORIZONTAL) {
			horizontal_fit = true;
			sensor_size = bcam->sensor_width;
		}
		else { /* vertical */
			horizontal_fit = false;
			sensor_size = bcam->sensor_height;
		}

		if(horizontal_fit) {
			cam->sensorwidth = sensor_size;
			cam->sensorheight = sensor_size * fit_yratio / fit_xratio;
		}
		else {
			cam->sensorwidth = sensor_size * fit_xratio / fit_yratio;
			cam->sensorheight = sensor_size;
		}
	}

	/* clipping distances */
	cam->nearclip = bcam->nearclip;
	cam->farclip = bcam->farclip;

	/* type */
	cam->type = bcam->type;

	/* panorama */
	cam->panorama_type = bcam->panorama_type;
	cam->fisheye_fov = bcam->fisheye_fov;
	cam->fisheye_lens = bcam->fisheye_lens;

	/* perspective */
	cam->fov = 2.0f * atanf((0.5f * sensor_size) / bcam->lens / aspectratio);
	cam->focaldistance = bcam->focaldistance;
	cam->aperturesize = bcam->aperturesize;
	cam->blades = bcam->apertureblades;
	cam->bladesrotation = bcam->aperturerotation;

	/* transform */
	cam->matrix = blender_camera_matrix(bcam->matrix, bcam->type);
	cam->motion.pre = cam->matrix;
	cam->motion.post = cam->matrix;
	cam->use_motion = false;
	cam->shuttertime = bcam->shuttertime;

	/* border */
	cam->border = bcam->border;

	/* set update flag */
	if(cam->modified(prevcam))
		cam->tag_update();
}

/* Sync Render Camera */

void BlenderSync::sync_camera(BL::RenderSettings b_render, BL::Object b_override, int width, int height)
{
	BlenderCamera bcam;
	blender_camera_init(&bcam, b_render, b_scene);

	/* pixel aspect */
	bcam.pixelaspect.x = b_render.pixel_aspect_x();
	bcam.pixelaspect.y = b_render.pixel_aspect_y();
	bcam.shuttertime = b_render.motion_blur_shutter();

	/* border */
	if(b_render.use_border()) {
		bcam.border.left = b_render.border_min_x();
		bcam.border.right = b_render.border_max_x();
		bcam.border.bottom = b_render.border_min_y();
		bcam.border.top = b_render.border_max_y();
	}

	/* camera object */
	BL::Object b_ob = b_scene.camera();

	if(b_override)
		b_ob = b_override;

	if(b_ob) {
		blender_camera_from_object(&bcam, b_ob);
		bcam.matrix = get_transform(b_ob.matrix_world());
	}

	/* sync */
	Camera *cam = scene->camera;
	blender_camera_sync(cam, &bcam, width, height);
}

void BlenderSync::sync_camera_motion(BL::Object b_ob, int motion)
{
	Camera *cam = scene->camera;

	Transform tfm = get_transform(b_ob.matrix_world());
	tfm = blender_camera_matrix(tfm, cam->type);

	if(tfm != cam->matrix) {
		if(motion == -1)
			cam->motion.pre = tfm;
		else
			cam->motion.post = tfm;

		cam->use_motion = true;
	}
}

/* Sync 3D View Camera */

static void blender_camera_view_subset(BL::RenderSettings b_render, BL::Scene b_scene, BL::Object b_ob, BL::SpaceView3D b_v3d,
	BL::RegionView3D b_rv3d, int width, int height, BoundBox2D *view_box, BoundBox2D *cam_box);

static void blender_camera_from_view(BlenderCamera *bcam, BL::Scene b_scene, BL::SpaceView3D b_v3d, BL::RegionView3D b_rv3d, int width, int height, bool skip_panorama = false)
{
	/* 3d view parameters */
	bcam->nearclip = b_v3d.clip_start();
	bcam->farclip = b_v3d.clip_end();
	bcam->lens = b_v3d.lens();
	bcam->shuttertime = b_scene.render().motion_blur_shutter();

	if(b_rv3d.view_perspective() == BL::RegionView3D::view_perspective_CAMERA) {
		/* camera view */
		BL::Object b_ob = (b_v3d.lock_camera_and_layers())? b_scene.camera(): b_v3d.camera();

		if(b_ob) {
			blender_camera_from_object(bcam, b_ob, skip_panorama);

			if(!skip_panorama && bcam->type == CAMERA_PANORAMA) {
				/* in panorama camera view, we map viewplane to camera border */
				BoundBox2D view_box, cam_box;

				blender_camera_view_subset(b_scene.render(), b_scene, b_ob, b_v3d, b_rv3d, width, height,
					&view_box, &cam_box);

				bcam->pano_viewplane = view_box.make_relative_to(cam_box);
			}
			else {
				/* magic zoom formula */
				bcam->zoom = (float)b_rv3d.view_camera_zoom();
				bcam->zoom = (1.41421f + bcam->zoom/50.0f);
				bcam->zoom *= bcam->zoom;
				bcam->zoom = 2.0f/bcam->zoom;

				/* offset */
				bcam->offset = get_float2(b_rv3d.view_camera_offset());
			}
		}
	}
	else if(b_rv3d.view_perspective() == BL::RegionView3D::view_perspective_ORTHO) {
		/* orthographic view */
		bcam->farclip *= 0.5f;
		bcam->nearclip = -bcam->farclip;

		float sensor_size;
		if(bcam->sensor_fit == BlenderCamera::VERTICAL)
			sensor_size = bcam->sensor_height;
		else
			sensor_size = bcam->sensor_width;

		bcam->type = CAMERA_ORTHOGRAPHIC;
		bcam->ortho_scale = b_rv3d.view_distance() * sensor_size / b_v3d.lens();
	}

	bcam->zoom *= 2.0f;

	/* 3d view transform */
	bcam->matrix = transform_inverse(get_transform(b_rv3d.view_matrix()));
}

static void blender_camera_view_subset(BL::RenderSettings b_render, BL::Scene b_scene, BL::Object b_ob, BL::SpaceView3D b_v3d,
	BL::RegionView3D b_rv3d, int width, int height, BoundBox2D *view_box, BoundBox2D *cam_box)
{
	BoundBox2D cam, view;
	float view_aspect, cam_aspect, sensor_size;

	/* get viewport viewplane */
	BlenderCamera view_bcam;
	blender_camera_init(&view_bcam, b_render, b_scene);
	blender_camera_from_view(&view_bcam, b_scene, b_v3d, b_rv3d, width, height, true);

	blender_camera_viewplane(&view_bcam, width, height,
		&view, &view_aspect, &sensor_size);

	/* get camera viewplane */
	BlenderCamera cam_bcam;
	blender_camera_init(&cam_bcam, b_render, b_scene);
	blender_camera_from_object(&cam_bcam, b_ob, true);

	blender_camera_viewplane(&cam_bcam, cam_bcam.full_width, cam_bcam.full_height,
		&cam, &cam_aspect, &sensor_size);
	
	/* return */
	*view_box = view * (1.0f/view_aspect);
	*cam_box = cam * (1.0f/cam_aspect);
}

static void blender_camera_border(BlenderCamera *bcam, BL::RenderSettings b_render, BL::Scene b_scene, BL::SpaceView3D b_v3d,
	BL::RegionView3D b_rv3d, int width, int height)
{
	bool is_camera_view;

	/* camera view? */
	is_camera_view = b_rv3d.view_perspective() == BL::RegionView3D::view_perspective_CAMERA;

	if(!is_camera_view) {
		/* for non-camera view check whether render border is enabled for viewport
		 * and if so use border from 3d viewport
		 * assume viewport has got correctly clamped border already
		 */
		if(b_v3d.use_render_border()) {
			bcam->border.left = b_v3d.render_border_min_x();
			bcam->border.right = b_v3d.render_border_max_x();
			bcam->border.bottom = b_v3d.render_border_min_y();
			bcam->border.top = b_v3d.render_border_max_y();

			return;
		}
	}
	else if(!b_render.use_border())
		return;

	BL::Object b_ob = (b_v3d.lock_camera_and_layers())? b_scene.camera(): b_v3d.camera();

	if(!b_ob)
		return;

	bcam->border.left = b_render.border_min_x();
	bcam->border.right = b_render.border_max_x();
	bcam->border.bottom = b_render.border_min_y();
	bcam->border.top = b_render.border_max_y();

	/* determine camera viewport subset */
	BoundBox2D view_box, cam_box;

	blender_camera_view_subset(b_render, b_scene, b_ob, b_v3d, b_rv3d, width, height,
		&view_box, &cam_box);

	/* determine viewport subset matching camera border */
	cam_box = cam_box.make_relative_to(view_box);
	bcam->border = cam_box.subset(bcam->border).clamp();
}

void BlenderSync::sync_view(BL::SpaceView3D b_v3d, BL::RegionView3D b_rv3d, int width, int height)
{
	BlenderCamera bcam;
	blender_camera_init(&bcam, b_scene.render(), b_scene);
	blender_camera_from_view(&bcam, b_scene, b_v3d, b_rv3d, width, height);
	blender_camera_border(&bcam, b_scene.render(), b_scene, b_v3d, b_rv3d, width, height);

	blender_camera_sync(scene->camera, &bcam, width, height);
}

BufferParams BlenderSync::get_buffer_params(BL::RenderSettings b_render, BL::Scene b_scene, BL::SpaceView3D b_v3d, BL::RegionView3D b_rv3d, Camera *cam, int width, int height)
{
	BufferParams params;
	bool use_border = false;

	params.full_width = width;
	params.full_height = height;

	if(b_v3d && b_rv3d && b_rv3d.view_perspective() != BL::RegionView3D::view_perspective_CAMERA)
		use_border = b_v3d.use_render_border();
	else
		use_border = b_render.use_border();

	if(use_border) {
		/* border render */
		params.full_x = cam->border.left*width;
		params.full_y = cam->border.bottom*height;
		params.width = (int)(cam->border.right*width) - params.full_x;
		params.height = (int)(cam->border.top*height) - params.full_y;

		/* survive in case border goes out of view or becomes too small */
		params.width = max(params.width, 1);
		params.height = max(params.height, 1);
	}
	else {
		params.width = width;
		params.height = height;
	}

	return params;
}

CCL_NAMESPACE_END

