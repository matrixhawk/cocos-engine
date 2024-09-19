#include "BytecodeManager.h"
#include "config.h"
//#include "utils/SimpleAsyncTaskManager.h"

#include <sstream>
#include <stdlib.h>
#include "base/md5.h"
#include "base/ThreadPool.h"
#include "platform/FileUtils.h"

namespace se {

namespace {

    cc::LegacyThreadPool *gThreadPool{nullptr};

    std::mutex __fileMutex;

    std::string getDataMD5Hash(const void* buf, size_t len)
    {
        static const unsigned int MD5_DIGEST_LENGTH = 16;

        md5_state_t state;
        md5_byte_t digest[MD5_DIGEST_LENGTH];
        char hexOutput[(MD5_DIGEST_LENGTH << 1) + 1] = { 0 };

        md5_init(&state);
        md5_append(&state, (const md5_byte_t *)buf, (int)len);
        md5_finish(&state, digest);

        for (int di = 0; di < 16; ++di)
            sprintf(hexOutput + di * 2, "%02x", digest[di]);

        return std::string(hexOutput);
    }

} // namespace {

BytecodeManager::V8CachedData::V8CachedData(const std::string& pathMD5)
: _pathMD5(pathMD5)
{
    CC_LOG_DEBUG("[bm] V8CachedData(%p)\n", this);
}

BytecodeManager::V8CachedData::~V8CachedData()
{
    CC_LOG_DEBUG("[bm] ~V8CachedData(%p), _data: %p\n", this, _data);
    ::free(_data);
}

BytecodeManager* BytecodeManager::create()
{
    return new BytecodeManager();
}

BytecodeManager::BytecodeManager()
{
    CC_LOG_DEBUG("[bm] BytecodeManager(%p)\n", this);
    if (gThreadPool == nullptr) {
        gThreadPool = cc::LegacyThreadPool::newSingleThreadPool();
    }
}

BytecodeManager::~BytecodeManager()
{
    CC_LOG_DEBUG("[bm] ~BytecodeManager(%p)\n", this);
}

void BytecodeManager::addRef()
{
    std::unique_lock<std::mutex> lg(_refCountMutex);
    ++_refCount;
    CC_LOG_DEBUG("[bm] addRef: ref: %d\n", _refCount);
}

void BytecodeManager::release()
{
    std::unique_lock<std::mutex> ug(_refCountMutex);
    int refCount = --_refCount;
    CC_LOG_DEBUG("[bm] release: ref: %d\n", refCount);
    if (refCount == 0)
    {
        ug.unlock();
        delete this;
    }
}

// fastSet takes ownership of 'data'.
void BytecodeManager::V8CachedData::fastSet(const void* data, int length)
{
    if (_data != nullptr)
    {
        ::free(_data);
    }

    _data = (uint8_t*)data;
    _length = length;
}

void BytecodeManager::V8CachedData::copy(const void* data, int length)
{
    CC_ASSERT(length > 0);
    if (_data)
    {
        ::free(_data);
    }

    _data = (uint8_t*)::malloc(length);
    _length = length;
    memcpy(_data, data, _length);
}

bool BytecodeManager::init(const std::string& v8CachedDir, const std::string& configFileName, const std::string& vmVersion, bool isAsyncWriteBytecode)
{
    CC_ASSERT(!v8CachedDir.empty());
    CC_ASSERT(!configFileName.empty());
    CC_ASSERT(!vmVersion.empty());

    CC_LOG_INFO("[bm] init\n");

    std::lock(__fileMutex, _codeCacheMutex);
    std::lock_guard<std::mutex> lg(__fileMutex, std::adopt_lock);
    std::lock_guard<std::mutex> lk_(_codeCacheMutex, std::adopt_lock);

    bool ret = false;
    _cacheConfigMap.clear();

    _cachedDir = v8CachedDir;
    _configFileName = configFileName;
    _vmVersion = vmVersion;
    _asyncWriteBytecode = isAsyncWriteBytecode;

    CC_LOG_INFO("[bm] asyncWriteBytecode: %d\n", (_asyncWriteBytecode ? 1 : 0));
    auto *fileUtils = cc::FileUtils::getInstance();
    if (!fileUtils->isDirectoryExist(v8CachedDir)) {
        if (!fileUtils->createDirectory(v8CachedDir)) {
            return false;
        }
    }

    std::string cacheConfigFilePath = _cachedDir + _configFileName;
    FILE* fp = fopen(cacheConfigFilePath.c_str(), "rb");
    if (fp == nullptr)
    {
        return false;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    do
    {
        if (len <= 0)
            break;

        char* buf = (char*)::malloc(len + 1);
        if (buf == nullptr)
            break;

        buf[len] = '\0';
        size_t readCount = fread(buf, len, 1, fp);
        if (readCount != 1)
        {
            ::free(buf);
            break;
        }

        char* lineStart = buf;
        char* p = buf;
        char* q, *r;

        int lineIndex = 0;
        int index = 0;

        std::string keyStr;
        std::string valueStr;

        do
        {
            // find a line, 'p' points to line end
            if (*p == '\n' || *p == '\0')
            {
                ++lineIndex;
                index = 0;
                r = lineStart;
                if (lineStart != p)
                {
                    for (q = lineStart; q <= p; ++q)
                    {
                        // remove redundant spaces or tabs
                        if (*r == ' ' || *r == '\t')
                        {
                            ++r;
                            continue;
                        }

                        // 'q' points to the end of a section.
                        if (*q == ' ' || *q == '\n' || *q == '\0')
                        {
                            std::string tmp(r, q-r);
                            if (index == 0)
                            {
                                keyStr = std::move(tmp);
                            }
                            else if (index == 1)
                            {
                                valueStr = std::move(tmp);
                            }

                            ++index;
                            r = q + 1;
                        }
                    }

                    CC_ASSERT(index == 2);

                    if (index != 2)
                    {
                        break;
                    }

                    if (lineIndex == 1) // First line
                    {
                        const std::string& v8VersionKey = keyStr;
                        const std::string& v8VersionValue = valueStr;

                        if (v8VersionKey == "v8_version")
                        {
                            // If v8 is upgraded, drop old cache.
                            const std::string& currentV8Version = vmVersion;
                            if (v8VersionValue != currentV8Version)
                            {
                                CC_LOG_INFO("[bm] Found different V8 version: old: %s, new: %s\n", v8VersionValue.c_str(), currentV8Version.c_str());
                                break;
                            }
                        }
                        else
                        {
                            SE_LOGE("[bm] Could not find v8 version key!\n");
                            break;
                        }
                    }
                    else if (lineIndex == 2) // The second line stores 64 bit or 32 bit
                    {
                        const std::string& archKey = keyStr;
                        const std::string& archValue = valueStr;
                        if (archKey == "arch")
                        {
                            const size_t longSize = sizeof(long);
                            if ((archValue == "64" && longSize == 8) || (archValue == "32" && longSize == 4))
                            {
                                CC_LOG_INFO("[bm] Found matched arch: %s", archValue.c_str());
                            }
                            else
                            {
                                CC_LOG_WARNING("[bm] Found different arch: %s, %d, ignore v8 cache", archValue.c_str(), (int)longSize);
                                break;
                            }
                        }
                        else
                        {
                            CC_LOG_INFO("[bm] Old config format, ignore v8 cache");
                            break;
                        }
                    }
                    else
                    {
                        // CC_LOG_INFO("%s %s\n", keyStr.c_str(), valueStr.c_str());
                        CC_ASSERT(_cacheConfigMap.find(keyStr) == _cacheConfigMap.end());
                        _cacheConfigMap.emplace(keyStr, valueStr);
                    }

                    lineStart = p + 1;
                }
            }

            if (*p == '\0')
            {
                ret = true;
                break;
            }
            ++p;
        }
        while (true);

        if (!ret)
        {
            _cacheConfigMap.clear();
        }

        ::free(buf);
    } while (false);

    fclose(fp);

    // If failed, re-create the cache directory
    if (!ret) {
        CC_LOG_INFO("[bm] BytecodeManager init failed, remove cache dir: %s", _cachedDir.c_str());
        fileUtils->removeDirectory(_cachedDir);
        fileUtils->createDirectory(_cachedDir);
    }

    return ret;
}

void BytecodeManager::destroy()
{
    CC_LOG_INFO("[bm] BytecodeManager::destroy, _needSaveCachedData: %d\n", (int)_needSaveCachedData);
    if (_needSaveCachedData)
    {
        saveV8CacheConfig();
    }
}

bool BytecodeManager::getV8CacheConfig(const std::string& pathMD5, std::string* outFileContentMD5)
{
    std::lock_guard<std::mutex> lk_(_codeCacheMutex);
    CC_ASSERT(!pathMD5.empty());
    CC_ASSERT(outFileContentMD5 != nullptr);

    auto iter = _cacheConfigMap.find(pathMD5);
    if (iter != _cacheConfigMap.end())
    {
        *outFileContentMD5 = iter->second;
        return true;
    }
    outFileContentMD5->clear();
    return false;
}

bool BytecodeManager::setV8CacheConfig(const std::string& pathMD5, const std::string& fileContentMD5)
{
    CC_LOG_INFO("[bm] setV8CacheConfig");
    std::lock_guard<std::mutex> lk_(_codeCacheMutex);
    CC_ASSERT(!pathMD5.empty());
    CC_ASSERT(!fileContentMD5.empty());

    auto iter = _cacheConfigMap.find(pathMD5);
    if (iter != _cacheConfigMap.end())
        _cacheConfigMap.erase(iter);

    _cacheConfigMap.emplace(pathMD5, fileContentMD5);
    return true;
}

void BytecodeManager::saveV8CacheConfig()
{
    if (_cachedDir.empty() || _vmVersion.empty())
        return;

    auto createConfigContent = [&]() -> std::string {
        std::ostringstream ss;

        ss << "v8_version" << ' ' << _vmVersion << '\n';

        std::string arch;
        if (sizeof(long) == 8)
        {
            arch = "64";
        }
        else if (sizeof(long) == 4)
        {
            arch = "32";
        }
        else
        {
            arch = "unknown";
        }

        ss << "arch" << ' ' << arch << '\n';

        for (const auto& e : _cacheConfigMap)
        {
            ss << e.first << ' ' << e.second << '\n';
        }

        return ss.str();
    };

    std::string cacheConfigFilePath = _cachedDir + _configFileName;
    if (_asyncWriteBytecode)
    {
        addRef();

        gThreadPool->pushTask([=](int tid){
            std::lock_guard<std::mutex> lk_(_codeCacheMutex);

            std::string result = createConfigContent();
            //    CC_LOG_INFO(">>>>> result: \n%s\n", result.c_str());
            bool succeed = writeFile(cacheConfigFilePath, result.c_str(), result.length());
            if (succeed) {
                CC_LOG_INFO("[bm] Save V8 Cache config succeed!\n");
            } else {
                SE_LOGE("[bm] Save V8 Cache config (%s) failed!\n", cacheConfigFilePath.c_str());
            }

            release();
        });
    }
    else
    {
        std::lock_guard<std::mutex> lk_(_codeCacheMutex);
        std::string result = createConfigContent();
        writeFile(cacheConfigFilePath, result.c_str(), result.length());
    }
}

BytecodeManager::V8CachedData* BytecodeManager::readCachedData(const std::string& scriptFullPath, const char* scriptBuffer, size_t scriptBufferLength, std::string* outPathMD5, std::string* outScriptContentMD5)
{
    CC_ASSERT(!_cachedDir.empty());
    CC_ASSERT(!scriptFullPath.empty());
    CC_ASSERT(scriptBuffer != nullptr);
    CC_ASSERT(scriptBufferLength > 0);
    CC_ASSERT(outPathMD5 != nullptr);
    CC_ASSERT(outScriptContentMD5 != nullptr);

    V8CachedData* cachedData = nullptr;

    *outPathMD5 = getDataMD5Hash(scriptFullPath.c_str(), scriptFullPath.length());
    const std::string& pathMD5 = *outPathMD5;

    std::string fileContentMD5;
    if (getV8CacheConfig(pathMD5, &fileContentMD5) && !fileContentMD5.empty())
    {
        *outScriptContentMD5 = getDataMD5Hash(scriptBuffer, scriptBufferLength);
        if (*outScriptContentMD5 == fileContentMD5)
        {
            std::lock(__fileMutex, _codeCacheMutex);
            std::lock_guard<std::mutex> lg(__fileMutex, std::adopt_lock);
            std::lock_guard<std::mutex> lk_(_codeCacheMutex, std::adopt_lock);

            std::string filePath = _cachedDir + pathMD5;

            FILE* fp = fopen(filePath.c_str(), "rb");
            if (fp != nullptr)
            {
                fseek(fp, 0, SEEK_END);
                long fileSize = ftell(fp);
                fseek(fp, 0, SEEK_SET);

                if (fileSize > 0)
                {
                    uint8_t* buf = (uint8_t*)::malloc(fileSize);
                    if (buf != nullptr)
                    {
                        size_t readCount = fread(buf, fileSize, 1, fp);
                        if (readCount == 1)
                        {
                            cachedData = new V8CachedData(pathMD5);
                            cachedData->fastSet(buf, (int) fileSize);
                        }
                        else
                        {
                            ::free(buf);
                        }
                    }
                }

                fclose(fp);
            }
            else
            {
                SE_LOGE("[bm] Could not find bytecode file (%s)\n", filePath.c_str());
            }
        }
        else
        {
            CC_LOG_INFO("[bm] JS file (%s) MD5 is changed, ignore v8 cache!\n", scriptFullPath.c_str());
        }
    }

    return cachedData;
}

void BytecodeManager::releaseCachedData(V8CachedData** cachedData)
{
    if (cachedData != nullptr && *cachedData != nullptr)
    {
        delete (*cachedData);
        *cachedData = nullptr;
    }
}

bool BytecodeManager::saveCachedData(const std::string& pathMD5, std::string& scriptContentMD5, const void* bytecode, size_t bytecodeSize, const void* script, size_t scriptLength)
{
    CC_ASSERT(!pathMD5.empty());
    CC_ASSERT(bytecode != nullptr);
    CC_ASSERT(bytecodeSize > 0);
    CC_ASSERT(script != nullptr);
    CC_ASSERT(scriptLength > 0);

    _needSaveCachedData = true;

    V8CachedData* v8CachedData = new V8CachedData(pathMD5);
    v8CachedData->copy(bytecode, bytecodeSize);

    if (scriptContentMD5.empty())
    {
        scriptContentMD5 = getDataMD5Hash(script, scriptLength);
    }

    bool saveV8CacheDataSucceed = saveCachedData(v8CachedData);
    if (saveV8CacheDataSucceed)
    {
        setV8CacheConfig(pathMD5, scriptContentMD5);
    }

    delete v8CachedData;

    return saveV8CacheDataSucceed;
}

void BytecodeManager::saveCachedDataAsync(const std::string& pathMD5, std::string& scriptContentMD5_, const void* bytecode, size_t bytecodeSize, const void* script, size_t scriptLength, const WriteFileAsyncCallback& cb)
{
    CC_ASSERT(!pathMD5.empty());
    CC_ASSERT(bytecode != nullptr);
    CC_ASSERT(bytecodeSize > 0);
    CC_ASSERT(script != nullptr);
    CC_ASSERT(scriptLength > 0);

    _needSaveCachedData = true;

    V8CachedData* v8CachedData = new V8CachedData(pathMD5);
    v8CachedData->copy(bytecode, bytecodeSize);

    if (scriptContentMD5_.empty())
    {
        scriptContentMD5_ = getDataMD5Hash(script, scriptLength);
    }

    std::string scriptContentMD5 = scriptContentMD5_;

    saveCachedDataAsync(v8CachedData, [=](bool saveV8CacheDataSucceed){
        if (saveV8CacheDataSucceed)
        {
            setV8CacheConfig(pathMD5, scriptContentMD5);
        }
        cb(saveV8CacheDataSucceed);

        //NOTE: Delete cachedData after callback, otherwise it will cause memory leak.
        delete v8CachedData;
    });
}

bool BytecodeManager::saveCachedData(V8CachedData* cachedData)
{
    std::lock_guard<std::mutex> lk_(_codeCacheMutex);
    CC_ASSERT(cachedData != nullptr);
    CC_ASSERT(!_cachedDir.empty());
    CC_ASSERT(!cachedData->getPathMD5().empty());
    CC_ASSERT(cachedData != nullptr && cachedData->getLength() > 0 && cachedData->getData() != nullptr);

    std::string filePath = _cachedDir + cachedData->getPathMD5();

    return writeFile(filePath, cachedData->getData(), cachedData->getLength());
}

void BytecodeManager::saveCachedDataAsync(V8CachedData* cachedData, const WriteFileAsyncCallback& cb)
{
    std::lock_guard<std::mutex> lk_(_codeCacheMutex);
    CC_ASSERT(cachedData != nullptr);
    CC_ASSERT(!_cachedDir.empty());
    CC_ASSERT(!cachedData->getPathMD5().empty());
    CC_ASSERT(cachedData != nullptr && cachedData->getLength() > 0 && cachedData->getData() != nullptr);

    std::string filePath = _cachedDir + cachedData->getPathMD5();

    // The cachedData was copied, we don't need to copy it in writeFileAsync.
    writeFileAsync(filePath, cachedData->getData(), cachedData->getLength(), cb, false);
}

bool BytecodeManager::writeFile(const std::string& path, const void* buffer, size_t bufferByteLength)
{
    std::string tmpPath = path + ".tmp";
    bool ret = false;
    FILE* fp = fopen(tmpPath.c_str(), "wb");
    if (fp != nullptr)
    {
        size_t writtenCount = fwrite(buffer, 1, bufferByteLength, fp);
        fflush(fp);
        fclose(fp);

        if (writtenCount == bufferByteLength)
        {
            ret = true;
        }

        if (ret)
        {
            std::lock_guard<std::mutex> lg(__fileMutex);
            // Rename file to the destination
            if (0 == ::rename(tmpPath.c_str(), path.c_str()))
            {
                CC_LOG_DEBUG("[bm] Rename file (%s) to (%s) succeed!", tmpPath.c_str(), path.c_str());
            }
            else
            {
                SE_LOGE("[bm] Rename file (%s) to (%s) failed!", tmpPath.c_str(), path.c_str());
                ret = false;
                if (0 == ::remove(tmpPath.c_str()))
                    CC_LOG_INFO("[bm] Remove temp file (%s) succeed!", tmpPath.c_str());
                else
                    SE_LOGE("[bm] Remove temp file (%s) failed!", tmpPath.c_str());
            }
        }
        else
        {
            if (0 == ::remove(tmpPath.c_str()))
                CC_LOG_INFO("[bm] Remove temp file (%s) succeed!", tmpPath.c_str());
            else
                SE_LOGE("[bm] Remove temp file (%s) failed!", tmpPath.c_str());
        }
    }
    else
    {
        SE_LOGE("[bm] setV8CachedData failed, filePath: %s\n!", path.c_str());
    }

    return ret;
}

void BytecodeManager::writeFileAsync(const std::string& path, const void* srcBuffer, size_t bufferByteLength, const BytecodeManager::WriteFileAsyncCallback& cb, bool copyBuffer)
{
    addRef();

    void* buffer = (void*)srcBuffer;

    if (copyBuffer)
    {
        buffer = malloc(bufferByteLength);
        memcpy(buffer, srcBuffer, bufferByteLength);
    }

    gThreadPool->pushTask([=](int tid){
        bool ret = writeFile(path, buffer, bufferByteLength);
        cb(ret);

        if (copyBuffer)
        {
            free(buffer);
        }

        release();
    });
}

} // namespace se {
