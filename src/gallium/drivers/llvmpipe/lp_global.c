

#include "util/u_simple_list.h"
#include "lp_global.h"


struct llvmpipe_globals llvmpipe_global;


void
llvmpipe_init_globals(void)
{
   static boolean initialized = FALSE;
   if (!initialized) {
      //make_empty_list(&llvmpipe_global.shaders);
      make_empty_list(&llvmpipe_global.fs_variants_list);

      llvmpipe_global.nr_fs_variants = 0;

      initialized = TRUE;
   }
}

