#pragma once

#include <glew.h>
#include "Shader.h"
#include "Vertex.h"
#include <QGLWidget>
#include <QMouseEvent>
#include <QTimer>
#include "Camera.h"
#include "ShadowMapping.h"
#include "RenderManager.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "Shape.h"

class MainWindow;

class GLWidget3D : public QGLWidget {
public:
	MainWindow* mainWin;

	// camera
	Camera camera;
	glm::vec3 light_dir;
	glm::mat4 light_mvpMatrix;

	// rendering engine
	RenderManager renderManager;

	// key status
	bool shiftPressed;
	bool ctrlPressed;

	bool first_paint;

	QPointF mouse_prev_pt;
	boost::shared_ptr<canvas::Shape> current_shape;
	std::vector<boost::shared_ptr<canvas::Shape>> shapes;

public:
	GLWidget3D(MainWindow *parent = 0);

	void drawScene();
	void render();
	glm::dvec2 screenToWorldCoordinates(const glm::dvec2& p);
	glm::dvec2 screenToWorldCoordinates(double x, double y);
	glm::dvec2 worldToScreenCoordinates(const glm::dvec2& p);

	void keyPressEvent(QKeyEvent* e);
	void keyReleaseEvent(QKeyEvent* e);

protected:
	void initializeGL();
	void resizeGL(int width, int height);
	void paintEvent(QPaintEvent *event);
	void mousePressEvent(QMouseEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);
	void mouseDoubleClickEvent(QMouseEvent* e);
	void wheelEvent(QWheelEvent* e);

};

