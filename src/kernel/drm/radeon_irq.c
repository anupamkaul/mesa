/**
 * \file radeon_irq.c 
 * \brief IRQ handling for radeon
 * 
 * \author Keith Whitwell <keith@tungstengraphics.com>
 * \author Michel Dänzer <michel@daenzer.net>
 * 
 * Interrupts - Used for device synchronization and flushing in the
 * following circumstances:
 *
 * - Exclusive FB access with hardware idle:
 *    - Wait for GUI idle (?) interrupt, then do normal flush.
 *
 * - Frame throttling, NV_fence:
 *    - Drop marker IRQ's into command stream ahead of time.
 *    - Wait on IRQ's with lock *not held*
 *    - Check each for termination condition
 *
 * - Internally in cp_getbuffer, etc:
 *    - as above, but wait with lock held???
 *
 * \note These functions are misleadingly named -- the IRQ's aren't
 * tied to DMA at all, this is just a hangover from DRI prehistory.
 */

/**
 * Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.
 * 
 * The Weather Channel (TM) funded Tungsten Graphics to develop the
 * initial release of the Radeon 8500 driver under the XFree86 license.
 * This notice must be preserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


#include "radeon.h"
#include "drmP.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_drv.h"


/**
 * IRQ handler.  Wakes up the SW interrupt or VBLANK interrupt queues according
 * to the value of the GEN_INT_STATUS register, and acknowledge the interrupts.
 */
void DRM(dma_service)( DRM_IRQ_ARGS )
{
	drm_device_t *dev = (drm_device_t *) arg;
	drm_radeon_private_t *dev_priv = 
	   (drm_radeon_private_t *)dev->dev_private;
   	u32 stat;

	/* Only consider the bits we're interested in - others could be used
	 * outside the DRM
	 */
	stat = RADEON_READ(RADEON_GEN_INT_STATUS)
	     & (RADEON_SW_INT_TEST | RADEON_CRTC_VBLANK_STAT);
	if (!stat)
		return;

	/* SW interrupt */
	if (stat & RADEON_SW_INT_TEST) {
		DRM_WAKEUP( &dev_priv->swi_queue );
	}

	/* VBLANK interrupt */
	if (stat & RADEON_CRTC_VBLANK_STAT) {
		atomic_inc(&dev->vbl_received);
		DRM_WAKEUP(&dev->vbl_queue);
		DRM(vbl_send_signals)( dev );
	}

	/* Acknowledge interrupts we handle */
	RADEON_WRITE(RADEON_GEN_INT_STATUS, stat);
}

/**
 * Used by the other functions to acknowledge the IRQs and reset their status.
 *
 * \param dev_priv device private data.
 */
static __inline__ void radeon_acknowledge_irqs(drm_radeon_private_t *dev_priv)
{
	u32 tmp = RADEON_READ( RADEON_GEN_INT_STATUS )
		& (RADEON_SW_INT_TEST_ACK | RADEON_CRTC_VBLANK_STAT);
	if (tmp)
		RADEON_WRITE( RADEON_GEN_INT_STATUS, tmp );
}

/**
 * Emit a SW IRQ into the ring. The value emitted,
 * drm_radeon_private::swi_emitted, is incremented atomically.
 *
 * \param dev device structure.
 * \return the SW interrupt value emitted.
 */
int radeon_emit_irq(drm_device_t *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	unsigned int ret;
	RING_LOCALS;

	atomic_inc(&dev_priv->swi_emitted);
	ret = atomic_read(&dev_priv->swi_emitted);

	BEGIN_RING( 4 );
	OUT_RING_REG( RADEON_LAST_SWI_REG, ret );
	OUT_RING_REG( RADEON_GEN_INT_STATUS, RADEON_SW_INT_FIRE );
	ADVANCE_RING(); 
 	COMMIT_RING();

	return ret;
}

/**
 * Wait on the SW interrupt. 
 *
 * \param dev device structure.
 * \param swi_nr the SW interrupt value to wait for.
 * \return zero on success or a negative value on failure.
 *
 * Waits on the drm_radeon_private::swi_queue queue until the value of
 * RADEON_LAST_SWI_REG is greater or equal to \p swi_nr .
 *
 * \sa \c DRM_WAIT_ON.
 */
int radeon_wait_irq(drm_device_t *dev, int swi_nr)
{
  	drm_radeon_private_t *dev_priv = 
	   (drm_radeon_private_t *)dev->dev_private;
	int ret = 0;

 	if (RADEON_READ( RADEON_LAST_SWI_REG ) >= swi_nr)  
 		return 0; 

	dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;

	/* This is a hack to work around mysterious freezes on certain
	 * systems:
	 */ 
	radeon_acknowledge_irqs( dev_priv );

	DRM_WAIT_ON( ret, dev_priv->swi_queue, 3 * DRM_HZ, 
		     RADEON_READ( RADEON_LAST_SWI_REG ) >= swi_nr );

	if (ret == -EBUSY)
	   radeon_probable_lockup( dev_priv );

	return ret;
}

/**
 * Emit and wait for a SW IRQ. A short-hand for calling radeon_emit_irq() and
 * radeon_wait_irq() with the returned.
 *
 * \param dev device structure.
 * \return zero on success or a negative value on failure.
 */
int radeon_emit_and_wait_irq(drm_device_t *dev)
{
	return radeon_wait_irq( dev, radeon_emit_irq(dev) );
}

/**
 * Wait for VBLANK interrupt. 
 *
 * \param dev device structure.
 * \param sequence sequence number. The current value on return.
 * \return zero on success or a negative value on failure.
 *
 * Waits on the drm_device::vbl_queue queue until the current VBLANK
 * value on drm_device::vbl_received until it's greater than \p
 * sequence or the difference is more than a day.
 *
 * \sa DRM_WAIT_ON.
 */
int DRM(vblank_wait)(drm_device_t *dev, unsigned int *sequence)
{
  	drm_radeon_private_t *dev_priv = 
	   (drm_radeon_private_t *)dev->dev_private;
	unsigned int cur_vblank;
	int ret = 0;

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return DRM_ERR(EINVAL);
	}

	radeon_acknowledge_irqs( dev_priv );

	dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;

	/* Assume that the user has missed the current sequence number
	 * by about a day rather than she wants to wait for years
	 * using vertical blanks... 
	 */
	DRM_WAIT_ON( ret, dev->vbl_queue, 3*DRM_HZ, 
		     ( ( ( cur_vblank = atomic_read(&dev->vbl_received ) )
			 - *sequence ) <= (1<<23) ) );

	*sequence = cur_vblank;

	if (ret == -EBUSY)
	   radeon_probable_lockup( dev_priv );

	return ret;
}

/**
 * Emit a SW IRQ (ioctl).  Needs the lock as it touches the ring.
 *
 * \return zero on success, or a negative value on failure.
 *
 * Verifies the caller holds the lock, copies the argument structure from user
 * space, and calls radeon_emit_irq().
 */
int radeon_irq_emit( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_irq_emit_t emit;
	int result;

	LOCK_TEST_WITH_RETURN( dev, filp );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( emit, (drm_radeon_irq_emit_t *)data,
				  sizeof(emit) );

	result = radeon_emit_irq( dev );

	if ( DRM_COPY_TO_USER( emit.irq_seq, &result, sizeof(int) ) ) {
		DRM_ERROR( "copy_to_user\n" );
		return DRM_ERR(EFAULT);
	}

	return 0;
}

/**
 * Wait on a IRQ (ioctl).  Doesn't need the hardware lock.
 *
 * \return zero on success, or a negative value on failure.
 *
 * Copies the argument structure from user space and passes it to
 * radeon_wait_irq().
 */
int radeon_irq_wait( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_irq_wait_t irqwait;

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL( irqwait, (drm_radeon_irq_wait_t *)data,
				  sizeof(irqwait) );

	return radeon_wait_irq( dev, irqwait.irq_seq );
}


/** \name drm_dma.h hooks */
/*@{*/

/** Disable \e all interrupts and reset their status */
void DRM(driver_irq_preinstall)( drm_device_t *dev ) {
	drm_radeon_private_t *dev_priv =
		(drm_radeon_private_t *)dev->dev_private;

 	/* Disable *all* interrupts */
      	RADEON_WRITE( RADEON_GEN_INT_CNTL, 0 );

	/* Clear bits if they're already high */
	radeon_acknowledge_irqs( dev_priv );
}

/** Resets drm_radeon_private::swi_emitted and turns the SW and VBLANK interrupts */
void DRM(driver_irq_postinstall)( drm_device_t *dev ) {
	drm_radeon_private_t *dev_priv =
		(drm_radeon_private_t *)dev->dev_private;

   	atomic_set(&dev_priv->swi_emitted, 0);
	DRM_INIT_WAITQUEUE( &dev_priv->swi_queue );

	/* Turn on SW and VBL ints */
   	RADEON_WRITE( RADEON_GEN_INT_CNTL,
		      RADEON_CRTC_VBLANK_MASK |	
		      RADEON_SW_INT_ENABLE );
}

/** Disable \e all interrupts */
void DRM(driver_irq_uninstall)( drm_device_t *dev ) {
	drm_radeon_private_t *dev_priv =
		(drm_radeon_private_t *)dev->dev_private;
	if ( dev_priv ) {
		/* Disable *all* interrupts */
		RADEON_WRITE( RADEON_GEN_INT_CNTL, 0 );
	}
}

/*@}*/
