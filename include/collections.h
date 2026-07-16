#ifndef CNEXT_COLLECTIONS_H
#define CNEXT_COLLECTIONS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define COLL_MAX 64

typedef struct { int* data; int cap; int len; } List;
typedef struct { int key; int val; int used; } MapEntry;
typedef struct { MapEntry* entries; int cap; int len; } Map;
typedef struct { int* data; int cap; int len; } Set;

// String-keyed HashMap
typedef struct HashMapEntry {
    char* key;
    void* value;
    struct HashMapEntry* next;
} HashMapEntry;

typedef struct {
    HashMapEntry** buckets;
    int capacity;
    int size;
} HashMap;

static HashMap* hashmap_new(int capacity) {
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    if (!map) return NULL;
    map->capacity = capacity > 0 ? capacity : 16;
    map->size = 0;
    map->buckets = (HashMapEntry**)calloc(map->capacity, sizeof(HashMapEntry*));
    if (!map->buckets) { free(map); return NULL; }
    return map;
}

static unsigned int hashmap_hash(const char* key, int capacity) {
    unsigned int hash = 5381;
    while (*key) {
        hash = ((hash << 5) + hash) + (unsigned char)*key++;
    }
    return hash % capacity;
}

static void hashmap_resize(HashMap* map) {
    int new_cap = map->capacity * 2;
    HashMapEntry** new_buckets = (HashMapEntry**)calloc(new_cap, sizeof(HashMapEntry*));
    if (!new_buckets) return;
    for (int i = 0; i < map->capacity; i++) {
        HashMapEntry* e = map->buckets[i];
        while (e) {
            HashMapEntry* next = e->next;
            unsigned int idx = hashmap_hash(e->key, new_cap);
            e->next = new_buckets[idx];
            new_buckets[idx] = e;
            e = next;
        }
    }
    free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_cap;
}

static void hashmap_put(HashMap* map, const char* key, void* value) {
    if (!map || !key) return;
    if (map->size >= map->capacity / 2) hashmap_resize(map);
    unsigned int idx = hashmap_hash(key, map->capacity);
    HashMapEntry* entry = map->buckets[idx];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    entry = (HashMapEntry*)malloc(sizeof(HashMapEntry));
    if (!entry) return;
    entry->key = strdup(key);
    if (!entry->key) { free(entry); return; }
    entry->value = value;
    entry->next = map->buckets[idx];
    map->buckets[idx] = entry;
    map->size++;
}

static void* hashmap_get(HashMap* map, const char* key) {
    if (!map || !key) return NULL;
    unsigned int idx = hashmap_hash(key, map->capacity);
    HashMapEntry* entry = map->buckets[idx];
    while (entry) {
        if (strcmp(entry->key, key) == 0) return entry->value;
        entry = entry->next;
    }
    return NULL;
}

static bool hashmap_contains(HashMap* map, const char* key) {
    return hashmap_get(map, key) != NULL;
}

static void hashmap_remove(HashMap* map, const char* key) {
    if (!map || !key) return;
    unsigned int idx = hashmap_hash(key, map->capacity);
    HashMapEntry** prev = &map->buckets[idx];
    HashMapEntry* entry = *prev;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            *prev = entry->next;
            free(entry->key);
            free(entry);
            map->size--;
            return;
        }
        prev = &entry->next;
        entry = entry->next;
    }
}

static void hashmap_free(HashMap* map) {
    if (!map) return;
    for (int i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(map->buckets);
    free(map);
}

static List* _lists[COLL_MAX]; static int _lc = 0;
static Map* _maps[COLL_MAX]; static int _mc = 0;
static Set* _sets[COLL_MAX]; static int _sc = 0;

static int list_new(void) {
    if (_lc >= COLL_MAX) return -1;
    List* l = (List*)malloc(sizeof(List));
    if (!l) return -1;
    l->cap = 4; l->len = 0;
    l->data = (int*)malloc(l->cap * sizeof(int));
    if (!l->data) { free(l); return -1; }
    _lists[_lc] = l;
    return _lc++;
}

static void list_free(int h) {
    if (h < 0 || h >= _lc || !_lists[h]) return;
    free(_lists[h]->data);
    free(_lists[h]);
    _lists[h] = NULL;
}

static int list_add(int h, int val) {
    if (h < 0 || h >= _lc || !_lists[h]) return -1;
    List* l = _lists[h];
    if (l->len >= l->cap) {
        int new_cap = l->cap * 2;
        if (new_cap < l->cap) return -1; // overflow check
        l->cap = new_cap;
        int* nd = (int*)realloc(l->data, l->cap * sizeof(int));
        if (!nd) return -1;
        l->data = nd;
    }
    l->data[l->len++] = val;
    return l->len - 1;
}

static int list_get(int h, int idx) {
    if (h < 0 || h >= _lc || !_lists[h]) return 0;
    List* l = _lists[h];
    if (idx < 0 || idx >= l->len) return 0;
    return l->data[idx];
}

static void list_set(int h, int idx, int val) {
    if (h < 0 || h >= _lc || !_lists[h]) return;
    List* l = _lists[h];
    if (idx >= 0 && idx < l->len) l->data[idx] = val;
}

static int list_size(int h) {
    if (h < 0 || h >= _lc || !_lists[h]) return 0;
    return _lists[h]->len;
}

static void list_remove(int h, int idx) {
    if (h < 0 || h >= _lc || !_lists[h]) return;
    List* l = _lists[h];
    if (idx < 0 || idx >= l->len) return;
    memmove(l->data + idx, l->data + idx + 1, (l->len - idx - 1) * sizeof(int));
    l->len--;
}

static void list_clear(int h) {
    if (h < 0 || h >= _lc || !_lists[h]) return;
    _lists[h]->len = 0;
}

static int list_contains(int h, int val) {
    if (h < 0 || h >= _lc || !_lists[h]) return 0;
    List* l = _lists[h];
    for (int i = 0; i < l->len; i++) {
        if (l->data[i] == val) return 1;
    }
    return 0;
}

static int intcmp(const void* a, const void* b) {
    int va = *(const int*)a;
    int vb = *(const int*)b;
    return (va > vb) - (va < vb);
}

static void list_sort(int h) {
    if (h < 0 || h >= _lc || !_lists[h]) return;
    List* l = _lists[h];
    if (l->len > 1) qsort(l->data, l->len, sizeof(int), intcmp);
}

static int map_new(void) {
    if (_mc >= COLL_MAX) return -1;
    Map* m = (Map*)malloc(sizeof(Map));
    if (!m) return -1;
    m->cap = 16; m->len = 0;
    m->entries = (MapEntry*)calloc(m->cap, sizeof(MapEntry));
    if (!m->entries) { free(m); return -1; }
    _maps[_mc] = m;
    return _mc++;
}

static void map_free(int h) {
    if (h < 0 || h >= _mc || !_maps[h]) return;
    free(_maps[h]->entries);
    free(_maps[h]);
    _maps[h] = NULL;
}

static unsigned int hash_int(int key) {
    return (unsigned int)(key * 2654435761U);
}

static void map_set(int h, int key, int val) {
    if (h < 0 || h >= _mc || !_maps[h]) return;
    Map* m = _maps[h];
    if (m->len * 2 >= m->cap) {
        int oldcap = m->cap;
        int newcap = oldcap * 2;
        if (newcap < oldcap) return; // overflow check
        m->cap = newcap;
        MapEntry* old = m->entries;
        m->entries = (MapEntry*)calloc(m->cap, sizeof(MapEntry));
        if (!m->entries) { m->entries = old; m->cap = oldcap; return; }
        m->len = 0;
        for (int i = 0; i < oldcap; i++) {
            if (old[i].used) {
                // Direct insert to avoid recursive rehash
                unsigned int idx = hash_int(old[i].key) % (unsigned int)m->cap;
                while (m->entries[idx].used) {
                    if (m->entries[idx].key == old[i].key) { m->entries[idx].val = old[i].val; goto next_entry; }
                    idx = (idx + 1) % (unsigned int)m->cap;
                }
                m->entries[idx].used = 1;
                m->entries[idx].key = old[i].key;
                m->entries[idx].val = old[i].val;
                m->len++;
                next_entry:;
            }
        }
        free(old);
    }
    unsigned int idx = hash_int(key) % (unsigned int)m->cap;
    while (m->entries[idx].used) {
        if (m->entries[idx].key == key) { m->entries[idx].val = val; return; }
        idx = (idx + 1) % (unsigned int)m->cap;
    }
    m->entries[idx].used = 1;
    m->entries[idx].key = key;
    m->entries[idx].val = val;
    m->len++;
}

static int map_get(int h, int key) {
    if (h < 0 || h >= _mc || !_maps[h]) return 0;
    Map* m = _maps[h];
    unsigned int idx = hash_int(key) % (unsigned int)m->cap;
    while (m->entries[idx].used) {
        if (m->entries[idx].key == key) return m->entries[idx].val;
        idx = (idx + 1) % (unsigned int)m->cap;
    }
    return 0;
}

static int map_has(int h, int key) {
    if (h < 0 || h >= _mc || !_maps[h]) return 0;
    Map* m = _maps[h];
    unsigned int idx = hash_int(key) % (unsigned int)m->cap;
    while (m->entries[idx].used) {
        if (m->entries[idx].key == key) return 1;
        idx = (idx + 1) % (unsigned int)m->cap;
    }
    return 0;
}

static void map_remove(int h, int key) {
    if (h < 0 || h >= _mc || !_maps[h]) return;
    Map* m = _maps[h];
    unsigned int idx = hash_int(key) % (unsigned int)m->cap;
    while (m->entries[idx].used) {
        if (m->entries[idx].key == key) {
            m->entries[idx].used = 0;
            m->len--;
            unsigned int j = idx;
            while (1) {
                j = (j + 1) % (unsigned int)m->cap;
                if (!m->entries[j].used) break;
                unsigned int k = hash_int(m->entries[j].key) % (unsigned int)m->cap;
                if ((j > idx && (k <= idx || k > j)) || (j < idx && k <= idx && k > j)) {
                    m->entries[idx] = m->entries[j];
                    m->entries[j].used = 0;
                    idx = j;
                }
            }
            return;
        }
        idx = (idx + 1) % (unsigned int)m->cap;
    }
}

static int map_size(int h) {
    if (h < 0 || h >= _mc || !_maps[h]) return 0;
    return _maps[h]->len;
}

static int set_new(void) {
    if (_sc >= COLL_MAX) return -1;
    Set* s = (Set*)malloc(sizeof(Set));
    if (!s) return -1;
    s->cap = 4; s->len = 0;
    s->data = (int*)malloc(s->cap * sizeof(int));
    if (!s->data) { free(s); return -1; }
    _sets[_sc] = s;
    return _sc++;
}

static void set_free(int h) {
    if (h < 0 || h >= _sc || !_sets[h]) return;
    free(_sets[h]->data);
    free(_sets[h]);
    _sets[h] = NULL;
}

static int set_has(int h, int val) {
    if (h < 0 || h >= _sc || !_sets[h]) return 0;
    Set* s = _sets[h];
    for (int i = 0; i < s->len; i++) {
        if (s->data[i] == val) return 1;
    }
    return 0;
}

static void set_add(int h, int val) {
    if (h < 0 || h >= _sc || !_sets[h]) return;
    if (set_has(h, val)) return;
    Set* s = _sets[h];
    if (s->len >= s->cap) {
        int new_cap = s->cap * 2;
        if (new_cap < s->cap) return; // overflow check
        s->cap = new_cap;
        int* nd = (int*)realloc(s->data, s->cap * sizeof(int));
        if (!nd) return;
        s->data = nd;
    }
    s->data[s->len++] = val;
}

static void set_remove(int h, int val) {
    if (h < 0 || h >= _sc || !_sets[h]) return;
    Set* s = _sets[h];
    for (int i = 0; i < s->len; i++) {
        if (s->data[i] == val) {
            s->data[i] = s->data[--s->len];
            return;
        }
    }
}

static int set_size(int h) {
    if (h < 0 || h >= _sc || !_sets[h]) return 0;
    return _sets[h]->len;
}

#endif
