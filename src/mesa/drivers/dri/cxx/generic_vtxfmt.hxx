
namespace Generic {

//! Abstract DMA vertex format
/*!
 *
 * \note This class doesn't actually represent vertices, but only operations on
 * vertices. The reason to not model the vertices as C++ classes is that we
 * would loose control over the actual memory layout.
 * 
 * The idea is that this class is subclassed (preferrably with member function
 * defined inline) by the drivers for each vertex format supported by the card,
 * so that they can be used in clipping and 
 *
 * They can be extended to do more optimized stuff such as vertex
 * transformations (e.g., using gcc SIMD instrcutions) and used more early in
 * the pipeline.
 *
 * \todo Implement a D3DVertexFormat for the D3D-like vertex formats used in
 * most cards.
 */
class VertexFormat {

	public:
		virtual ~VertexFormat() {
		}

		//! Vertex size
		virtual unsigned size(void) const = 0;

		//! Allocate a vertice
		void *alloc(void) {
			return std::malloc(size());
		}

		//! Free a vertice
		void free(void *v) {
			std::free(v);
		}
			
		//! Copy two vertices
		void copy(void *dst, const void *src) {
			memcpy(src, dst, size());
		}

		//! Copy a vertice into a DMA stream
		/*!
		 * This will be used by the primitive drawing functions to copy
		 * the vertices to their final destinations in the DMA vertex
		 * buffer. This function exists because quite often there is
		 * necessity of more info (such as DMA commands, or the vertex
		 * number) in the DMA stream which is usually not included in
		 * the vertex.
		 * 
		 * \param dst destination pointer into to the DMA stream.
		 * \param src source pointer to the vertex.
		 * \param seq vertex sequence number, i.e., 0, 1, 2 for a
		 * triangle, 0, 1, 2, 3 for a quad, if supported.
		 * 
		 * 
		 */
		virtual void copy_dma(void *dst, const void *src, unsigned seq) = 0;
		
				
		//! Interpolate two vertices
		void interpolate(float t, const void *p, const void *q, void *r) = 0;

		//! Get vertex coordinates
		virtual void get_coords(const void *v, float &x, float &y, float &z, float &w) = 0;

		//! Set vertex coordiantes 
		virtual void set_coords(void *v, float x, float y, float z, float w) = 0;

		//! Has color componens?
		virtual bool has_color(void) const = 0;

		//! Get color components
		virtual vois get_color(const *v, unsigned char r, unsigned char g, unsigned char b, unsigned char a) = 0;
		
		// Fill in the rest ...
} ;

//! Emit the vertices
/*!
 * A C++ template version of t_dd_vbtmp.h
 *
 * \todo Implement it.
 */
template< class V , bool hwdivide, bool viewport >
emit(GLcontext *ctx, GLuint start, GLuint end, void *dest, GLuint stride);

}
