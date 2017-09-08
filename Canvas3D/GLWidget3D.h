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
#include "Operation.h"
#include "Layer.h"
#include "History.h"

class MainWindow;

class GLWidget3D : public QGLWidget {
public:
	static enum { MODE_SELECT = 0, MODE_MOVE, MODE_ROTATION, MODE_RESIZE, MODE_RECTANGLE, MODE_CIRCLE, MODE_POLYGON };

public:
	MainWindow* mainWin;

	int mode;

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
	bool front_faced;

	QPointF mouse_prev_pt;
	boost::shared_ptr<canvas::Shape> current_shape;
	boost::shared_ptr<canvas::Operation> operation;
	boost::shared_ptr<canvas::Shape> selected_shape;
	std::vector<boost::shared_ptr<canvas::Shape>> copied_shapes;
	canvas::Layer layer;
	canvas::History history;
	//std::vector<boost::shared_ptr<canvas::Shape>> shapes;
	//std::vector<std::vector<Vertex>> objects;

public:
	GLWidget3D(MainWindow *parent = 0);

	void drawScene();
	void render();
	void clear();
	void selectAll();
	void unselectAll();
	void deleteSelectedShapes();
	void undo();
	void redo();
	void copySelectedShapes();
	void pasteCopiedShapes();
	void setMode(int mode);
	void open(const QString& filename);
	void save(const QString& filename);
	glm::dvec2 screenToWorldCoordinates(const glm::dvec2& p);
	glm::dvec2 screenToWorldCoordinates(double x, double y);
	glm::dvec2 worldToScreenCoordinates(const glm::dvec2& p);
	double scale();
	void update3DGeometry();

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

