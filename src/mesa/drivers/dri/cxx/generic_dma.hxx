
namespace Generic {

//! DMA buffer abstract class
/*!
 * This concept can be extended/subclassed to deal which DMA regions, i.e.,
 * individual portions of the buffer, for cards which can deal such fine
 * granularity as radeon.
 */
DMABuffer {
	protected:
		friend class DMAEngine;
		unsigned size;
	
	public:
		//! Flush the DMA buffer
		virtual void flush() = 0;

		//! Discard the DMA buffer
		virtual void discard() = 0;
		
} ;
	
//! Generic DMA engine
/*!
 * This is the minimum required stuff for the basic DMA operations, i.e., get
 * and dispatch vertex DMA buffers. This will be used by throughout the generic DMA driver.
 * 
 * Drivers can and should extend this to do more stuff. E.g., one subclass should be extended
 * to expose and use Ian Romanick's texmem-2 stuff when it's available.
 */
class DMAEngine {
	public:
		
		DMABuffer * get_buffer();
} ; 

}
