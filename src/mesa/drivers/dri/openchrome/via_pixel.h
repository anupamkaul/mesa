#ifndef _VIA_PIXEL_H_
#define _VIA_PIXEL_H_

extern void viaInitPixelFuncs(struct dd_function_table *functions);
extern GLboolean
via_compute_image_transfer(GLcontext * ctx,
			   uint32_t * scale_rgba, uint32_t * bias_rgba);

extern void
via_draw_pixels(GLcontext * ctx,
		GLint x, GLint y,
		GLsizei width, GLsizei height,
		GLenum format,
		GLenum type,
		const struct gl_pixelstore_attrib *unpack,
		const GLvoid * pixels);

extern void
via_read_pixels(GLcontext * ctx,
		GLint x, GLint y, GLsizei width, GLsizei height,
		GLenum format, GLenum type,
		const struct gl_pixelstore_attrib *pack, GLvoid * pixels);

#endif
