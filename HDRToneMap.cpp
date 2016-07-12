#include "HDRToneMap.h"

#include <QtGlobal>

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTime>

#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>

#include <cmath>
#include <cstring>

MyWindow::~MyWindow()
{
    if (mProgram != 0) delete mProgram;
}

MyWindow::MyWindow()
    : mProgram(0), currentTimeMs(0), currentTimeS(0), tPrev(0), angle(M_PI / 2.0f)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setMajorVersion(4);
    format.setMinorVersion(3);
    format.setSamples(4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
    create();

    resize(800, 600);

    mContext = new QOpenGLContext(this);
    mContext->setFormat(format);
    mContext->create();

    mContext->makeCurrent( this );

    mFuncs = mContext->versionFunctions<QOpenGLFunctions_4_3_Core>();
    if ( !mFuncs )
    {
        qWarning( "Could not obtain OpenGL versions object" );
        exit( 1 );
    }
    if (mFuncs->initializeOpenGLFunctions() == GL_FALSE)
    {
        qWarning( "Could not initialize core open GL functions" );
        exit( 1 );
    }

    initializeOpenGLFunctions();

    QTimer *repaintTimer = new QTimer(this);
    connect(repaintTimer, &QTimer::timeout, this, &MyWindow::render);
    repaintTimer->start(1000/60);

    QTimer *elapsedTimer = new QTimer(this);
    connect(elapsedTimer, &QTimer::timeout, this, &MyWindow::modCurTime);
    elapsedTimer->start(1);       
}

void MyWindow::modCurTime()
{
    currentTimeMs++;
    currentTimeS=currentTimeMs/1000.0f;
}

void MyWindow::initialize()
{
    CreateVertexBuffer();
    initShaders();
    pass1Index = mFuncs->glGetSubroutineIndex( mProgram->programId(), GL_FRAGMENT_SHADER, "pass1");
    pass2Index = mFuncs->glGetSubroutineIndex( mProgram->programId(), GL_FRAGMENT_SHADER, "pass2");
    pass3Index = mFuncs->glGetSubroutineIndex( mProgram->programId(), GL_FRAGMENT_SHADER, "pass3");

    initMatrices();
    setupFBO();

    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
}

void MyWindow::CreateVertexBuffer()
{
    // *** Teapot
    mFuncs->glGenVertexArrays(1, &mVAOTeapot);
    mFuncs->glBindVertexArray(mVAOTeapot);

    QMatrix4x4 transform;
    //transform.translate(QVector3D(0.0f, 1.5f, 0.25f));
    mTeapot = new Teapot(14, transform);

    // Create and populate the buffer objects
    unsigned int TeapotHandles[3];
    glGenBuffers(3, TeapotHandles);

    glBindBuffer(GL_ARRAY_BUFFER, TeapotHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mTeapot->getnVerts()) * sizeof(float), mTeapot->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, TeapotHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mTeapot->getnVerts()) * sizeof(float), mTeapot->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TeapotHandles[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * mTeapot->getnFaces() * sizeof(unsigned int), mTeapot->getelems(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, TeapotHandles[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, TeapotHandles[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TeapotHandles[2]);

    mFuncs->glBindVertexArray(0);

    // *** Plane
    mFuncs->glGenVertexArrays(1, &mVAOPlane);
    mFuncs->glBindVertexArray(mVAOPlane);

    mPlane = new VBOPlane(20.0f, 10.0f, 1.0, 1.0);

    // Create and populate the buffer objects
    unsigned int PlaneHandles[3];
    glGenBuffers(3, PlaneHandles);

    glBindBuffer(GL_ARRAY_BUFFER, PlaneHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mPlane->getnVerts()) * sizeof(float), mPlane->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, PlaneHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mPlane->getnVerts()) * sizeof(float), mPlane->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, PlaneHandles[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * mPlane->getnFaces() * sizeof(unsigned int), mPlane->getelems(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, PlaneHandles[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, PlaneHandles[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, PlaneHandles[2]);

    mFuncs->glBindVertexArray(0);

    // *** Sphere
    mFuncs->glGenVertexArrays(1, &mVAOSphere);
    mFuncs->glBindVertexArray(mVAOSphere);

    mSphere = new VBOSphere(2.0f, 50, 50);

    // Create and populate the buffer objects
    unsigned int SphereHandles[3];
    glGenBuffers(3, SphereHandles);

    glBindBuffer(GL_ARRAY_BUFFER, SphereHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mSphere->getnVerts()) * sizeof(float), mSphere->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, SphereHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mSphere->getnVerts()) * sizeof(float), mSphere->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, SphereHandles[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mSphere->getnFaces() * sizeof(unsigned int), mSphere->getelems(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, SphereHandles[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, SphereHandles[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, SphereHandles[2]);


    // *** Array for full-screen quad
    GLfloat verts[] = {
        -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f
    };
    GLfloat tc[] = {
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
    };

    // Set up the buffers

    unsigned int fsqhandle[2];
    glGenBuffers(2, fsqhandle);

    glBindBuffer(GL_ARRAY_BUFFER, fsqhandle[0]);
    glBufferData(GL_ARRAY_BUFFER, 6 * 3 * sizeof(float), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, fsqhandle[1]);
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), tc, GL_STATIC_DRAW);

    // Set up the VAO
    mFuncs->glGenVertexArrays( 1, &mVAOFSQuad );
    mFuncs->glBindVertexArray(mVAOFSQuad);

    // Vertex positions
    mFuncs->glBindVertexBuffer(0, fsqhandle[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex texture coordinates
    mFuncs->glBindVertexBuffer(1, fsqhandle[1], 0, sizeof(GLfloat) * 2);
    mFuncs->glVertexAttribFormat(2, 2, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(2, 2);

    mFuncs->glBindVertexArray(0);

}

void MyWindow::initMatrices()
{
    ModelMatrixTeapot.translate( 3.0f, -5.0f, 1.5f);
    ModelMatrixTeapot.rotate( -90.0f, QVector3D(1.0f, 0.0f, 0.0f));

    ModelMatrixSphere.translate( -3.0f, -3.0f, 2.0f);

    ModelMatrixBackPlane.rotate(90.0f, QVector3D(1.0f, 0.0f, 0.0f));
    ModelMatrixBotPlane.translate(0.0f, -5.0f, 0.0f);
    ModelMatrixTopPlane.translate(0.0f,  5.0f, 0.0f);
    ModelMatrixTopPlane.rotate(180.0f, 1.0f, 0.0f, 0.0f);

    //ModelMatrixPlane.translate(0.0f, -0.45f, 0.0f);

    ViewMatrix.lookAt(QVector3D(2.0f, 0.0f, 14.0f), QVector3D(0.0f,0.0f,0.0f), QVector3D(0.0f,1.0f,0.0f));
}

void MyWindow::resizeEvent(QResizeEvent *)
{
    mUpdateSize = true;

    ProjectionMatrix.setToIdentity();
    ProjectionMatrix.perspective(60.0f, (float)this->width()/(float)this->height(), 0.3f, 100.0f);
}

void MyWindow::render()
{
    if(!isVisible() || !isExposed())
        return;

    if (!mContext->makeCurrent(this))
        return;

    static bool initialized = false;
    if (!initialized) {
        initialize();
        initialized = true;
    }

    if (mUpdateSize) {
        glViewport(0, 0, size().width(), size().height());
        mUpdateSize = false;
    }

    float deltaT = currentTimeS - tPrev;
    if(tPrev == 0.0f) deltaT = 0.0f;
    tPrev = currentTimeS;
    angle += 0.25f * deltaT;
    if (angle > TwoPI) angle -= TwoPI;

    static float EvolvingVal = 0;
    EvolvingVal += 0.1f;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (displayMode)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mFBOHandle);
        pass1();

        glBindFramebuffer(GL_FRAMEBUFFER, intermediateFBO);
        pass2();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        pass3();
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        pass1();
    }
    mContext->swapBuffers(this);
}

void MyWindow::pass1()
{   
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // *** Draw teapot
    mFuncs->glBindVertexArray(mVAOTeapot);       

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    QVector4D worldLight = QVector4D(0.0f, 4.0f, 2.5f, 1.0f);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass1Index);

        mProgram->setUniformValue("Light.Position",  ViewMatrix * worldLight );
        mProgram->setUniformValue("Light.Intensity", QVector3D(1.0f, 1.0f, 1.0f));

        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        mProgram->setUniformValue("Material.Kd", 0.4f, 0.4f, 0.9f);
        mProgram->setUniformValue("Material.Ks", 1.0f, 1.0f, 1.0f);
        mProgram->setUniformValue("Material.Ka", 0.2f, 0.2f, 0.2f);
        mProgram->setUniformValue("Material.Shininess", 100.0f);

        QMatrix4x4 mv1 = ViewMatrix * ModelMatrixTeapot;
        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);

        glDrawElements(GL_TRIANGLES, 6 * mTeapot->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();

    // *** Draw planes
    mFuncs->glBindVertexArray(mVAOPlane);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass1Index);

        mProgram->setUniformValue("Light.Position",  ViewMatrix * worldLight );
        mProgram->setUniformValue("Light.Intensity", QVector3D(1.0f, 1.0f, 1.0f));

        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        mProgram->setUniformValue("Material.Kd", 0.9f, 0.3f, 0.2f);
        mProgram->setUniformValue("Material.Ks", 1.0f, 1.0f, 1.0f);
        mProgram->setUniformValue("Material.Ka", 0.2f, 0.2f, 0.2f);
        mProgram->setUniformValue("Material.Shininess", 100.0f);

        // back plane
        QMatrix4x4 mvback = ViewMatrix * ModelMatrixBackPlane;
        mProgram->setUniformValue("ModelViewMatrix", mvback);
        mProgram->setUniformValue("NormalMatrix", mvback.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mvback);
        glDrawElements(GL_TRIANGLES, 6 * mPlane->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        // Top plane
        QMatrix4x4 mvtop = ViewMatrix * ModelMatrixTopPlane;
        mProgram->setUniformValue("ModelViewMatrix", mvtop);
        mProgram->setUniformValue("NormalMatrix", mvtop.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mvtop);
        glDrawElements(GL_TRIANGLES, 6 * mPlane->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        // Bot plane
        QMatrix4x4 mvbot = ViewMatrix * ModelMatrixBotPlane;
        mProgram->setUniformValue("ModelViewMatrix", mvbot);
        mProgram->setUniformValue("NormalMatrix", mvbot.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mvbot);
        glDrawElements(GL_TRIANGLES, 6 * mPlane->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();

    // *** Draw sphere
    mFuncs->glBindVertexArray(mVAOSphere);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass1Index);

        mProgram->setUniformValue("Light.Position",  ViewMatrix * worldLight );
        mProgram->setUniformValue("Light.Intensity", QVector3D(1.0f, 1.0f, 1.0f));

        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        mProgram->setUniformValue("Material.Kd", 0.4f, 0.9f, 0.4f);
        mProgram->setUniformValue("Material.Ks", 1.0f, 1.0f, 1.0f);
        mProgram->setUniformValue("Material.Ka", 0.2f, 0.2f, 0.2f);
        mProgram->setUniformValue("Material.Shininess", 100.0f);

        QMatrix4x4 mv1 = ViewMatrix * ModelMatrixSphere;
        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);
        mProgram->setUniformValue("Worldlight",       worldLight);
        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        glDrawElements(GL_TRIANGLES, mSphere->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();
}

void MyWindow::pass2()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderTex);
    glDisable(GL_DEPTH_TEST);

    glClear(GL_COLOR_BUFFER_BIT);

    mFuncs->glBindVertexArray(mVAOFSQuad);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass2Index);

        QMatrix4x4 mv1 ,proj;

        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", proj * mv1);

        // Render the full-screen quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
    mProgram->release();    
}

void MyWindow::pass3()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, intermediateTex);

    mFuncs->glBindVertexArray(mVAOFSQuad);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    mProgram->bind();
    {
        mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass3Index);

        QMatrix4x4 mv1 ,proj;

        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", proj * mv1);

        // Render the full-screen quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }
    mProgram->release();
}

void MyWindow::initShaders()
{
    QOpenGLShader vShader(QOpenGLShader::Vertex);
    QOpenGLShader fShader(QOpenGLShader::Fragment);    
    QFile         shaderFile;
    QByteArray    shaderSource;

    //Simple ADS
    shaderFile.setFileName(":/vshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "vertex compile: " << vShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/fshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "frag   compile: " << fShader.compileSourceCode(shaderSource);

    mProgram = new (QOpenGLShaderProgram);
    mProgram->addShader(&vShader);
    mProgram->addShader(&fShader);
    qDebug() << "shader link: " << mProgram->link();
}

void MyWindow::PrepareTexture(GLenum TextureTarget, const QString& FileName, GLuint& TexObject, bool flip)
{
    QImage TexImg;

    if (!TexImg.load(FileName)) qDebug() << "Erreur chargement texture";
    if (flip==true) TexImg=TexImg.mirrored();

    glGenTextures(1, &TexObject);
    glBindTexture(TextureTarget, TexObject);
    glTexImage2D(TextureTarget, 0, GL_RGB, TexImg.width(), TexImg.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, TexImg.bits());
    glTexParameterf(TextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(TextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void MyWindow::keyPressEvent(QKeyEvent *keyEvent)
{
    switch(keyEvent->key())
    {
        case Qt::Key_P:
            break;
        case Qt::Key_O:
            displayMode = !displayMode;
            break;
        case Qt::Key_Up:
            break;
        case Qt::Key_Down:
            break;
        case Qt::Key_Left:
            break;
        case Qt::Key_Right:
            break;
        case Qt::Key_Delete:
            break;
        case Qt::Key_PageDown:
            break;
        case Qt::Key_Home:
            break;
        case Qt::Key_Z:
            break;
        case Qt::Key_Q:
            break;
        case Qt::Key_S:
            break;
        case Qt::Key_D:
            break;
        case Qt::Key_A:
            break;
        case Qt::Key_E:
            break;
        default:
            break;
    }
}

void MyWindow::printMatrix(const QMatrix4x4& mat)
{
    const float *locMat = mat.transposed().constData();

    for (int i=0; i<4; i++)
    {
        qDebug() << locMat[i*4] << " " << locMat[i*4+1] << " " << locMat[i*4+2] << " " << locMat[i*4+3];
    }
}

void MyWindow::setupFBO() {
    // Generate and bind the framebuffer
    glGenFramebuffers(1, &mFBOHandle);
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOHandle);

    // Create the texture object    
    glGenTextures(1, &renderTex);
    glActiveTexture(GL_TEXTURE0);  // Use texture unit 0
    glBindTexture(GL_TEXTURE_2D, renderTex);
    mFuncs->glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, this->width(), this->height());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Bind the texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTex, 0);

    // Create the depth buffer
    GLuint depthBuf;
    glGenRenderbuffers(1, &depthBuf);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuf);
    mFuncs->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, this->width(), this->height());

    // Bind the depth buffer to the FBO
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depthBuf);

    // Set the targets for the fragment output variables
    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
    mFuncs->glDrawBuffers(1, drawBuffers);

    // Unbind the framebuffer, and revert to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Generate and bind the intermediate framebuffer
    glGenFramebuffers(1, &intermediateFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, intermediateFBO);

    // Create the texture object
    glGenTextures(1, &intermediateTex);
    glActiveTexture(GL_TEXTURE0);  // Use texture unit 0
    glBindTexture(GL_TEXTURE_2D, intermediateTex);
    mFuncs->glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, this->width(), this->height());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Bind the texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, intermediateTex, 0);

    // No depth buffer needed for this FBO

    // Set the targets for the fragment output variables
    mFuncs->glDrawBuffers(1, drawBuffers);

    // Unbind the framebuffer, and revert to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

