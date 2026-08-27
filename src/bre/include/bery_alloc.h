#pragma once
#include "bery_object.h"
#include <cstddef>

extern "C" {
    void* bery_alloc(size_t size, uint32_t typeId);
    void bery_object_destroy(void* payload);
}

BeryObjectHeader* bery_header_from_payload(void* payload);