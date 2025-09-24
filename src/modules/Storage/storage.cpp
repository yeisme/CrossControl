#include "storage.h"

#if defined(TINYORM_FOUND) || defined(TARGET_TinyOrm) || defined(TinyORM_FOUND)
// If TinyORM is available, include its headers (the exact macro may differ by
// how it's provided; we rely on CMake to add include paths when linking)
#include <TinyORM/TinyORM.h>
#define HAVE_TINYORM 1
#else
#define HAVE_TINYORM 0
#endif

namespace Storage {

StorageManager::StorageManager() {}
StorageManager::~StorageManager() {}

bool StorageManager::initialize() {
#if HAVE_TINYORM
    // TODO: initialize TinyORM connection(s) here. Placeholder for now.
    return true;
#else
    // TinyORM not available: nothing to initialize.
    return false;
#endif
}

void StorageManager::shutdown() {
#if HAVE_TINYORM
    // TODO: shutdown/cleanup TinyORM resources if needed
#endif
}

} // namespace Storage
