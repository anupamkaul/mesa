/*!
 * \file generic_tris.hxx
 * \brief Templates for vertex buffer layout.
 *
 * \todo (Any/every)thing!
 *
 * \sa *_tris.c in the current DRI drivers.
 */
namespace Generic {

//! Vertex sink
/*!
 * This is the final 
 *
 * This class is responsible for laying out the vertices in the DMA buffer and dispatch it when full.
 * 
 * It should also be smart enough to handle:
 * - primitiv
 */
class VertexSink {
	public:
		
		/*
		 * \param dmaEngine DMA engine to which the vertex buffers will be sent.
		 */
		VertexSink();
			
		//! Forget all previous vertices.
		reset();

		//! Restart 
		/*!
		 * Used when the current DMA buffer ended, and we need to
		 * resend the last few vertices to resume the current primtive.
		 */
		resume();

		//! Change the primitive
		virtual void set_primitive();

		//! Feed a vertex of the current primitive
		virtual void feed_vertex(void *V);

		//! Shortand to send a triangle
		virtual void triangle(void *v1, void *v2, void *v3);
} ;
