#ifndef __TUNABLES_H__
#define __TUNABLES_H__

#include "tunables-common.h"

#if AVD_VER == 2
#include "tunables-v2.h"
#elif AVD_VER == 3
#include "tunables-v3.h"
#elif AVD_VER == 4
#include "tunables-v4.h"
#elif AVD_VER == 5
#include "tunables-v5.h"
#else
const struct tunable avd_tunables[] = {
    { },
};
#endif

#endif /* __TUNABLES__H__ */
