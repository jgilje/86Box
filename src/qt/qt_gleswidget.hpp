#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QEvent>
#include <QKeyEvent>

#include <atomic>
#include <qapplication.h>

#ifdef WAYLAND
#include "wl_mouse.hpp"
#endif

class GLESWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT

private:
    QImage m_image{QSize(2048 + 64, 2048 + 64), QImage::Format_RGB32};
    uint32_t m_bltbuf[(2048 + 64) * (2048 + 64)];
    std::atomic<int> x, y, w, h, sx, sy, sw, sh;
    bool wayland = false;
public:
    void resizeGL(int w, int h) override;
    void initializeGL() override;
    void paintGL() override;
    GLESWidget(QWidget* parent = nullptr)
    : QOpenGLWidget(parent), QOpenGLFunctions()
    {
        setMinimumSize(16, 16);
#ifdef WAYLAND
        if (QApplication::platformName().contains("wayland")) {
            wayland = true;
            wl_init();
        }
#endif
    }
    ~GLESWidget()
    {
        makeCurrent();
    }
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent* event) override
    {
        event->ignore();
    }
    void keyReleaseEvent(QKeyEvent* event) override
    {
        event->ignore();
    }
signals:
    void reqUpdate();

public slots:
	void qt_real_blit(int x, int y, int w, int h);
    void qt_mouse_poll();
    void reqUpdate_();

private:
    struct mouseinputdata {
        int deltax, deltay, deltaz;
        int mousebuttons;
    };
    mouseinputdata mousedata;
};
