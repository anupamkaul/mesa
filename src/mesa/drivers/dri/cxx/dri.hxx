/*!
 * \file dri.hxx
 * \brief DRI C++ wrapper classes.
 *
 * \author José Fonseca <jrfonseca@tungstengraphics.com>
 */

#include "GL/GL.h"

//! Abstract classes for a DRI driver
/*!
 * All DRI drivers should inherit these classes and fullfill all pure virtual
 * member functions.
 * 
 * This DRI C++ wrapper classes are meant to be completely independent of Mesa,
 * giving the possibility to build drivers based on other OpenGL software
 * implementations.
 *
 * \sa
 * These interfaces are modelled around dri_util.h.
 *
 * \todo Things to do here:
 * - DRM abstraction.
 */
namespace DRI {

//! Abstract DRI drawable
class Drawable {
	private:
		__DRIdrawablePrivate *driDrawablePriv;

	public:
		//! Constructor
		Drawable(__DRIdrawablePrivate *_driDrawablePriv) {
			driDrawablePriv = _driDrawablePriv;
		}

		//! Destructor
		virtual ~Drawable() {
		}

		//! Swap front and back color buffers
		virtual void swapBuffers(void) = 0;
} ;

//! Abstract DRI context
class Context {
	private:
		__DRIcontextPrivate *driContextPriv;

	public:
		//! Constructor
		Context(__DRIcontextPrivate *_driContextPriv) {
			driContextPriv = _driContextPriv;
		}

		//! Destructor
		virtual ~Context() {
		}

		//! Bind context
		virtual bool bind(Drawable *readDrawable, Drawable *writeDrawable) = 0;

		//! Unbind context
		virtual bool unbind(void) = 0;

		//! Open full screen
		virtual bool openFullScreen(void) { 
			return true; 
		}

		//! Close full screen
		virtual bool closeFullScreen(void) { 
			return true; 
		}
} ;

//! Abstract DRI screen
class Screen {
	private:
		__DRIscreenPrivate *driScreenPriv;

	public:
		//! Constructor
		Screen(__DRIscreenPrivate *_driScreenPriv) {
			driScreenPriv = _driScreenPriv;
		}

		//! Destructor
		virtual ~Screen() {
		}

		//! Create context
		virtual Context *createContext(const __GLcontextModes *glVisual, __DRIcontextPrivate *driContextPriv, Context *sharedContext) = 0;
		
		//! Create drawable
		virtual Context *createDrawable(__DRIdrawablePrivate *driDrawablePriv, const __GLcontextModes *mesaVis, bool isPixmap) = 0;
} ;

//! Abstract DRI driver
class Driver 
{
	public:
		//! Destructor
		virtual ~Driver() {
		}

		//! Create screen
		virtual Screen *createScreen(Display *dpy, int scrn, int numConfigs, __GLXvisualConfig *config) = 0;

		//! Create drawable
		virtual Drawable *createDrawable(GLXDrawable draw, VisualID vid) = 0;
} ;

//! DRI driver instance
extern Driver driver;

}
