//#define FAKE_IT
#define USE_COMPLEX
#define QUADS

#include <algorithm>
#include <new>
#include <GL/glut.h>

#include "MT_MinMax.h"
#include "MT_Point3.h"
#include "MT_Vector3.h"
#include "MT_Quaternion.h"
#include "MT_Matrix3x3.h"
#include "MT_Transform.h"

#include "SM_Object.h"
#include "SM_FhObject.h"
#include "SM_Scene.h"

#include <SOLID/SOLID.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

const MT_Scalar bowl_curv = 0.10;
const MT_Scalar timeStep = 0.02;
const MT_Scalar ground_margin = 0.0;
const MT_Scalar sphere_radius = 0.5;

const MT_Vector3 gravity(0, -9.8, 0);

static MT_Scalar DISTANCE = 5; 

static MT_Scalar ele = 0, azi = 0;
static MT_Point3 eye(0, 0, DISTANCE);
static MT_Point3 center(0, 0, 0);

inline double irnd() { return 2 * MT_random() - 1; }  

static const double SCALE_BOTTOM = 0.5;
static const double SCALE_FACTOR = 2.0;

SM_ShapeProps g_shapeProps = {
	1.0,    // mass
	1.0,    // inertia 
 	0.1,    // linear drag
	0.1,    // angular drag
	{ 1.0, 0.0, 0.0 }, // anisotropic friction
	false,   // do anisotropic friction?
	true,   // do fh?
	true    // do rot fh?
};

SM_MaterialProps g_materialProps = {
	0.7,    // restitution
	0.0,    // friction 
	10.0,    // Fh spring constant
	1.0,    // Fh damping
	0.5,    // Fh distance 
	true    // Fh leveling 
};
 

void toggleIdle();


void newRandom();

void coordSystem() {
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0);
    glVertex3d(0, 0, 0);
    glVertex3d(10, 0, 0);
    glColor3f(0, 1, 0);
    glVertex3d(0, 0, 0);
    glVertex3d(0, 10, 0);
    glColor3f(0, 0, 1);
    glVertex3d(0, 0, 0);
    glVertex3d(0, 0, 10);
    glEnd();
    glEnable(GL_LIGHTING);
}


void display_bbox(const MT_Point3& min, const MT_Point3& max) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glColor3f(0, 1, 1);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_QUAD_STRIP);
    glVertex3d(min[0], min[1], min[2]);
    glVertex3d(min[0], min[1], max[2]);
    glVertex3d(max[0], min[1], min[2]);
    glVertex3d(max[0], min[1], max[2]);
    glVertex3d(max[0], max[1], min[2]);
    glVertex3d(max[0], max[1], max[2]);
    glVertex3d(min[0], max[1], min[2]);
    glVertex3d(min[0], max[1], max[2]);
    glVertex3d(min[0], min[1], min[2]);
    glVertex3d(min[0], min[1], max[2]);
    glEnd();  
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
}




class GLShape {
public:
    virtual void paint(GLdouble *m) const = 0;
};


class GLSphere : public GLShape {
    MT_Scalar radius;
public:
    GLSphere(MT_Scalar r) : radius(r) {}

    void paint(GLdouble *m) const {
        glPushMatrix(); 
        glLoadMatrixd(m);
        coordSystem();
        glutSolidSphere(radius, 20, 20);
        glPopMatrix();
    }
};


class GLBox : public GLShape {
    MT_Vector3 extent;
public:
    GLBox(MT_Scalar x, MT_Scalar y, MT_Scalar z) : 
        extent(x, y, z) {}

    void paint(GLdouble *m) const {
        glPushMatrix(); 
        glLoadMatrixd(m);
        coordSystem();
        glPushMatrix();
        glScaled(extent[0], extent[1], extent[2]);
        glutSolidCube(1.0);
        glPopMatrix();
        glPopMatrix();
    }
};


class GLCone : public GLShape {
    MT_Scalar bottomRadius;
    MT_Scalar height;
    mutable GLuint displayList;

public:
    GLCone(MT_Scalar r, MT_Scalar h) :  
        bottomRadius(r), 
        height(h), 
        displayList(0) {}
  
    void paint(GLdouble *m) const { 
        glPushMatrix(); 
        glLoadMatrixd(m);
        coordSystem();
        if (displayList) glCallList(displayList); 
        else {
            GLUquadricObj *quadObj = gluNewQuadric();
            displayList = glGenLists(1);
            glNewList(displayList, GL_COMPILE_AND_EXECUTE);
            glPushMatrix();
            glRotatef(-90.0, 1.0, 0.0, 0.0);
            glTranslatef(0.0, 0.0, -1.0);
            gluQuadricDrawStyle(quadObj, (GLenum)GLU_FILL);
            gluQuadricNormals(quadObj, (GLenum)GLU_SMOOTH);
            gluCylinder(quadObj, bottomRadius, 0, height, 15, 10);
            glPopMatrix();
            glEndList();
        }
        glPopMatrix();
    }
};

class GLCylinder : public GLShape {
    MT_Scalar radius;
    MT_Scalar height;
    mutable GLuint displayList;

public:
    GLCylinder(MT_Scalar r, MT_Scalar h) : 
        radius(r), 
        height(h), 
        displayList(0) {}

    void paint(GLdouble *m) const { 
        glPushMatrix(); 
        glLoadMatrixd(m);
        coordSystem();
        if (displayList) glCallList(displayList); 
        else {
            GLUquadricObj *quadObj = gluNewQuadric();
            displayList = glGenLists(1);
            glNewList(displayList, GL_COMPILE_AND_EXECUTE);
            glPushMatrix();
            glRotatef(-90.0, 1.0, 0.0, 0.0);
            glTranslatef(0.0, 0.0, -1.0);
            gluQuadricDrawStyle(quadObj, (GLenum)GLU_FILL);
            gluQuadricNormals(quadObj, (GLenum)GLU_SMOOTH);
            gluCylinder(quadObj, radius, radius, height, 15, 10);
            glPopMatrix ();
            glEndList();
        }
        glPopMatrix();
    }
};

class Object;

class Callback : public SM_Callback {
public:
	Callback(Object& object) : m_object(object) {}

	virtual void do_me();

private:
	Object& m_object;
};


class Object {
public:
    Object(GLShape *gl_shape, SM_Object& object) :
        m_gl_shape(gl_shape),
        m_object(object),
		m_callback(*this)
        {
			m_object.registerCallback(m_callback);
		}
	
    ~Object() {}

    void paint() {
		if (m_gl_shape) {
			m_gl_shape->paint(m);
			//      display_bbox(m_bbox.lower(), m_bbox.upper());
		}
	}

	void print_reaction_force() const { 
		std::cout << m_object.getReactionForce() << std::endl; 
	}
	
	MT_Vector3 getAhead() {
		return MT_Vector3(&m[4]);
	}

	MT_Vector3 getUp() {
		return MT_Vector3(&m[8]);
	}

    void clearMomentum() {
		m_object.clearMomentum();
    }
	
    void setMargin(MT_Scalar margin) {
        m_object.setMargin(margin);
    }
	
    void setScaling(const MT_Vector3& scaling) {
        m_object.setScaling(scaling); 
    }

    const MT_Point3& getPosition() {
		return m_object.getPosition();
    }

    void setPosition(const MT_Point3& pos) {
		m_object.setPosition(pos);
    }

    void setOrientation(const MT_Quaternion& orn) {
        m_object.setOrientation(orn);
    }

    void applyCenterForce(const MT_Vector3& force) {
		m_object.applyCenterForce(force);
    }

    void applyTorque(const MT_Vector3& torque) {
		m_object.applyTorque(torque);
    }

    MT_Point3 getWorldCoord(const MT_Point3& local) const {
        return m_object.getWorldCoord(local);
    }

    MT_Vector3 getLinearVelocity() const {
        return m_object.getLinearVelocity();
    }

    MT_Vector3 getAngularVelocity() const {
        return m_object.getAngularVelocity();
    }

    void setMatrix() {
		m_object.calcXform();
		m_object.getMatrix(m);
    }

    const double *getMatrix() {
		m_object.calcXform();
		return m_object.getMatrix();
    }

private:
    GLShape        *m_gl_shape;
    SM_Object&      m_object;
	DT_Scalar       m[16];
	Callback        m_callback;
};



const MT_Scalar SPACE_SIZE = 2;

static GLSphere gl_sphere(sphere_radius);
static GLBox gl_ground(50.0, 0.0, 50.0);



#ifdef USE_COMPLEX

const int GRID_SCALE = 10;
const MT_Scalar GRID_UNIT  = 25.0 / GRID_SCALE;

DT_ShapeHandle createComplex() {
    DT_ShapeHandle shape = DT_NewComplexShape();
    for (int i0 = -GRID_SCALE; i0 != GRID_SCALE; ++i0) {
        for (int j0 = -GRID_SCALE; j0 != GRID_SCALE; ++j0) {
            int i1 = i0 + 1;
            int j1 = j0 + 1;
#ifdef QUADS
            DT_Begin();
            DT_Vertex(GRID_UNIT * i0, bowl_curv * i0*i0, GRID_UNIT * j0);
            DT_Vertex(GRID_UNIT * i0, bowl_curv * i0*i0, GRID_UNIT * j1);
            DT_Vertex(GRID_UNIT * i1, bowl_curv * i1*i1, GRID_UNIT * j1);
            DT_Vertex(GRID_UNIT * i1, bowl_curv * i1*i1, GRID_UNIT * j0);
            DT_End();
#else
            DT_Begin();
            DT_Vertex(GRID_UNIT * i0, 0, GRID_UNIT * j0);
            DT_Vertex(GRID_UNIT * i0, 0, GRID_UNIT * j1);
            DT_Vertex(GRID_UNIT * i1, 0, GRID_UNIT * j1);
            DT_End();

            DT_Begin();
            DT_Vertex(GRID_UNIT * i0, 0, GRID_UNIT * j1);
            DT_Vertex(GRID_UNIT * i1, 0, GRID_UNIT * j1);
            DT_Vertex(GRID_UNIT * i1, 0, GRID_UNIT * j0);
            DT_End();
#endif

        }
    }
    DT_EndComplexShape();
    return shape;
}


static DT_ShapeHandle ground_shape = createComplex();

#else 

static DT_ShapeHandle ground_shape = DT_Box(50, 0, 50);

#endif

static SM_Object sm_ground(ground_shape, &g_materialProps, 0, 0);
static Object    ground(&gl_ground, sm_ground);

static SM_Object   sm_sphere(DT_Sphere(0.0), &g_materialProps, &g_shapeProps, 0);
static Object      object(&gl_sphere, sm_sphere);


static SM_Scene   g_scene;


bool g_hit = false;
MT_Point3 g_spot;
MT_Vector3 g_normal;


void Callback::do_me()
{
	m_object.setMatrix();
	m_object.print_reaction_force();
}

void myinit(void) {

    GLfloat light_ambient[] = { 0.0, 0.0, 0.0, 1.0 };
    GLfloat light_diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 };

    /*	light_position is NOT default value	*/
    GLfloat light_position0[] = { 1.0, 1.0, 1.0, 0.0 };
    GLfloat light_position1[] = { -1.0, -1.0, -1.0, 0.0 };
  
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT0, GL_POSITION, light_position0);
  
    glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular);
    glLightfv(GL_LIGHT1, GL_POSITION, light_position1);
  

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
  
    glShadeModel(GL_SMOOTH);
  
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
  
    //  glEnable(GL_CULL_FACE);
    //  glCullFace(GL_BACK);

    ground.setPosition(MT_Point3(0, -10, 0));
    ground.setOrientation(MT_Quaternion(0, 0, 0, 1));
    ground.setMatrix();
    center.setValue(0.0, 0.0, 0.0);
	sm_ground.setMargin(ground_margin);

	g_scene.setForceField(gravity);
    g_scene.add(sm_ground);

	object.setMargin(sphere_radius);
	
    g_scene.add(sm_sphere);


    newRandom();
}


//MT_Point3 cp1, cp2;
//bool intersection;

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

    ground.paint();
	object.paint();
	
	if (g_hit) {
		glDisable(GL_LIGHTING);
		glColor3f(1, 0, 0);

		glPointSize(5);
		glBegin(GL_POINTS);
		glVertex3d(g_spot[0], g_spot[1], g_spot[2]);
		glEnd();
		glPointSize(1);

		glColor3f(1, 1, 0);
        glBegin(GL_LINES); 
		glVertex3d(g_spot[0], g_spot[1], g_spot[2]);
		glVertex3d(g_spot[0] + g_normal[0], 
				   g_spot[1] + g_normal[1],
				   g_spot[2] + g_normal[2]);
        glEnd();
		glEnable(GL_LIGHTING);
	}

	

#ifdef COLLISION
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glColor3f(1, 1, 0);
    if (intersection) {
        glPointSize(5);
        glBegin(GL_POINTS);
        glVertex3d(cp1[0], cp1[1], cp1[2]);
        glEnd();
        glPointSize(1);
    }
    else {
        glBegin(GL_LINES); 
        glVertex3d(cp1[0], cp1[1], cp1[2]);
        glVertex3d(cp2[0], cp2[1], cp2[2]);
        glEnd();
    }
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
#endif

    glFlush();
    glutSwapBuffers();
}





void newRandom() {
	object.setPosition(MT_Point3(0, 0, 0));
	object.setOrientation(MT_Quaternion::random());
	object.clearMomentum();
	object.setMatrix();
	
    display();
}

void moveAndDisplay() {
	g_scene.proceed(timeStep, 0.01);

    display();
	g_hit = false;
}


void turn_left() {
	object.applyTorque(5.0 * object.getUp());
}

void turn_right() {
	object.applyTorque(-5.0 * object.getUp());
}

void forward() {
	object.applyCenterForce(10.0 * object.getAhead());
}

void backward() {
	object.applyCenterForce(-10.0 * object.getAhead());
}

void jump() {
	object.applyCenterForce(MT_Vector3(0.0, 200.0, 0.0));
}


void toggleIdle() {
    static bool idle = true;
    if (idle) {
        glutIdleFunc(moveAndDisplay);
        idle = false;
    }
    else {
        glutIdleFunc(NULL);
        idle = true;
    }
}


void setCamera() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -1.0, 1.0, 1.0, 200.0);
    MT_Scalar rele = MT_radians(ele);
    MT_Scalar razi = MT_radians(azi);
    eye.setValue(DISTANCE * sin(razi) * cos(rele), 
                 DISTANCE * sin(rele),
                 DISTANCE * cos(razi) * cos(rele));
    gluLookAt(eye[0], eye[1], eye[2], 
              center[0], center[1], center[2], 
              0, 1, 0);
    glMatrixMode(GL_MODELVIEW);
    display();
}

const MT_Scalar STEPSIZE = 5;

void stepLeft() { azi -= STEPSIZE; if (azi < 0) azi += 360; setCamera(); }
void stepRight() { azi += STEPSIZE; if (azi >= 360) azi -= 360; setCamera(); }
void stepFront() { ele += STEPSIZE; if (azi >= 360) azi -= 360; setCamera(); }
void stepBack() { ele -= STEPSIZE; if (azi < 0) azi += 360; setCamera(); }
void zoomIn() { DISTANCE -= 1; setCamera(); }
void zoomOut() { DISTANCE += 1; setCamera(); }


void myReshape(int w, int h) {
    glViewport(0, 0, w, h);
    setCamera();
}

void myKeyboard(unsigned char key, int x, int y)
{
    switch (key) 
    {
	case 'w': forward();	break;
	case 's': backward();	break;
	case 'a': turn_left();	break;
	case 'd': turn_right();	break;
	case 'e': jump(); break;	
    case 'l' : stepLeft(); break;
    case 'r' : stepRight(); break;
    case 'f' : stepFront(); break;
    case 'b' : stepBack(); break;
    case 'z' : zoomIn(); break;
    case 'x' : zoomOut(); break;
    case 'i' : toggleIdle(); break;
    case ' ' : newRandom(); break;
    default:
//        std::cout << "unused key : " << key << std::endl;
        break;
    }
}

void mySpecial(int key, int x, int y)
{
    switch (key) 
    {
    case GLUT_KEY_LEFT : stepLeft(); break;
    case GLUT_KEY_RIGHT : stepRight(); break;
    case GLUT_KEY_UP : stepFront(); break;
    case GLUT_KEY_DOWN : stepBack(); break;
    case GLUT_KEY_PAGE_UP : zoomIn(); break;
    case GLUT_KEY_PAGE_DOWN : zoomOut(); break;
    case GLUT_KEY_HOME : toggleIdle(); break;
    default:
//        std::cout << "unused (special) key : " << key << std::endl;
        break;
    }
}

void goodbye( void)
{
	g_scene.remove(sm_ground);
	g_scene.remove(sm_sphere);

    std::cout << "goodbye ..." << std::endl;
    exit(0);
}

void menu(int choice)
{

    static int fullScreen = 0;
    static int px, py, sx, sy;
 
    switch(choice) {
    case 1:
        if (fullScreen == 1) {
            glutPositionWindow(px,py);
            glutReshapeWindow(sx,sy);
            glutChangeToMenuEntry(1,"Full Screen",1);
            fullScreen = 0;
        } else {
            px=glutGet((GLenum)GLUT_WINDOW_X);
            py=glutGet((GLenum)GLUT_WINDOW_Y);
            sx=glutGet((GLenum)GLUT_WINDOW_WIDTH);
            sy=glutGet((GLenum)GLUT_WINDOW_HEIGHT);
            glutFullScreen();
            glutChangeToMenuEntry(1,"Close Full Screen",1);
            fullScreen = 1;
        }
        break;
    case 2:
        toggleIdle();
        break;
    case 3:
        goodbye();
        break;
    default:
        break;
    }
}

void createMenu()
{
    glutCreateMenu(menu);
    glutAddMenuEntry("Full Screen", 1);
    glutAddMenuEntry("Toggle Idle (Start/Stop)", 2);
    glutAddMenuEntry("Quit", 3);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowPosition(0, 0);
    glutInitWindowSize(500, 500);
    glutCreateWindow("Physics demo");

    myinit();
    glutKeyboardFunc(myKeyboard);
    glutSpecialFunc(mySpecial);
    glutReshapeFunc(myReshape);
    createMenu();
    glutIdleFunc(NULL);

    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}







