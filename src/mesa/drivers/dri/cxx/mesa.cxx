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
	return ((Mesa::Context *)ctx->DriverCtx)->GetString(name);
}

static void UpdateState(GLcontext *ctx, GLuint new_state) {
	((Mesa::Context *)ctx->DriverCtx)->UpdateState(new_state);
}

static void Clear(GLcontext *ctx, GLbitfield mask, GLboolean all, GLint x, GLint y, GLint width, GLint height) {
	((Mesa::Context *)ctx->DriverCtx)->Clear(mask, all, x, y, width, height);
}

static void DrawBuffer(GLcontext *ctx, GLenum buffer) {
	((Mesa::Context *)ctx->DriverCtx)->DrawBuffer(buffer);
}

static void ReadBuffer(GLcontext *ctx, GLenum buffer) {
	((Mesa::Context *)ctx->DriverCtx)->ReadBuffer(buffer);
}

static void GetBufferSize(GLframebuffer *buffer, GLuint *width, GLuint *height) {
	((Mesa::Framebuffer *)buffer)->getSize(width, height);
}

static void ResizeBuffers(GLframebuffer *buffer) {
	((Mesa::Framebuffer *)buffer)->resize();
}

static void Finish(GLcontext *ctx) {
	((Mesa::Context *)ctx->DriverCtx)->Finish();
}

static void Flush(GLcontext *ctx) {
	((Mesa::Context *)ctx->DriverCtx)->Flush();
}

static void Error(GLcontext *ctx) {
	((Mesa::Context *)ctx->DriverCtx)->Error(ctx->ErrorValue);
}

static void Accum(GLcontext *ctx, GLenum op, GLfloat value, GLint xpos, GLint ypos, GLint width, GLint height) {
	((Mesa::Context *)ctx->DriverCtx)->Accum(op, value, xpos, ypos, width, height);
}

static void DrawPixels(GLcontext *ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const struct gl_pixelstore_attrib *unpack, const GLvoid *pixels) {
	((Mesa::Context *)ctx->DriverCtx)->DrawPixels(x, y, width, height, format, type, unpack, pixels);
}

static void ReadPixels(GLcontext *ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const struct gl_pixelstore_attrib *unpack, GLvoid *dest) {
	((Mesa::Context *)ctx->DriverCtx)->ReadPixels(x, y, width, height, format, type, unpack, dest);
}

static void CopyPixels(GLcontext *ctx, GLint srcx, GLint srcy, GLsizei width, GLsizei height, GLint dstx, GLint dsty, GLenum type) {
	((Mesa::Context *)ctx->DriverCtx)->CopyPixels(srcx, srcy, width, height, dstx, dsty, type);
}

static void Bitmap(GLcontext *ctx, GLint x, GLint y, GLsizei width, GLsizei height, const struct gl_pixelstore_attrib *unpack, const GLubyte *bitmap) {
	((Mesa::Context *)ctx->DriverCtx)->Bitmap(x, y, width, height, unpack, bitmap);
}

static const struct gl_texture_format *ChooseTextureFormat(GLcontext *ctx, GLint internalFormat, GLenum srcFormat, GLenum srcType) {
	return ((Mesa::Context *)ctx->DriverCtx)->ChooseTextureFormat(internalFormat, srcFormat, srcType);
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
	((Mesa::Texture *)texObj->DriverData)->image1D(
		(Mesa::Context *)ctx->DriverCtx,
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
	((Mesa::Texture *)texObj->DriverData)->image2D(
		(Mesa::Context *)ctx->DriverCtx,
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
	((Mesa::Texture *)texObj->DriverData)->image3D(
		(Mesa::Context *)ctx->DriverCtx,
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
	((Mesa::Texture *)texObj->DriverData)->subImage1D(
		(Mesa::Context *)ctx->DriverCtx,
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
	((Mesa::Texture *)texObj->DriverData)->subImage2D(
		(Mesa::Context *)ctx->DriverCtx, 
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
	((Mesa::Texture *)texObj->DriverData)->subImage3D(
		(Mesa::Context *)ctx->DriverCtx,
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
	((Mesa::Context *)ctx->DriverCtx)->CopyTexImage1D(target, level, internalFormat, x, y, width, border);
}

static void CopyTexImage2D(GLcontext *ctx, GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
	((Mesa::Context *)ctx->DriverCtx)->CopyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}

static void CopyTexSubImage1D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
	((Mesa::Context *)ctx->DriverCtx)->CopyTexSubImage1D(target, level, xoffset, x, y, width);
}

static void CopyTexSubImage2D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
	((Mesa::Context *)ctx->DriverCtx)->CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

static void CopyTexSubImage3D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
	((Mesa::Context *)ctx->DriverCtx)->CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

static GLboolean TestProxyTexImage(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLenum format, GLenum type, GLint width, GLint height, GLint depth, GLint border) {
	return ((Mesa::Context *)ctx->DriverCtx)->TestProxyTexImage(target, level, internalFormat, format, type, width, height, depth, border);
}

static void CompressedTexImage1D(GLcontext *ctx, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data, struct gl_texture_object *texObj, struct gl_texture_image *texImage) {
	assert(texObj->DriverData);
	((Mesa::Texture *)texObj->DriverData)->compressedImage3D(
		(Mesa::Context *)ctx->DriverCtx,
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
	((Mesa::Texture *)texObj->DriverData)->compressedImage3D(
		(Mesa::Context *)ctx->DriverCtx,
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
	((Mesa::Texture *)texObj->DriverData)->compressedImage3D(
		(Mesa::Context *)ctx->DriverCtx,
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
	((Mesa::Texture *)texObj->DriverData)->compressedSubImage3D(
		(Mesa::Context *)ctx->DriverCtx,
		target, level,
		xoffset,
		width,
		format,	
		imageSize, data,
		(Mesa::Image *)texImage
	);
}

static void CompressedTexSubImage2D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLint height, GLenum format, GLsizei imageSize, const GLvoid *data, struct gl_texture_object *texObj, struct gl_texture_image *texImage) {
	((Mesa::Texture *)texObj->DriverData)->compressedSubImage3D(
		(Mesa::Context *)ctx->DriverCtx,
		target, level,
		xoffset, yoffset,
		width, height,
		format,	
		imageSize, data,
		(Mesa::Image *)texImage
	);
}

static void CompressedTexSubImage3D(GLcontext *ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLint height, GLint depth, GLenum format, GLsizei imageSize, const GLvoid *data, struct gl_texture_object *texObj, struct gl_texture_image *texImage) {
	((Mesa::Texture *)texObj->DriverData)->compressedSubImage3D(
		(Mesa::Context *)ctx->DriverCtx,
		target, level,
		xoffset, yoffset, zoffset,
		width, height, depth,
		format,	
		imageSize, data,
		(Mesa::Image *)texImage
	);
}

static void BindTexture(GLcontext *ctx, GLenum target, struct gl_texture_object *tObj) {
	Mesa::Texture *texture = (Mesa::Texture *)tObj->DriverData;
	if(!texture) {
		texture = ((Mesa::Context *)ctx->DriverCtx)->AllocTexture(target);
		texture->init(tObj);
	}
	((Mesa::Context *)ctx->DriverCtx)->BindTexture(target, texture);
}

static void DeleteTexture(GLcontext *ctx, struct gl_texture_object *t) {
	delete ((Mesa::Texture *)(t->DriverData));
}

static GLboolean IsTextureResident(GLcontext *ctx, struct gl_texture_object *t) {
	return ((Mesa::Texture *)t->DriverData)->isResident();
}

static void PrioritizeTexture(GLcontext *ctx, struct gl_texture_object *t, GLclampf priority) {
	return ((Mesa::Texture *)t->DriverData)->prioritize(priority);
}

static void ActiveTexture(GLcontext *ctx, GLuint texUnitNumber) {
	((Mesa::Context *)ctx->DriverCtx)->ActiveTexture(texUnitNumber);
}

static void UpdateTexturePalette(GLcontext *ctx, struct gl_texture_object *tObj) {
	((Mesa::Context *)ctx->DriverCtx)->UpdateTexturePalette((Mesa::Texture *)tObj->DriverData);
}

static void CopyColorTable(GLcontext *ctx, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width) {
	((Mesa::Context *)ctx->DriverCtx)->CopyColorTable(target, internalformat, x, y, width);
}

static void CopyColorSubTable(GLcontext *ctx, GLenum target, GLsizei start, GLint x, GLint y, GLsizei width) {
	((Mesa::Context *)ctx->DriverCtx)->CopyColorSubTable(target, start, x, y, width);
}

static void CopyConvolutionFilter1D(GLcontext *ctx, GLenum target, GLenum internalFormat, GLint x, GLint y, GLsizei width) {
	((Mesa::Context *)ctx->DriverCtx)->CopyConvolutionFilter1D(target, internalFormat, x, y, width);
}

static void CopyConvolutionFilter2D(GLcontext *ctx, GLenum target, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height) {
	((Mesa::Context *)ctx->DriverCtx)->CopyConvolutionFilter2D(target, internalFormat, x, y, width, height);
}

static void AlphaFunc(GLcontext *ctx, GLenum func, GLfloat ref) {
	((Mesa::Context *)ctx->DriverCtx)->AlphaFunc(func, ref);
}

static void BlendColor(GLcontext *ctx, const GLfloat color[4]) {
	((Mesa::Context *)ctx->DriverCtx)->BlendColor(color);
}

static void BlendEquation(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx->DriverCtx)->BlendEquation(mode);
}

static void BlendFunc(GLcontext *ctx, GLenum sfactor, GLenum dfactor) {
	((Mesa::Context *)ctx->DriverCtx)->BlendFunc(sfactor, dfactor);
}

static void BlendFuncSeparate(GLcontext *ctx, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorA, GLenum dfactorA) {
	((Mesa::Context *)ctx->DriverCtx)->BlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorA, dfactorA);
}

static void ClearColor(GLcontext *ctx, const GLfloat color[4]) {
	((Mesa::Context *)ctx->DriverCtx)->ClearColor(color);
}

static void ClearDepth(GLcontext *ctx, GLclampd d) {
	((Mesa::Context *)ctx->DriverCtx)->ClearDepth(d);
}

static void ClearIndex(GLcontext *ctx, GLuint index) {
	((Mesa::Context *)ctx->DriverCtx)->ClearIndex(index);
}

static void ClearStencil(GLcontext *ctx, GLint s) {
	((Mesa::Context *)ctx->DriverCtx)->ClearStencil(s);
}

static void ClipPlane(GLcontext *ctx, GLenum plane, const GLfloat *equation) {
	((Mesa::Context *)ctx->DriverCtx)->ClipPlane(plane, equation);
}

static void ColorMask(GLcontext *ctx, GLboolean rmask, GLboolean gmask, GLboolean bmask, GLboolean amask) {
	((Mesa::Context *)ctx->DriverCtx)->ColorMask(rmask, gmask, bmask, amask);
}

static void ColorMaterial(GLcontext *ctx, GLenum face, GLenum mode) {
	((Mesa::Context *)ctx->DriverCtx)->ColorMaterial(face, mode);
}

static void CullFace(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx->DriverCtx)->CullFace(mode);
}

static void FrontFace(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx->DriverCtx)->FrontFace(mode);
}

static void DepthFunc(GLcontext *ctx, GLenum func) {
	((Mesa::Context *)ctx->DriverCtx)->DepthFunc(func);
}

static void DepthMask(GLcontext *ctx, GLboolean flag) {
	((Mesa::Context *)ctx->DriverCtx)->DepthMask(flag);
}

static void DepthRange(GLcontext *ctx, GLclampd nearval, GLclampd farval) {
	((Mesa::Context *)ctx->DriverCtx)->DepthRange(nearval, farval);
}

static void Enable(GLcontext*ctx, GLenum cap, GLboolean state) {
	((Mesa::Context *)ctx->DriverCtx)->Enable(cap, state);
}

static void Fogfv(GLcontext *ctx, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx->DriverCtx)->Fogfv(pname, params);
}

static void Hint(GLcontext *ctx, GLenum target, GLenum mode) {
	((Mesa::Context *)ctx->DriverCtx)->Hint(target, mode);
}

static void IndexMask(GLcontext *ctx, GLuint mask) {
	((Mesa::Context *)ctx->DriverCtx)->IndexMask(mask);
}

static void Lightfv(GLcontext *ctx, GLenum light, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx->DriverCtx)->Lightfv(light, pname, params);
}

static void LightModelfv(GLcontext *ctx, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx->DriverCtx)->LightModelfv(pname, params);
}

static void LineStipple(GLcontext *ctx, GLint factor, GLushort pattern) {
	((Mesa::Context *)ctx->DriverCtx)->LineStipple(factor, pattern);
}

static void LineWidth(GLcontext *ctx, GLfloat width) {
	((Mesa::Context *)ctx->DriverCtx)->LineWidth(width);
}

static void LogicOpcode(GLcontext *ctx, GLenum opcode) {
	((Mesa::Context *)ctx->DriverCtx)->LogicOpcode(opcode);
}

static void PointParameterfv(GLcontext *ctx, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx->DriverCtx)->PointParameterfv(pname, params);
}

static void PointSize(GLcontext *ctx, GLfloat size) {
	((Mesa::Context *)ctx->DriverCtx)->PointSize(size);
}

static void PolygonMode(GLcontext *ctx, GLenum face, GLenum mode) {
	((Mesa::Context *)ctx->DriverCtx)->PolygonMode(face, mode);
}

static void PolygonOffset(GLcontext *ctx, GLfloat factor, GLfloat units) {
	((Mesa::Context *)ctx->DriverCtx)->PolygonOffset(factor, units);
}

static void PolygonStipple(GLcontext *ctx, const GLubyte *mask) {
	((Mesa::Context *)ctx->DriverCtx)->PolygonStipple(mask);
}

static void RenderMode(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx->DriverCtx)->RenderMode(mode);
}

static void Scissor(GLcontext *ctx, GLint x, GLint y, GLsizei w, GLsizei h) {
	((Mesa::Context *)ctx->DriverCtx)->Scissor(x, y, w, h);
}

static void ShadeModel(GLcontext *ctx, GLenum mode) {
	((Mesa::Context *)ctx->DriverCtx)->ShadeModel(mode);
}

static void StencilFunc(GLcontext *ctx, GLenum func, GLint ref, GLuint mask) {
	((Mesa::Context *)ctx->DriverCtx)->StencilFunc(func, ref, mask);
}

static void StencilMask(GLcontext *ctx, GLuint mask) {
	((Mesa::Context *)ctx->DriverCtx)->StencilMask(mask);
}

static void StencilOp(GLcontext *ctx, GLenum fail, GLenum zfail, GLenum zpass) {
	((Mesa::Context *)ctx->DriverCtx)->StencilOp(fail, zfail, zpass);
}

static void ActiveStencilFace(GLcontext *ctx, GLuint face) {
	((Mesa::Context *)ctx->DriverCtx)->ActiveStencilFace(face);
}

static void TexGen(GLcontext *ctx, GLenum coord, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx->DriverCtx)->TexGen(coord, pname, params);
}

static void TexEnv(GLcontext *ctx, GLenum target, GLenum pname, const GLfloat *param) {
	((Mesa::Context *)ctx->DriverCtx)->TexEnv(target, pname, param);
}

static void TexParameter(GLcontext *ctx, GLenum target, struct gl_texture_object *texObj, GLenum pname, const GLfloat *params) {
	((Mesa::Context *)ctx->DriverCtx)->TexParameter(target, texObj, pname, params);
}

static void TextureMatrix(GLcontext *ctx, GLuint unit, const GLmatrix *mat) {
	((Mesa::Context *)ctx->DriverCtx)->TextureMatrix(unit, mat);
}

static void Viewport(GLcontext *ctx, GLint x, GLint y, GLsizei w, GLsizei h) {
	((Mesa::Context *)ctx->DriverCtx)->Viewport(x, y, w, h);
}

static void VertexPointer(GLcontext *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx->DriverCtx)->VertexPointer(size, type, stride, ptr);
}

static void NormalPointer(GLcontext *ctx, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx->DriverCtx)->NormalPointer(type, stride, ptr);
}

static void ColorPointer(GLcontext *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx->DriverCtx)->ColorPointer(size, type, stride, ptr);
}

static void FogCoordPointer(GLcontext *ctx, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx->DriverCtx)->FogCoordPointer(type, stride, ptr);
}

static void IndexPointer(GLcontext *ctx, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx->DriverCtx)->IndexPointer(type, stride, ptr);
}

static void SecondaryColorPointer(GLcontext *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx->DriverCtx)->SecondaryColorPointer(size, type, stride, ptr);
}

static void TexCoordPointer(GLcontext *ctx, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx->DriverCtx)->TexCoordPointer(size, type, stride, ptr);
}

static void EdgeFlagPointer(GLcontext *ctx, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx->DriverCtx)->EdgeFlagPointer(stride, ptr);
}

static void VertexAttribPointer(GLcontext *ctx, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr) {
	((Mesa::Context *)ctx->DriverCtx)->VertexAttribPointer(index, size, type, stride, ptr);
}

static GLboolean GetBooleanv(GLcontext *ctx, GLenum pname, GLboolean *result) {
	return ((Mesa::Context *)ctx->DriverCtx)->GetBooleanv(pname, result);
}

static GLboolean GetDoublev(GLcontext *ctx, GLenum pname, GLdouble *result) {
	return ((Mesa::Context *)ctx->DriverCtx)->GetDoublev(pname, result);
}

static GLboolean GetFloatv(GLcontext *ctx, GLenum pname, GLfloat *result) {
	return ((Mesa::Context *)ctx->DriverCtx)->GetFloatv(pname, result);
}

static GLboolean GetIntegerv(GLcontext *ctx, GLenum pname, GLint *result) {
	return ((Mesa::Context *)ctx->DriverCtx)->GetIntegerv(pname, result);
}

static GLboolean GetPointerv(GLcontext *ctx, GLenum pname, GLvoid **result) {
	return ((Mesa::Context *)ctx->DriverCtx)->GetPointerv(pname, result);
}

static void ValidateTnlModule(GLcontext *ctx, GLuint new_state) {
	((Mesa::Context *)ctx->DriverCtx)->ValidateTnlModule(new_state);
}

static void FlushVertices(GLcontext *ctx, GLuint flags) {
	((Mesa::Context *)ctx->DriverCtx)->FlushVertices(flags);
}

static void LightingSpaceChange(GLcontext *ctx) {
	((Mesa::Context *)ctx->DriverCtx)->LightingSpaceChange();
}

static void NewList(GLcontext *ctx, GLuint list, GLenum mode) {
	((Mesa::Context *)ctx->DriverCtx)->NewList(list, mode);
}

static void EndList(GLcontext *ctx) {
	((Mesa::Context *)ctx->DriverCtx)->EndList();
}

static void BeginCallList(GLcontext *ctx, GLuint list) {
	((Mesa::Context *)ctx->DriverCtx)->BeginCallList(list);
}

static void EndCallList(GLcontext *ctx) {
	((Mesa::Context *)ctx->DriverCtx)->EndCallList();
}

static void MakeCurrent(GLcontext *ctx, GLframebuffer *drawBuffer, GLframebuffer *readBuffer) {
//	((Mesa::Context *)ctx->DriverCtx)->MakeCurrent((Mesa::Framebuffer *)drawBuffer, (Mesa::Framebuffer *)readBuffer);
}

static void LockArraysEXT(GLcontext *ctx, GLint first, GLsizei count) {
	((Mesa::Context *)ctx->DriverCtx)->LockArraysEXT(first, count);
}

static void UnlockArraysEXT(GLcontext *ctx) {
	((Mesa::Context *)ctx->DriverCtx)->UnlockArraysEXT();
}

}

/*@}*/


/**********************************************************************/
/*! \name Context implementation */
/*@{*/

namespace Mesa {

bool Context::init(const Visual *visual, Context *sharedCtx, bool direct) {
	if(!_mesa_initialize_context(&glCtx, &visual->glVisual, sharedCtx ? &sharedCtx->glCtx : NULL, (void *)this, direct))
		return false;

	glCtx.Driver.GetString = ::GetString;
	glCtx.Driver.UpdateState = ::UpdateState;
	glCtx.Driver.Clear = ::Clear;
	glCtx.Driver.DrawBuffer = ::DrawBuffer;
	glCtx.Driver.ReadBuffer = ::ReadBuffer;
	glCtx.Driver.GetBufferSize = ::GetBufferSize;
	glCtx.Driver.ResizeBuffers = ::ResizeBuffers;
	glCtx.Driver.Finish = ::Finish;
	glCtx.Driver.Flush = ::Flush;
	glCtx.Driver.Error = ::Error;
	glCtx.Driver.Accum = ::Accum;
	glCtx.Driver.DrawPixels = ::DrawPixels;
	glCtx.Driver.ReadPixels = ::ReadPixels;
	glCtx.Driver.CopyPixels = ::CopyPixels;
	glCtx.Driver.Bitmap = ::Bitmap;
	glCtx.Driver.ChooseTextureFormat = ::ChooseTextureFormat;
	glCtx.Driver.TexImage1D = ::TexImage1D;
	glCtx.Driver.TexImage2D = ::TexImage2D;
	glCtx.Driver.TexImage3D = ::TexImage3D;
	glCtx.Driver.TexSubImage1D = ::TexSubImage1D;
	glCtx.Driver.TexSubImage2D = ::TexSubImage2D;
	glCtx.Driver.TexSubImage3D = ::TexSubImage3D;
	glCtx.Driver.CopyTexImage1D = ::CopyTexImage1D;
	glCtx.Driver.CopyTexImage2D = ::CopyTexImage2D;
	glCtx.Driver.CopyTexSubImage1D = ::CopyTexSubImage1D;
	glCtx.Driver.CopyTexSubImage2D = ::CopyTexSubImage2D;
	glCtx.Driver.CopyTexSubImage3D = ::CopyTexSubImage3D;
	glCtx.Driver.TestProxyTexImage = ::TestProxyTexImage;
	glCtx.Driver.CompressedTexImage1D = ::CompressedTexImage1D;
	glCtx.Driver.CompressedTexImage2D = ::CompressedTexImage2D;
	glCtx.Driver.CompressedTexImage3D = ::CompressedTexImage3D;
	glCtx.Driver.CompressedTexSubImage1D = ::CompressedTexSubImage1D;
	glCtx.Driver.CompressedTexSubImage2D = ::CompressedTexSubImage2D;
	glCtx.Driver.CompressedTexSubImage3D = ::CompressedTexSubImage3D;
	glCtx.Driver.BindTexture = ::BindTexture;
//	glCtx.Driver.CreateTexture = ::CreateTexture;
	glCtx.Driver.DeleteTexture = ::DeleteTexture;
	glCtx.Driver.IsTextureResident = ::IsTextureResident;
	glCtx.Driver.PrioritizeTexture = ::PrioritizeTexture;
	glCtx.Driver.ActiveTexture = ::ActiveTexture;
	glCtx.Driver.UpdateTexturePalette = ::UpdateTexturePalette;
	glCtx.Driver.CopyColorTable = ::CopyColorTable;
	glCtx.Driver.CopyColorSubTable = ::CopyColorSubTable;
	glCtx.Driver.CopyConvolutionFilter1D = ::CopyConvolutionFilter1D;
	glCtx.Driver.CopyConvolutionFilter2D = ::CopyConvolutionFilter2D;
	glCtx.Driver.AlphaFunc = ::AlphaFunc;
	glCtx.Driver.BlendColor = ::BlendColor;
	glCtx.Driver.BlendEquation = ::BlendEquation;
	glCtx.Driver.BlendFunc = ::BlendFunc;
	glCtx.Driver.BlendFuncSeparate = ::BlendFuncSeparate;
	glCtx.Driver.ClearColor = ::ClearColor;
	glCtx.Driver.ClearDepth = ::ClearDepth;
	glCtx.Driver.ClearIndex = ::ClearIndex;
	glCtx.Driver.ClearStencil = ::ClearStencil;
	glCtx.Driver.ClipPlane = ::ClipPlane;
	glCtx.Driver.ColorMask = ::ColorMask;
	glCtx.Driver.ColorMaterial = ::ColorMaterial;
	glCtx.Driver.CullFace = ::CullFace;
	glCtx.Driver.FrontFace = ::FrontFace;
	glCtx.Driver.DepthFunc = ::DepthFunc;
	glCtx.Driver.DepthMask = ::DepthMask;
	glCtx.Driver.DepthRange = ::DepthRange;
	glCtx.Driver.Enable = ::Enable;
	glCtx.Driver.Fogfv = ::Fogfv;
	glCtx.Driver.Hint = ::Hint;
	glCtx.Driver.IndexMask = ::IndexMask;
	glCtx.Driver.Lightfv = ::Lightfv;
	glCtx.Driver.LightModelfv = ::LightModelfv;
	glCtx.Driver.LineStipple = ::LineStipple;
	glCtx.Driver.LineWidth = ::LineWidth;
	glCtx.Driver.LogicOpcode = ::LogicOpcode;
	glCtx.Driver.PointParameterfv = ::PointParameterfv;
	glCtx.Driver.PointSize = ::PointSize;
	glCtx.Driver.PolygonMode = ::PolygonMode;
	glCtx.Driver.PolygonOffset = ::PolygonOffset;
	glCtx.Driver.PolygonStipple = ::PolygonStipple;
	glCtx.Driver.RenderMode = ::RenderMode;
	glCtx.Driver.Scissor = ::Scissor;
	glCtx.Driver.ShadeModel = ::ShadeModel;
	glCtx.Driver.StencilFunc = ::StencilFunc;
	glCtx.Driver.StencilMask = ::StencilMask;
	glCtx.Driver.StencilOp = ::StencilOp;
	glCtx.Driver.ActiveStencilFace = ::ActiveStencilFace;
	glCtx.Driver.TexGen = ::TexGen;
	glCtx.Driver.TexEnv = ::TexEnv;
	glCtx.Driver.TexParameter = ::TexParameter;
	glCtx.Driver.TextureMatrix = ::TextureMatrix;
	glCtx.Driver.Viewport = ::Viewport;
	glCtx.Driver.VertexPointer = ::VertexPointer;
	glCtx.Driver.NormalPointer = ::NormalPointer;
	glCtx.Driver.ColorPointer = ::ColorPointer;
	glCtx.Driver.FogCoordPointer = ::FogCoordPointer;
	glCtx.Driver.IndexPointer = ::IndexPointer;
	glCtx.Driver.SecondaryColorPointer = ::SecondaryColorPointer;
	glCtx.Driver.TexCoordPointer = ::TexCoordPointer;
	glCtx.Driver.EdgeFlagPointer = ::EdgeFlagPointer;
	glCtx.Driver.VertexAttribPointer = ::VertexAttribPointer;
	glCtx.Driver.GetBooleanv = ::GetBooleanv;
	glCtx.Driver.GetDoublev = ::GetDoublev;
	glCtx.Driver.GetFloatv = ::GetFloatv;
	glCtx.Driver.GetIntegerv = ::GetIntegerv;
	glCtx.Driver.GetPointerv = ::GetPointerv;
	glCtx.Driver.ValidateTnlModule = ::ValidateTnlModule;
	glCtx.Driver.FlushVertices = ::FlushVertices;
	glCtx.Driver.LightingSpaceChange = ::LightingSpaceChange;
	glCtx.Driver.NewList = ::NewList;
	glCtx.Driver.EndList = ::EndList;
	glCtx.Driver.BeginCallList = ::BeginCallList;
	glCtx.Driver.EndCallList = ::EndCallList;
	glCtx.Driver.MakeCurrent = ::MakeCurrent;
	glCtx.Driver.LockArraysEXT = ::LockArraysEXT;
	glCtx.Driver.UnlockArraysEXT = ::UnlockArraysEXT;

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
	Image *image
) {
	_mesa_store_teximage1d(
		&context->glCtx,
		target,	level,
		internalFormat,
		width,
		border,
		format,	type,
		pixels,	&context->glCtx.Unpack,
		glTexObj,
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
	Image *image
) {
	_mesa_store_teximage2d(
		&context->glCtx,
		target, level,
		internalFormat,
		width, height,
		border,
		format, type,
		pixels,
		&context->glCtx.Unpack,
		glTexObj,
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
	Image *image
) {
	_mesa_store_teximage3d(
		&context->glCtx,
		target, level,
		internalFormat,
		width, height, depth,
		border,
		format, type,
		pixels,
		&context->glCtx.Unpack,
		glTexObj,
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
	Image *image
) {
	_mesa_store_texsubimage1d(
		&context->glCtx,
		target, level,
		xoffset,
		width,
		format, type,
		pixels,
		&context->glCtx.Unpack,
		glTexObj,
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
	Image *image
) {
	_mesa_store_texsubimage2d(
		&context->glCtx,
		target, level,
		xoffset, yoffset,
		width, height,
		format, type,
		pixels,
		&context->glCtx.Unpack,
		glTexObj,
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
	Image *image
) {
	_mesa_store_texsubimage3d(
		&context->glCtx,
		target, level,
		xoffset, yoffset, zoffset,
		width, height, depth,
		format, type,
		pixels,
		&context->glCtx.Unpack,
		glTexObj,
		image
	);
}

}

/*@}*/
