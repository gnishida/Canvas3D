#include "GLWidget3D.h"
#include "MainWindow.h"
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include "GLUtils.h"
#include <QDir>
#include <QTextStream>
#include <QDate>
#include <iostream>
#include <QProcess>
#include "Rectangle.h"
#include "Circle.h"
#include "Polygon.h"

GLWidget3D::GLWidget3D(MainWindow *parent) : QGLWidget(QGLFormat(QGL::SampleBuffers)) {
	this->mainWin = parent;
	ctrlPressed = false;
	shiftPressed = false;

	first_paint = true;
	front_faced = true;
	current_shape.reset();
	operation.reset();

	// This is necessary to prevent the screen overdrawn by OpenGL
	setAutoFillBackground(false);

	// 光源位置をセット
	// ShadowMappingは平行光源を使っている。この位置から原点方向を平行光源の方向とする。
	light_dir = glm::normalize(glm::vec3(-4, -5, -8));

	// シャドウマップ用のmodel/view/projection行列を作成
	glm::mat4 light_pMatrix = glm::ortho<float>(-50, 50, -50, 50, 0.1, 200);
	glm::mat4 light_mvMatrix = glm::lookAt(-light_dir * 50.0f, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	light_mvpMatrix = light_pMatrix * light_mvMatrix;
}

/**
* Draw the scene.
*/
void GLWidget3D::drawScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(true);

	renderManager.renderAll();
}

void GLWidget3D::render() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PASS 1: Render to texture
	glUseProgram(renderManager.programs["pass1"]);
	
	glBindFramebuffer(GL_FRAMEBUFFER, renderManager.fragDataFB);
	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderManager.fragDataTex[0], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, renderManager.fragDataTex[1], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, renderManager.fragDataTex[2], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, renderManager.fragDataTex[3], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderManager.fragDepthTex, 0);

	// Set the list of draw buffers.
	GLenum DrawBuffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	glDrawBuffers(4, DrawBuffers); // "3" is the size of DrawBuffers
	// Always check that our framebuffer is ok
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("+ERROR: GL_FRAMEBUFFER_COMPLETE false\n");
		exit(0);
	}

	glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["pass1"], "mvpMatrix"), 1, false, &camera.mvpMatrix[0][0]);
	glUniform3f(glGetUniformLocation(renderManager.programs["pass1"], "lightDir"), light_dir.x, light_dir.y, light_dir.z);
	glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["pass1"], "light_mvpMatrix"), 1, false, &light_mvpMatrix[0][0]);

	glUniform1i(glGetUniformLocation(renderManager.programs["pass1"], "shadowMap"), 6);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, renderManager.shadow.textureDepth);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	drawScene();
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// PASS 2: Create AO
	if (renderManager.renderingMode == RenderManager::RENDERING_MODE_SSAO) {
		glUseProgram(renderManager.programs["ssao"]);
		glBindFramebuffer(GL_FRAMEBUFFER, renderManager.fragDataFB_AO);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderManager.fragAOTex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderManager.fragDepthTex_AO, 0);
		GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Always check that our framebuffer is ok
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			printf("++ERROR: GL_FRAMEBUFFER_COMPLETE false\n");
			exit(0);
		}

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUniform2f(glGetUniformLocation(renderManager.programs["ssao"], "pixelSize"), 2.0f / this->width(), 2.0f / this->height());

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex0"), 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[0]);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex1"), 2);
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[1]);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex2"), 3);
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[2]);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "noiseTex"), 7);
		glActiveTexture(GL_TEXTURE7);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragNoiseTex);

		{
			glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["ssao"], "mvpMatrix"), 1, false, &camera.mvpMatrix[0][0]);
			glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["ssao"], "pMatrix"), 1, false, &camera.pMatrix[0][0]);
		}

		glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "uKernelSize"), renderManager.uKernelSize);
		glUniform3fv(glGetUniformLocation(renderManager.programs["ssao"], "uKernelOffsets"), renderManager.uKernelOffsets.size(), (const GLfloat*)renderManager.uKernelOffsets.data());

		glUniform1f(glGetUniformLocation(renderManager.programs["ssao"], "uPower"), renderManager.uPower);
		glUniform1f(glGetUniformLocation(renderManager.programs["ssao"], "uRadius"), renderManager.uRadius);

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);
	}
	else if (renderManager.renderingMode == RenderManager::RENDERING_MODE_LINE || renderManager.renderingMode == RenderManager::RENDERING_MODE_HATCHING || renderManager.renderingMode == RenderManager::RENDERING_MODE_SKETCHY) {
		glUseProgram(renderManager.programs["line"]);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUniform2f(glGetUniformLocation(renderManager.programs["line"], "pixelSize"), 1.0f / this->width(), 1.0f / this->height());
		glUniformMatrix4fv(glGetUniformLocation(renderManager.programs["line"], "pMatrix"), 1, false, &camera.pMatrix[0][0]);
		if (renderManager.renderingMode == RenderManager::RENDERING_MODE_HATCHING) {
			glUniform1i(glGetUniformLocation(renderManager.programs["line"], "useHatching"), 1);
		}
		else {
			glUniform1i(glGetUniformLocation(renderManager.programs["line"], "useHatching"), 0);
		}

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex0"), 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[0]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex1"), 2);
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[1]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex2"), 3);
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[2]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "tex3"), 4);
		glActiveTexture(GL_TEXTURE4);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[3]);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glUniform1i(glGetUniformLocation(renderManager.programs["line"], "hatchingTexture"), 5);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_3D, renderManager.hatchingTextures);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		
		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);
	}
	else if (renderManager.renderingMode == RenderManager::RENDERING_MODE_CONTOUR) {
		glUseProgram(renderManager.programs["contour"]);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUniform2f(glGetUniformLocation(renderManager.programs["contour"], "pixelSize"), 1.0f / this->width(), 1.0f / this->height());

		glUniform1i(glGetUniformLocation(renderManager.programs["contour"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Blur

	if (renderManager.renderingMode == RenderManager::RENDERING_MODE_BASIC || renderManager.renderingMode == RenderManager::RENDERING_MODE_SSAO) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		glUseProgram(renderManager.programs["blur"]);
		glUniform2f(glGetUniformLocation(renderManager.programs["blur"], "pixelSize"), 2.0f / this->width(), 2.0f / this->height());
		//printf("pixelSize loc %d\n", glGetUniformLocation(vboRenderManager.programs["blur"], "pixelSize"));

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex0"), 1);//COLOR
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[0]);

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex1"), 2);//NORMAL
		glActiveTexture(GL_TEXTURE2);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[1]);

		/*glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex2"), 3);
		glActiveTexture(GL_TEXTURE3);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDataTex[2]);*/

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "depthTex"), 8);
		glActiveTexture(GL_TEXTURE8);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragDepthTex);

		glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "tex3"), 4);//AO
		glActiveTexture(GL_TEXTURE4);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, renderManager.fragAOTex);

		if (renderManager.renderingMode == RenderManager::RENDERING_MODE_SSAO) {
			glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "ssao_used"), 1); // ssao used
		}
		else {
			glUniform1i(glGetUniformLocation(renderManager.programs["blur"], "ssao_used"), 0); // no ssao
		}

		glBindVertexArray(renderManager.secondPassVAO);

		glDrawArrays(GL_QUADS, 0, 4);
		glBindVertexArray(0);
		glDepthFunc(GL_LEQUAL);

	}

	// REMOVE
	glActiveTexture(GL_TEXTURE0);
}

void GLWidget3D::clear() {
	layer.clear();
	selected_shape.reset();

	// update 3D geometry
	update3DGeometry();

	update();
}

void GLWidget3D::selectAll() {
	layer.selectAll();

	mode = MODE_SELECT;
	update();
}

void GLWidget3D::unselectAll() {
	layer.unselectAll();

	current_shape.reset();
	update();
}

void GLWidget3D::deleteSelectedShapes() {
	for (int i = layer.shapes.size() - 1; i >= 0; --i) {
		if (layer.shapes[i]->isSelected()) {
			layer.shapes.erase(layer.shapes.begin() + i);
		}
	}

	// update 3D geometry
	update3DGeometry();

	current_shape.reset();
	update();
}

void GLWidget3D::undo() {
	try {
		layer = history.undo();

		// update 3D geometry
		update3DGeometry();

		update();
	}
	catch (char* ex) {
	}
}

void GLWidget3D::redo() {
	try {
		layer = history.redo();

		// update 3D geometry
		update3DGeometry();

		update();
	}
	catch (char* ex) {
	}
}

void GLWidget3D::copySelectedShapes() {
	layer.copySelectedShapes(copied_shapes);

	// update 3D geometry
	update3DGeometry();

	update();
}

void GLWidget3D::pasteCopiedShapes() {
	layer.pasteCopiedShapes(copied_shapes);
	
	// update 3D geometry
	update3DGeometry();

	current_shape.reset();
	mode = MODE_SELECT;
	update();
}


void GLWidget3D::setMode(int mode) {
	if (this->mode != mode) {
		this->mode = mode;

		update();
	}
}

void GLWidget3D::open(const QString& filename) {
	QFile file(filename);
	if (!file.open(QFile::ReadOnly | QFile::Text)) throw "File cannot open.";

	QDomDocument doc;
	doc.setContent(&file);

	QDomElement root = doc.documentElement();
	if (root.tagName() != "design")	throw "Invalid file format.";

	// clear the data
	layer.clear();
	selected_shape.reset();
	mode = MODE_SELECT;

	QDomNode layer_node = root.firstChild();
	while (!layer_node.isNull()) {
		if (layer_node.toElement().tagName() == "layer") {
			layer.load(layer_node.toElement());
		}

		layer_node = layer_node.nextSibling();
	}

	// update 3D geometry
	update3DGeometry();

	// no currently drawing shape
	current_shape.reset();

	update();
}

void GLWidget3D::save(const QString& filename) {
	QFile file(filename);
	if (!file.open(QFile::WriteOnly)) throw "File cannot open.";

	QDomDocument doc;

	// set root node
	QDomElement root = doc.createElement("design");
	root.setAttribute("author", "Gen Nishida");
	root.setAttribute("version", "1.0");
	root.setAttribute("date", QDate::currentDate().toString("MM/dd/yyyy"));
	doc.appendChild(root);

	// write layers
	QDomElement layer_node = layer.toXml(doc);
	root.appendChild(layer_node);

	QTextStream out(&file);
	doc.save(out, 4);
}

glm::dvec2 GLWidget3D::screenToWorldCoordinates(const glm::dvec2& p) {
	return screenToWorldCoordinates(p.x, p.y);
}

glm::dvec2 GLWidget3D::screenToWorldCoordinates(double x, double y) {
	glm::vec2 offset = glm::vec2(camera.pos.x, -camera.pos.y) * (float)scale();
	return glm::dvec2((x - width() * 0.5 + offset.x) / scale(), -(y - height() * 0.5 + offset.y) / scale());
}

glm::dvec2 GLWidget3D::worldToScreenCoordinates(const glm::dvec2& p) {
	//return glm::dvec2(width() * 0.5 + p.x * camera.f() / camera.pos.z * width() * 0.5, height() * 0.5 - p.y * scale());
	return glm::dvec2(width() * 0.5 + (p.x - camera.pos.x) * scale(), height() * 0.5 - (p.y - camera.pos.y) * scale());
}

double GLWidget3D::scale() {
	return camera.f() / camera.pos.z * height() * 0.5;
}

void GLWidget3D::update3DGeometry() {
	renderManager.removeObjects();
	for (int i = 0; i < layer.shapes.size(); i++) {
		QString obj_name = QString("object_%1").arg(i);
		renderManager.addObject(obj_name, "", layer.shapes[i]->getVertices(), true);
	}

	// update shadow map
	renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
}

void GLWidget3D::keyPressEvent(QKeyEvent *e) {
	ctrlPressed = false;
	shiftPressed = false;

	if (e->modifiers() & Qt::ControlModifier) {
		ctrlPressed = true;
	}
	if (e->modifiers() & Qt::ShiftModifier) {
		shiftPressed = true;
	}

	switch (e->key()) {
	default:
		break;
	}
}

void GLWidget3D::keyReleaseEvent(QKeyEvent* e) {
	switch (e->key()) {
	case Qt::Key_Control:
		ctrlPressed = false;
		break;
	case Qt::Key_Shift:
		shiftPressed = false;
		break;
	default:
		break;
	}
}

/**
* This function is called once before the first call to paintGL() or resizeGL().
*/
void GLWidget3D::initializeGL() {
	// init glew
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		std::cout << "Error: " << glewGetErrorString(err) << std::endl;
	}

	if (glewIsSupported("GL_VERSION_4_2"))
		printf("Ready for OpenGL 4.2\n");
	else {
		printf("OpenGL 4.2 not supported\n");
		exit(1);
	}
	const GLubyte* text = glGetString(GL_VERSION);
	printf("VERSION: %s\n", text);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnable(GL_TEXTURE_2D);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexGenf(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenf(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glDisable(GL_TEXTURE_2D);

	glEnable(GL_TEXTURE_3D);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glDisable(GL_TEXTURE_3D);

	glEnable(GL_TEXTURE_2D_ARRAY);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glDisable(GL_TEXTURE_2D_ARRAY);

	////////////////////////////////
	renderManager.init("", "", "", true, 8192);
	renderManager.resize(this->width(), this->height());

	glUniform1i(glGetUniformLocation(renderManager.programs["ssao"], "tex0"), 0);//tex0: 0
}

/**
* This function is called whenever the widget has been resized.
*/
void GLWidget3D::resizeGL(int width, int height) {
	height = height ? height : 1;
	glViewport(0, 0, width, height);
	camera.updatePMatrix(width, height);

	renderManager.resize(width, height);
}

/**
* This function is called whenever the widget needs to be painted.
*/
void GLWidget3D::paintEvent(QPaintEvent *event) {
	if (first_paint) {
		std::vector<Vertex> vertices;
		glutils::drawQuad(0.001, 0.001, glm::vec4(1, 1, 1, 1), glm::mat4(), vertices);
		renderManager.addObject("dummy", "", vertices, true);
		renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
		first_paint = false;
	}

	// OpenGLで描画
	makeCurrent();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	render();

	// REMOVE
	glActiveTexture(GL_TEXTURE0);

	// OpenGLの設定を元に戻す
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	// draw 2D
	QPainter painter(this);
	painter.setOpacity(1.0f);
	if (abs(camera.xrot) < 10 && abs(camera.yrot) < 10) {
		// draw grid
		painter.save();
		painter.setPen(QPen(QColor(224, 224, 224), 1));
		for (int i = -200; i <= 200; i++) {
			glm::dvec2 p = worldToScreenCoordinates(glm::dvec2(i * 5, i * 5));
			painter.drawLine(p.x, 0, p.x, height());
			glm::dvec2 p2 = worldToScreenCoordinates(glm::dvec2(0, i * 5));
			painter.drawLine(0, p.y, width(), p.y);
		}
		painter.restore();

		glm::vec2 offset = glm::vec2(camera.pos.x, -camera.pos.y) * (float)scale();
		for (int i = 0; i < layer.shapes.size(); i++) {
			layer.shapes[i]->draw(painter, QPointF(width() * 0.5 - offset.x, height() * 0.5 - offset.y), scale());
		}
		if (current_shape) {
			current_shape->draw(painter, QPointF(width() * 0.5 - offset.x, height() * 0.5 - offset.y), scale());
		}
	}
	painter.end();

	glEnable(GL_DEPTH_TEST);
}

/**
* This event handler is called when the mouse press events occur.
*/
void GLWidget3D::mousePressEvent(QMouseEvent *e) {
	// This is necessary to get key event occured even after the user selects a menu.
	setFocus();

	if (e->buttons() & Qt::LeftButton) {
		if (mode == MODE_SELECT) {
			// hit test for rotation marker
			for (int i = 0; i < layer.shapes.size(); ++i) {
				if (glm::length(layer.shapes[i]->getRotationMarkerPosition(scale()) - layer.shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start rotating
					mode = MODE_ROTATION;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::RotateOperation(screenToWorldCoordinates(e->x(), e->y()), layer.shapes[i]->worldCoordinate(layer.shapes[i]->getCenter())));
					selected_shape = layer.shapes[i];
					if (!layer.shapes[i]->isSelected()) {
						unselectAll();
						layer.shapes[i]->select();
					}
					update();
					return;
				}
			}

			// hit test for resize marker
			for (int i = 0; i < layer.shapes.size(); ++i) {
				canvas::BoundingBox bbox = layer.shapes[i]->boundingBox();
				if (glm::length(bbox.minPt - layer.shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start resizing
					mode = MODE_RESIZE;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::ResizeOperation(screenToWorldCoordinates(e->x(), e->y()), layer.shapes[i]->worldCoordinate(bbox.maxPt)));
					selected_shape = layer.shapes[i];
					if (!layer.shapes[i]->isSelected()) {
						unselectAll();
						layer.shapes[i]->select();
					}
					update();
					return;
				}

				if (glm::length(glm::dvec2(bbox.maxPt.x, bbox.minPt.y) - layer.shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start resizing
					mode = MODE_RESIZE;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::ResizeOperation(screenToWorldCoordinates(e->x(), e->y()), layer.shapes[i]->worldCoordinate(glm::dvec2(bbox.minPt.x, bbox.maxPt.y))));
					selected_shape = layer.shapes[i];
					if (!layer.shapes[i]->isSelected()) {
						unselectAll();
						layer.shapes[i]->select();
					}
					update();
					return;
				}

				if (glm::length(glm::dvec2(bbox.minPt.x, bbox.maxPt.y) - layer.shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start resizing
					mode = MODE_RESIZE;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::ResizeOperation(screenToWorldCoordinates(e->x(), e->y()), layer.shapes[i]->worldCoordinate(glm::dvec2(bbox.maxPt.x, bbox.minPt.y))));
					selected_shape = layer.shapes[i];
					if (!layer.shapes[i]->isSelected()) {
						unselectAll();
						layer.shapes[i]->select();
					}
					update();
					return;
				}

				if (glm::length(bbox.maxPt - layer.shapes[i]->localCoordinate(screenToWorldCoordinates(e->x(), e->y()))) < 10 / scale()) {
					// start resizing
					mode = MODE_RESIZE;
					operation = boost::shared_ptr<canvas::Operation>(new canvas::ResizeOperation(screenToWorldCoordinates(e->x(), e->y()), layer.shapes[i]->worldCoordinate(bbox.minPt)));
					selected_shape = layer.shapes[i];
					if (!layer.shapes[i]->isSelected()) {
						unselectAll();
						layer.shapes[i]->select();
					}
					update();
					return;
				}
			}

			// hit test for the selected shapes first
			for (int i = 0; i < layer.shapes.size(); ++i) {
				if (layer.shapes[i]->isSelected()) {
					if (layer.shapes[i]->hit(screenToWorldCoordinates(e->x(), e->y()))) {
						// reselecting the already selected shapes
						mode = MODE_MOVE;
						operation = boost::shared_ptr<canvas::Operation>(new canvas::MoveOperation(screenToWorldCoordinates(e->x(), e->y())));
						update();
						return;
					}
				}
			}

			// hit test for the shape
			for (int i = 0; i < layer.shapes.size(); ++i) {
				if (layer.shapes[i]->getSubType() == canvas::Shape::TYPE_BODY) {
					if (layer.shapes[i]->hit(screenToWorldCoordinates(e->x(), e->y()))) {
						// start moving
						mode = MODE_MOVE;
						operation = boost::shared_ptr<canvas::Operation>(new canvas::MoveOperation(screenToWorldCoordinates(e->x(), e->y())));
						if (!layer.shapes[i]->isSelected()) {
							if (!ctrlPressed) {
								// If CTRL is not pressed, then deselect all other shapes.
								unselectAll();
							}
							layer.shapes[i]->select();
						}
						update();
						return;
					}
				}
			}

			unselectAll();
		}
		else if (mode == MODE_RECTANGLE) {
			if (!current_shape) {
				// start drawing a rectangle
				unselectAll();
				current_shape = boost::shared_ptr<canvas::Shape>(new canvas::Rectangle(canvas::Shape::TYPE_BODY, screenToWorldCoordinates(e->x(), e->y())));
				current_shape->startDrawing();
				setMouseTracking(true);
			}
		}
		else if (mode == MODE_CIRCLE) {
			if (!current_shape) {
				// start drawing a rectangle
				unselectAll();
				current_shape = boost::shared_ptr<canvas::Shape>(new canvas::Circle(canvas::Shape::TYPE_BODY, screenToWorldCoordinates(e->x(), e->y())));
				current_shape->startDrawing();
				setMouseTracking(true);
			}
		}
		else if (mode == MODE_POLYGON) {
			if (current_shape) {
				current_shape->addPoint(current_shape->localCoordinate(screenToWorldCoordinates(e->x(), e->y())));
			}
			else {
				// start drawing a rectangle
				unselectAll();
				current_shape = boost::shared_ptr<canvas::Shape>(new canvas::Polygon(canvas::Shape::TYPE_BODY, screenToWorldCoordinates(e->x(), e->y())));
				current_shape->startDrawing();
				setMouseTracking(true);
			}
		}
	}
	else if (e->buttons() & Qt::RightButton) {
		camera.mousePress(e->x(), e->y());
	}
}

/**
* This event handler is called when the mouse move events occur.
*/

void GLWidget3D::mouseMoveEvent(QMouseEvent *e) {
	if (mode == MODE_MOVE) {
		boost::shared_ptr<canvas::MoveOperation> op = boost::static_pointer_cast<canvas::MoveOperation>(operation);
		glm::dvec2 dir = screenToWorldCoordinates(e->x(), e->y()) - op->pivot;
		for (int i = 0; i < layer.shapes.size(); ++i) {
			if (layer.shapes[i]->isSelected()) {
				layer.shapes[i]->translate(dir);

				// update 3D geometry
				QString obj_name = QString("object_%1").arg(i);
				renderManager.removeObject(obj_name);
				renderManager.addObject(obj_name, "", layer.shapes[i]->getVertices(), true);

				// update shadow map
				renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
			}
		}
		op->pivot = screenToWorldCoordinates(e->x(), e->y());
		update();
	}
	else if (mode == MODE_ROTATION) {
		boost::shared_ptr<canvas::RotateOperation> op = boost::static_pointer_cast<canvas::RotateOperation>(operation);
		glm::dvec2 dir1 = op->pivot - op->rotation_center;
		glm::dvec2 dir2 = screenToWorldCoordinates(e->x(), e->y()) - op->rotation_center;
		double theta = atan2(dir2.y, dir2.x) - atan2(dir1.y, dir1.x);
		for (int i = 0; i < layer.shapes.size(); ++i) {
			if (layer.shapes[i]->isSelected()) {
				layer.shapes[i]->rotate(theta);

				// update 3D geometry
				QString obj_name = QString("object_%1").arg(i);
				renderManager.removeObject(obj_name);
				renderManager.addObject(obj_name, "", layer.shapes[i]->getVertices(), true);

				// update shadow map
				renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
			}
		}
		op->pivot = screenToWorldCoordinates(e->x(), e->y());
		update();
	}
	else if (mode == MODE_RESIZE) {
		boost::shared_ptr<canvas::ResizeOperation> op = boost::static_pointer_cast<canvas::ResizeOperation>(operation);
		glm::dvec2 resize_center = selected_shape->localCoordinate(op->resize_center);
		glm::dvec2 dir1 = selected_shape->localCoordinate(op->pivot) - resize_center;
		glm::dvec2 dir2 = selected_shape->localCoordinate(screenToWorldCoordinates(e->x(), e->y())) - resize_center;
		glm::dvec2 resize_scale(dir2.x / dir1.x, dir2.y / dir1.y);
		for (int i = 0; i < layer.shapes.size(); ++i) {
			if (layer.shapes[i]->isSelected()) {
				layer.shapes[i]->resize(resize_scale, resize_center);

				// update 3D geometry
				QString obj_name = QString("object_%1").arg(i);
				renderManager.removeObject(obj_name);
				renderManager.addObject(obj_name, "", layer.shapes[i]->getVertices(), true);

				// update shadow map
				renderManager.updateShadowMap(this, light_dir, light_mvpMatrix);
			}
		}
		op->pivot = screenToWorldCoordinates(e->x(), e->y());
		update();
	}
	else if (mode == MODE_RECTANGLE || mode == MODE_CIRCLE || mode == MODE_POLYGON) {
		if (current_shape) {
			current_shape->updateByNewPoint(current_shape->localCoordinate(screenToWorldCoordinates(e->x(), e->y())), shiftPressed);
		}
	}
	else if (e->buttons() & Qt::RightButton) {
		if (shiftPressed) {
			camera.move(e->x(), e->y());
		}
		else {
			camera.rotate(e->x(), e->y(), (ctrlPressed ? 0.1 : 1));
		}
	}

	update();
}

/**
* This event handler is called when the mouse release events occur.
*/
void GLWidget3D::mouseReleaseEvent(QMouseEvent *e) {
	if (mode == MODE_MOVE || mode == MODE_ROTATION || mode == MODE_RESIZE) {
		history.push(layer);
		mode = MODE_SELECT;
	}
	else if (e->button() == Qt::RightButton) {
		if (abs(camera.xrot) < 20 && abs(camera.yrot) < 20) {
			camera.xrot = 0;
			camera.yrot = 0;
			camera.updateMVPMatrix();
			front_faced = true;
		}
		else {
			front_faced = false;
		}
	}

	update();
}

void GLWidget3D::mouseDoubleClickEvent(QMouseEvent* e) {
	if (mode == MODE_RECTANGLE || mode == MODE_CIRCLE || mode == MODE_POLYGON) {
		if (e->button() == Qt::LeftButton) {
			if (current_shape) {
				// The shape is created.
				current_shape->completeDrawing();
				layer.shapes.push_back(current_shape->clone());

				// update 3D geometry
				update3DGeometry();

				layer.shapes.back()->select();
				mode = MODE_SELECT;
				history.push(layer);
				current_shape.reset();
				operation.reset();
				mainWin->ui.actionSelect->setChecked(true);
			}
		}
	}

	setMouseTracking(false);

	update();
}

void GLWidget3D::wheelEvent(QWheelEvent* e) {
	camera.zoom(e->delta() * 0.1);
	update();
}
