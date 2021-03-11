#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "types.h"

hashtable *hashtable_new(void);
void hashtable_free(hashtable *obj);

void hashtable_put(hashtable *ht, char *key, char *val);
char *hashtable_get(hashtable const *ht, char const *key);
void hashtable_erase(hashtable *ht, char const *key);

#endif
