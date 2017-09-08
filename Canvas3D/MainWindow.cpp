#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
	ui.setupUi(this);

	glWidget = new GLWidget3D(this);
	setCentralWidget(glWidget);

	QActionGroup* groupMode = new QActionGroup(this);
	groupMode->addAction(ui.actionSelect);
	groupMode->addAction(ui.actionRectangle);
	groupMode->addAction(ui.actionCircle);
	groupMode->addAction(ui.actionPolygon);
	ui.actionSelect->setChecked(true);

	connect(ui.actionNew, SIGNAL(triggered()), this, SLOT(onNew()));
	connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(close()));
	connect(ui.actionSelect, SIGNAL(triggered()), this, SLOT(onModeChanged()));
	connect(ui.actionRectangle, SIGNAL(triggered()), this, SLOT(onModeChanged()));
	connect(ui.actionCircle, SIGNAL(triggered()), this, SLOT(onModeChanged()));
	connect(ui.actionPolygon, SIGNAL(triggered()), this, SLOT(onModeChanged()));
}

MainWindow::~MainWindow() {
}

void MainWindow::onNew() {
	glWidget->clear();
}

void MainWindow::keyPressEvent(QKeyEvent *e) {
	glWidget->keyPressEvent(e);
}

void MainWindow::keyReleaseEvent(QKeyEvent* e) {
	glWidget->keyReleaseEvent(e);
}

void MainWindow::onModeChanged() {
	if (ui.actionSelect->isChecked()) {
		glWidget->setMode(GLWidget3D::MODE_SELECT);
	}
	else if (ui.actionRectangle->isChecked()) {
		glWidget->setMode(GLWidget3D::MODE_RECTANGLE);
	}
	else if (ui.actionCircle->isChecked()) {
		glWidget->setMode(GLWidget3D::MODE_CIRCLE);
	}
	else if (ui.actionPolygon->isChecked()) {
		glWidget->setMode(GLWidget3D::MODE_POLYGON);
	}
	update();
}