#include "slicewidget.h"
#include "window.h"
#include "data_cube.h"
#include <QMouseEvent>
#include <QCoreApplication>
#include <math.h>
#include <sstream>
#include <string.h>
#include <QLabel>
#include <QPainter>
#include <QLineF>

using namespace std;
#define PI 3.141592


SliceWidget::SliceWidget(int t, int s)
{
	slice_type = t;
	is_valid = 0;
	is_line_visible = 1;
	slice_size = s;

	QString blank_path = "images/blank_image.png";
	QString loading_path = "images/loading_image.png";

	QImage *img;
	img = new QImage();
	blank_img = new QPixmap();
	loading_img = new QPixmap();
	img_buffer = new QPixmap();

	img->load(blank_path);
	*blank_img = QPixmap::fromImage(*img);

	img->load(loading_path);
	*loading_img = QPixmap::fromImage(*img);

	//slice = NULL;
	set_pixmap();
}

void SliceWidget::set_data(DataCube *d)
{
	is_valid = 1;
	data_cube = d;
	tie(pixel_num, rescale_slope, rescale_intercept, pixel_min, pixel_max) = data_cube->get_pixel_info();
	slice_data = (int*)malloc(pixel_num * pixel_num * sizeof(int) * 7 / 4);
	windowed_slice = (unsigned char*)malloc(pixel_num * pixel_num * sizeof(unsigned char) * 7 / 4);
	window_level = 50;
	window_width = 350;
	window_changed = 0;

	get_slice();
}

void SliceWidget::get_slice()
{
	// get raw slice from data cube
	//free(slice);
	//slice = data_cube->get_slice(slice_type); // slice with actual pixel values(not 0~255)
	//for (int i = 0; i < pixel_num * pixel_num * 7 / 4; i++)
	//	slice_copy[i] = slice[i];

	data_cube->get_slice(slice_type, slice_data); // slice with actual pixel values(not 0~255)
	apply_windowing();
}

void SliceWidget::init_windowing(int apply)
{
	window_level = 50;
	window_width = 350;

	if (apply == 1)
		apply_windowing();
}

void SliceWidget::set_windowing(int wl, int ww)
{
	window_level = wl;
	window_width = ww;
	apply_windowing();
}

void SliceWidget::apply_windowing()
{
	// rescale pixel value to HU unit, apply windowing, then convert to 0~255
	for (int i = 0; i < pixel_num * pixel_num * 7 / 4; i++) {
		float temp = (float)(slice_data[i]);

		if (temp < pixel_min) {
			windowed_slice[i] = 255;
		}
		else {
			temp = rescale_slope * temp + rescale_intercept;
			temp -= window_level - window_width / 2;

			if (temp < 0) {
				temp = 0;
			}
			else if (temp > window_width) {
				temp = window_width;
			}
			temp = temp * 255 / window_width;
			windowed_slice[i] = (unsigned char)temp;
		}
	}

	//free(slice);
	QString msg = "WL: " + QString::number(window_level) + "\nWW: " + QString::number(window_width);
	emit windowing_info_sig(msg);
	set_pixmap();
}

void SliceWidget::toggle_slice_line()
{
	is_line_visible = (is_line_visible + 1) % 2;
	set_pixmap();
}

void SliceWidget::toggle_border_line()
{
	data_cube->toggle_border_line(slice_type);
	get_slice();
}

void SliceWidget::set_pixmap()
{

	if (is_valid == 0) {
		// set blank img when not valid
		//img_buffer = blank_img;
		//*img_buffer = img_buffer->scaledToHeight(slice_size);
		//*img_buffer = img_buffer->scaled(slice_size * 7 / 4, slice_size);

		this->setPixmap(blank_img->scaled(slice_size * 7 / 4, slice_size));
		return;
	}

	// draw line, then set pixmap of this widget
	//QImage *img;
	//img = new QImage(windowed_slice, pixel_num * 7 / 4, pixel_num, QImage::Format_Indexed8);

	//QImage img2(windowed_slice, pixel_num * 7 / 4, pixel_num, QImage::Format_Indexed8);

	//QPixmap *img_buffer;
	//img_buffer = new QPixmap();
	*img_buffer = QPixmap::fromImage(QImage(windowed_slice, pixel_num * 7 / 4, pixel_num, QImage::Format_Indexed8));
	//*img_buffer = img_buffer->scaledToHeight(slice_size);
	*img_buffer = img_buffer->scaled(slice_size * 7 / 4, slice_size);

	// draw line
	if (is_line_visible == 1) {
		QPainter *painter = new QPainter(img_buffer);
		QLineF angleline;
		float line_angle_deg;

		tie(line_x, line_y, line_angle_rad) = data_cube->get_line_info(slice_type);
		line_angle_deg = line_angle_rad * -180 / PI;
		line_x_scaled = line_x * slice_size / pixel_num;
		line_y_scaled = line_y * slice_size / pixel_num;
		angleline.setP1(QPointF(line_x_scaled, line_y_scaled));
		angleline.setLength(4 * slice_size);

		// red: z-slice, green: x-slice, blue: y-slice
		if (slice_type == 0) { // z slice
			if (is_line_mouseover_v == 1 || line_clicked_v == 1)
				painter->setPen(QPen(Qt::green, 5));
			else
				painter->setPen(QPen(Qt::green, 2));
		}
		else if (slice_type == 1) { // x slice
			if (is_line_mouseover_v == 1 || line_clicked_v == 1)
				painter->setPen(QPen(Qt::red, 5));
			else
				painter->setPen(QPen(Qt::red, 2));
		}
		else { // y slice
			if (is_line_mouseover_v == 1 || line_clicked_v == 1)
				painter->setPen(QPen(Qt::green, 5));
			else
				painter->setPen(QPen(Qt::green, 2));
		}

		angleline.setAngle(line_angle_deg + 90);
		painter->drawLine(angleline);
		angleline.setAngle(line_angle_deg + 270);
		painter->drawLine(angleline);

		if (slice_type == 0) { // z slice
			if (is_line_mouseover_h == 1 || line_clicked_h == 1)
				painter->setPen(QPen(Qt::blue, 5));
			else
				painter->setPen(QPen(Qt::blue, 2));
		}
		else if (slice_type == 1) { // x slice
			if (is_line_mouseover_h == 1 || line_clicked_h == 1)
				painter->setPen(QPen(Qt::blue, 5));
			else
				painter->setPen(QPen(Qt::blue, 2));
		}
		else { // y slice
			if (is_line_mouseover_h == 1 || line_clicked_h == 1)
				painter->setPen(QPen(Qt::red, 5));
			else
				painter->setPen(QPen(Qt::red, 2));
		}

		angleline.setAngle(line_angle_deg);
		painter->drawLine(angleline);
		angleline.setAngle(line_angle_deg + 180);
		painter->drawLine(angleline);
		painter->end();

		//free(painter);
	}

	// set pixmap
	this->setPixmap(*img_buffer);

	//free(img);
	//free(img_buffer);
}

void SliceWidget::emit_coord_sig(int is_inside, int mouse_x, int mouse_y)
{
	if (is_inside == 0) {
		emit coord_info_sig("");
		return;
	}

	int point_x, point_y, point_z, pixel_v, angle_deg;
	int m_x = mouse_x * pixel_num / slice_size;
	int m_y = mouse_y * pixel_num / slice_size;

	tie(point_x, point_y, point_z, pixel_v) = data_cube->get_coord(slice_type, m_x, m_y);
	pixel_v = rescale_slope * pixel_v + rescale_intercept;
	QString msg = "Coord: (" + QString::number(point_x) + ", " + QString::number(point_y) + ", " + QString::number(point_z) + ")\nIntensity(HU): " + QString::number(pixel_v);
	emit coord_info_sig(msg);
}

void SliceWidget::leaveEvent(QEvent *event)
{
	if (is_valid == 0)
		return;

	line_clicked_h = 0;
	line_clicked_v = 0;
	is_line_mouseover_h = 0;
	is_line_mouseover_v = 0;

	set_pixmap();
	emit emit_coord_sig(0, 0, 0);
}

void SliceWidget::mousePressEvent(QMouseEvent *event)
{
	if (is_valid == 0)
		return;

	mouse_last_x = event->x();
	mouse_last_y = event->y();
	mouse_last_a = get_mouse_angle(event->x(), event->y());
	line_clicked_h = is_line_mouseover_h;
	line_clicked_v = is_line_mouseover_v;
}

void SliceWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if (is_valid == 0)
		return;

	line_clicked_h = 0;
	line_clicked_v = 0;

	if (window_changed == 1) {
		emit windowing_changed_sig(window_level, window_width);
		window_changed = 0;
	}
}

void SliceWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (is_valid == 0)
		return;

	float dx = (float)(event->x() - line_x_scaled);
	float dy = (float)(event->y() - line_y_scaled);
	dx = dx * pixel_num / slice_size;
	dy = dy * pixel_num / slice_size;

	int moved = data_cube->move_center(slice_type, dx, dy);
	if (moved == 1) {
		get_slice();
		emit line_moved_sig(2);
		emit_coord_sig(1, event->x(), event->y());
	}
}

void SliceWidget::wheelEvent(QWheelEvent *event)
{
	if (is_valid == 0)
		return;

	QPoint wd = event->angleDelta();
	int wheel_delta = wd.y(); // only check if positive or negative
	int moved = 0;

	if (wheel_delta > 0)
		moved = data_cube->move_slice(slice_type, slice_type, 1);
	else
		moved = data_cube->move_slice(slice_type, slice_type, -1);

	if (moved == 0) // not moved
		return;

	get_slice();
	emit line_moved_sig(3);
	emit_coord_sig(1, event->x(), event->y());
}

void SliceWidget::mouseMoveEvent(QMouseEvent *event)
{
	// TODO: thickening the slice -> avg/max/min

	if (is_valid == 0)
		return;

	// send coord info
	int mouse_x = event->x();
	int mouse_y = event->y();
	if (mouse_x < 0)
		mouse_x = 0;
	if (mouse_y < 0)
		mouse_y = 0;
	if (mouse_x >= slice_size * 7 / 4)
		mouse_x = slice_size * 7 / 4 - 1;
	if (mouse_y >= slice_size)
		mouse_y = slice_size - 1;

	float mouse_angle_rad = get_mouse_angle(mouse_x, mouse_y);

	// get angle diff
	float angle_diff_h, angle_diff_v;
	angle_diff_h = fabs(line_angle_rad - mouse_angle_rad);
	if (angle_diff_h > PI)
		angle_diff_h = 2 * PI - angle_diff_h;
	if (angle_diff_h > PI / 2)
		angle_diff_h = PI - angle_diff_h;

	angle_diff_v = PI / 2 - angle_diff_h;

	// check if mouse is on the line
	int line_mouseover_v, line_mouseover_h;
	float r_mouse;
	line_mouseover_h = 0;
	line_mouseover_v = 0;
	r_mouse = sqrt(pow(line_x_scaled - mouse_x, 2) + pow(line_y_scaled - mouse_y, 2));
	if (angle_diff_h * r_mouse <= 4)
		line_mouseover_h = 1;
	if (angle_diff_v * r_mouse <= 4)
		line_mouseover_v = 1;

	// if mouseover changed, change line thickness
	if (line_mouseover_v != is_line_mouseover_v || line_mouseover_h != is_line_mouseover_h) {
		is_line_mouseover_v = line_mouseover_v;
		is_line_mouseover_h = line_mouseover_h;
		set_pixmap();
	}

	// mouse click event
	if (event->buttons() & Qt::LeftButton) // when left clicked
	{
		if (is_line_visible == 1 && (line_clicked_h + line_clicked_v) > 0) { // when clicked line
			float dx = (float)(mouse_x - mouse_last_x);
			float dy = (float)(mouse_y - mouse_last_y);
			dx = dx * pixel_num / slice_size;
			dy = dy * pixel_num / slice_size;
			int moved_v = 0;
			int moved_h = 0;

			if (line_clicked_v == 1) {
				float d = dx*sin(line_angle_rad + PI/2) - dy*cos(line_angle_rad + PI/2);
				if (slice_type == 0 || slice_type == 2) // from z,y slice, move x slice
					moved_v = data_cube->move_slice(slice_type, 1, d);
				else // from x slice, move z slice
					moved_v = data_cube->move_slice(slice_type, 0, d);
			}
			if (line_clicked_h == 1) {
				float d = dy*cos(line_angle_rad) - dx*sin(line_angle_rad);
				if (slice_type == 0 || slice_type == 1) // from z,x slice, move y slice
					moved_h = data_cube->move_slice(slice_type, 2, d);
				else // from y slice, move z slice
					moved_h = data_cube->move_slice(slice_type, 0, d);
			}

			if (moved_v == 1 && moved_h == 1)
				emit line_moved_sig(2);
			else if (moved_v == 1)
				emit line_moved_sig(0);
			else if (moved_h == 1)
				emit line_moved_sig(1);

			if (moved_h == 1 || moved_v == 1) {
				mouse_last_x = mouse_x;
				mouse_last_y = mouse_y;
				set_pixmap();
			}
		}
		else { // when clicked screen, not line = zooming
			int dy = mouse_y - mouse_last_y;
			int moved = data_cube->zoom_slice(slice_type, dy);

			if (moved == 1) {
				mouse_last_y = mouse_y;
				get_slice();

				emit zoom_panning_sig();
			}
		}
	}
	else if (event->buttons() & Qt::RightButton) { // when right clicked
		if (is_line_visible == 1 && (line_clicked_h + line_clicked_v) > 0) { // when clicked line = rotation
			// TODO(maybe): change rotation event to something other than right-clicking line
			// get rotated angle, and check if rotated clockwise or counter-clockwise
			float angle_diff, angle_temp;
			int rot_direction;

			angle_diff = fabs(mouse_angle_rad - mouse_last_a);
			if (angle_diff > PI)
				angle_diff = 2 * PI - angle_diff;

			angle_temp = mouse_angle_rad - mouse_last_a;
			if (angle_temp < 0)
				angle_temp += 2 * PI;
			if (angle_temp < PI)
				rot_direction = 1;
			else
				rot_direction = -1;

			int moved = 0;
			moved = data_cube->rotate_slice(slice_type, angle_diff * rot_direction);

			if (moved == 1) {
				mouse_last_x = mouse_x;
				mouse_last_y = mouse_y;
				mouse_last_a = get_mouse_angle(mouse_x, mouse_y);
				emit line_moved_sig(2);
				set_pixmap();
			}
		}
		else { // when clicked screen, not line = windowing
			int dx = mouse_x - mouse_last_x; // change window width
			int dy = mouse_last_y - mouse_y; // change window level

			window_width += dx;
			window_level += dy;
			if (window_width < 10)
				window_width = 10;
			if (window_width > 4000)
				window_width = 4000;
			if (window_level < -1000)
				window_level = -1000;
			if (window_level > 3096)
				window_level = 3096;

			apply_windowing();
			window_changed = 1;
			mouse_last_x = mouse_x;
			mouse_last_y = mouse_y;
		}
	}
	else if (event->buttons() & Qt::MidButton) { // when mid(wheel) pressed == panning
		float dx = (float)(mouse_last_x - mouse_x);
		float dy = (float)(mouse_last_y - mouse_y);
		dx = dx * pixel_num / slice_size;
		dy = dy * pixel_num / slice_size;

		int moved = data_cube->slice_panning(slice_type, dx, dy);
		if (moved == 1) {
			mouse_last_x = mouse_x;
			mouse_last_y = mouse_y;
			get_slice();

			emit zoom_panning_sig();
		}
	}

	emit_coord_sig(1, mouse_x, mouse_y);
}

float SliceWidget::get_mouse_angle(int mouse_x, int mouse_y)
{
	float mouse_angle_rad, angle_diff_h, angle_diff_v;

	if (mouse_x == line_x_scaled) {
		if (mouse_y >= line_y_scaled)
			mouse_angle_rad = PI / 2;
		else
			mouse_angle_rad = 3 * PI / 2;
	}
	else if (mouse_y == line_x_scaled) {
		if (mouse_x >= line_x_scaled)
			mouse_angle_rad = 0;
		else
			mouse_angle_rad = PI;
	}
	else if (mouse_x > line_x_scaled && mouse_y > line_y_scaled)
		mouse_angle_rad = atan2(mouse_y - line_y_scaled, mouse_x - line_x_scaled);
	else if (mouse_x < line_x_scaled && mouse_y > line_y_scaled)
		mouse_angle_rad = atan2(line_x_scaled - mouse_x, mouse_y - line_y_scaled) + PI / 2;
	else if (mouse_x < line_x_scaled && mouse_y < line_y_scaled)
		mouse_angle_rad = atan2(line_y_scaled - mouse_y, line_x_scaled - mouse_x) + PI;
	else
		mouse_angle_rad = atan2(mouse_x - line_x_scaled, line_y_scaled - mouse_y) + 3 * PI / 2;

	return mouse_angle_rad;
}
