#include "dri.hxx"
#include "mesa.hxx"

//! Generic Mesa DRI driver.
/*!
 * This will the bits to build a generic Mesa based DRI driver.
 *
 * \todo Things to do here:
 * - texture management abstraction (have to study with more detail Ian Romanick's texmem-[1-2])
 * - vertex-buffer construction templates 
 * - primitives emition
 * 
 */
namespace Generic {

class Context : DRI::Context, Mesa::Context {
} ;

class Driver : DRI::Driver {
} ;

}
