#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"
#endif

static GLfloat spin, spin2 = 0.0;
float angle = 0;
using namespace std;

float lastx, lasty;
GLint stencilBits;
static int viewx = 50;
static int viewy = 24;
static int viewz = 80;

float rot = 0;

//train 2D
class Terrain {
private:
	int w; //Width
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	~Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};

//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
Terrain* loadTerrain(const char* filename, float height) {
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.5f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}

float _angle = 60.0f;
Terrain* _terrain;
Terrain* _terrainTanah;
Terrain* _terrainAir;

void cleanup() {
	delete _terrain;
	delete _terrainTanah;
}

/*
 void handleKeypress(unsigned char key, int x, int y) {
 switch (key) {
 case 27: //Escape key
 cleanup();
 exit(0);
 }
 }
 */
void initRendering() {
	glEnable( GL_DEPTH_TEST);
	glEnable( GL_COLOR_MATERIAL);
	glEnable( GL_LIGHTING);
	glEnable( GL_LIGHT0);
	glEnable( GL_NORMALIZE);
	glShadeModel( GL_SMOOTH);
}

void drawScene() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	/*
	 glMatrixMode(GL_MODELVIEW);
	 glLoadIdentity();
	 glTranslatef(0.0f, 0.0f, -10.0f);
	 glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
	 glRotatef(-_angle, 0.0f, 1.0f, 0.0f);

	 GLfloat ambientColor[] = {0.4f, 0.4f, 0.4f, 1.0f};
	 glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientColor);

	 GLfloat lightColor0[] = {0.6f, 0.6f, 0.6f, 1.0f};
	 GLfloat lightPos0[] = {-0.5f, 0.8f, 0.1f, 0.0f};
	 glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor0);
	 glLightfv(GL_LIGHT0, GL_POSITION, lightPos0);
	 */
	float scale = 500.0f / max(_terrain->width() - 1, _terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (_terrain->width() - 1) / 2, 0.0f,
			-(float) (_terrain->length() - 1) / 2);

	glColor3f(0.3f, 0.9f, 0.0f);
	for (int z = 0; z < _terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin( GL_TRIANGLE_STRIP);
		for (int x = 0; x < _terrain->width(); x++) {
			Vec3f normal = _terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, _terrain->getHeight(x, z), z);
			normal = _terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, _terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}

void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {
	//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	/*
	 glMatrixMode(GL_MODELVIEW);
	 glLoadIdentity();
	 glTranslatef(0.0f, 0.0f, -10.0f);
	 glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
	 glRotatef(-_angle, 0.0f, 1.0f, 0.0f);

	 GLfloat ambientColor[] = {0.4f, 0.4f, 0.4f, 1.0f};
	 glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientColor);

	 GLfloat lightColor0[] = {0.6f, 0.6f, 0.6f, 1.0f};
	 GLfloat lightPos0[] = {-0.5f, 0.8f, 0.1f, 0.0f};
	 glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor0);
	 glLightfv(GL_LIGHT0, GL_POSITION, lightPos0);
	 */
	float scale = 500.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin( GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}

void update(int value) {
	/*
	 _angle += 1.0f;
	 if (_angle > 360) {
	 _angle -= 360;
	 }
	 */
	glutPostRedisplay();
	glutTimerFunc(25, update, 0);
}
/*
 void handleResize(int w, int h) {
 glViewport(0, 0, w, h);
 glMatrixMode(GL_PROJECTION);
 glLoadIdentity();
 gluPerspective(45.0, (double)w / (double)h, 1.0, 200.0);
 }
 */

GLuint texture[40];
void freetexture(GLuint texture) {
	glDeleteTextures(1, &texture);
}

GLuint loadtextures(const char *filename, int width, int height) {
	GLuint texture;

	unsigned char *data;
	FILE *file;

	/*
	 if (filename == "water.bmp") {
	 Image *img = loadBMP(filename);
	 width = img->width;
	 height = img->height;
	 }
	 */

	file = fopen(filename, "rb");
	if (file == NULL)
		return 0;

	data = (unsigned char *) malloc(width * height * 3);
	fread(data, width * height * 3, 1, file);

	fclose(file);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_LINEAR_MIPMAP_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameterf(  GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, width, height, GL_RGB,
			GL_UNSIGNED_BYTE, data);

	data = NULL;

	return texture;
}

GLuint loadtextures3D(const char *filename, int width, int height) {
	GLuint texture;

	unsigned char *data;
	FILE *file;

	file = fopen(filename, "rb");
	if (file == NULL)
		return 0;

	data = (unsigned char *) malloc(width * height * 3);
	fread(data, width * height * 3, 1, file);

	fclose(file);

	glGenTextures(1, &texture);
	//glBindTexture(GL_TEXTURE_3D, texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	//glTexParameterf(  GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	//	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	//glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, width, height, 8, 0, GL_RGB,GL_UNSIGNED_BYTE,width);
	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	//gluBuild3DMipmapLevels( GL_TEXTURE_2D, 3, width, height, GL_RGB, GL_UNSIGNED_BYTE, data, );

	data = NULL;

	return texture;
}

const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

unsigned int texture_lantai;
unsigned int texture_pagar;
unsigned int kayu;
unsigned int LoadTextureFromBmpFile(char *filename);

void drawBebek() {

	GLfloat xS = 20;
	GLfloat yS = 12;
	GLfloat zS = 50;
	//kepala
	glColor3f(0.88, 0.88, 0.1);
	//glPushMatrix();
	glPushMatrix();

	glTranslatef(xS + 1, yS + 3, zS);
	glScalef(3 * 4, 3 * 4, 3.5 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();

	glColor3f(0.8, 0.3, 0.4);
	//glPushMatrix();

	glPushMatrix();
	glTranslatef(xS + 1, yS + 6, zS);
	glScalef(3 * 4, 3 * 4, 0.5 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();

	GLfloat x = 0.6 - 50;
	GLfloat y = 2 + 12;
	GLfloat z = 1.4 + 24;

	glColor3f(0.7, 0.6, 0.4);
	glPushMatrix();
	glRotatef(95, 0, 1, 0);
	glTranslatef(x - 4.4, y + 2, z - 4.6);
	glScalef(1.5 * 4, 1.5 * 4, 1.5 * 4);

	glutSolidSphere(0.3, 20, 30);

	glPopMatrix();

	glColor3f(0.0, 0.0, 0.0);
	glPushMatrix();
	glRotatef(95, 0, 1, 0);
	glTranslatef(x - 4.7, y + 2.1, z - 3.4);
	glScalef(1.5 * 3, 1.5 * 3, 1.5 * 3);
	glutSolidSphere(0.18, 20, 30);

	glPopMatrix();

	glPushMatrix();
	glColor3f(0.7, 0.6, 0.4);
	glRotatef(83, 0, 1, 0);
	glTranslatef(x + 4.9, y + 2.1, z + 5.8);
	glScalef(1.5 * 4, 1.5 * 4, 1.5 * 4);
	glutSolidSphere(0.3, 20, 30);

	glPopMatrix();

	glPushMatrix();
	glColor3f(0.0, 0.0, 0.0);
	glRotatef(83, 0, 1, 0);
	glTranslatef(x + 4.9, y + 2.1, z + 6.65);
	glScalef(1.5 * 4, 1.5 * 4, 1.5 * 4);
	glutSolidSphere(0.18, 20, 30);

	glPopMatrix();

	glColor3f(0.88, 0.88, 0.3);

	//mulut


	glPushMatrix();

	glTranslatef(xS + 8, yS + 2, zS);
	glRotatef(180, 0, 0, 1);
	glScalef(4 * 3, 0.6 * 3, 2.5 * 3);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();
	//

	glColor3f(0.88, 0.2, 0.5);

	glPushMatrix();

	glTranslatef(xS + 1, yS - 3, zS);
	glScalef(3 * 4, 3 * 4, 3.5 * 4);
	glRotatef(90, 1, 0, 0);

	glutSolidTorus(0.2, 0.3, 20, 20);

	//badan agak atas
	glPopMatrix();

	glColor3f(0.88, 0.88, 0.1);

	glPushMatrix();
	glTranslatef(xS + 1, yS - 6, zS - 0.4);
	glScalef(3 * 4, 3 * 4, 3.5 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();

	//badan utama
	glPushMatrix();
	glColor3f(0.88, 0.88, 0.1);
	glTranslatef(xS - 7, yS - 10, zS - 0.3);
	glScalef(5 * 4, 4 * 4, 4 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();

	//badan depan bawah
	glPushMatrix();
	glColor3f(0.88, 0.88, 0.1);
	glTranslatef(xS, yS - 8, zS - 0.2);
	glScalef(3 * 4, 3 * 4, 3.5 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();
	//
	int sudutSayap = 80;
	//sayap
	for (int i = 0; i < 3; i++) {
		//kanan
		glPushMatrix();
		glTranslatef(xS - 6, yS - 5 - i, zS + 7);
		glRotatef(sudutSayap, 1, 0, 0);
		glRotatef(-15 + (i * 8), 0, 1, 0);
		glRotatef(-8.5, 0, 0, 1);
		glScalef(4 * 4.5 - i, 0.6 * 3, 2.5 * 3 - 1);

		glutSolidSphere(0.5, 20, 30);
		glPopMatrix();
		//kiri
		glPushMatrix();
		glTranslatef(xS - 6, yS - 5 - i, zS - 8.2);
		glRotatef(sudutSayap + 15, 1, 0, 0);
		glRotatef(-15 + (i * 8), 0, 1, 0);
		glRotatef(10.5, 0, 0, 1);
		glScalef(4 * 4.5 - i, 0.6 * 3, 2.5 * 3 - 1);

		glutSolidSphere(0.5, 20, 30);
		glPopMatrix();

	}

	//buntut
	int sudut = 145;
	for (int i = 0; i < 4; i++) {
		glPushMatrix();

		glTranslatef(xS - 12.5 + i, yS - 5 - i, zS);
		glRotatef(sudut, 0, 0, 1);
		glScalef(4 * 6.5, 0.6 * 6, (2.5 * 4) - i);

		glutSolidSphere(0.5, 20, 30);
		glPopMatrix();
		sudut += 10;
	}

	//glutSolidCone(2,20,30,40);
	/*
	 glScalef(0.5, 0.5, 0.5);
	 if (kualitasGambar > DRAFT) {
	 glutSolidCube(3);

	 } else {
	 glutWireCube(3);

	 }

	 glTranslatef(-4.8, -0.1, 0.0);
	 */
	/*
	 glTranslatef(4.6, 0.0, 0.0);
	 if (kualitasGambar > DRAFT)
	 glutSolidCube(1);
	 else
	 glutWireCube(1);
	 glPopMatrix();
	 */

	glPopMatrix();
}

void balok3() {
	glBegin( GL_POLYGON);
	//sisi depan
	glTexCoord2f(0, 0);
	glVertex3f(-0.5, -0.5, 0.5);

	glTexCoord2f(0, 1);
	glVertex3f(-0.5, 0.5, 0.5);

	glTexCoord2f(1, 1);
	glVertex3f(0.5, 0.5, 0.5);

	glTexCoord2f(1, 0);
	glVertex3f(0.5, -0.5, 0.5);
	glEnd();

	glBegin(GL_POLYGON);
	//sisi  kanan
	glTexCoord2f(0, 1);
	glVertex3f(0.5, 0.5, 0.5);

	glTexCoord2f(0, 0);
	glVertex3f(0.5, -0.5, 0.5);

	glTexCoord2f(1, 0);
	glVertex3f(0.5, -0.5, -0.5);

	glTexCoord2f(1, 1);
	glVertex3f(0.5, 0.5, -0.5);
	glEnd();

	glBegin(GL_POLYGON);
	//sisi  belakang
	glTexCoord2f(0, 0);
	glVertex3f(0.5, -0.5, -0.5);

	glTexCoord2f(0, 1);
	glVertex3f(0.5, 0.5, -0.5);

	glTexCoord2f(1, 1);
	glVertex3f(-0.5, 0.5, -0.5);

	glTexCoord2f(1, 0);
	glVertex3f(-0.5, -0.5, -0.5);
	glEnd();

	glBegin(GL_POLYGON);
	//sisi kiri balok
	glTexCoord2f(0, 1);
	glVertex3f(-0.5, 0.5, -0.5);

	glTexCoord2f(0, 0);
	glVertex3f(-0.5, -0.5, -0.5);

	glTexCoord2f(1, 0);
	glVertex3f(-0.5, -0.5, 0.5);

	glTexCoord2f(1, 1);
	glVertex3f(-0.5, 0.5, 0.5);
	glEnd();

	glBegin(GL_POLYGON);
	//sisi atas balok
	glTexCoord2f(0, 0);
	glVertex3f(-0.5, 0.5, 0.5);

	glTexCoord2f(1, 0);
	glVertex3f(0.5, 0.5, 0.5);

	glTexCoord2f(1, 1);
	glVertex3f(0.5, 0.5, -0.5);

	glTexCoord2f(0, 1);
	glVertex3f(-0.5, 0.5, -0.5);
	glEnd();

	glBegin(GL_POLYGON);
	//sisi bawah balok
	glTexCoord2f(0, 1);
	glVertex3f(-0.5, -0.5, 0.5);

	glTexCoord2f(1, 1);
	glVertex3f(0.5, -0.5, 0.5);

	glTexCoord2f(1, 0);
	glVertex3f(0.5, -0.5, -0.5);

	glTexCoord2f(0, 0);
	glVertex3f(-0.5, -0.5, -0.5);
	glEnd();
}
void bangku() {

	//kaki bangku
	//pilih_texture(2);
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glScaled(0.2, 0.9, 0.2);
	glTranslated(0.0f, 0.7, 0.0f);
	balok3();
	glPopMatrix();

	//pilih_texture(2);
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glScaled(0.2, 0.9, 0.2);
	glTranslated(0.0f, 0.7, 5.0f);
	balok3();
	glPopMatrix();

	// pilih_texture(2);
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glScaled(0.2, 0.9, 0.2);
	glTranslated(30.0f, 0.7, 0.0f);
	balok3();
	glPopMatrix();

	//pilih_texture(2);
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glScaled(0.2, 0.9, 0.2);
	glTranslated(30.0f, 0.7, 5.0f);
	balok3();
	glPopMatrix();

	//alas bangku
	//pilih_texture(2);
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glScaled(7.0, 0.2, 1.5);
	glTranslated(0.43f, 5.0, 0.32f);
	balok3();
	glPopMatrix();

	//pilih_texture(2);
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glScaled(0.18, 0.8, 0.18);
	glTranslated(0.0f, 1.8, 0.0f);
	balok3();
	glPopMatrix();

	//pilih_texture(2);
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glScaled(0.18, 0.8, 0.18);
	glTranslated(34.0f, 1.8, 0.0f);
	balok3();
	glPopMatrix();

	//pilih_texture(2);
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glRotatef(90.0, 1.0, 0.0, 0.0);
	glScaled(7.0, 0.2, 0.8);
	glTranslated(0.440f, 0.0, -2.5f);
	balok3();
	glPopMatrix();
}

void batu() {

	//glColor3f(0, 0, 0);
	//glScaled(0,0,0);
	//glTranslated(0,0,0);
	glutSolidIcosahedron();
}

void balok() {
	glPushMatrix();
	glColor3f(1, 0, 0);
	glScaled(17, 1.2, 6.5);
	glTranslated(-0.32, 2.1, -0.55);
	glutSolidCube(1);
	glPopMatrix();

}

void balok2() {
	glColor3f(0, 0, 0);

	glRotatef(90, 0, 0, 1);
	glTranslated(1, 2, 1);
	glutSolidCube(5);
}

void kotak() {
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	glScalef(30, 1, 30);
	glRotatef(45, 0, 1, 0);
	glRotatef(90, 1, 0, 0);
	glTranslatef(-1.2, 1.7, -0.5);

	glBegin( GL_QUADS);
	glTexCoord2f(1, 1);
	glVertex3f(-1, -1, 0);
	glTexCoord2f(1, 0);
	glVertex3f(-1, 1, 0);
	glTexCoord2f(0, 0);
	glVertex3f(1, 1, 0);
	glTexCoord2f(0, 1);
	glVertex3f(1, -1, 0);
	glEnd();
	glPopMatrix();
}

void kolam() { //Kolam
	glPushMatrix();
	glRotatef(90, 1, 0, 0);
	glTranslatef(11, 60, 0);
	glColor3f(1, 0, 1);
	glScaled(30, 30, 17);
	glutSolidTorus(0.1, 1.4, 4, 4);
	glPopMatrix();

}
//Tanah
void tanah(void) {
	glPushMatrix();
	glBindTexture(GL_TEXTURE_2D, texture[2]);
	glColor4f(1, 1, 1, 1);
	glRotatef(180, 0, 0, 1);
	glScalef(80, 0, 120);
	glBegin( GL_QUADS);
	glTexCoord2f(1, 0);
	glVertex3f(-1, -1, 1);
	glTexCoord2f(1, 1);
	glVertex3f(-1, 1, -0.0);
	glTexCoord2f(0, 1);
	glVertex3f(1, 1, -0.0);
	glTexCoord2f(0, 0);
	glVertex3f(1, -1, 1);
	glEnd();
	glPopMatrix();
}

/*void lampu(){
 glPushMatrix(); 
 glutSolidSphere (16, 50, 16);
 glScaled(-7,1,1);                                    
 //glTranslated(-0.32,2.1,-0.55);
 
 glPopMatrix();
 }  
 */

void display(void) {
	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0f);
	glClearColor(0.0, 0.6, 0.8, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); //clear the buffers
	glLoadIdentity();
	gluLookAt(viewx, viewy, viewz, 0.0, 0.0, 5.0, 0.0, 1.0, 0.0);

	glPushMatrix();

	//	glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawScene();

	glPopMatrix();

	glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrainTanah, 0.7f, 0.2f, 0.1f);
	glPopMatrix();

	glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrainAir, 0.0f, 0.2f, 0.5f);
	glPopMatrix();

	/*  glPushMatrix();
	 glRotatef(270,1,0,0);
	 glColor3f(1,1,1);
	 glTranslatef(-1,-60,5);
	 glScaled(4,4,0);   
	 createcircle(0,10,0);    
	 glPopMatrix(); 
	 */
	glPushMatrix();
	glColor3f(1, 0, 0);
	glTranslatef(-7, 0, 15);
	glScalef(0.5, 0.5, 0.5);
	glRotatef(30, 0, 1, 0);
	//glBindTexture(GL_TEXTURE_3D,texture[0]);
	drawBebek();
	glPopMatrix();

	glPushMatrix();
	glColor3f(1, 0, 0);
	glTranslatef(35, 0, 33);
	glScalef(0.7, 0.7, 0.7);
	glRotatef(-30, 0, 1, 0);
	//glBindTexture(GL_TEXTURE_3D,texture[0]);
	drawBebek();
	glPopMatrix();

	glPushMatrix();
	glColor3f(1, 0, 0);
	glTranslatef(025, 0, 109);
	glScalef(0.5, 0.5, 0.5);
	glRotatef(180, 0, 1, 0);
	//glBindTexture(GL_TEXTURE_3D,texture[0]);
	drawBebek();
	glPopMatrix();
	//------------------------- batu gede ------------------- //

	glPushMatrix();
	glScaled(7, 7, 7);
	glRotatef(0, 1, 0, 0);
	glTranslated(1, 0, 10);
	glColor3f(0, 0, 0);
	batu();
	glPopMatrix();

	//------------------------- batu kecil ------------------- //
	glPushMatrix();
	glScaled(3, 3, 3);
	glTranslated(3, 0, 8);
	glColor3f(0, 0, 0);
	batu();
	glPopMatrix();

	//----------------------- end batubatuan ---------------- //


	//-----------------------------------------------------------------------------------------
	// start. Pinggiran kolam    / pinggiran yang mengelilingi kolam
	//-----------------------------------------------------------------------------------------
	glPushMatrix();
	kolam();
	glPopMatrix();
	//-----------------------------------------------------------------------------------------
	// end. Pinggiran Kolam    / p pinggiran yang mengelilingi kolam
	//-----------------------------------------------------------------------------------------


	//-----------------------------------------------------------------------------------------
	// start. BANGKU    / bangku yang berada di pinggir kolam
	//-----------------------------------------------------------------------------------------
	glPushMatrix();
	glTranslated(-65, -1, 45);
	glRotatef(45, 0, 1, 0);
	glScaled(7, 7, 7);
	bangku();
	glPopMatrix();

	glPushMatrix();
	glTranslated(-40, -1, 110);
	glRotatef(-230, 0, 1, 0);
	glScaled(7, 7, 7);
	bangku();
	glPopMatrix();

	//-----------------------------------------------------------------------------------------
	// end. BANGKU   / bangku yang berada di pinggir kolam
	//-----------------------------------------------------------------------------------------


	/*glPushMatrix();
	 glRotatef(90, 1,0,0);
	 glTranslatef(0,60,0);
	 glColor3f(1, 1, 0.5);
	 glScaled(40,30,17);
	 glutSolidTorus(0.1, 1, 30, 90);
	 glPopMatrix(); */

	/*    //Taman Tengah
	 glPushMatrix();    //Mulai Gambar
	 //glBindTexture(GL_TEXTURE_2D,texture[0]);
	 glColor3f(0.1,0.7,0.2);
	 glTranslatef(3,0,60);
	 glScaled(11,2,9.3);
	 glutSolidSphere(4,25,17);
	 glPopMatrix();  //penutup objek
	 */

	//Sphere
	/*glPushMatrix();
	 glColor3f(1.0,1.0,1.0);
	 glTranslatef(78.5,18,118);
	 glutSolidSphere(4,25,17);
	 glPopMatrix();
	 
	 glPushMatrix();    
	 glColor3f(1.0,1.0,1.0);
	 glTranslatef(-78,18,118);
	 glutSolidSphere(4,25,17);
	 glPopMatrix();
	 
	 glPushMatrix();    
	 glColor3f(1.0,1.0,1.0);
	 glTranslatef(-78,18,2.5);
	 glutSolidSphere(4,25,17);
	 glPopMatrix();
	 
	 glPushMatrix();    
	 glColor3f(1.0,1.0,1.0);
	 glTranslatef(78.5,18,2.5);
	 glutSolidSphere(4,25,17);
	 glPopMatrix();    
	 */
	//glTranslatef(0, 0, -10);


	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); //disable the color mask
	glDepthMask( GL_FALSE); //disable the depth mask

	glEnable( GL_STENCIL_TEST); //enable the stencil testing

	glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
	glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE); //set the stencil buffer to replace our next lot of data

	//ground
	//tanah(); //set the data plane to be replaced
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); //enable the color mask
	glDepthMask( GL_TRUE); //enable the depth mask

	glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); //set the stencil buffer to keep our next lot of data

	glDisable( GL_DEPTH_TEST); //disable depth testing of the reflection

	// glPopMatrix();  
	glEnable(GL_DEPTH_TEST); //enable the depth testing
	glDisable(GL_STENCIL_TEST); //disable the stencil testing
	//end of ground
	glEnable( GL_BLEND); //enable alpha blending
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //set the blending function
	glRotated(1, 0, 0, 0);

	//-----------------------------------------------------------------------------------------
	// start. L A N D S C A P E  / mulai buat tanah,ground area
	//-----------------------------------------------------------------------------------------

	tanah();
	glDisable(GL_BLEND); //disable alpha blending


	//-----------------------------------------------------------------------------------------
	// end. L A N D S C A P E  / akhir tanah, ground area
	//-----------------------------------------------------------------------------------------


	//-----------------------------------------------------------------------------------------
	// start. P A G A R    / pagar yang mengelilingi taman
	//-----------------------------------------------------------------------------------------
	// kanan atas
	glPushMatrix();
	glScaled(3, 1, 0.5);
	glRotatef(-90, 0, 0, 1);
	glTranslatef(-3, 23.5, 8);
	glColor3f(0, 1, 0);
	balok();
	glPopMatrix();

	// kiri atas
	glPushMatrix();
	glScaled(3, 1, 0.5);
	glRotatef(-90, 0, 0, 1);
	glTranslatef(-3, -28.5, 8);
	glColor3f(1, 0, 0);
	balok();
	glPopMatrix();

	glPushMatrix();
	glScaled(3, 1, 0.5);
	glRotatef(90, 0, 0, 1);
	glTranslatef(14, 23.5, 240);
	glColor3f(1, 0, 0);
	balok();
	glPopMatrix();

	glPushMatrix();
	glScaled(3, 1, 0.5);
	glRotatef(90, 0, 0, 1);
	glTranslatef(14, -28.6, 240);
	glColor3f(1, 0, 0);
	balok();
	glPopMatrix();

	glPushMatrix();
	glScaled(3, 1, 0.5);
	glRotatef(90, 0, 0, 1);
	glTranslatef(14, -28.6, 240);
	glColor3f(1, 0, 0);
	balok();
	glPopMatrix();

	//Pagar
	glPushMatrix();
	glScaled(33, 1, 1);
	glRotatef(90, 0, 0, 1);
	glTranslatef(8, 0.001, 1.2);
	glutSolidCube(5);
	glPopMatrix();

	glPushMatrix();
	glScaled(33, 1, 1);
	glRotatef(90, 0, 0, 1);
	glTranslatef(8, 0.001, 120);
	glutSolidCube(5);
	glPopMatrix();

	glPushMatrix();
	glRotatef(90, 0, 1, 0);
	glScaled(28, 1, 1);
	glTranslatef(-2, 8, 80);
	glutSolidCube(5);
	glPopMatrix();
	//------------------------------------	
	glPushMatrix();
	glScaled(1, 0.1, 1);
	glRotatef(90, 0, 0, 1);
	glTranslatef(5, 70, 60);
	balok2();
	glPopMatrix();

	glPushMatrix();
	glScaled(1, 0.1, 1);
	glRotatef(90, 0, 0, 1);
	glTranslatef(5, 60, 65);
	balok2();
	glPopMatrix();

	glPushMatrix();
	glScaled(1, 0.1, 1);
	glRotatef(90, 0, 0, 1);
	glTranslatef(5, 50, 60);
	balok2();
	glPopMatrix();

	glPushMatrix();
	kotak();
	glPopMatrix();

	//-----------------------------------------------------------------------------------------
	// end. P A G A R    / pagar yang mengelilingi taman
	//-----------------------------------------------------------------------------------------


	glutSwapBuffers();
	glFlush();
	rot++;
	angle++;

}

void init(void) {
	glEnable( GL_DEPTH_TEST);
	glEnable( GL_LIGHTING);
	glEnable( GL_LIGHT0);
	glDepthFunc( GL_LESS);
	glEnable( GL_NORMALIZE);
	glEnable( GL_COLOR_MATERIAL);
	glDepthFunc( GL_LEQUAL);
	glShadeModel( GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable( GL_CULL_FACE);
	glEnable( GL_TEXTURE_2D);
	//glEnable(GL_TEXTURE_3D);
	initRendering();
	_terrain = loadTerrain("heightmap.bmp", 20);
	_terrainTanah = loadTerrain("heightmapTanah.bmp", 20);
	_terrainAir = loadTerrain("heightmapAir.bmp", 20);
	texture[3] = loadtextures("wood.raw", 256, 256);
	texture[1] = loadtextures("air.raw", 256, 256);
	texture[2] = loadtextures("rumput.raw", 400, 199);//lantai
	texture[0] = loadtextures3D("rumput.bmp", 400, 199);

}

static void kibor(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_HOME:
		viewy++;
		break;
	case GLUT_KEY_END:
		viewy--;
		break;
	case GLUT_KEY_UP:
		viewz--;
		break;
	case GLUT_KEY_DOWN:
		viewz++;
		break;

	case GLUT_KEY_RIGHT:
		viewx++;
		break;
	case GLUT_KEY_LEFT:
		viewx--;
		break;

	case GLUT_KEY_F1: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	case GLUT_KEY_F2: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient2);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse2);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	default:
		break;
	}
}

void keyboard(unsigned char key, int x, int y) {
	if (key == 'd') {

		spin = spin - 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'a') {
		spin = spin + 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'q') {
		viewz++;
	}
	if (key == 'e') {
		viewz--;
	}
	if (key == 's') {
		viewy--;
	}
	if (key == 'w') {
		viewy++;
	}
}

void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode( GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat) w / (GLfloat) h, 0.1, 1000.0);
	glMatrixMode( GL_MODELVIEW);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Sample Grafkom Taman");
	init();

	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutReshapeFunc(reshape);
	glutSpecialFunc(kibor);

	glutKeyboardFunc(keyboard);

	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);

	glutMainLoop();
	return 0;
}
