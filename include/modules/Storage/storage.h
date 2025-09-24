// Simple storage wrapper for database operations.
#pragma once

namespace Storage {

class StorageManager {
public:
    StorageManager();
    ~StorageManager();

    bool initialize();
    void shutdown();
};

} // namespace Storage
