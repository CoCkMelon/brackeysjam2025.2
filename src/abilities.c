#include "abilities.h"
#include <string.h>

AbilityState g_abilities; // zero-initialized

void abilities_init(void){ abilities_reset(); }
void abilities_reset(void){ memset(&g_abilities, 0, sizeof(g_abilities)); }

