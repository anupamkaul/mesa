/*!
 * \file mesa.cxx
 * \brief Mesa C++ wrapper classes implementation.
 * 
 * \author José Fonseca <jrfonseca@tungstengraphics.com>
 */

#include <assert.h>

#include "mesa.hxx"

extern "C" {
	
#include "texstore.h"

}

/**********************************************************************/
/*! 
 * \name Device driver callbacks.
 *  
 * \sa dd_function_table.
 */
/*@{*/

extern "C" {
	
static const GLubyte *GetString(GLcontext *ctx, GLenum name) {
	return ((Mesa::Context *)ctx)->GetString(name);
}

static void UpdateState(GLcontext *ctx, GLuint new_state) {
	((Mesa::Context *)ctx)->UpdateState(new_state);
}

static void Clear(GLcontext *ctx, GLbitfield mask, GLboolean all, GLint x, GLint y, GLint width, GLint height) {
	((Mesa::Context *)ctx)->Clear(mask, all, x, y, width, height);
}

static void DrawBuffer(GLcontext *ctx, GLenum buffer) {
	((Mesa::Context *)ctx)->DrawBuffer(buffer);
}

static void ReadBuffer(GLcontext *ctx, GLenum buffer) {
	((Mesa::Context *)ctx)->ReadBuffer(buffer);
}

static void GetBufferSize(GLframebuffer *buffer, GLuint *width, GLuint *height) {
	((Mesa::Framebuffer *)buffer)->getSize(width, height);
}

static void ResizeBuffers(GLframebuffer *buffer) {
	((Mesa::Framebuffer *)buffer)->resize();
}

static void Finish(GLcontext *ctx) {
	((Mesa::Context *)ctx)->Finish();
}

static void Flush(GLcontext *ctx) {
	((Mesa::Context *)ctx)->Flush();
}

static void Error(GLcontext *ctx) {
	((Mesa::Context *)ctx)->Error(ctx->ErrorValue);
}

static void Accum(GLcontext *ctx, GLenum op, GLfloat value, GLint xpos, GLint ypos, GLint width, GLint height) {
	((Mesa::Context *)ctx)->Accum(op, value, xpos, ypos, width, height);
}

static void DrawPixels(GLcontext *ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const struct gl_pixelstore_attrib *unpack, const GLvoid *pixels) {
	((Mesa::Context *)ctx)->DrawPixels(x, y, width, height, format, type, unpack, pixels);
}

static void ReadPixels(GLcontext *ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const struct gl_pixelstore_attrib *unpack, GLvoid *dest) {
	((Mesa::Context *)ctx)->ReadPixels(x, y, width, height, format, type, unpack, dest);
}

static void CopyPixels(GLcontext *ctx, GLint srcx, GLint srcy, GLsizei width, GLsizei height, GLint dstx, GLint dsty, GLenum type) {
	((Mesa::Context *)ctx)->CopyPixels(srcx, srcy, width, height, dstx, dsty, type);
}

static void Bitmap(GLcontext *ctx, GLint x, GLint y, GLsizei width, GLsizei height, const struct gl_pixelstore_attrib *unpack, const GLubyte *bitmap) {
	((Mesa::Context *)ctx)->Bitmap(x, y, width, height, unpack, bitmap);
}

static const struct gl_texture_format *ChooseTextureFormat(GLcontext *ctx, GLint internalFormat, GLenum srcFormat, GLenum srcType) {
	return ((Mesa::Context *)ctx)->ChooseTextureFormat(internalFormat, srcFormat, srcType);
}

static void TexImage1D(GLcontext *ctx,
	GLenum target, GLint level,
	GLint internalFormat,
	GLint width,
	GLint border,
	GLenum format, GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_object *texObj,
	struct gl_texture_image *texImage
) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->image1D(
		(Mesa::Context *)ctx,
		target, level,
		internalFormat,
		width,
		border,
		format, type,
		pixels,
		packing,
		(Mesa::Image *)texImage
	);
}

static void TexImage2D(GLcontext *ctx,
	GLenum target, GLint level,
	GLint internalFormat,
	GLint width, GLint height,
	GLint border,
	GLenum format, GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_object *texObj,
	struct gl_texture_image *texImage
) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->image2D(
		(Mesa::Context *)ctx,
		target, level,
		internalFormat,
		width,
		height,
		border,
		format,
		type,
		pixels,
		packing,
		(Mesa::Image *)texImage
	);
}

static void TexImage3D(GLcontext *ctx,
	GLenum target,
	GLint level,
	GLint internalFormat,
	GLint width,
	GLint height,
	GLint depth,
	GLint border,
	GLenum format,
	GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_object *texObj,
	struct gl_texture_image *texImage
) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->image3D(
		(Mesa::Context *)ctx,
		target,
		level,
		internalFormat,
		width,
		height,
		depth,
		border,
		format,
		type,
		pixels,
		packing,
		(Mesa::Image *)texImage
	);
}

static void TexSubImage1D(GLcontext *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLsizei width,
	GLenum format,
	GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_object *texObj,
	struct gl_texture_image *texImage
) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->subImage1D(
		(Mesa::Context *)ctx,
		target,
		level,
		xoffset,
		width,
		format,
		type,
		pixels,
		packing,
		(Mesa::Image *)texImage
	);
}

static void TexSubImage2D(
	GLcontext *ctx,
	GLenum target, GLint level,
	GLint xoffset, GLint yoffset,
	GLsizei width, GLsizei height,
	GLenum format, GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_object *texObj,
	struct gl_texture_image *texImage
) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->subImage2D(
		(Mesa::Context *)ctx, 
		target, 
		level, 
		xoffset, yoffset, 
		width, height, 
		format, type, 
		pixels, packing, 
		(Mesa::Image *)texImage
	);
}

static void TexSubImage3D(GLcontext *ctx,
	GLenum target,
	GLint level,
	GLint xoffset,
	GLint yoffset,
	GLint zoffset,
	GLsizei width,
	GLsizei height,
	GLint depth,
	GLenum format,
	GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	struct gl_texture_object *texObj,
	struct gl_texture_image *texImage
) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->subImage3D(
		(Mesa::Context *)ctx,
		target, level,
		xoffset, yoffset, zoffset,
		width, height, depth,
		format, type,
		pixels,
		packing,
		(Mesa::Image *)texImage
	);
}

static void CopyTexImage1D(GLcontext *ctx, GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border) {
	((Mesa::Context *)ctx)->CopyTexImage1D(target, level, internalFormat, x, y, width, border);
}

static void CopyTexImage2D(GLcontext *ctx, GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
	((Mesa::Context *)ctx)->CopyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}

static void CopyTexSubImage1D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
	((Mesa::Context *)ctx)->CopyTexSubImage1D(target, level, xoffset, x, y, width);
}

static void CopyTexSubImage2D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
	((Mesa::Context *)ctx)->CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

static void CopyTexSubImage3D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
	((Mesa::Context *)ctx)->CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

static GLboolean TestProxyTexImage(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLenum format, GLenum type, GLint width, GLint height, GLint depth, GLint border) {
	return ((Mesa::Context *)ctx)->TestProxyTexImage(target, level, internalFormat, format, type, width, height, depth, border);
}

static void CompressedTexImage1D(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data, struct gl_texture_object *texObj, struct gl_texture_image *texImage) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->compressedImage3D(
		(Mesa::Context *)ctx,
		target, level,
		internalFormat,
		width,
		border,
		imageSize, data,
		(Mesa::Image *)texImage
	);
}

static void CompressedTexImage2D(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data, struct gl_texture_object *texObj, struct gl_texture_image *texImage) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->compressedImage3D(
		(Mesa::Context *)ctx,
		target, level,
		internalFormat,
		width, height,
		border,
		imageSize, data,
		(Mesa::Image *)texImage
	);
}

static void CompressedTexImage3D(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data, struct gl_texture_object *texObj, struct gl_texture_image *texImage) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->compressedImage3D(
		(Mesa::Context *)ctx,
		target, level,
		internalFormat,
		width, height, depth,
		border,
		imageSize, data,
		(Mesa::Image *)texImage
	);
}

static void CompressedTexSubImage1D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data, struct gl_texture_object *texObj, struct gl_texture_image *texImage) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj)->compressedSubImage3D(
		(Mesa::Context *)ctx,
		target, level,
		xoffset,
		width,
		format,	
		imageSize, data,
		(Mesa::Image *)texImage
	);
}

static void CompressedTexSubImage2D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLint height, GLenum format, GLsizei imageSize, const GLvoid *data, struct gl_texture_object *texObj, struct gl_texture_image *texImage) {
	((Mesa::Texture *)texObj)->compressedSubImage3D(
		(Mesa::Context *)ctx,
		target, level,
		xoffset, yoffset,
		width, height,
		format,	
		imageSize, data,
		(Mesa::Image *)texImage
	);
}

static void CompressedTexSubImage3D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLint height, GLint depth, GLenum format, GLsizei imageSize, const GLvoid *data, struct gl_texture_object *texObj, struct gl_texture_image *texImage) {
	((Mesa::Texture *)texObj)->compressedSubImage3D(
		(Mesa::Context *)ctx,
		target, level,
		xoffset, yoffset, zoffset,
		width, height, depth,
		format,	
		imageSize, data,
		(Mesa::Image *)texImage
	);
}

static void BindTexture(GLcontext *ctx, GLenum target, struct gl_texture_object *texObj) {
	((Mesa::Context *)ctx)->BindTexture(target, (Mesa::Texture *)texObj);
}

static gl_texture_object *NewTextureObject(GLcontext *ctx, GLuint name, GLenum target) {
	return ((Mesa::Context *)ctx)->NewTexture(name, target);
}

static gl_texture_image *NewTextureImage(GLcontext *ctx) {
	return ((Mesa::Context *)ctx)->NewImage();
}

static void DeleteTexture(GLcontext *ctx, struct gl_texture_object *texObj) {
	delete (Mesa::Texture *)texObj;
}

static GLboolean IsTextureResident(GLcontext *ctx, struct gl_texture_object *texObj) {
	return ((Mesa::Texture *)texObj)->isResident();
}

static void PrioritizeTexture(GLcontext *ctx, struct gl_texture_object *texObj, GLclampf priority) {
	return ((Mesa::Texture *)texObj)->prioritize(priority);
}

static void ActiveTexture(GLcontext *ctx, GLuint texUnitNumber) {
	((Mesa::Context *)ctx)->ActiveTexture(texUnitNumber);
}

static void UpdateTexturePalette(GLcontext *ctx, struct gl_texture_object *texObj) {
	((Mesa::Context *)ctx)->UpdateTexturePalette((Mesa::Texture *)texObj);
}

static void CopyColorTable(GLcontext *ctx, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width) {
	((Mesa::Context *)ctx)->CopyColorTable(target, internalformat, x, y, width);
}

static void CopyColorSubTable(GLcontext *ctx, GLenum target, GLsizei start, GLint x, GLint y, GLsizei width) {
	((Mesa::Context *)ctx)->CopyColorSubTable(target, start, x, y, width);
}

static void CopyConvolutionFilter1D(GLcontext *ctx, GLenum target, GLenum internalFormat, GLint x, GLint y, GLsizei width) {
	((Mesa::Context *)ctx)->CopyConvolutionFilter1D(target, internalFormat, x, y, width);
}

static void CopyConvolutionFilter2D(GLcontext *ctx, GLenum target, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height) {
	((Mesa::Context *)ctx)->CopyConvolutionFilter2D(target, internalFormat, x, y, width, height);
}

static void AlphaFunc(GLcontext *ctx, GLenum func, GLfloat ref) {
	((Mesa::Context *)ctx)->AlphaFunc(func, ref);
}

static void BlendColor(GLcontext *ctx, const GLfloat color[4]) {
	((Mesa::Context *)ctx)->BlendColor(color);
}

static void BlendEquation(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx)->BlendEquation(mode);
}

static void BlendFunc(GLcontext *ctx, GLenum sfactor, GLenum dfactor) {
	((Mesa::Context *)ctx)->BlendFunc(sfactor, dfactor);
}

static void BlendFuncSeparate(GLcontext *ctx, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorA, GLenum dfactorA) {
	((Mesa::Context *)ctx)->BlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorA, dfactorA);
}

static void ClearColor(GLcontext *ctx, const GLfloat color[4]) {
	((Mesa::Context *)ctx)->ClearColor(color);
}

static void ClearDepth(GLcontext *ctx, GLclampd d) {
	((Mesa::Context *)ctx)->ClearDepth(d);
}

static void ClearIndex(GLcontext *ctx, GLuint index) {
	((Mesa::Context *)ctx)->ClearIndex(index);
}

static void ClearStencil(GLcontext *ctx, GLint s) {
	((Mesa::Context *)ctx)->ClearStencil(s);
}

static void ClipPlane(GLcontext *ctx, GLenum plane, const GLfloat *equation) {
	((Mesa::Context *)ctx)->ClipPlane(plane, equation);
}

static void ColorMask(GLcontext *ctx, GLboolean rmask, GLboolean gmask, GLboolean bmask, GLboolean amask) {
	((Mesa::Context *)ctx)->ColorMask(rmask, gmask, bmask, amask);
}

static void ColorMaterial(GLcontext *ctx, GLenum face, GLenum mode) {
	((Mesa::Context *)ctx)->ColorMaterial(face, mode);
}

static void CullFace(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx)->CullFace(mode);
}

static void FrontFace(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx)->FrontFace(mode);
}

static void DepthFunc(GLcontext *ctx, GLenum func) {
	((Mesa::Context *)ctx)->DepthFunc(func);
}

static void DepthMask(GLcontext *ctx, GLboolean flag) {
	((Mesa::Context *)ctx)->DepthMask(flag);
}

static void DepthRange(GLcontext *ctx, GLclampd nearval, GLclampd farval) {
	((Mesa::Context *)ctx)->DepthRange(nearval, farval);
}

static void Enable(GLcontext*ctx, GLenum cap, GLboolean state) {
	((Mesa::Context *)ctx)->Enable(cap, state);
}

static void Fogfv(GLcontext *ctx, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx)->Fogfv(pname, params);
}

static void Hint(GLcontext *ctx, GLenum target, GLenum mode) {
	((Mesa::Context *)ctx)->Hint(target, mode);
}

static void IndexMask(GLcontext *ctx, GLuint mask) {
	((Mesa::Context *)ctx)->IndexMask(mask);
}

static void Lightfv(GLcontext *ctx, GLenum light, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx)->Lightfv(light, pname, params);
}

static void LightModelfv(GLcontext *ctx, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx)->LightModelfv(pname, params);
}

static void LineStipple(GLcontext *ctx, GLint factor, GLushort pattern) {
	((Mesa::Context *)ctx)->LineStipple(factor, pattern);
}

static void LineWidth(GLcontext *ctx, GLfloat width) {
	((Mesa::Context *)ctx)->LineWidth(width);
}

static void LogicOpcode(GLcontext *ctx, GLenum opcode) {
	((Mesa::Context *)ctx)->LogicOpcode(opcode);
}

static void PointParameterfv(GLcontext *ctx, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx)->PointParameterfv(pname, params);
}

static void PointSize(GLcontext *ctx, GLfloat size) {
	((Mesa::Context *)ctx)->PointSize(size);
}

static void PolygonMode(GLcontext *ctx, GLenum face, GLenum mode) {
	((Mesa::Context *)ctx)->PolygonMode(face, mode);
}

static void PolygonOffset(GLcontext *ctx, GLfloat factor, GLfloat units) {
	((Mesa::Context *)ctx)->PolygonOffset(factor, units);
}

static void PolygonStipple(GLcontext *ctx, const GLubyte *mask) {
	((Mesa::Context *)ctx)->PolygonStipple(mask);
}

static void RenderMode(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx)->RenderMode(mode);
}

static void Scissor(GLcontext *ctx, GLint x, GLint y, GLsizei w, GLsizei h) {
	((Mesa::Context *)ctx)->Scissor(x, y, w, h);
}

static void ShadeModel(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx)->ShadeModel(mode);
}

static void StencilFunc(GLcontext *ctx, GLenum func, GLint ref, GLuint mask) {
	((Mesa::Context *)ctx)->StencilFunc(func, ref, mask);
}

static void StencilMask(GLcontext *ctx, GLuint mask) {
	((Mesa::Context *)ctx)->StencilMask(mask);
}

static void StencilOp(GLcontext *ctx, GLenum fail, GLenum zfail, GLenum zpass) {
	((Mesa::Context *)ctx)->StencilOp(fail, zfail, zpass);
}

static void ActiveStencilFace(GLcontext *ctx, GLuint face) {
	((Mesa::Context *)ctx)->ActiveStencilFace(face);
}

static void TexGen(GLcontext *ctx, GLenum coord, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx)->TexGen(coord, pname, params);
}

static void TexEnv(GLcontext *ctx, GLenum target, GLenum pname, const GLfloat *param) {
	((Mesa::Context *)ctx)->TexEnv(target, pname, param);
}

static void TexParameter(GLcontext *ctx, GLenum target, struct gl_texture_object *texObj, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx)->TexParameter(target, texObj, pname, params);
}

static void TextureMatrix(GLcontext *ctx, GLuint unit, const GLmatrix *mat) {
	((Mesa::Context *)ctx)->TextureMatrix(unit, mat);
}

static void Viewport(GLcontext *ctx, GLint x, GLint y, GLsizei w, GLsizei h) {
	((Mesa::Context *)ctx)->Viewport(x, y, w, h);
}

static void VertexPointer(GLcontext *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx)->VertexPointer(size, type, stride, ptr);
}

static void NormalPointer(GLcontext *ctx, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx)->NormalPointer(type, stride, ptr);
}

static void ColorPointer(GLcontext *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx)->ColorPointer(size, type, stride, ptr);
}

static void FogCoordPointer(GLcontext *ctx, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx)->FogCoordPointer(type, stride, ptr);
}

static void IndexPointer(GLcontext *ctx, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx)->IndexPointer(type, stride, ptr);
}

static void SecondaryColorPointer(GLcontext *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx)->SecondaryColorPointer(size, type, stride, ptr);
}

static void TexCoordPointer(GLcontext *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx)->TexCoordPointer(size, type, stride, ptr);
}

static void EdgeFlagPointer(GLcontext *ctx, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx)->EdgeFlagPointer(stride, ptr);
}

static void VertexAttribPointer(GLcontext *ctx, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx)->VertexAttribPointer(index, size, type, stride, ptr);
}

static GLboolean GetBooleanv(GLcontext *ctx, GLenum pname, GLboolean *result) {
	return ((Mesa::Context *)ctx)->GetBooleanv(pname, result);
}

static GLboolean GetDoublev(GLcontext *ctx, GLenum pname, GLdouble *result) {
	return ((Mesa::Context *)ctx)->GetDoublev(pname, result);
}

static GLboolean GetFloatv(GLcontext *ctx, GLenum pname, GLfloat *result) {
	return ((Mesa::Context *)ctx)->GetFloatv(pname, result);
}

static GLboolean GetIntegerv(GLcontext *ctx, GLenum pname, GLint *result) {
	return ((Mesa::Context *)ctx)->GetIntegerv(pname, result);
}

static GLboolean GetPointerv(GLcontext *ctx, GLenum pname, GLvoid **result) {
	return ((Mesa::Context *)ctx)->GetPointerv(pname, result);
}

static void ValidateTnlModule(GLcontext *ctx, GLuint new_state) {
	((Mesa::Context *)ctx)->ValidateTnlModule(new_state);
}

static void FlushVertices(GLcontext *ctx, GLuint flags) {
	((Mesa::Context *)ctx)->FlushVertices(flags);
}

static void LightingSpaceChange(GLcontext *ctx) {
	((Mesa::Context *)ctx)->LightingSpaceChange();
}

static void NewList(GLcontext *ctx, GLuint list, GLenum mode) {
	((Mesa::Context *)ctx)->NewList(list, mode);
}

static void EndList(GLcontext *ctx) {
	((Mesa::Context *)ctx)->EndList();
}

static void BeginCallList(GLcontext *ctx, GLuint list) {
	((Mesa::Context *)ctx)->BeginCallList(list);
}

static void EndCallList(GLcontext *ctx) {
	((Mesa::Context *)ctx)->EndCallList();
}

static void MakeCurrent(GLcontext *ctx, GLframebuffer *drawBuffer, GLframebuffer *readBuffer) {
//	((Mesa::Context *)ctx)->MakeCurrent((Mesa::Framebuffer *)drawBuffer, (Mesa::Framebuffer *)readBuffer);
}

static void LockArraysEXT(GLcontext *ctx, GLint first, GLsizei count) {
	((Mesa::Context *)ctx)->LockArraysEXT(first, count);
}

static void UnlockArraysEXT(GLcontext *ctx) {
	((Mesa::Context *)ctx)->UnlockArraysEXT();
}

}

/*@}*/


/**********************************************************************/
/*! \name Context implementation */
/*@{*/

namespace Mesa {

bool Context::init(const Mesa::Visual *visual, Context *sharedCtx, bool direct) {
	memset((GLcontext *)this, 0, sizeof(GLcontext));
	
	Driver.NewTextureObject = ::NewTextureObject;
	Driver.NewTextureImage = ::NewTextureImage;
	
	if(!_mesa_initialize_context((GLcontext *)this, (GLvisual *)visual, sharedCtx ? (GLcontext *)sharedCtx : NULL, (void *)this, direct))
		return false;

	// Set the driver callback functions
	Driver.GetString = ::GetString;
	Driver.UpdateState = ::UpdateState;
	Driver.Clear = ::Clear;
	Driver.DrawBuffer = ::DrawBuffer;
	Driver.ReadBuffer = ::ReadBuffer;
	Driver.GetBufferSize = ::GetBufferSize;
	Driver.ResizeBuffers = ::ResizeBuffers;
	Driver.Finish = ::Finish;
	Driver.Flush = ::Flush;
	Driver.Error = ::Error;
	Driver.Accum = ::Accum;
	Driver.DrawPixels = ::DrawPixels;
	Driver.ReadPixels = ::ReadPixels;
	Driver.CopyPixels = ::CopyPixels;
	Driver.Bitmap = ::Bitmap;
	Driver.ChooseTextureFormat = ::ChooseTextureFormat;
	Driver.TexImage1D = ::TexImage1D;
	Driver.TexImage2D = ::TexImage2D;
	Driver.TexImage3D = ::TexImage3D;
	Driver.TexSubImage1D = ::TexSubImage1D;
	Driver.TexSubImage2D = ::TexSubImage2D;
	Driver.TexSubImage3D = ::TexSubImage3D;
	Driver.CopyTexImage1D = ::CopyTexImage1D;
	Driver.CopyTexImage2D = ::CopyTexImage2D;
	Driver.CopyTexSubImage1D = ::CopyTexSubImage1D;
	Driver.CopyTexSubImage2D = ::CopyTexSubImage2D;
	Driver.CopyTexSubImage3D = ::CopyTexSubImage3D;
	Driver.TestProxyTexImage = ::TestProxyTexImage;
	Driver.CompressedTexImage1D = ::CompressedTexImage1D;
	Driver.CompressedTexImage2D = ::CompressedTexImage2D;
	Driver.CompressedTexImage3D = ::CompressedTexImage3D;
	Driver.CompressedTexSubImage1D = ::CompressedTexSubImage1D;
	Driver.CompressedTexSubImage2D = ::CompressedTexSubImage2D;
	Driver.CompressedTexSubImage3D = ::CompressedTexSubImage3D;
	Driver.BindTexture = ::BindTexture;
	Driver.DeleteTexture = ::DeleteTexture;
	Driver.IsTextureResident = ::IsTextureResident;
	Driver.PrioritizeTexture = ::PrioritizeTexture;
	Driver.ActiveTexture = ::ActiveTexture;
	Driver.UpdateTexturePalette = ::UpdateTexturePalette;
	Driver.CopyColorTable = ::CopyColorTable;
	Driver.CopyColorSubTable = ::CopyColorSubTable;
	Driver.CopyConvolutionFilter1D = ::CopyConvolutionFilter1D;
	Driver.CopyConvolutionFilter2D = ::CopyConvolutionFilter2D;
	Driver.AlphaFunc = ::AlphaFunc;
	Driver.BlendColor = ::BlendColor;
	Driver.BlendEquation = ::BlendEquation;
	Driver.BlendFunc = ::BlendFunc;
	Driver.BlendFuncSeparate = ::BlendFuncSeparate;
	Driver.ClearColor = ::ClearColor;
	Driver.ClearDepth = ::ClearDepth;
	Driver.ClearIndex = ::ClearIndex;
	Driver.ClearStencil = ::ClearStencil;
	Driver.ClipPlane = ::ClipPlane;
	Driver.ColorMask = ::ColorMask;
	Driver.ColorMaterial = ::ColorMaterial;
	Driver.CullFace = ::CullFace;
	Driver.FrontFace = ::FrontFace;
	Driver.DepthFunc = ::DepthFunc;
	Driver.DepthMask = ::DepthMask;
	Driver.DepthRange = ::DepthRange;
	Driver.Enable = ::Enable;
	Driver.Fogfv = ::Fogfv;
	Driver.Hint = ::Hint;
	Driver.IndexMask = ::IndexMask;
	Driver.Lightfv = ::Lightfv;
	Driver.LightModelfv = ::LightModelfv;
	Driver.LineStipple = ::LineStipple;
	Driver.LineWidth = ::LineWidth;
	Driver.LogicOpcode = ::LogicOpcode;
	Driver.PointParameterfv = ::PointParameterfv;
	Driver.PointSize = ::PointSize;
	Driver.PolygonMode = ::PolygonMode;
	Driver.PolygonOffset = ::PolygonOffset;
	Driver.PolygonStipple = ::PolygonStipple;
	Driver.RenderMode = ::RenderMode;
	Driver.Scissor = ::Scissor;
	Driver.ShadeModel = ::ShadeModel;
	Driver.StencilFunc = ::StencilFunc;
	Driver.StencilMask = ::StencilMask;
	Driver.StencilOp = ::StencilOp;
	Driver.ActiveStencilFace = ::ActiveStencilFace;
	Driver.TexGen = ::TexGen;
	Driver.TexEnv = ::TexEnv;
	Driver.TexParameter = ::TexParameter;
	Driver.TextureMatrix = ::TextureMatrix;
	Driver.Viewport = ::Viewport;
	Driver.VertexPointer = ::VertexPointer;
	Driver.NormalPointer = ::NormalPointer;
	Driver.ColorPointer = ::ColorPointer;
	Driver.FogCoordPointer = ::FogCoordPointer;
	Driver.IndexPointer = ::IndexPointer;
	Driver.SecondaryColorPointer = ::SecondaryColorPointer;
	Driver.TexCoordPointer = ::TexCoordPointer;
	Driver.EdgeFlagPointer = ::EdgeFlagPointer;
	Driver.VertexAttribPointer = ::VertexAttribPointer;
	Driver.GetBooleanv = ::GetBooleanv;
	Driver.GetDoublev = ::GetDoublev;
	Driver.GetFloatv = ::GetFloatv;
	Driver.GetIntegerv = ::GetIntegerv;
	Driver.GetPointerv = ::GetPointerv;
	Driver.ValidateTnlModule = ::ValidateTnlModule;
	Driver.FlushVertices = ::FlushVertices;
	Driver.LightingSpaceChange = ::LightingSpaceChange;
	Driver.NewList = ::NewList;
	Driver.EndList = ::EndList;
	Driver.BeginCallList = ::BeginCallList;
	Driver.EndCallList = ::EndCallList;
	Driver.MakeCurrent = ::MakeCurrent;
	Driver.LockArraysEXT = ::LockArraysEXT;
	Driver.UnlockArraysEXT = ::UnlockArraysEXT;

	return true;
}

void Texture::image1D(
	Context *context,
	GLenum target, GLint level,
	GLint internalFormat,
	GLint width,
	GLint border,
	GLenum format, GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	Mesa::Image *image
) {
	_mesa_store_teximage1d(
		(GLcontext *)context,
		target,	level,
		internalFormat,
		width,
		border,
		format,	type,
		pixels,	
		&context->Unpack,
		(gl_texture_object *)this,
		(struct gl_texture_image *)image
	);
}

void Texture::image2D(
	Context *context,
	GLenum target, GLint level,
	GLint internalFormat,
	GLint width, GLint height,
	GLint border,
	GLenum format, GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	Mesa::Image *image
) {
	_mesa_store_teximage2d(
		(GLcontext *)context,
		target, level,
		internalFormat,
		width, height,
		border,
		format, type,
		pixels,
		&context->Unpack,
		(gl_texture_object *)this,
		image
	);
}

void Texture::image3D(
	Context *context,
	GLenum target, GLint level,
	GLint internalFormat,
	GLint width, GLint height, GLint depth,
	GLint border,
	GLenum format, GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	Mesa::Image *image
) {
	_mesa_store_teximage3d(
		(GLcontext *)context,
		target, level,
		internalFormat,
		width, height, depth,
		border,
		format, type,
		pixels,
		&context->Unpack,
		(gl_texture_object *)this,
		image
	);
}

void Texture::subImage1D(Context *context,
	GLenum target, GLint level,
	GLint xoffset,
	GLsizei width,
	GLenum format, GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	Mesa::Image *image
) {
	_mesa_store_texsubimage1d(
		(GLcontext *)context,
		target, level,
		xoffset,
		width,
		format, type,
		pixels,
		&context->Unpack,
		(gl_texture_object *)this,
		image
	);
}

void Texture::subImage2D(
	Context *context,
	GLenum target, GLint level,
	GLint xoffset, GLint yoffset,
	GLsizei width, GLsizei height,
	GLenum format, GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	Mesa::Image *image
) {
	_mesa_store_texsubimage2d(
		(GLcontext *)context,
		target, level,
		xoffset, yoffset,
		width, height,
		format, type,
		pixels,
		&context->Unpack,
		(gl_texture_object *)this,
		image
	);
}

void Texture::subImage3D(
	Context *context,
	GLenum target, GLint level,
	GLint xoffset, GLint yoffset, GLint zoffset,
	GLsizei width, GLsizei height, GLsizei depth,
	GLenum format, GLenum type,
	const GLvoid *pixels,
	const struct gl_pixelstore_attrib *packing,
	Mesa::Image *image
) {
	_mesa_store_texsubimage3d(
		(GLcontext *)context,
		target, level,
		xoffset, yoffset, zoffset,
		width, height, depth,
		format, type,
		pixels,
		&context->Unpack,
		(gl_texture_object *)this,
		image
	);
}

}

/*@}*/
