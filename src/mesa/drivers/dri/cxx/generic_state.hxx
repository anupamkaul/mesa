
namespace Generic {

//! A minimal discrete ammount of state
/*!
 * This could be the value of a hardware register or a collection of hardware
 * registers, depending of the grain you want to control. The appropriate 
 * granularity is not always evident, but usually it takes more time to send
 * somthing through the bus than the CPU cycles necessary for these checks.
 */
class StateAtom {
	public:
		//! Mark as dirty
		virtual void dirty(void) = 0;

		//! Determines whether is on the current context
		/*!
		 * The context pointer is not passed on this call and should be
		 * passed on the initalization.
		 */
		virtual bool isactive(void) const = 0;

		//! Is it dirty on the current context?
		/*!
		 * \note This method probably isn't necessary, and its removal
		 * would simplify some implementation details.
		 */
		virtual bool isdirty(void) const = 0;

		//! Upload
		virtual void upload(void) = 0;
} ;

//! State container
/*!
 * Used by the drivers to manage state, i.e., decide what, when and how to upload
 * it.
 */
class State {
	
	public:
		//! Mark all state as dirty
		virtual void dirty(void) = 0;

		//! Is any active state dirty?
		virtual bool isdirty(void) const = 0;

		//! Upload dirty active state
		virtual void upload(void) = 0;
} ;

typedef unsigned long BitMask;

/*! \sa BitMaskState */
class BitMaskStateAtom : StateAtom {
	private:
		// The friend is completely unecessary - just too lazy to write
		// good replacement data access methods.
		friend 	template< int N > class BitMaskState;
	
		//! Pointer to template< int N > BitMaskState::dirty_mask;
		BitMask *pDirty;

		//! Dirty mask flag value
		BitMask mask;
		
	public:
		BitMaskStateAtom() {
			pDirty = NULL;
			mask = 0;
		}
		
		virtual dirty(void) {
			assert(pDirty);

			*pDirty &= mask;
		}
		
		virtual bool isdirty(void) const {
			assert(pDirty);
			
			return *pDirty & mask;
		}
} ;

/*!
 * Assigns to each state a bit in a dirty bitmask. It's limited to 32/64 state
 * atoms.
 *
 * In future there will be a template version of the BitMaskState and
 * BitMaskStateAtoms classes, where the bit masks (known at compile time) will
 * be the parameters. Therefore achieving the same performance as in the current
 * drivers which using explicit "if-then" constructs for each state.
 */
class BitMaskState : State {
	private:
		unsigned n;
		BitMask dirty_mask;
		BitMaskState *states;
	
	public:
		BitMaskState(_n, BitMaskState *_states[]) {
			assert(_n <= sizeof(BitMask*8));
			
			n = _n;
			dirty_mask = 0;
			states = _states;

			for(unsigned i = 0; i < n; ++i) {
				state[i]->pDirty = &dirty_mask;
				state[i]->mask = 1 << i;
			}
		}

		virtual void dirty(void) {
			dirty_mask = ~0;
		}
	
		virtual bool isdirty(void) {
			return dirty_mask;
		}
	
		virtual void upload(void) {
			unsigned i;
			
			for(unsigned i = 0; i < n; ++i)
				if(state[i]->active() && state[i]->pDirty & 1 << i)
						state[i]->upload();
		}
} ;

/*!
 * \sa Based on Keith Whitwell's radeon_state_atom of the Radeon driver.
 *
 * Each state atom is part of a doubly-linked list.
 */
class LinkedListStateAtom : StateAtom {
	private:
		friend class LinkedListState;
			
		LinkedListStateAtom *prev, *next;

	public:
	
} ;

//! Linked-list state container
/*!
 * State atoms are hold either in a clean or dirty doubly-linked list.
 * Therefore, it's not limited to 32 state atoms and allows to quickly
 * determine the dirty state.
 */
class LinkedListState : State {
	private:
		LinkedListStateAtom *clean;
		LinkedListStateAtom *dirty;
	
	public:
		/* ... */
} ;


}
