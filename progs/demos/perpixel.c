/*
 *  PerPixel Lighting Demo
 *
 *  Holger Waechtler, Summer 2000
 *  This program is in the public domain.
 *
 *  Uses GL_EXT_texture_combine and GL_EXT_texture_combine2.
 *  ARB_texture_cube_map is required.
 *
 *  A good general introduction to this technique you find in Mark Kilgard's 
 *  paper "A Practical and Robust Bump-mapping Technique for today's GPUs".
 *  Our demo uses a different order of operations: We first draw the specular
 *  highlight. This allows arbitrary specular exponents realized by color lookup
 *  in a glCopyPixels pass.
 *
 *  Exact per-pixel normalization by cube mapping is only performed for the
 *  halfangle S vectors (specular pass).
 *  The L vectors for diffuse lighting are interpolated linear, resulting
 *  artifacts were small in my experiments. Doing this right would cost an
 *  extra pass.
 *
 *
 *  Open issues:
 *
 *  - 16 Bit packed modes have problems with high specular exponents - copypix
 *    colors highlights violet ...
 *    (The X driver ReadSpan/WriteSpan functions may not be asymmetric ?)
 *
 *  - for some strange reason the first frame doesn't applies the color texture
 *
 *  - the 'real' OpenGL lighting places the highlight a bit different,
 *    perhaps the L/S calculations are still buggy.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>

#include "../util/readtex.c"


#define COLOR_MAP_FILE        "../images/color.rgb"
#define SPECULAR_MAP_FILE     "../images/white.rgb"
#define DISPLACEMENT_MAP_FILE "../images/disp.rgb"

#define MESHSIZE_M  2          /* set this to >15 for curved surfaces     */
#define MESHSIZE_N  2          /* this too                                */
#define CUBEMAP_SIZE  32       /* resolution of the normalizing cube map  */

GLfloat normal_displacement = 0.1;  /* 'height' of bumps                  */

GLfloat shininess = 10;
GLfloat shininess_color [3] = { 0.5, 0.3, 0.3 };
GLfloat light_position0 [3] = { 1.0, 2.0, 0.5 };

GLfloat ambient_light   [4] = { 0.2, 0.2, 0.2, 1.0 };
GLfloat diffuse_light   [4] = { 0.9, 1.0, 0.9, 1.0 };

GLfloat mat [16]     = { 1.0, 0.0, 0.0, 0.0,
                         0.0, 1.0, 0.0, 0.0,
                         0.0, 0.0, 1.0, 0.0,
                         0.0, 0.0, -0.5, 1.0 };
GLfloat mat_inv [16] = { 1.0, 0.0, 0.0, 0.0,   /* keep this in sync with mat */
                         0.0, 1.0, 0.0, 0.0,
                         0.0, 0.0, 1.0, 0.0,
                         0.0, 0.0, 0.5, 1.0 };


#define MALLOC malloc
#define FREE   free




typedef struct {
   GLuint     face_count;
   GLuint     vertex_count;

   GLuint   (*index)[3];
   GLfloat  (*vertex)[3];
   GLfloat  (*normal)[3];
   GLfloat  (*texcoord)[2];

   GLfloat  (*tangent)[3];
   GLfloat  (*binormal)[3];
   GLfloat  (*L)[3];
   GLfloat  (*S)[3];

   GLuint     color_texture;
   GLuint     normal_specular_texture;
   GLubyte    shininess_table [256][3];

   GLfloat    modelview_mat [16];
   GLfloat    modelview_mat_inv [16];
} Mesh;


static void
setup_shininess_tab (Mesh *m, GLfloat shininess, GLfloat color [3])
{
   GLuint i;

   for (i = 0; i < 255; i++) {
      GLfloat s = pow(i*1.0/254.0, shininess) * 255.0;
      m->shininess_table[i][0] = (GLubyte) (color[0] * s);
      m->shininess_table[i][1] = (GLubyte) (color[1] * s);
      m->shininess_table[i][2] = (GLubyte) (color[2] * s);
   }
   m->shininess_table[255][0] = (GLubyte) (color[0] * 255.0);
   m->shininess_table[255][1] = (GLubyte) (color[1] * 255.0);
   m->shininess_table[255][2] = (GLubyte) (color[2] * 255.0);
}


/*  This function should be able to generate good normals for arbitrary
 *  triangular meshes. For each vertex we weight triangle normals of
 *  surrounding faces together. Resulting normal vectors are not normalized.
 */
static void
setup_normals (Mesh *m)
{
   GLuint i;
   GLfloat (*N)[3] = m->normal;
   GLfloat (*V)[3] = m->vertex;

   for (i = 0; i < m->vertex_count; i++)
      N[i][0] = N[i][1] = N[i][2] = 0.0;

   for (i = 0; i < m->face_count; i++) {
      GLuint v0 = m->index[i][0];
      GLuint v1 = m->index[i][1];
      GLuint v2 = m->index[i][2];
      GLfloat x1 = V[v2][0] - V[v0][0];
      GLfloat y1 = V[v2][1] - V[v0][1];
      GLfloat z1 = V[v2][2] - V[v0][2];
      GLfloat x2 = V[v1][0] - V[v0][0];
      GLfloat y2 = V[v1][1] - V[v0][1];
      GLfloat z2 = V[v1][2] - V[v0][2];
      GLfloat n0 = y1 * z2 - z1 * y2;    /*  the triangle normal   */
      GLfloat n1 = z1 * x2 - x1 * z2;
      GLfloat n2 = x1 * y2 - y1 * x2;

      N[v0][0] += n0; N[v0][1] += n1; N[v0][2] += n2;
      N[v1][0] += n0; N[v1][1] += n1; N[v1][2] += n2;
      N[v2][0] += n0; N[v2][1] += n1; N[v2][2] += n2;
   }
}


/*  This function should be able to generate good tangent vectors for arbitrary
 *  triangular meshes with smooth and continous texture coordinates.
 *  Resulting tangent vectors are not normalized.
 */
static void
setup_tangents (Mesh *m)
{
   GLuint i;
   GLfloat (*T)[3] = m->tangent;
   GLfloat (*V)[3] = m->vertex;
   GLfloat (*tc)[2] = m->texcoord;

   for (i = 0; i < m->vertex_count; i++)
      T[i][0] = T[i][1] = T[i][2] = 0.0;

   for (i = 0; i < m->face_count; i++) {
      GLuint v0 = m->index[i][0];
      GLuint v1 = m->index[i][1];
      GLuint v2 = m->index[i][2];
      GLfloat k;
      GLfloat du1, du2, t0, t1, t2;

      du1 = tc[v1][0] - tc[v2][0];
      du2 = tc[v1][0] + tc[v2][0] - 2 * tc[v0][0];

      if (du1 > du2) {
         k = (tc[v0][0] - tc[v2][0]) / du1;
         t0 = k * (V[v1][0] - V[v0][0]) + (1 - k) * (V[v2][0] - V[v0][0]);
         t1 = k * (V[v1][1] - V[v0][1]) + (1 - k) * (V[v2][1] - V[v0][1]);
         t2 = k * (V[v1][2] - V[v0][2]) + (1 - k) * (V[v2][2] - V[v0][2]);
      } else {
         k = (tc[v2][0] - tc[v0][0]) / du2;
         t0 = k * (V[v1][0] - V[v0][0]) - (1 - k) * (V[v2][0] - V[v0][0]);
         t1 = k * (V[v1][1] - V[v0][1]) - (1 - k) * (V[v2][1] - V[v0][1]);
         t2 = k * (V[v1][2] - V[v0][2]) - (1 - k) * (V[v2][2] - V[v0][2]);
      }
      T[v0][0] += t0; T[v0][1] += t1; T[v0][2] += t2;
      T[v1][0] += t0; T[v1][1] += t1; T[v1][2] += t2;
      T[v2][0] += t0; T[v2][1] += t1; T[v2][2] += t2;
   }
}


/*  This creates a per-vertex coordinate frame TxBxN. When done, T, B and
 *  N are normalized. Set up normals and tangents before calling this function.
 *  (You don't have to normalize them and they don't have to be exactly
 *   orthogonal)
 */
static void
setup_coordinate_frame (Mesh *m)
{
   GLuint i;
   GLfloat (*N)[3] = m->normal;
   GLfloat (*T)[3] = m->tangent;
   GLfloat (*B)[3] = m->binormal;

   for (i = 0; i < m->vertex_count; i++) {
      GLfloat b0 = N[i][1] * T[i][2] - N[i][2] * T[i][1];
      GLfloat b1 = N[i][2] * T[i][0] - N[i][0] * T[i][2];
      GLfloat b2 = N[i][0] * T[i][1] - N[i][1] * T[i][0];
      GLfloat scale = 1.0/sqrt(b0 * b0 + b1 * b1 + b2 * b2);

      B[i][0] = scale * b0;
      B[i][1] = scale * b1;
      B[i][2] = scale * b2;

      scale = 1.0/sqrt(N[i][0] * N[i][0] + N[i][1] * N[i][1] + N[i][2] * N[i][2]);
      N[i][0] *= scale;
      N[i][1] *= scale;
      N[i][2] *= scale;

      T[i][0] = B[i][1] * N[i][2] - B[i][2] * N[i][1];
      T[i][1] = B[i][2] * N[i][0] - B[i][0] * N[i][2];
      T[i][2] = B[i][0] * N[i][1] - B[i][1] * N[i][0];
   }
}



/*  Transform Eye and Light position in per-vertex tangent space, calculate then
 *  L and S. Calculations are performed as described in the Red Book.
 *
 *  This is written for local light sources.
 */
static void
setup_L_and_S (Mesh *m, GLfloat light_pos [3])
{
   GLuint i;
   GLfloat (*V)[3] = m->vertex;
   GLfloat (*N)[3] = m->normal;
   GLfloat (*T)[3] = m->tangent;
   GLfloat (*B)[3] = m->binormal;
   GLfloat   lp0, lp1, lp2;
   GLfloat   ep0, ep1, ep2;
   GLfloat  *mm = m->modelview_mat_inv;

   lp0 = mm[0]*light_pos[0] + mm[4]*light_pos[1] + mm[8]*light_pos[2] + mm[12];
   lp1 = mm[1]*light_pos[0] + mm[5]*light_pos[1] + mm[9]*light_pos[2] + mm[13];
   lp2 = mm[2]*light_pos[0] + mm[6]*light_pos[1] + mm[10]*light_pos[2] + mm[14];

   ep0 = m->modelview_mat_inv[12];
   ep1 = m->modelview_mat_inv[13];
   ep2 = m->modelview_mat_inv[14];

   for (i = 0; i < m->vertex_count; i++) {
      GLfloat v0 = V[i][0];
      GLfloat v1 = V[i][1];
      GLfloat v2 = V[i][2];
      GLfloat L0 = T[i][0]*(lp0-v0) + T[i][1]*(lp1-v1) + T[i][2]*(lp2-v2);
      GLfloat L1 = B[i][0]*(lp0-v0) + B[i][1]*(lp1-v1) + B[i][2]*(lp2-v2);
      GLfloat L2 = N[i][0]*(lp0-v0) + N[i][1]*(lp1-v1) + N[i][2]*(lp2-v2);
      GLfloat scale = 1.0/sqrt(L0 * L0 + L1 * L1 + L2 * L2);
      GLfloat S0, S1, S2;

      L0 *= scale;
      L1 *= scale;
      L2 *= scale;

      m->L[i][0] = 0.5 * L0 + 0.5; /* == light vector in tangent space */
      m->L[i][1] = 0.5 * L1 + 0.5;
      m->L[i][2] = 0.5 * L2 + 0.5;

      S0 = T[i][0]*(ep0-v0) + T[i][1]*(ep1-v1) + T[i][2]*(ep2-v2);
      S1 = B[i][0]*(ep0-v0) + B[i][1]*(ep1-v1) + B[i][2]*(ep2-v2);
      S2 = N[i][0]*(ep0-v0) + N[i][1]*(ep1-v1) + N[i][2]*(ep2-v2);
      scale = 1.0/sqrt(S0 * S0 + S1 * S1 + S2 * S2);

      m->S[i][0] = S0 * scale + L0;
      m->S[i][1] = S1 * scale + L1;
      m->S[i][2] = S2 * scale + L2;
   }
}


/*  Create a normal map from displacement map. scale is the displacement for a
 *  pixel with value 255 in displacement map.
 *  The specular map is encoded into Alpha component of normal map.
 */
/*  TODO: border handling.
 */
static void
setup_normal_map (Mesh *m, GLubyte *disp_map, GLfloat scale,
                  GLubyte *specular_map, GLuint width, GLuint height)
{
   GLuint i, j;
   GLubyte (*normal_map)[4] = MALLOC(4*width*height*sizeof(GLubyte));
   GLfloat rscale = 1.0/scale;
   GLfloat dhdx, dhdy, nscale;

   for (j = 0; j < height-1; j++) {
      for (i = 0; i < width-1; i++) {
         dhdx = disp_map[j*width+i+1] - disp_map[j*width+i];
         dhdy = disp_map[(j+1)*width+i] - disp_map[j*width+i];
         nscale = 127.0/sqrt(rscale * rscale + dhdx * dhdx + dhdy * dhdy);

         normal_map[j*width+i][0] = (GLubyte) (-nscale * dhdy + 128.0);
         normal_map[j*width+i][1] = (GLubyte) (nscale * dhdx + 128.0); 
         normal_map[j*width+i][2] = (GLubyte) (nscale * rscale + 128.0); 
         normal_map[j*width+i][3] = specular_map[j*width+i]; 
      }
   }

   glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
   glGenTextures (1, & m->normal_specular_texture);
   glBindTexture (GL_TEXTURE_2D, m->normal_specular_texture);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, normal_map);

   FREE(normal_map);
}



static void
render_perpixel (Mesh *m, int width, int height)
{
   glEnableClientState (GL_VERTEX_ARRAY);
   glEnableClientState (GL_TEXTURE_COORD_ARRAY);
   glVertexPointer (3, GL_FLOAT, 0, m->vertex);

   glEnable(GL_BLEND);

#if 1
  /* 1st pass: r = g = b = N.S, Alpha = k_specular
   */
   glDepthFunc (GL_LEQUAL);
   glEnable (GL_DEPTH_TEST);
   glBlendFunc (GL_ONE, GL_ZERO);

   glActiveTextureARB (GL_TEXTURE0_ARB);
   glClientActiveTextureARB (GL_TEXTURE0_ARB);
   glTexCoordPointer (3, GL_FLOAT, 0, m->S);
   glEnable (GL_TEXTURE_CUBE_MAP_ARB);
   glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

   glActiveTextureARB (GL_TEXTURE1_ARB);
   glClientActiveTextureARB (GL_TEXTURE1_ARB);
   glTexCoordPointer (2, GL_FLOAT, 0, m->texcoord);
   glEnable (GL_TEXTURE_2D);
   glBindTexture (GL_TEXTURE_2D, m->normal_specular_texture);
   glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
   glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_DOT3_MESA);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
   glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);

   glDrawElements (GL_TRIANGLES, 3*m->face_count, GL_UNSIGNED_INT, m->index);

   glActiveTextureARB (GL_TEXTURE0_ARB);
   glDisable (GL_TEXTURE_CUBE_MAP_ARB);
   glDepthFunc (GL_EQUAL);

  /*  Now we have the specular dot product N.S
   *  The Alpha components contain the specular coefficients which we use 
   *  in the blendequation of the second pass.
   */
#endif

#if 1
  /* 2nd pass: the specular exponent (N.S)^shininess
   */
   glBlendFunc (GL_SRC_ALPHA, GL_ZERO);
   glDisable (GL_DEPTH_TEST);
   glColorTable (GL_COLOR_TABLE, GL_RGB, 256, GL_RGB,
                 GL_UNSIGNED_BYTE, m->shininess_table);
   glEnable (GL_COLOR_TABLE);
   glMatrixMode (GL_PROJECTION);
   glPushMatrix ();
   glLoadIdentity ();
   glMatrixMode (GL_MODELVIEW);
   glPushMatrix ();
   glLoadIdentity ();
   glRasterPos2f (-1.0, -1.0);
   glCopyPixels (0, 0, width, height, GL_COLOR);
   glMatrixMode (GL_PROJECTION);
   glPopMatrix ();
   glMatrixMode (GL_MODELVIEW);
   glPopMatrix ();
   glDisable (GL_COLOR_TABLE);
#endif

#if 1
  /* 3rd pass: diffuse lighting * color texture
   *  Cf += (N.L * diffuse_light + ambient_light) * color_texture
   *  This pass uses color texture alpha components.
   */
   glBlendFunc (GL_SRC_ALPHA, GL_ONE);
   glEnableClientState (GL_COLOR_ARRAY);
   glColorPointer (3, GL_FLOAT, 0, m->L);
   glEnable (GL_DEPTH_TEST);

   glActiveTextureARB (GL_TEXTURE0_ARB);
   glClientActiveTextureARB (GL_TEXTURE0_ARB);
   glTexCoordPointer (2, GL_FLOAT, 0, m->texcoord);
   glBindTexture (GL_TEXTURE_2D, m->normal_specular_texture);
   glEnable (GL_TEXTURE_2D);
   glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
   glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_DOT3_MODULATE_MESA);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PRIMARY_COLOR_EXT);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_CONSTANT_EXT);
   glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, diffuse_light);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_SRC_COLOR);
   glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_CONSTANT_EXT);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);

   glActiveTextureARB (GL_TEXTURE1_ARB);
   glClientActiveTextureARB (GL_TEXTURE1_ARB);
   glTexCoordPointer (2, GL_FLOAT, 0, m->texcoord);
   glBindTexture (GL_TEXTURE_2D, m->color_texture);
   glEnable (GL_TEXTURE_2D);
   glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
   glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_ADD_MODULATE_MESA);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_CONSTANT_EXT);
   glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, ambient_light);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_TEXTURE);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_SRC_COLOR);
   glTexEnvi (GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
   glTexEnvi (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
   glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);
   glDrawElements (GL_TRIANGLES, 3*m->face_count, GL_UNSIGNED_INT, m->index);

   glDisableClientState (GL_COLOR_ARRAY);
#endif

   glDisable (GL_TEXTURE_2D);
   glActiveTextureARB (GL_TEXTURE0_ARB);
   glDisable (GL_TEXTURE_2D);
   glDisableClientState (GL_TEXTURE_COORD_ARRAY);
   glDisableClientState (GL_VERTEX_ARRAY);
   glDisable(GL_BLEND);
   glDepthFunc (GL_LEQUAL);
}



/* The following two functions are taken from Mark Kilgard's 
 * paper "A Practical and Robust Bump-mapping Technique for today's GPUs" 
 *
 */
static void getCubeVector (int i, int cubesize, int x, int y, float *vector)
{
   float s, t, sc, tc, mag;

   s = ((float) x + 0.5) / (float) cubesize;
   t = ((float) y + 0.5) / (float) cubesize;
   sc = s * 2.0 - 1.0;
   tc = t * 2.0 - 1.0;

   switch (i) {
   case 0: vector[0] =  1.0; vector [1] = -tc; vector[2] = -sc; break;
   case 1: vector[0] = -1.0; vector [1] = -tc; vector[2] = sc; break;
   case 2: vector[0] = sc;   vector [1] = 1.0; vector[2] = tc; break;
   case 3: vector[0] = sc;   vector [1] = -1.0; vector[2] = -tc; break;
   case 4: vector[0] = sc;   vector [1] = -tc; vector[2] = 1.0; break;
   case 5: vector[0] = -sc;  vector [1] = -tc; vector[2] = -1.0; break;
   }

   mag = 1.0/sqrt(vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2]);
   vector[0] *= mag;
   vector[1] *= mag;
   vector[2] *= mag;
}


static void
setup_normalizing_cubemap (GLuint size)
{
   float vector[3];
   int i, x, y;
   GLubyte *pixels;

   pixels = MALLOC(3*size*size);

   glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   for (i=0; i<6; i++) {
      for (y=0; y<size; y++) {
         for (x=0; x<size; x++) {
            getCubeVector(i, size, x, y, vector);
            pixels[3*(y*size+x)+0] = 128 + 127.0 * vector[0];
            pixels[3*(y*size+x)+1] = 128 + 127.0 * vector[1];
            pixels[3*(y*size+x)+2] = 128 + 127.0 * vector[2];
         }
      }
      glTexImage2D (GL_TEXTURE_CUBE_MAP_POSITIVE_X_EXT+i, 0, GL_RGB8,
                    size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
   }
   FREE(pixels);
}


/******************************************************************************/


static Mesh *sample_mesh;
static int   window_width;
static int   window_height;

GLfloat light_position  [3];
GLfloat light_angle = 0.0;


/*  Sphere. Looks not very impressive because of texture distortion.
 *  Useful to compare perpixel lighting with 'real' OpenGL lighting if you
 *  use white textures.
 */
/*static void
obj_func (GLfloat u, GLfloat v, GLfloat x[3])
{
   x[1] = sin (M_PI * u) * cos (2.0 * M_PI * v);
   x[2] = sin (M_PI * u) * sin (2.0 * M_PI * v);
   x[0] = cos (M_PI * u);
}
*/

/*
 *  Simple Quad.
 */
static void
obj_func (GLfloat u, GLfloat v, GLfloat x[3])
{
   x[0] = 2.0 * u - 1.0;
   x[1] = 2.0 * v - 1.0;
   x[2] = 0;
}



/*  This looks a bit confusing. Sorry.
 *
 */
static Mesh*
init_sample_mesh (void)
{
   GLubyte *disp_map, *specular_map, *color_map, *_disp_map, *_specular_map;
   GLint    w, w2, w3, h, h2, h3;
   GLenum   fmt1, fmt2, fmt3;
   GLuint   i, j;
   GLuint count = 0;
   Mesh *m = MALLOC(sizeof(Mesh));

   m->vertex_count = MESHSIZE_M * MESHSIZE_N;
   m->face_count = 2 * (MESHSIZE_M-1) * (MESHSIZE_N-1);

   m->index = MALLOC(3 * m->face_count * sizeof(GLuint));
   m->vertex = MALLOC(3 * m->vertex_count * sizeof(GLfloat));
   m->normal = MALLOC(3 * m->vertex_count * sizeof(GLfloat));
   m->tangent = MALLOC(3 * m->vertex_count * sizeof(GLfloat));
   m->binormal = MALLOC(3 * m->vertex_count * sizeof(GLfloat));
   m->texcoord = MALLOC(2 * m->vertex_count * sizeof(GLfloat));
   m->L = MALLOC(3 * m->vertex_count * sizeof(GLfloat));
   m->S = MALLOC(3 * m->vertex_count * sizeof(GLfloat));

   setup_shininess_tab (m, shininess, shininess_color);

   for (i = 0; i < 16; i++) {
      m->modelview_mat[i] = mat[i];
      m->modelview_mat_inv[i] = mat_inv[i];
   }

   for (j = 0; j < MESHSIZE_N; j++) {         /*  create the object  */
      for (i = 0; i < MESHSIZE_M; i++) {
         GLfloat u = (GLfloat) i/(MESHSIZE_M-1.0);
         GLfloat v = (GLfloat) j/(MESHSIZE_N-1.0);
         obj_func (u, v, m->vertex[j*MESHSIZE_N+i]);
         m->texcoord [j*MESHSIZE_N+i][0] = u;
         m->texcoord [j*MESHSIZE_N+i][1] = v;
      }
   }

   for (j = 0; j < MESHSIZE_M-1; j++)
      for (i = 0; i < MESHSIZE_N-1; i++)
   {
      GLuint ind = j*MESHSIZE_M+i;
      m->index[count][0] = ind;                    /*  build index array */
      m->index[count][1] = ind+MESHSIZE_N;
      m->index[count][2] = ind+1;
      count++;
      m->index[count][0] = ind+MESHSIZE_N;
      m->index[count][1] = ind+MESHSIZE_N+1;
      m->index[count][2] = ind+1;
      count++;
   }

   setup_normals (m);
   setup_tangents (m);
   setup_coordinate_frame (m);

   light_position [0] = light_position0[0] * cos (light_angle * M_PI/180.0);
   light_position [1] = light_position0[1] * sin (light_angle * M_PI/180.0);
   light_position [2] = light_position0[2];

   setup_L_and_S (m, light_position);

   _disp_map = LoadRGBImage (DISPLACEMENT_MAP_FILE, &w, &h, &fmt1);
   disp_map = MALLOC(w * h * sizeof(GLubyte));
   if (fmt1 == GL_RGB)
      for (i = 0; i < w*h; i++)
         disp_map[i] = (_disp_map[3*i] +
                        _disp_map[3*i+1] +
                        _disp_map[3*i+2]) / 3.0;
   else
      for (i = 0; i < w*h; i++)
         disp_map[i] = (_disp_map[4*i] +
                        _disp_map[4*i+1] +
                        _disp_map[4*i+2]) / 3.0;
   FREE(_disp_map);

   _specular_map = LoadRGBImage (SPECULAR_MAP_FILE, &w2, &h2, &fmt2);
   specular_map = MALLOC(w * h * sizeof(GLubyte));
   if (fmt2 == GL_RGB)
      for (i = 0; i < w*h; i++)
         specular_map[i] = (_specular_map[3*i] +
                            _specular_map[3*i+1] +
                            _specular_map[3*i+2]) / 3.0;
   else
      for (i = 0; i < w*h; i++)
         specular_map[i] = (_specular_map[4*i] +
                            _specular_map[4*i+1] +
                            _specular_map[4*i+2]) / 3.0;
   FREE(_specular_map);

   setup_normal_map (m, disp_map, normal_displacement, specular_map, w, h);
   FREE(disp_map);
   FREE(specular_map);

   color_map = LoadRGBImage (COLOR_MAP_FILE, &w3, &h3, &fmt3);
   glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
   glGenTextures (1, & m->color_texture);
   glBindTexture (GL_TEXTURE_2D, m->color_texture);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexImage2D (GL_TEXTURE_2D, 0, fmt3, w, h, 0,
                 fmt3, GL_UNSIGNED_BYTE, color_map);
   FREE(color_map);

   if (!(w == w2 && w == w3 && h == h2 && h == h3)) {
      printf("texture sizes don't match !!!"
             " (%u <-> %u <-> %u, %u <-> %u <-> %u)\n", w, w2, w3, h, h2, h3);
/*    exit (1);*/
   }

   return m;
}



static void display (void)
{
   glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glPushMatrix ();
   glMultMatrixf (sample_mesh->modelview_mat);
   render_perpixel (sample_mesh, window_width, window_height);
   glMatrixMode (GL_MODELVIEW);
   glPopMatrix ();

   glMatrixMode (GL_MODELVIEW);
   glPushMatrix ();
   glTranslatef (light_position[0], light_position[1], light_position[2]);
   glColor4f (1.0, 1.0, 0.0, 1.0);
   glutSolidSphere (0.05, 10, 10);
   glPopMatrix ();

   glMatrixMode (GL_MODELVIEW);
   glutSwapBuffers();
}


static void reshape (int width, int height)
{
   glViewport (0, 0, width, height);
   glMatrixMode (GL_PROJECTION);
   glLoadIdentity ();
   glOrtho (-1.2, 1.2, -1.2, 1.2, -30.0, 30.0);
   glMatrixMode (GL_MODELVIEW);
   glLoadIdentity ();

   window_width = width;
   window_height = height;
}


int wireframe = 0;

static void key (unsigned char key, int x, int y)
{
   (void) x;
   (void) y;
   switch (key) {
      case 'w':
         wireframe = !wireframe;
         glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
         break;
      case 27:
         exit(0);
         break;
   }
   glutPostRedisplay();
}


/* XXX: it would be nice to rotate the object using cursor keys.
 *      We have to update both the modelview matrix and its inverse.
 */
static void special_key (int key, int x, int y)
{
   (void) x;
   (void) y;

   switch (key) {
      case GLUT_KEY_UP:
         break;
      case GLUT_KEY_DOWN:
         break;
      case GLUT_KEY_LEFT:
         break;
      case GLUT_KEY_RIGHT:
         break;
   }
   glutPostRedisplay();
}


static void initGL (void)
{
   const char *exten = (const char *) glGetString(GL_EXTENSIONS);

   if (!strstr(exten, "GL_ARB_multitexture")) {
      printf("Sorry, GL_ARB_multitexture not supported by this renderer.\n");
      exit(1);
   }
   if (!strstr(exten, "GL_ARB_texture_cube_map")) {
      printf("Sorry, GL_ARB_texture_cube_map not supported.\n");
      exit(1);
   }
   if (!strstr(exten, "GL_EXT_texture_env_combine")) {
      printf("Sorry, GL_EXT_texture_env_combine not supported.\n");
      exit(1);
   }
   if (!strstr(exten, "GL_MESA_texture_env_combine2")) {
      printf("Sorry, GL_MESA_texture_env_combine2 not supported.\n");
      exit(1);
   }

   glShadeModel (GL_SMOOTH);
   glClearColor (0.0, 0.0, 0.0, 0.0);
   glEnable (GL_DEPTH_TEST);

   sample_mesh = init_sample_mesh ();
   setup_normalizing_cubemap (CUBEMAP_SIZE);
}


static void
idle (void)
{
   light_angle += 10.0;
   if (light_angle >= 360.0)
      light_angle = 0.0;

   light_position [0] = light_position0[0] * cos (light_angle * M_PI/180.0);
   light_position [1] = light_position0[1] * sin (light_angle * M_PI/180.0);
   light_position [2] = light_position0[2];

   setup_L_and_S (sample_mesh, light_position);

   glutPostRedisplay();
}



char usage [] = 
"                                                                          \n"
"   Demonstration of perpixel lighting using EXT_texture_combine2.         \n"
"   Written by Holger Waechtler                                            \n"
"                                                                          \n"
"   Press 'w' to toggle wireframe mode.                                    \n"
"                                                                          \n";



int main (int argc, char *argv[])
{
   glutInit (&argc, argv);
   glutInitWindowSize (300, 300);
   glutInitDisplayMode (GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
   glutCreateWindow (argv[0]);

   if (argc > 1 && strcmp(argv[1], "-info")==0) {
      printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
      printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
      printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
      printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
   }

   initGL ();

   printf (usage, argv[0]);

   glutReshapeFunc (reshape);
   glutKeyboardFunc (key);
   glutSpecialFunc (special_key);
   glutDisplayFunc (display);
   glutIdleFunc (idle);

   glutMainLoop ();
   return 0;
}

