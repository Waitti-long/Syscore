#include "Heap.h"
#include "memory.h"

HeapAllocator heap_allocator;

void init_heap(){
    size_t start = alloc_page(KERNEL_HEAP_PAGE_NUM * 4096);
    heap_allocator.SetStart(start);
    heap_allocator.SetEnd(start + KERNEL_HEAP_PAGE_NUM * 4096);
    heap_allocator.Init();
}

void* k_malloc(size_t size){
    return (void*)heap_allocator.Alloc(size);
}

void k_free(size_t p){
    if(p != 0){
        heap_allocator.DeAlloc(p);
    }
}

void *operator new (size_t size){
    return (void*)heap_allocator.Alloc(size);
}

void *operator new[](size_t size){
    return (void*)heap_allocator.Alloc(size);
}

void operator delete(void *p){
    if(p != nullptr){
        heap_allocator.DeAlloc((size_t) p);
    }
}

void operator delete[](void *p){
    if(p != nullptr){
        heap_allocator.DeAlloc((size_t) p);
    }
}