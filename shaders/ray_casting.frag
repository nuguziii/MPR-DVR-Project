#version 330 core

uniform sampler2D slice_data;
uniform sampler3D volume_data;
uniform sampler3D border_data;
uniform sampler2D octree_data;
uniform int N_x;
uniform int N_y;
uniform int N_z;
uniform int octree_length;
uniform float slice_thickness;
uniform vec3 P_screen;
uniform vec3 v_width;
uniform vec3 v_height;
uniform vec3 v_normal;
uniform float window_level;
uniform float window_width;
uniform bool mode; // true = MIP, False = OTF
uniform bool skipping; // true = empty-space skipping, False = none
uniform bool border_visible;
uniform bool axial_visible;
uniform bool sagittal_visible;
uniform bool coronal_visible;

in vec3 vert_out;
out vec3 color;

float small_d, max_intensity, accumulated_opacity, slice_opacity;
vec3 accumulated_intensity, ray_start;
vec3 slice_color_list[3];
float slice_depth_list[3];
bool slice_visible_list[3];


bool is_penetrated(float x, float y, float z)
{
	int t = 4;
	if (t < x && x < N_x - t && t < y && y < N_y - t &&	t * slice_thickness  < z && z < (N_z - t) * slice_thickness)
		return true;

	return false;
}


void get_t_front_back(in float x_min, in float x_max, in float y_min, in float y_max, in float z_min, in float z_max, out float t0_x, out float t0_y, out float t0_z, out float t1_x, out float t1_y, out float t1_z)
{
	float a, b, c, temp;
	t0_x = -1.0 / 0.0;
	t0_y = -1.0 / 0.0;
	t0_z = -1.0 / 0.0;
	t1_x = 1.0 / 0.0;
	t1_y = 1.0 / 0.0;
	t1_z = 1.0 / 0.0;
	a = v_normal.x;
	b = v_normal.y;
	c = v_normal.z;

	if (a < 0) {
		temp = x_min;
		x_min = x_max;
		x_max = temp;
	}
	if (b < 0) {
		temp = y_min;
		y_min = y_max;
		y_max = temp;
	}
	if (c < 0) {
		temp = z_min;
		z_min = z_max;
		z_max = temp;
	}

	if (a != 0) {
		t0_x = (x_min - ray_start.x) / a;
		t1_x = (x_max - ray_start.x) / a;
	}
	if (b != 0) {
		t0_y = (y_min - ray_start.y) / b;
		t1_y = (y_max - ray_start.y) / b;
	}
	if (c != 0) {
		t0_z = (z_min - ray_start.z) / c;
		t1_z = (z_max - ray_start.z) / c;
	}
}


int get_first_child_idx(int idx, int depth)
{
	int i = 0, j = 1;
	int sib_idx = idx;
	int first_child_idx = idx;
	while (i < depth) {
		sib_idx -= j;
		j *= 8;
		i++;
	}

	first_child_idx += pow(8, depth) + 7 * sib_idx;
	return first_child_idx;
}


void sample_through(in float t0, in float t1)
{
	vec3 cur = P_screen + v_width * vert_out.x + v_height * vert_out.y + t0 * v_normal;
	float cur_t = t0;
	float cur_intensity, cur_opacity, dx, dy, dz, cur_shadowed;

	while (true) {

		// stop when reached end
		if (cur_t > t1)
			break;

		// sample from data & calculate shadow
		cur_intensity = texture(volume_data, vec3(cur.x / N_x, cur.y / N_y, cur.z / (N_z * slice_thickness))).x;
		cur_opacity = clamp((cur_intensity - window_level + window_width / 2) / window_width, 0.0f, 1.0f);
		dx = texture(volume_data, vec3(cur.x / N_x + small_d, cur.y / N_y, cur.z / (N_z * slice_thickness))).x - texture(volume_data, vec3(cur.x / N_x - small_d, cur.y / N_y, cur.z / (N_z * slice_thickness))).x;
		dy = texture(volume_data, vec3(cur.x / N_x, cur.y / N_y + small_d, cur.z / (N_z * slice_thickness))).x - texture(volume_data, vec3(cur.x / N_x, cur.y / N_y - small_d, cur.z / (N_z * slice_thickness))).x;
		dz = texture(volume_data, vec3(cur.x / N_x, cur.y / N_y, cur.z / (N_z * slice_thickness) + small_d)).x - texture(volume_data, vec3(cur.x / N_x, cur.y / N_y, cur.z / (N_z * slice_thickness) - small_d)).x;
		cur_shadowed = clamp(0.1f + 0.9f * abs(dot(normalize(vec3(dx, dy, dz) / (2 * small_d)), normalize(v_normal))), 0.0f, 1.0f);

		if (!mode) // OTF mode
		{
			// blend slice plane if passed through
			for (int i = 0; i < 3; i++) {
				if (slice_visible_list[i]) {
					if (slice_depth_list[i] <= cur_t) {
						accumulated_intensity += accumulated_opacity * slice_opacity * slice_color_list[i];
						accumulated_opacity *= 1.0f - slice_opacity;
						slice_visible_list[i] = false;
					}
				}
			}

			// accumulate intensity & opacity
			accumulated_intensity += accumulated_opacity * cur_opacity * cur_shadowed * vec3(1.0f, 1.0f, 1.0f);
			accumulated_opacity *= 1.0f - cur_opacity;

			// early ray termination
			if (accumulated_opacity < 0.001f)
				break;
		}
		else // MIP mode
		{
			max_intensity = max(max_intensity, cur_opacity);
			if (max_intensity >= 1.0f)
				break;
		}

		// go to next sampling point
		cur = cur + v_normal;
		cur_t += length(v_normal);
	}
}


void main()
{
	// set initial values
	ray_start = P_screen + v_width * vert_out.x + v_height * vert_out.y;
	small_d = 0.5f / max(max(N_x, N_y), N_z * slice_thickness);
	max_intensity = 0.0f;
	accumulated_opacity = 1.0f;
	accumulated_intensity = vec3(0.0f, 0.0f, 0.0f);
	slice_opacity = 0.5f;

	// add slice information to list, sort by depth
	vec3 slice_depth = texture(slice_data, vec2(vert_out.x / 895, vert_out.y / 511)).xyz * 32768;
	slice_depth_list[0] = slice_depth.z;
	slice_depth_list[1] = slice_depth.x;
	slice_depth_list[2] = slice_depth.y;
	slice_visible_list[0] = axial_visible && slice_depth.z > 1;
	slice_visible_list[1] = sagittal_visible && slice_depth.x > 1;
	slice_visible_list[2] = coronal_visible && slice_depth.y > 1;
	slice_color_list[0] = vec3(1.0f, 0.0f, 0.0f);
	slice_color_list[1] = vec3(0.0f, 1.0f, 0.0f);
	slice_color_list[2] = vec3(0.0f, 0.0f, 1.0f);

	for (int i = 0; i < 2; i++) {
		for (int j = i+1; j < 3; j++) {
			if (slice_depth_list[i] > slice_depth_list[j]) {
				float temp_f = slice_depth_list[i];
				slice_depth_list[i] = slice_depth_list[j];
				slice_depth_list[j] = temp_f;
				bool temp_b = slice_visible_list[i];
				slice_visible_list[i] = slice_visible_list[j];
				slice_visible_list[j] = temp_b;
				vec3 temp_v = slice_color_list[i];
				slice_color_list[i] = slice_color_list[j];
				slice_color_list[j] = temp_v;
			}
		}
	}

	float t_front, t_back, t0_x, t0_y, t0_z, t1_x, t1_y, t1_z, tm_x, tm_y, tm_z;
	get_t_front_back(1, N_x-1, 0, N_y-1, 0, slice_thickness * N_z - 1, t0_x, t0_y, t0_z, t1_x, t1_y, t1_z);
	tm_x = isinf(t0_x) ? 1.0 / 0.0 : (t0_x + t1_x) / 2;
	tm_y = isinf(t0_y) ? 1.0 / 0.0 : (t0_y + t1_y) / 2;
	tm_z = isinf(t0_z) ? 1.0 / 0.0 : (t0_z + t1_z) / 2;
	t_front = max(t0_x, max(t0_y, t0_z));
	t_back = min(t1_x, min(t1_y, t1_z));

	if (t_front < t_back)
		sample_through(t_front, t_back);

	// blend slice plane if not passed through yet
	for (int i = 0; i < 3; i++) {
		if (slice_visible_list[i]) {
			accumulated_intensity += accumulated_opacity * slice_opacity * slice_color_list[i];
			accumulated_opacity *= 1.0f - slice_opacity;
		}
	}

	// determine final color value
	if (!mode) { // OTF mode
		//if (border_type == 1)
		//	accumulated_intensity += accumulated_opacity * vec3(1.0f, 0.0f, 1.0f);
		//else if (border_type == 2)
		//	accumulated_intensity += accumulated_opacity * vec3(0.3f, 0.0f, 0.3f);

		color = accumulated_intensity;
	}
	else { // MIP mode
		//if (border_type == 1)
		//	color = vec3(1.0f, 0.0f, 1.0f);
		//else if (border_type == 2)
		//	color = vec3(0.3f, 0.3f, 0.0f);
		//else
		color = vec3(max_intensity, max_intensity, max_intensity);
	}
}
