
#include "render.h"

#include "model.h"

#include <QtQuick/qquickwindow.h>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLShaderProgram>

#define GL_COLOR 0x1800
#define GL_COLOR_ATTACHMENT1 (GL_COLOR_ATTACHMENT0 + 1)

SwitchRender::SwitchRender() : mSize(4)
{
    srand(time(NULL));

    mSwitchAngles.resize(mSize * mSize);
    mSwitchAnglesAspire.resize(mSize * mSize);

    init();
}

SwitchRender::~SwitchRender()
{}

void SwitchRender::init()
{
    mWin = false;

    for (int i = 0; i < mSize * mSize; i++)
    {
        int v = rand() % 2;
        mSwitchAngles[i] = 0.0f;
        mSwitchAnglesAspire[i] = v;
    }

    mTime.restart();
}

QOpenGLFramebufferObject* SwitchRender::createFramebufferObject(
    const QSize& size)
{
    QOpenGLExtraFunctions* f =
        QOpenGLContext::currentContext()->extraFunctions();

    QOpenGLFramebufferObject* frameBuffer = new QOpenGLFramebufferObject(
        size.width(),
        size.height(),
        QOpenGLFramebufferObject::CombinedDepthStencil);
    frameBuffer->addColorAttachment(size.width(), size.height());

    static GLenum drawBufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    f->glDrawBuffers(2, drawBufs);

    mProj.setToIdentity();
    mProj.perspective(
        45.0f, GLfloat(size.width()) / size.height(), 10.0f, 1000.0f);

    return frameBuffer;
}

void SwitchRender::render()
{
    QOpenGLExtraFunctions* f =
        QOpenGLContext::currentContext()->extraFunctions();

    if (!mSwitches.valid())
    {
        mSwitches.init("switch.usda", mSize);
        mTime.start();
    }

    float delta = float(mTime.restart()) * 0.003f;
    bool needUpdate = false;

    for (int i = 0; i < mSize * mSize; i++)
    {
        if (mSwitchAnglesAspire[i] > mSwitchAngles[i])
        {
            float animated = mSwitchAngles[i] + delta;
            mSwitchAngles[i] =
                std::min<float>(animated, mSwitchAnglesAspire[i]);
            needUpdate = true;
        }
    }

    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_CULL_FACE);
    f->glDepthMask(GL_TRUE);
    f->glDepthFunc(GL_LESS);
    f->glFrontFace(GL_CCW);
    f->glCullFace(GL_BACK);

    // Clear the first AOV.
    static const float background[] = {0.1f, 0.2f, 0.3f, 1.0f};
    f->glClearBufferfv(GL_COLOR, 0, background);
    // Clear the second AOV.
    static const float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
    f->glClearBufferfv(GL_COLOR, 1, black);
    // Clear the depth buffer.
    f->glClear(GL_DEPTH_BUFFER_BIT);

    // Setup camera.
    QMatrix4x4 camera;
    camera.setToIdentity();
    camera.lookAt(
        QVector3D(0.0f, 500.0f, 250.0f),
        QVector3D(0.0f, 0.0f, 30.0f),
        QVector3D(0.0f, 1.0f, 0.0f));

    mSwitches.render(
        mProj * camera,
        QVector3D(0.0f, 500.0f, 250.0f),
        QVector3D(0.0f, 300.0f, 0.0f),
        mSwitchAngles.data());

    if (needUpdate)
    {
        // We need to call this function once again.
        update();
    }
}

void SwitchRender::synchronize(QQuickFramebufferObject* item)
{
    if (mWin)
    {
        return;
    }

    Switch* sw = reinterpret_cast<Switch*>(item);

    if (sw->mLastClickX > 0 && sw->mLastClickY > 0)
    {
        int ID = getObjectID(sw->mLastClickX, sw->mLastClickY);

        sw->mLastClickX = -1;
        sw->mLastClickY = -1;

        if (ID >= 0)
        {
            int x = ID % mSize;
            int y = ID / mSize;

            int wrongPlaced = 0;

            for (int i = 0; i < mSize; i++)
            {
                for (int j = 0; j < mSize; j++)
                {
                    int& current = mSwitchAnglesAspire[j * mSize + i];

                    if (i == x || j == y)
                    {
                        current += 1;
                    }

                    wrongPlaced += 1 - current % 2;
                }
            }

            if (wrongPlaced == 0)
            {
                emit sw->winGame();
            }
        }

        mTime.restart();
    }

    if (sw->mNewGamePressed)
    {
        sw->mNewGamePressed = false;
        init();
    }
}

int SwitchRender::getObjectID(int x, int y)
{
    QOpenGLExtraFunctions* f =
        QOpenGLContext::currentContext()->extraFunctions();

    framebufferObject()->bind();

    f->glReadBuffer(GL_COLOR_ATTACHMENT1);

    float d[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    f->glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, &d);

    framebufferObject()->release();

    return static_cast<int>(roundf(d[0] * 10) + roundf(d[1] * 10) * 10) - 1;
}

Switch::Switch(QQuickItem* parent) :
        QQuickFramebufferObject(parent),
        mLastClickX(-1),
        mLastClickY(-1),
        mNewGamePressed(false)
{
    // This call is crucial to even get any clicks at all
    setAcceptedMouseButtons(Qt::LeftButton);
}

QQuickFramebufferObject::Renderer* Switch::createRenderer() const
{
#if QT_CONFIG(opengl)
    qDebug("Create renderer");
    return new SwitchRender;
#else
    qDebug("No OpenGL, no renderer");
    return nullptr;
#endif
}

void Switch::mousePressEvent(QMouseEvent* ev)
{
    ev->accept();
}

void Switch::mouseReleaseEvent(QMouseEvent* ev)
{
    mLastClickX = ev->x();
    mLastClickY = ev->y();
    ev->accept();

    update();
}

void Switch::newGame()
{
    mNewGamePressed = true;

    update();
}
