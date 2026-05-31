#ifndef RINHA_SHM_H
#define RINHA_SHM_H

#include "types.h"

ref_store_t *shm_create(void);
ref_store_t *shm_attach(void);
void shm_detach(ref_store_t *store);

#endif
