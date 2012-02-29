/*
 * Copyright 2011, Blender Foundation.
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

	float aperturesize;
	uint apertureblades;
	float aperturerotation;
	float focaldistance;

	float2 shift;
	float2 offset;
	float zoom;

	float2 pixelaspect;

	enum { AUTO, HORIZONTAL, VERTICAL } sensor_fit;
	float sensor_width;
	float sensor_height;

	Transform matrix;
};

static void blender_camera_init(BlenderCamera *bcam)
{
	memset(bcam, 0, sizeof(BlenderCamera));

	bcam->type = CAMERA_PERSPECTIVE;
	bcam->zoom = 1.0f;
	bcam->pixelaspect = make_float2(1.0f, 1.0f);
	bcam->sensor_width = 32.0f;
	bcam->sensor_height = 18.0f;
	bcam->sensor_fit = BlenderCamera::AUTO;
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

static void blender_camera_from_object(BlenderCamera *bcam, BL::Object b_ob)
{
	BL::ID b_ob_data = b_ob.data();

	if(b_ob_data.is_a(&RNA_Camera)) {
		BL::Camera b_camera(b_ob_data);
		PointerRNA ccamera = RNA_pointer_get(&b_camera.ptr, "cycles");

		bcam->nearclip = b_camera.clip_start();
		bcam->farclip = b_camera.clip_end();

		bcam->type = (b_camera.type() == BL::Camera::type_ORTHO)? CAMERA_ORTHOGRAPHIC: CAMERA_PERSPECTIVE;
		if(bcam->type == CAMERA_PERSPECTIVE && b_camera.use_panorama())
			bcam->type = CAMERA_ENVIRONMENT;
		bcam->ortho_scale = b_camera.ortho_scale();

		bcam->lens = b_camera.lens();
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

static void blender_camera_sync(Camera *cam, BlenderCamera *bcam, int width, int height)
{
	/* copy camera to compare later */
	Camera prevcam = *cam;

	/* dimensions */
	float xratio = width*bcam->pixelaspect.x;
	float yratio = height*bcam->pixelaspect.y;

	/* compute x/y aspect and ratio */
	float aspectratio, xaspect, yaspect;

	/* sensor fitting */
	bool horizontal_fit;
	float sensor_size;

	if(bcam->sensor_fit == BlenderCamera::AUTO) {
		horizontal_fit = (xratio > yratio);
		sensor_size = bcam->sensor_width;
	}
	else if(bcam->sensor_fit == BlenderCamera::HORIZONTAL) {
		horizontal_fit = true;
		sensor_size = bcam->sensor_width;
	}
	else {
		horizontal_fit = false;
		sensor_size = bcam->sensor_height;
	}

	if(horizontal_fit) {
		aspectratio= xratio/yratio;
		xaspect= aspectratio;
		yaspect= 1.0f;
	}
	else {
		aspectratio= yratio/xratio;
		xaspect= 1.0f;
		yaspect= aspectratio;
	}

	/* modify aspect for orthographic scale */
	if(bcam->type == CAMERA_ORTHOGRAPHIC) {
		xaspect = xaspect*bcam->ortho_scale/(aspectratio*2.0f);
		yaspect = yaspect*bcam->ortho_scale/(aspectratio*2.0f);
		aspectratio = bcam->ortho_scale/2.0f;
	}

	if(bcam->type == CAMERA_ENVIRONMENT) {
		/* set viewplane */
		cam->left = 0.0f;
		cam->right = 1.0f;
		cam->bottom = 0.0f;
		cam->top = 1.0f;
	}
	else {
		/* set viewplane */
		cam->left = -xaspect;
		cam->right = xaspect;
		cam->bottom = -yaspect;
		cam->top = yaspect;

		/* zoom for 3d camera view */
		cam->left *= bcam->zoom;
		cam->right *= bcam->zoom;
		cam->bottom *= bcam->zoom;
		cam->top *= bcam->zoom;

		/* modify viewplane with camera shift and 3d camera view offset */
		float dx = 2.0f*(aspectratio*bcam->shift.x + bcam->offset.x*xaspect*2.0f);
		float dy = 2.0f*(aspectratio*bcam->shift.y + bcam->offset.y*yaspect*2.0f);

		cam->left += dx;
		cam->right += dx;
		cam->bottom += dy;
		cam->top += dy;
	}

	/* clipping distances */
	cam->nearclip = bcam->nearclip;
	cam->farclip = bcam->farclip;

	/* type */
	cam->type = bcam->type;

	/* perspective */
	cam->fov = 2.0f*atan((0.5f*sensor_size)/bcam->lens/aspectratio);
	cam->focaldistance = bcam->focaldistance;
	cam->aperturesize = bcam->aperturesize;
	cam->blades = bcam->apertureblades;
	cam->bladesrotation = bcam->aperturerotation;

	/* transform */
	cam->matrix = bcam->matrix;

	if(bcam->type == CAMERA_ENVIRONMENT) {
		/* make it so environment camera needs to be pointed in the direction
		   of the positive x-axis to match an environment texture, this way
		   it is looking at the center of the texture */
		cam->matrix = cam->matrix *
			make_transform( 0.0f, -1.0f, 0.0f, 0.0f,
			                0.0f,  0.0f, 1.0f, 0.0f,
			               -1.0f,  0.0f, 0.0f, 0.0f,
			                0.0f,  0.0f, 0.0f, 1.0f);
	}
	else {
		/* note the blender camera points along the negative z-axis */
		cam->matrix = cam->matrix * transform_scale(1.0f, 1.0f, -1.0f);
	}

	cam->matrix = transform_clear_scale(cam->matrix);

	/* set update flag */
	if(cam->modified(prevcam))
		cam->tag_update();
}

/* Sync Render Camera */

void BlenderSync::sync_camera(int width, int height)
{
	BlenderCamera bcam;
	blender_camera_init(&bcam);

	/* pixel aspect */
	BL::RenderSettings r = b_scene.render();

	bcam.pixelaspect.x = r.pixel_aspect_x();
	bcam.pixelaspect.y = r.pixel_aspect_y();

	/* camera object */
	BL::Object b_ob = b_scene.camera();

	if(b_ob) {
		blender_camera_from_object(&bcam, b_ob);
		bcam.matrix = get_transform(b_ob.matrix_world());
	}

	/* sync */
	Camera *cam = scene->camera;
	blender_camera_sync(cam, &bcam, width, height);
}

/* Sync 3D View Camera */

void BlenderSync::sync_view(BL::SpaceView3D b_v3d, BL::RegionView3D b_rv3d, int width, int height)
{
	BlenderCamera bcam;
	blender_camera_init(&bcam);

	/* 3d view parameters */
	bcam.nearclip = b_v3d.clip_start();
	bcam.farclip = b_v3d.clip_end();
	bcam.lens = b_v3d.lens();

	if(b_rv3d.view_perspective() == BL::RegionView3D::view_perspective_CAMERA) {
		/* camera view */
		BL::Object b_ob = b_scene.camera();

		if(b_ob) {
			blender_camera_from_object(&bcam, b_ob);

			/* magic zoom formula */
			bcam.zoom = (float)b_rv3d.view_camera_zoom();
			bcam.zoom = (1.41421f + bcam.zoom/50.0f);
			bcam.zoom *= bcam.zoom;
			bcam.zoom = 2.0f/bcam.zoom;

			/* offset */
			bcam.offset = get_float2(b_rv3d.view_camera_offset());
		}
	}
	else if(b_rv3d.view_perspective() == BL::RegionView3D::view_perspective_ORTHO) {
		/* orthographic view */
		bcam.farclip *= 0.5;
		bcam.nearclip = -bcam.farclip;

		bcam.type = CAMERA_ORTHOGRAPHIC;
		bcam.ortho_scale = b_rv3d.view_distance();
	}

	bcam.zoom *= 2.0f;

	/* 3d view transform */
	bcam.matrix = transform_inverse(get_transform(b_rv3d.view_matrix()));

	/* sync */
	blender_camera_sync(scene->camera, &bcam, width, height);
}

BufferParams BlenderSync::get_buffer_params(BL::Scene b_scene, BL::RegionView3D b_rv3d, int width, int height)
{
	BufferParams params;

	params.full_width = width;
	params.full_height = height;

	/* border render */
	BL::RenderSettings r = b_scene.render();

	if(!b_rv3d && r.use_border()) {
		params.full_x = r.border_min_x()*width;
		params.full_y = r.border_min_y()*height;
		params.width = (int)(r.border_max_x()*width) - params.full_x;
		params.height = (int)(r.border_max_y()*height) - params.full_y;
	}
	else {
		params.width = width;
		params.height = height;
	}

	return params;
}

CCL_NAMESPACE_END

