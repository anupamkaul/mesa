/*!
 * \file mesa.hxx
 * \brief Mesa C++ wrapper classes definition.
 * 
 * \author José Fonseca <jrfonseca@tungstengraphics.com>
 */


#include <cstring>

extern "C" {
	
#include "context.h"
#include "mtypes.h"
#include "texstore.h"
#include "texobj.h"
#include "teximage.h"
#include "swrast/swrast.h"
	
}


//! Mesa C++ wrapper classes.
/*!
 * These classes are wrappers around the Mesa library.
 *
 * They aren't DRI specific. The idea is that they can be used for other
 * non-DRI drivers.
 *
 * \todo Nothing here yet, as I haven't study the Mesa architecture in
 * detail, but that will be done shortly. Probably there will be just a wrapper
 * for the Driver callbacks and for generic stuff like textures.
 */
namespace Mesa {

//! Mesa texture image
class Image : public gl_texture_image {
	public:
		//! Constructor
		Image(void) {
			std::memset((gl_texture_image *)this, 0, sizeof(gl_texture_image));
		}
		
		//! Destructor
		virtual ~Image() {}
} ;

class Context;

//! Mesa texture
class Texture : public gl_texture_object {
	public:
		//! Constructor
		Texture(GLuint name, GLenum target) {
			memset((gl_texture_object *)this, 0, sizeof(gl_texture_object));
			_mesa_initialize_texture_object((gl_texture_object *)this, name, target);
		}
		
		//! Destructor
		virtual ~Texture() {
			_mesa_free_texture_object_data((gl_texture_object *)this);
		}

		//! Determine if texture is loaded in texture memory
		virtual bool isResident(void) const {
			return true;
		}

		//! Set texture residence priority
		virtual void prioritize(GLclampf priority) {}
		
		//! \name Texture image functions @{

		//! One-dimensional texture image
		virtual void image1D(
			Context *context,
			GLenum target, GLint level,
			GLint internalFormat,
			GLint width,
			GLint border,
			GLenum format, GLenum type,
			const GLvoid *pixels,
			const struct gl_pixelstore_attrib *packing,
			Mesa::Image *image
		);

		//! Two-dimensional texture image
		virtual void image2D(
			Context *context,
			GLenum target, GLint level,
			GLint internalFormat,
			GLint width, GLint height,
			GLint border,
			GLenum format, GLenum type,
			const GLvoid *pixels,
			const struct gl_pixelstore_attrib *packing,
			Mesa::Image *image
		);
		
		//! Three-dimensional texture image
		virtual void image3D(
			Context *context,
			GLenum target, GLint level,
			GLint internalFormat,
			GLint width, GLint height, GLint depth,
			GLint border,
			GLenum format, GLenum type,
			const GLvoid *pixels,
			const struct gl_pixelstore_attrib *packing,
			Mesa::Image *image
		);

		//! One-dimensional texture subimage
		virtual void subImage1D(
			Context *context,
			GLenum target, GLint level,
			GLint xoffset,
			GLsizei width,
			GLenum format, GLenum type,
			const GLvoid *pixels,
			const struct gl_pixelstore_attrib *packing,
			Mesa::Image *image
		);

		//! Two-dimensional texture subimage
		virtual void subImage2D(
			Context *context,
			GLenum target, GLint level,
			GLint xoffset, GLint yoffset,
			GLsizei width, GLsizei height,
			GLenum format, GLenum type,
			const GLvoid *pixels,
			const struct gl_pixelstore_attrib *packing,
			Mesa::Image *image
		);

		//! Three-dimensional texture subimage
		virtual void subImage3D(
			Context *context,
			GLenum target, GLint level,
			GLint xoffset, GLint yoffset, GLint zoffset,
			GLsizei width, GLsizei height, GLsizei depth,
			GLenum format, GLenum type,
			const GLvoid *pixels,
			const struct gl_pixelstore_attrib *packing,
			Mesa::Image *image
		);
		
		//@}
		
		//! \name Compressed textures functions @{
		
		//! One-dimensional texture compressed image
		virtual void compressedImage3D(
			Context *context,
			GLenum target, GLint level,
			GLsizei width,
			GLint border,
			GLenum format, GLsizei imageSize,
			const GLvoid *data,
			Mesa::Image *image
		);

		//! Two-dimensional texture compressed image
		virtual void compressedImage3D(
			Context *context,
			GLenum target, GLint level,
			GLsizei width, GLsizei height,
			GLint border,
			GLenum format, GLsizei imageSize,
			const GLvoid *data,
			Mesa::Image *image
		);

		//! Three-dimensional texture compressed image
		virtual void compressedImage3D(
			Context *context,
			GLenum target, GLint level,
			GLsizei width, GLsizei height, GLsizei depth,
			GLint border,
			GLenum format, GLsizei imageSize,
			const GLvoid *data,
			Mesa::Image *image
		);

		//! One-dimensional texture compressed subimage
		virtual void compressedSubImage3D(
			Context *context,
			GLenum target, GLint level,
			GLint xoffset,
			GLsizei width,
			GLenum format, GLsizei imageSize,
			const GLvoid *data,
			Mesa::Image *image
		);

		//! Two-dimensional texture compressed subimage
		virtual void compressedSubImage3D(
			Context *context,
			GLenum target, GLint level,
			GLint xoffset, GLint yoffset,
			GLsizei width, GLsizei height,
			GLenum format, GLsizei imageSize,
			const GLvoid *data,
			Mesa::Image *image
		);

		//! Three-dimensional texture compressed subimage
		virtual void compressedSubImage3D(
			Context *context,
			GLenum target, GLint level,
			GLint xoffset, GLint yoffset, GLint zoffset,
			GLsizei width, GLsizei height, GLsizei depth,
			GLenum format, GLsizei imageSize,
			const GLvoid *data,
			Mesa::Image *image
		);

		//@}
} ;

//! Visual
class Visual /*: public GLvisual*/ {
	public:
		//! Initialize
		bool init(
			GLboolean rgbFlag,
			GLboolean dbFlag,
			GLboolean stereoFlag,
			GLint redBits,
			GLint greenBits,
			GLint blueBits,
			GLint alphaBits,
			GLint indexBits,
			GLint depthBits,
			GLint stencilBits,
			GLint accumRedBits,
			GLint accumGreenBits,
			GLint accumBlueBits,
			GLint accumAlphaBits,
			GLint numSamples
		) {
			return _mesa_initialize_visual(
				(GLvisual *)this,
				rgbFlag,
				dbFlag,
				stereoFlag,
				redBits,
				greenBits,
				blueBits,
				alphaBits,
				indexBits,
				depthBits,
				stencilBits,
				accumRedBits,
				accumGreenBits,
				accumBlueBits,
				accumAlphaBits,
				numSamples
			);
		}

		virtual ~Visual() {}
} ;

//! Framebuffer 
class Framebuffer : public GLframebuffer {
	public:
		bool init(
			const Mesa::Visual *visual, 
			GLboolean swDepth, 
			GLboolean swStencil, 
			GLboolean swAccum, 
			GLboolean swAlpha
		) {
			_mesa_initialize_framebuffer(
				(GLframebuffer *)this,
				(GLvisual *)visual,
				swDepth,
				swStencil,
				swAccum,
				swAlpha
			);
			return true;
		}
		
		virtual ~Framebuffer() {
			_mesa_free_framebuffer_data((GLframebuffer *)this);
		}

		virtual void getSize(GLuint *width, GLuint *height) const = 0;
		virtual void resize(void) = 0;
} ;

//! Abstract rasterizer
class Rasterizer {
} ;


class SoftwareRasterizer : public Rasterizer {
} ;

//! Abstract TNL
class TNL {
} ;

//! Abstract Mesa context
class Context: public GLcontext {
	public:
		//! Initialize
		bool init(const Mesa::Visual *visual, Context *sharedCtx = NULL, bool direct = true); 

		//! Destructor
		virtual ~Context() {
			_mesa_free_context_data((GLcontext *)this);
		}

		virtual void Error(GLenum error) {}

		//! \name Special functions @{
		//! Indicates that all commands that have previously been sent to the GL must complete in finite time.
		virtual void Flush(void) {}
		//! Forces all previous GL commands to complete.
		virtual void Finish(void) {}
		//@}

		//! Make context current
		void makeCurrent(Framebuffer *drawBuffer, Framebuffer *readBuffer) {
			_mesa_make_current2(
				(GLcontext *)this, 
				(GLframebuffer *)drawBuffer,
				(GLframebuffer *)readBuffer
			);
		}

		//! Swap buffers
		void swapBuffers(void) {
			_mesa_notifySwapBuffers((GLcontext *)this);
		}
	
		//! \name Texture object functions @{
		virtual Mesa::Texture *NewTexture(GLuint name, GLenum target) {
			return new Mesa::Texture(name, target);
		}

		virtual void BindTexture(GLenum target, Mesa::Texture *texture) {}
		
		virtual void ActiveTexture(GLuint texUnitNumber) {}
		
		virtual void UpdateTexturePalette(Mesa::Texture *texture) {}
		//@}

		//! \name Texture image functions @{
		virtual Mesa::Image *NewImage() {
			return new Mesa::Image;
		}

		virtual const struct gl_texture_format *ChooseTextureFormat(GLint internalFormat, GLenum srcFormat, GLenum srcType);

		virtual void CopyTexImage1D(
			GLenum target,
			GLint level,
			GLenum internalFormat,
			GLint x,
			GLint y,
			GLsizei width,
			GLint border
		) {
			_swrast_copy_teximage1d(
				(GLcontext *)this,
				target, level,
				internalFormat,
				x, y,
				width,
				border
			);
		}
		
		virtual void CopyTexImage2D(
			GLenum target, GLint level, 
			GLenum internalFormat,
			GLint x, GLint y,
			GLsizei width, GLsizei height,
			GLint border
		) {
			_swrast_copy_teximage2d(
				(GLcontext *)this,
				target, level,
				internalFormat,
				x, y,
				width, height,
				border
			);
		}
		
		virtual void CopyTexSubImage1D(
			GLenum target, GLint level,
			GLint xoffset, 
			GLint x, GLint y,
			GLsizei width
		) {
			_swrast_copy_texsubimage1d(
				(GLcontext *)this,
				target, level,
				xoffset,
				x, y,
				width
			);
		}
		
		virtual void CopyTexSubImage2D(
			GLenum target, GLint level,
			GLint xoffset, GLint yoffset,
			GLint x, GLint y,
			GLsizei width, GLsizei height
		) {
			_swrast_copy_texsubimage2d(
				(GLcontext *)this,
				target, level,
				xoffset, yoffset,
				x, y,
				width, height
			);
		}
		
		virtual void CopyTexSubImage3D(
			GLenum target, GLint level,
			GLint xoffset, GLint yoffset, GLint zoffset,
			GLint x, GLint y,
			GLsizei width, GLsizei height
		) {
			_swrast_copy_texsubimage3d(
				(GLcontext *)this,
				target, level,
				xoffset, yoffset, zoffset,
				x, y,
				width, height
			);
		}
		
		virtual GLboolean TestProxyTexImage(
			GLenum target, GLint level, 
			GLint internalFormat, 
			GLenum format, GLenum type, 
			GLint width, GLint height, GLint depth, 
			GLint border
		) {
			return _mesa_test_proxy_teximage(
				(GLcontext *)this,
				target, level,
				internalFormat,
				format, type,
				width, height, depth,
				border
			);
		}
		
		//@}

		//! \name State-query functions @{
		virtual GLboolean GetBooleanv(GLenum pname, GLboolean *result) const {
			return GL_FALSE;
		};
		virtual GLboolean GetDoublev(GLenum pname, GLdouble *result) const {
			return GL_FALSE;
		};
		virtual GLboolean GetFloatv(GLenum pname, GLfloat *result) const {
			return GL_FALSE;
		};
		virtual GLboolean GetIntegerv(GLenum pname, GLint *result) const {
			return GL_FALSE;
		};
		virtual GLboolean GetPointerv(GLenum pname, GLvoid **result) const {
			return GL_FALSE;
		};
		virtual const GLubyte *GetString(GLenum name) const {
			return NULL;
		};
		//@}

		//////////////////////////////////////////////////////////////////////////////////
		// Stuff below this point is still shacky...
		
		virtual void UpdateState(GLuint new_state) = 0;
		virtual void Clear(GLbitfield mask, GLboolean all, GLint x, GLint y, GLint width, GLint height) = 0;
		virtual void DrawBuffer(GLenum buffer) = 0;
		virtual void ReadBuffer(GLenum buffer) = 0;
		virtual void Accum(GLenum op, GLfloat value, GLint xpos, GLint ypos, GLint width, GLint height) = 0;

		//! \name Buffer IO functions @{
		virtual void DrawPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const struct gl_pixelstore_attrib *unpack, const GLvoid *pixels) = 0;
		virtual void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const struct gl_pixelstore_attrib *unpack, GLvoid *dest) = 0;
		virtual void CopyPixels(GLint srcx, GLint srcy, GLsizei width, GLsizei height, GLint dstx, GLint dsty, GLenum type);
		virtual void Bitmap(GLint x, GLint y, GLsizei width, GLsizei height, const struct gl_pixelstore_attrib *unpack, const GLubyte *bitmap);
		//@}



		//! \name Imaging functionality @{
		virtual void CopyColorTable(GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width); 
		virtual void CopyColorSubTable(GLenum target, GLsizei start, GLint x, GLint y, GLsizei width); 
		virtual void CopyConvolutionFilter1D(GLenum target, GLenum internalFormat, GLint x, GLint y, GLsizei width); 
		virtual void CopyConvolutionFilter2D(GLenum target, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height);
		//@}

		//! \name State-changing functions @{
		virtual void AlphaFunc(GLenum func, GLfloat ref);
		virtual void BlendColor(const GLfloat color[4]);
		virtual void BlendEquation(GLenum mode);
		virtual void BlendFunc(GLenum sfactor, GLenum dfactor);
		virtual void BlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorA, GLenum dfactorA);
		virtual void ClearColor(const GLfloat color[4]);
		virtual void ClearDepth(GLclampd d);
		virtual void ClearIndex(GLuint index);
		virtual void ClearStencil(GLint s);
		virtual void ClipPlane(GLenum plane, const GLfloat *equation);
		virtual void ColorMask(GLboolean rmask, GLboolean gmask, GLboolean bmask, GLboolean amask);
		virtual void ColorMaterial(GLenum face, GLenum mode);
		virtual void CullFace(GLenum mode);
		virtual void FrontFace(GLenum mode);
		virtual void DepthFunc(GLenum func);
		virtual void DepthMask(GLboolean flag);
		virtual void DepthRange(GLclampd nearval, GLclampd farval);
		virtual void Enable(GLenum cap, GLboolean state);
		virtual void Fogfv(GLenum pname, const GLfloat *params);
		virtual void Hint(GLenum target, GLenum mode);
		virtual void IndexMask(GLuint mask);
		virtual void Lightfv(GLenum light, GLenum pname, const GLfloat *params);
		virtual void LightModelfv(GLenum pname, const GLfloat *params);
		virtual void LineStipple(GLint factor, GLushort pattern);
		virtual void LineWidth(GLfloat width);
		virtual void LogicOpcode(GLenum opcode);
		virtual void PointParameterfv(GLenum pname, const GLfloat *params);
		virtual void PointSize(GLfloat size);
		virtual void PolygonMode(GLenum face, GLenum mode);
		virtual void PolygonOffset(GLfloat factor, GLfloat units);
		virtual void PolygonStipple(const GLubyte *mask);
		virtual void RenderMode(GLenum mode) {}
		virtual void Scissor(GLint x, GLint y, GLsizei w, GLsizei h);
		virtual void ShadeModel(GLenum mode);
		virtual void StencilFunc(GLenum func, GLint ref, GLuint mask);
		virtual void StencilMask(GLuint mask);
		virtual void StencilOp(GLenum fail, GLenum zfail, GLenum zpass);
		virtual void ActiveStencilFace(GLuint face);
		virtual void TexGen(GLenum coord, GLenum pname, const GLfloat *params);
		virtual void TexEnv(GLenum target, GLenum pname, const GLfloat *param);
		virtual void TexParameter(GLenum target, struct gl_texture_object *texObj, GLenum pname, const GLfloat *params);
		virtual void TextureMatrix(GLuint unit, const GLmatrix *mat);
		virtual void Viewport(GLint x, GLint y, GLsizei w, GLsizei h);
		//@}

		//! \name Vertex array functions @{
		virtual void VertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
		virtual void NormalPointer(GLenum type, GLsizei stride, const GLvoid *ptr);
		virtual void ColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
		virtual void FogCoordPointer(GLenum type, GLsizei stride, const GLvoid *ptr);
		virtual void IndexPointer(GLenum type, GLsizei stride, const GLvoid *ptr);
		virtual void SecondaryColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
		virtual void TexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
		virtual void EdgeFlagPointer(GLsizei stride, const GLvoid *ptr);
		virtual void VertexAttribPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);
		//@}

		virtual void ValidateTnlModule(GLuint new_state);
		virtual void FlushVertices(GLuint flags);
		virtual void LightingSpaceChange();
		virtual void NewList(GLuint list, GLenum mode);
		virtual void EndList();
		virtual void BeginCallList(GLuint list);
		virtual void EndCallList();
		
		virtual void LockArraysEXT(GLint first, GLsizei count);
		virtual void UnlockArraysEXT();
} ;

//! Abstract Mesa driver
class Driver {
} ;

}

/* :vim: set sw=2 ts=2 noet : */
