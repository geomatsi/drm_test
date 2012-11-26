#include "gl_utils.h"

/* */

void render_stuff(int width, int height, GLfloat rotz)
{
    GLfloat view_rotx = 0.0, view_roty = 0.0, view_rotz = rotz;
    static const GLfloat verts[3][2] = {
        { -1, -1 },
        {  1, -1 },
        {  0,  1 }
    };
    static const GLfloat colors[3][3] = {
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 }
    };
    GLfloat ar = (GLfloat) width / (GLfloat) height;

    glViewport(0, 0, (GLint) width, (GLint) height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-ar, ar, -1, 1, 5.0, 60.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -10.0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();
    glRotatef(view_rotx, 1, 0, 0);
    glRotatef(view_roty, 0, 1, 0);
    glRotatef(view_rotz, 0, 0, 1);

    glVertexPointer(2, GL_FLOAT, 0, verts);
    glColorPointer(3, GL_FLOAT, 0, colors);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glPopMatrix();

    glFinish();
}


