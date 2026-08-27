#include <cstdlib>
#include "../include/bery_alloc.h"
#include "../include/bery_gc.h"
#include "../include/bery_runtime.h"
#include "../include/bery_type_registry.h"



void* bery_alloc(size_t size, uint32_t typeId) {
    if (bery_gc_should_collect()) {
        bery_gc_collect();
    }
    size_t totalSize = sizeof(BeryObjectHeader) + size;
    BeryObjectHeader* header  = static_cast<BeryObjectHeader*>(malloc(totalSize));
    
    header->marked = 0;
    header->typeId = typeId;
    header->size   = size;
    header->next = g_beryRuntime.heapHead;
    g_beryRuntime.heapHead = header;

    g_beryRuntime.totalAllocated += size;
    g_beryRuntime.allocationCount += 1;
    g_beryRuntime.totalObjectsLive += 1;

    return reinterpret_cast<char*>(header) + sizeof(BeryObjectHeader);
}

void bery_object_destroy(void* payload) {
    BeryObjectHeader* header = bery_header_from_payload(payload);

    BeryObjectHeader** current = &g_beryRuntime.heapHead;
    while (*current) {
        if (*current == header) {
            *current = header->next;
            break;
        }
        current = &(*current)->next;
    }

    BeryTypeInfo* type = bery_type_lookup(header->typeId);
    
    if (type && type->destructor) {
        type->destructor(payload);
    }

    g_beryRuntime.totalAllocated -= header->size;
    g_beryRuntime.totalObjectsLive -= 1;
    free(header);
}

BeryObjectHeader* bery_header_from_payload(void* payload) {
    return reinterpret_cast<BeryObjectHeader*>(reinterpret_cast<char*>(payload) - sizeof(BeryObjectHeader));
}