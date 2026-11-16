#ifndef CACAO_ABI_H
#define CACAO_ABI_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include <binder/IMemory.h>
#include <utils/RefBase.h>
#include <utils/StrongPointer.h>
#include <utils/Vector.h>

namespace cacao {

struct ImageSize {
    uint32_t width;
    uint32_t height;
};

enum ImageFormat : uint32_t {
    IMAGE_FORMAT_DEFAULT = 0,
    IMAGE_FORMAT_YUV420 = 0x00020001,
    IMAGE_FORMAT_RAW16 = 0x01000001,
};

enum DataSpace : uint32_t {
    DATA_SPACE_UNKNOWN = 0,
};

struct SerializedData {
    int32_t numFds;
    int32_t fds[96];
    int32_t memSize;
    void* memPtr;
    int32_t reservedTail[2];
};

static const size_t kMaxSerializedFds = 96;

static const int32_t kErrNullCaps = -103;
static const int32_t kErrTimedOut = -110;
static const int32_t kErrTransactFailed = -111;

static_assert(offsetof(SerializedData, numFds) == 0, "numFds must be at +0");
static_assert(offsetof(SerializedData, fds) == 4, "fds must be at +4");
static_assert(offsetof(SerializedData, memSize) == 388, "memSize must be at +388");
static_assert(offsetof(SerializedData, memPtr) == 392, "memPtr must be at +392");
#if defined(__LP64__)
static_assert(sizeof(SerializedData) == 408, "SerializedData must be 408 bytes on LP64");
static_assert(offsetof(SerializedData, reservedTail) == 400, "tail must be at +400 on LP64");
#else
static_assert(sizeof(SerializedData) == 404, "SerializedData must be 404 bytes on ILP32");
static_assert(offsetof(SerializedData, reservedTail) == 396, "tail must be at +396 on ILP32");
#endif

class ObjectBase {
public:
    ObjectBase();
    virtual ~ObjectBase();
    virtual void dump(unsigned int);
    virtual const char* typeName();
    static void* operator new(size_t size);
    static void operator delete(void* p);
};

class ImageBuf : public ObjectBase {
public:
    ImageBuf(void* nativeHandle, ImageSize size, DataSpace dataSpace, ImageFormat format);
    ImageBuf(ImageSize size, uint32_t usage, ImageFormat format, DataSpace dataSpace);
    ~ImageBuf() override;

    const char* typeName() override;
    virtual void* getNative();
    virtual uint32_t getBufSize() const;
    virtual void* getBufAddress();
    virtual ImageSize getImageSize() const;
    virtual bool getDirtyFlag() const;
    virtual void setDirtyFlag(bool);
    virtual int32_t clearCache();
    virtual int32_t flushCache();
    virtual int32_t flushAndClearCache();
    virtual uint32_t getDataSize() const;
    virtual void setDataSize(uint32_t);
    virtual DataSpace getDataSpace() const;

    bool isValid() const { return mHandle != nullptr; }

    ImageSize mSize;
    void* mHandle;
    void* mBufAddress;
    uint32_t mBufSize;
    uint32_t mDataSpace;
    uint32_t mDataSize;
    uint8_t mFlagAt2C;
    uint8_t mDirtyFlag;
    uint16_t mPadAt2E;
    void* mShared;
};

struct ImageBufShared {
    int32_t refCount;
    pthread_mutex_t mutex;
};

#ifdef __LP64__
static_assert(sizeof(ImageBuf) == 0x38, "ImageBuf must be 0x38 bytes (LP64)");
static_assert(offsetof(ImageBuf, mSize) == 0x08, "mSize at +0x08");
static_assert(offsetof(ImageBuf, mHandle) == 0x10, "mHandle at +0x10");
static_assert(offsetof(ImageBuf, mBufAddress) == 0x18, "mBufAddress at +0x18");
static_assert(offsetof(ImageBuf, mBufSize) == 0x20, "mBufSize at +0x20");
static_assert(offsetof(ImageBuf, mDataSpace) == 0x24, "mDataSpace at +0x24");
static_assert(offsetof(ImageBuf, mDataSize) == 0x28, "mDataSize at +0x28");
static_assert(offsetof(ImageBuf, mDirtyFlag) == 0x2D, "mDirtyFlag at +0x2D");
static_assert(offsetof(ImageBuf, mShared) == 0x30, "mShared at +0x30");
static_assert(sizeof(ImageBufShared) == 44, "ImageBufShared must be 44 bytes (LP64, new(0x2c))");
#else
static_assert(sizeof(ImageBuf) == 40, "ImageBuf must be 40 bytes (ILP32)");
static_assert(offsetof(ImageBuf, mSize) == 4, "mSize at +4");
static_assert(offsetof(ImageBuf, mHandle) == 12, "mHandle at +12");
static_assert(offsetof(ImageBuf, mBufAddress) == 16, "mBufAddress at +16");
static_assert(offsetof(ImageBuf, mBufSize) == 20, "mBufSize at +20");
static_assert(offsetof(ImageBuf, mDataSpace) == 24, "mDataSpace at +24");
static_assert(offsetof(ImageBuf, mDataSize) == 28, "mDataSize at +28");
static_assert(offsetof(ImageBuf, mDirtyFlag) == 33, "mDirtyFlag at +33 (strh 0x100 at +32)");
static_assert(offsetof(ImageBuf, mShared) == 36, "mShared at +36");
static_assert(sizeof(ImageBufShared) == 8, "ImageBufShared must be 8 bytes (ILP32)");
#endif

class ProcessModeBase : public ObjectBase {
public:
    virtual int32_t reservedAt32() = 0;
    virtual uint32_t getSerializeSize() = 0;
    virtual int32_t serialize(SerializedData* out) = 0;
};

class ProcessConfigBase : public ObjectBase {
public:
    virtual uint32_t getType() = 0;
    virtual int32_t reservedAt40() = 0;
    virtual int32_t reservedAt48() = 0;
    virtual uint32_t getSerializeSize() = 0;
    virtual int32_t serialize(SerializedData* out) = 0;
    virtual int32_t deserialize(SerializedData* in) = 0;
};

class ProcessParamBase : public ObjectBase {
public:
    virtual int32_t reservedAt32() = 0;
    virtual int32_t reservedAt40() = 0;
    virtual uint32_t getSerializeSize() = 0;
    virtual int32_t serialize(SerializedData* out) = 0;
};

class ProcessResultBase : public ObjectBase {
public:
    virtual int32_t reservedAt32() = 0;
    virtual int32_t reservedAt40() = 0;
    virtual int32_t reservedAt48() = 0;
    virtual int32_t reservedAt56() = 0;
    virtual int32_t reservedAt64() = 0;
    virtual uint32_t getSerializeSize() = 0;
    virtual int32_t serialize(SerializedData* out) = 0;
    virtual int32_t deserialize(SerializedData* in) = 0;
};

class ICBProcessResultListener {
public:
    virtual ~ICBProcessResultListener();
    virtual void onProgress(ProcessResultBase*) = 0;
    virtual void onResult(ProcessResultBase*) = 0;
};

class ICBProcessEventListener;

class IGatewayProcess {
public:
    virtual ~IGatewayProcess();
    virtual int32_t subscribeEvents(ICBProcessEventListener*, void*) = 0;
    virtual int32_t start(const ProcessModeBase*) = 0;
    virtual int32_t stop() = 0;
    virtual int32_t setConfig(const ProcessConfigBase*) = 0;
    virtual int32_t getConfig(ProcessConfigBase*) = 0;
    virtual int32_t process(const ProcessParamBase*, ICBProcessResultListener*,
                            ProcessResultBase*) = 0;
    virtual int32_t processAsync(const ProcessParamBase*,
                                 android::Vector<ImageBuf*>*,
                                 ICBProcessResultListener*,
                                 ProcessResultBase*) = 0;
    virtual int32_t processAsyncWithBinder(const ProcessParamBase*,
                                           const android::sp<android::IBinder>&,
                                           ICBProcessResultListener*,
                                           ProcessResultBase*) = 0;
    virtual int32_t cancel(void*) = 0;
};

struct ProcessCtrlCaps {
    struct CameraIndex {
        int32_t index;
    };
};

class Caps : public ObjectBase {
public:
    virtual uint32_t getSerializeSize() = 0;
    virtual int32_t serialize(SerializedData* out) = 0;
    virtual int32_t deserialize(SerializedData* in) = 0;
};

struct ProcessFactory {
    static IGatewayProcess* createProcess();
    static int32_t getJpegBufferSize(ImageSize);
};

struct ProcessCtrlCapsFactory {
    static int32_t getCaps(const ProcessCtrlCaps::CameraIndex&, Caps*);
};

}

#endif
