/**** BSIM4.2.1, Released by Xuemei Xi 10/05/2001 ****/

/**********
 * Copyright 2001 Regents of the University of California. All rights reserved.
 * File: b4dest.c of BSIM4.2.1.
 * Author: 2000 Weidong Liu
 * Authors: Xuemei Xi, Kanyu M. Cao, Hui Wan, Mansun Chan, Chenming Hu.
 * Project Director: Prof. Chenming Hu.
 **********/

#include "spice.h"
#include <stdio.h>
#include "util.h"
#include "bsim4def.h"
#include "suffix.h"

void
BSIM4destroy(inModel)
GENmodel **inModel;
{
BSIM4model **model = (BSIM4model**)inModel;
BSIM4instance *here;
BSIM4instance *prev = NULL;
BSIM4model *mod = *model;
BSIM4model *oldmod = NULL;

    for (; mod ; mod = mod->BSIM4nextModel)
    {    if(oldmod) FREE(oldmod);
         oldmod = mod;
         prev = (BSIM4instance *)NULL;
         for (here = mod->BSIM4instances; here; here = here->BSIM4nextInstance)
	 {    if(prev) FREE(prev);
              prev = here;
         }
         if(prev) FREE(prev);
    }
    if(oldmod) FREE(oldmod);
    *model = NULL;
    return;
}