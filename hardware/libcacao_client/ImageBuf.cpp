#define LOG_TAG "CacaoImageBuf"

#include <string.h>
#include <unistd.h>

#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#include <log/log.h>
#include <utils/Mutex.h>

#include <android/hardware/graphics/allocator/2.0/IAllocator.h>

#include "include/cacao/cacao_abi.h"

using android::hardware::hidl_handle;
using android::hardware::graphics::mapper::V2_0::Error;
using android::hardware::graphics::mapper::V2_0::IMapper;
using android::hardware::graphics::allocator::V2_0::IAllocator;
using android::hardware::graphics::common::V1_0::BufferUsage;
using android::hardware::graphics::common::V1_0::PixelFormat;

namespace cacao {
namespace {

android::Mutex gMapperLock;
android::sp<IMapper> gMapper;

constexpr int32_t kErrNotReady = -101;
constexpr int32_t kErrHalFailed = -111;

int32_t ensureMapper() {
    android::AutoMutex _l(gMapperLock);
    if (gMapper != nullptr) {
        return 0;
    }
    android::sp<IMapper> mapper = IMapper::getService("default", false );
    if (mapper == nullptr || mapper->isRemote()) {
        ALOGE("could not get a passthrough graphics mapper@2.0");
        gMapper = nullptr;
        return kErrHalFailed;
    }
    gMapper = mapper;
    return 0;
}

android::Mutex gAllocatorLock;
android::sp<IAllocator> gAllocator;

int32_t ensureAllocator() {
    android::AutoMutex _l(gAllocatorLock);
    if (gAllocator != nullptr) {
        return 0;
    }
    android::sp<IAllocator> alloc = IAllocator::getService("default");
    if (alloc == nullptr) {
        ALOGE("could not get graphics allocator@2.0");
        return kErrHalFailed;
    }
    gAllocator = alloc;
    return 0;
}

android::sp<IAllocator> allocatorOrNull() {
    android::AutoMutex _l(gAllocatorLock);
    return gAllocator;
}

android::sp<IMapper> mapperOrNull() {
    android::AutoMutex _l(gMapperLock);
    return gMapper;
}

}

ImageBuf::ImageBuf(void* nativeHandle, ImageSize size, DataSpace dataSpace, ImageFormat format) {
    mSize = size;
    mHandle = nativeHandle;

    if (nativeHandle != nullptr &&
        (reinterpret_cast<uintptr_t>(nativeHandle) & 0x3) == 0) {
        const int* h = static_cast<const int*>(nativeHandle);
        ALOGI("ImageBuf(%ux%u): handle=%p version=%d numFds=%d numInts=%d %s", size.width,
              size.height, nativeHandle, h[0], h[1], h[2],
              (h[0] == 12 && h[1] >= 0 && h[1] < 64 && h[2] >= 0 && h[2] < 512)
                      ? "-> looks like a native_handle"
                      : "-> NOT a native_handle!");
    } else {
        ALOGW("ImageBuf(%ux%u): handle=%p is null or misaligned", size.width, size.height,
              nativeHandle);
    }
    mBufAddress = nullptr;
    mBufSize = 0;
    mDataSpace = static_cast<uint32_t>(dataSpace);
    mDataSize = 0;
    mFlagAt2C = 0;
    mDirtyFlag = 1;
    mPadAt2E = 0;
    mShared = nullptr;

    if (ensureMapper() != 0) {
        return;
    }

    switch (format) {
        case IMAGE_FORMAT_DEFAULT:
        case IMAGE_FORMAT_YUV420: {
            const uint32_t strideAligned = (mSize.width + 127u) & ~127u;
            const uint32_t lumaLines = (mSize.height + 31u) & ~31u;
            const uint32_t chromaLines = ((mSize.height >> 1) + 31u) & ~31u;
            mBufSize = strideAligned * (lumaLines + chromaLines);
            break;
        }
        case IMAGE_FORMAT_RAW16:
            mBufSize = mSize.width * mSize.height * 2u + 0x10000u;
            break;
        default:
            break;
    }

    ImageBufShared* shared =
            static_cast<ImageBufShared*>(::operator new(sizeof(ImageBufShared)));
    shared->refCount = 0;
    pthread_mutex_init(&shared->mutex, nullptr);
    pthread_mutex_lock(&shared->mutex);
    shared->refCount++;
    pthread_mutex_unlock(&shared->mutex);
    mShared = shared;
}

ImageBuf::ImageBuf(ImageSize size, uint32_t usage, ImageFormat format, DataSpace dataSpace) {
    mSize = size;
    mHandle = nullptr;
    mBufAddress = nullptr;
    mBufSize = 0;
    mDataSpace = static_cast<uint32_t>(dataSpace);
    mDataSize = 0;
    mFlagAt2C = 1;
    mDirtyFlag = 1;
    mShared = nullptr;

    if (ensureMapper() < 0 || ensureAllocator() < 0) {
        return;
    }

    uint32_t descWidth;
    uint32_t descHeight = 1;
    uint32_t halFormat;
    if (usage != 0) {
        descWidth = usage;
        halFormat = 0x21;
    } else if (static_cast<uint32_t>(format) == 0x01000001) {
        descWidth = size.width * size.height * 2 + 0x10000;
        halFormat = 0x21;
    } else if (static_cast<uint32_t>(format) == 0x00020001) {
        descWidth = (size.width + 1) & ~1u;
        descHeight = (size.height + 1) & ~1u;
        halFormat = 0x11;
    } else if (static_cast<uint32_t>(format) == 0) {
        descWidth = (size.width + 1) & ~1u;
        descHeight = (size.height + 1) & ~1u;
        halFormat = 0x22;
    } else {
        ALOGE("ImageBuf: unsupported format 0x%x", static_cast<uint32_t>(format));
        return;
    }

    android::sp<IMapper> mapper = mapperOrNull();
    android::sp<IAllocator> alloc = allocatorOrNull();
    if (mapper == nullptr || alloc == nullptr) {
        return;
    }

    IMapper::BufferDescriptorInfo info = {};
    info.width = descWidth;
    info.height = descHeight;
    info.layerCount = 1;
    info.format = static_cast<PixelFormat>(halFormat);
    info.usage = 0x33;

    android::hardware::hidl_vec<uint32_t> descriptor;
    Error err = Error::NO_RESOURCES;
    mapper->createDescriptor(info, [&](Error e, const auto& d) {
        err = e;
        descriptor = d;
    });
    if (err != Error::NONE) {
        ALOGE("ImageBuf: createDescriptor failed: %d", static_cast<int>(err));
        return;
    }

    android::hardware::hidl_vec<android::hardware::hidl_handle> buffers;
    alloc->allocate(descriptor, 1, [&](Error e, uint32_t , const auto& b) {
        err = e;
        if (e == Error::NONE) buffers = b;
    });
    if (err != Error::NONE || buffers.size() != 1) {
        ALOGE("ImageBuf: allocate failed: %d", static_cast<int>(err));
        return;
    }

    mapper->importBuffer(buffers[0], [&](Error e, void* buf) {
        err = e;
        if (e == Error::NONE) mHandle = buf;
    });
    if (err != Error::NONE || mHandle == nullptr) {
        ALOGE("ImageBuf: importBuffer failed: %d", static_cast<int>(err));
        mHandle = nullptr;
        return;
    }

    ImageBufShared* shared =
            static_cast<ImageBufShared*>(::operator new(sizeof(ImageBufShared)));
    shared->refCount = 1;
    pthread_mutex_init(&shared->mutex, nullptr);
    mShared = shared;
}

ImageBuf::~ImageBuf() {
    ImageBufShared* shared = static_cast<ImageBufShared*>(mShared);
    if (shared == nullptr) {
        return;
    }

    pthread_mutex_lock(&shared->mutex);
    int32_t remaining = shared->refCount;
    if (remaining - 1 >= 0) {
        remaining = remaining - 1;
        shared->refCount = remaining;
    }
    pthread_mutex_unlock(&shared->mutex);

    if (remaining != 0) {
        return;
    }

    pthread_mutex_destroy(&shared->mutex);
    ::operator delete(shared);
    mShared = nullptr;

    android::sp<IMapper> mapper = mapperOrNull();
    if (mapper == nullptr) {
        return;
    }

    if (mBufAddress != nullptr) {
        flushCache();
    }

    if (mFlagAt2C != 0) {
        if (mHandle != nullptr) {
            mapper->freeBuffer(mHandle);
        }
        mHandle = nullptr;
        mBufAddress = nullptr;
    }
}

const char* ImageBuf::typeName() {
    return "ImageBuf";
}

void* ImageBuf::getNative() {
    return mHandle;
}

uint32_t ImageBuf::getBufSize() const {
    if (mHandle == nullptr) {
        ALOGE("getBufSize: buffer is not ready");
        return 0;
    }
    return mBufSize;
}

void* ImageBuf::getBufAddress() {
    if (mHandle == nullptr) {
        ALOGE("getBufAddress: buffer is not ready");
        return nullptr;
    }
    if (mBufAddress != nullptr) {
        return mBufAddress;
    }
    clearCache();
    return mBufAddress;
}

ImageSize ImageBuf::getImageSize() const {
    return mSize;
}

bool ImageBuf::getDirtyFlag() const {
    if (mHandle == nullptr) {
        ALOGE("getDirtyFlag: buffer is not ready");
        return false;
    }
    return mDirtyFlag != 0;
}

void ImageBuf::setDirtyFlag(bool dirty) {
    if (mHandle == nullptr) {
        ALOGE("setDirtyFlag: buffer is not ready");
        return;
    }
    mDirtyFlag = dirty ? 1 : 0;
}

int32_t ImageBuf::clearCache() {
    if (mHandle == nullptr) {
        ALOGE("clearCache: buffer is not ready");
        return kErrNotReady;
    }
    android::sp<IMapper> mapper = mapperOrNull();
    if (mapper == nullptr) {
        ALOGE("clearCache: buffer is not ready (no mapper)");
        return kErrNotReady;
    }

    IMapper::Rect accessRegion;
    accessRegion.left = static_cast<int32_t>(mSize.width);
    accessRegion.top = static_cast<int32_t>(mSize.height);
    accessRegion.width = 0;
    accessRegion.height = 0;

    const hidl_handle acquireFence;

    constexpr uint64_t kCpuUsage = 0x66;

    bool ok = false;
    void* mapped = nullptr;
    mapper->lock(mHandle, kCpuUsage, accessRegion, acquireFence,
                 [&](Error error, void* data) {
                     if (error != Error::NONE) {
                         ALOGE("lock buffer failed %d", static_cast<int32_t>(error));
                         return;
                     }
                     ok = true;
                     mapped = data;
                 });

    if (!ok) {
        return kErrHalFailed;
    }
    mBufAddress = mapped;
    mDirtyFlag = 0;
    return 0;
}

int32_t ImageBuf::flushCache() {
    if (mHandle == nullptr) {
        ALOGE("flushCache: buffer is not ready");
        return kErrNotReady;
    }
    android::sp<IMapper> mapper = mapperOrNull();
    if (mapper == nullptr) {
        ALOGE("flushCache: buffer is not ready (no mapper)");
        return kErrNotReady;
    }

    bool ok = false;
    mapper->unlock(mHandle, [&](Error error, const hidl_handle& releaseFence) {
        (void)releaseFence;
        if (error != Error::NONE) {
            ALOGE("unlock buffer failed %d", static_cast<int32_t>(error));
            return;
        }
        ok = true;
    });

    if (!ok) {
        return kErrHalFailed;
    }
    mDirtyFlag = 0;
    return 0;
}

int32_t ImageBuf::flushAndClearCache() {
    if (mDirtyFlag != 0 && mBufAddress != nullptr) {
        const int32_t rc = flushCache();
        if (rc < 0) {
            return rc;
        }
    }
    return clearCache();
}

uint32_t ImageBuf::getDataSize() const {
    return mDataSize;
}

void ImageBuf::setDataSize(uint32_t dataSize) {
    if (mHandle == nullptr) {
        ALOGE("setDataSize: buffer is not ready");
        return;
    }
    mDataSize = dataSize;
}

DataSpace ImageBuf::getDataSpace() const {
    return static_cast<DataSpace>(mDataSpace);
}

}
