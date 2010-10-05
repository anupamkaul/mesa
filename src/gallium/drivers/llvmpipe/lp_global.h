

#ifndef LP_GLOBAL_H
#define LP_GLOBAL_H


#include "lp_state_fs.h"



struct llvmpipe_globals
{
   struct lp_fs_variant_list_item fs_variants_list;
   unsigned nr_fs_variants;
};


extern struct llvmpipe_globals llvmpipe_global;


extern void
llvmpipe_init_globals(void);



#endif /* LP_GLOBAL_H */
