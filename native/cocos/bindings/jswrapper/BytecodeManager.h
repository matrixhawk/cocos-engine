#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

#include <stdint.h>

namespace cc {
    class LegacyThreadPool;
}

namespace se {

    class BytecodeManager final
    {
    public:
        static BytecodeManager* create();

        void addRef();
        void release();

        struct V8CachedData final
        {
            V8CachedData(const std::string& pathMD5);
            ~V8CachedData();

            // fastSet takes ownership of 'data'.
            void fastSet(const void* data, int length);
            void copy(const void* data, int length);

            inline const std::string& getPathMD5() const { return _pathMD5; }
            inline const void* getData() const { return _data; }
            inline size_t getLength() const { return _length; }

        private:
            std::string _pathMD5;

            void* _data = nullptr;
            size_t _length = 0;
        };

        using WriteFileAsyncCallback = std::function<void(bool)>;

        bool init(const std::string& v8CachedDir, const std::string& configFileName, const std::string& vmVersion, bool isAsyncWriteBytecode);
        void destroy();

        V8CachedData* readCachedData(const std::string& scriptFullPath, const char* scriptBuffer, size_t scriptBufferLength, std::string* outPathMD5, std::string* outScriptContentMD5);
        void releaseCachedData(V8CachedData** cachedData);

        bool saveCachedData(const std::string& pathMD5, std::string& scriptContentMD5, const void* bytecode, size_t bytecodeSize, const void* script, size_t scriptLength);
        void saveCachedDataAsync(const std::string& pathMD5, std::string& scriptContentMD5, const void* bytecode, size_t bytecodeSize, const void* script, size_t scriptLength, const WriteFileAsyncCallback& cb);

    private:
        BytecodeManager();
        ~BytecodeManager();

        bool saveCachedData(V8CachedData* cachedData);
        void saveCachedDataAsync(V8CachedData* cachedData, const WriteFileAsyncCallback& cb);

        bool getV8CacheConfig(const std::string& pathMD5, std::string* outFileContentMD5);
        bool setV8CacheConfig(const std::string& pathMD5, const std::string& fileContentMD5);

        void saveV8CacheConfig();

        bool writeFile(const std::string& path, const void* buffer, size_t bufferByteLength);
        void writeFileAsync(const std::string& path, const void* buffer, size_t bufferByteLength, const BytecodeManager::WriteFileAsyncCallback& cb, bool copyBuffer);

        std::string _cachedDir;
        std::string _configFileName;
        std::string _vmVersion;

        std::unordered_map<std::string, std::string> _cacheConfigMap;
        std::mutex _codeCacheMutex;

        std::mutex _refCountMutex;
        volatile int _refCount = 1;
        bool _needSaveCachedData = false;
        bool _asyncWriteBytecode = true;
    };

} // namespace se {
