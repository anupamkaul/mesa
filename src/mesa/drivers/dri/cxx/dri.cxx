/**
 * \file dri.cxx
 * \brief DRI C++ wrapper classes.
 */

/**********************************************************************/
/*! 
 * \name dri_util API 
 *  
 * \sa __DriverAPIRec.
 */
/*@{*/

extern "C" {

//! Create screen callback
static GLboolean createScreen(__DRIscreenPrivate *driScreenPriv) {
	if(!(driScreenPriv->private = (void *)DRI::driver.createScreen(driScreenPriv)))
		return GL_FALSE;
	
	return GL_TRUE;
}

//! Destroy screen callback
static void destroyScreen(__DRIscreenPrivate *driScreenPriv) {
	DRI::Screen *screen = (DRI::Screen *)driScreenPriv->private;

	if(screen) {
		delete screen;
		driScreenPriv->private = NULL;
	}
}

//! Create context callback
static GLboolean createContext(const __GLcontextModes *glVisual, __DRIcontextPrivate *driContextPriv, void *sharedContextPrivate) {
	__DRIscreenPrivate *driScreenPriv = driContextPriv->driScreenPriv;
	DRI::Screen *screen = driScreenPriv->private;

	assert(screen);
	
	if(!(driContextPriv->driverPrivate = (void *)screen.createContext(glVisual, driContextPriv, (DRI::Context *)shareContextPrivate)))
		return GL_FALSE;
	
	return GL_TRUE;
}

//! Destroy context callback
static void DestroyContext(__DRIcontextPrivate *driContextPriv) {
	DRI::Context *context = (DRI::Context *)driContextPriv->driverPrivate;
	
	if(context) {
		delete context;
		driContextPriv->driverPrivate = NULL;
	}
}

//! Create drawable callback
static GLboolean createDrawable(__DRIscreenPrivate *driScreenPriv, __DRIdrawablePrivate *driDrawablePriv, const __GLcontextModes *mesaVis, GLboolean isPixmap) {
	DRI::Screen *screen = driScreenPriv->private;

	assert(screen);
	
	if(!(driContextPriv->driverPrivate = (void *)screen.createDrawable(driDrawablePriv, mesaVis, isPixmap)))
		return GL_FALSE;
	
	return GL_TRUE;
}

//! Destroy drawable callback
static void destroyDrawable(Display *dpy, void *driDrawablePriv) {
	DRI::Drawable *drawable = (DRI::Drawable *)driDrawablePriv->driverPrivate;
	
	if(drawable) {
		delete drawable;
		driDrawablePriv->driverPrivate = NULL;
	}
}

//! Swap buffers callback
static void swapBuffers(__DRIdrawablePrivate *driDrawablePriv) {
	DRI::Drawable *drawable = (DRI::Drawable *)driDrawablePriv->driverPrivate;
	
	assert(drawable);

	drawable->swapBuffers();
}

//! Bind context callback
static GLboolean bindContext(__DRIcontextPrivate *driContextPriv, __DRIdrawablePrivate *driReadDrawablePriv, __DRIdrawablePrivate *driWriteDrawablePriv) {
	DRI::Context *context = (DRI::Context *)driContextPriv->driverPrivate;
	DRI::Drawable *readDrawable = (DRI::Drawable *)driReadDrawablePriv->driverPrivate;
	DRI::Drawable *writeDrawable = (DRI::Drawable *)driWriteDrawablePriv->driverPrivate;

	assert(context);
	assert(readDrawable);
	assert(writeDrawable);
	
	return context->bind(read, write) ? GL_TRUE : GL_FALSE;
}

//! Unbind context callback
static GLboolean unbindContext(__DRIcontextPrivate *driContextPriv) {
	DRI::Context *context = (DRI::Context *)driContextPriv->driverPrivate;

	assert(context);
	
	return context->unbind() ? GL_TRUE : GL_FALSE;
}

//! Open full screen callback
static GLboolean openFullScreen(__DRIcontextPrivate *driContextPriv) {
	DRI::Context *context = (DRI::Context *)driContextPriv->driverPrivate;

	assert(context);
	
	return context->openFullScreen() ? GL_TRUE : GL_FALSE;
}
	
//! Close full screen callback
static GLboolean closeFullScreen(__DRIcontextPrivate *driContextPriv) {
	DRI::Context *context = (DRI::Context *)driContextPriv->driverPrivate;

	assert(context);
	
	return context->closeFullScreen() ? GL_TRUE : GL_FALSE;
}

//! Callback table
static struct __DriverAPIRec driverAPI = {
	createScreen,
	destroyScreen,
	createContext,
	destroyContext,
	createDrawable,
	destroyDrawable,
	swapBuffers,
	bindContext,
	unbindContext,
	openFullScreen,
	closeFullScreen
} ;

}

/*@}*/


/**********************************************************************/
/*! \name libGL API */
/*@{*/

extern "C" {
	
//! Create screen callback.
void *__driCreateScreen(Display *dpy, int scrn, __DRIscreen *driScreen, int numConfigs, __GLXvisualConfig *config)
{
	return __driUtilcreateScreen(dpy, scrn, driScreen, numConfigs, config, &driverAPI);
}

//! Register extensions callback.
void __driRegisterExtensions(void) {
}

}

/*@}*/
