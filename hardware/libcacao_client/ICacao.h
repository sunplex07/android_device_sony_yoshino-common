#ifndef CACAO_ICACAO_H
#define CACAO_ICACAO_H

#include <binder/IInterface.h>
#include <binder/IMemory.h>
#include <binder/Parcel.h>
#include <utils/String16.h>
#include <utils/Vector.h>

#include "include/cacao/cacao_abi.h"

namespace android {

enum CacaoServiceTxn {
    TXN_SVC_CONNECT = 1,
    TXN_SVC_GET_SERVICE_PID = 2,
    TXN_SVC_ALLOC_MEMORY = 3,
    TXN_SVC_GET_JPEG_BUFFER_SIZE = 4,
};

enum CacaoTxn {
    TXN_CACAO_DISCONNECT = 1,
    TXN_CACAO_START = 2,
    TXN_CACAO_STOP = 3,
    TXN_CACAO_PROCESS = 4,
    TXN_CACAO_GET_CONFIG = 5,
    TXN_CACAO_PROCESS_ASYNC = 6,
    TXN_CACAO_PROCESS_ASYNC_WITH_BINDER = 7,
    TXN_CACAO_CANCEL = 8,
};

enum CacaoClientTxn {
    TXN_CLIENT_NOTIFY_EVENT = 1,
};

enum CacaoProcessCallbackTxn {
    TXN_CB_NOTIFY_PROGRESS = 1,
    TXN_CB_NOTIFY_RESULT = 2,
};

class ICacaoProcessCallback;

class ICacao : public IInterface {
public:
    DECLARE_META_INTERFACE(Cacao)

    virtual int32_t disconnect() = 0;
    virtual int32_t setConfig(const sp<IMemory>& mem, cacao::SerializedData sd) = 0;
    virtual int32_t stop() = 0;

    struct ImageBufWire {
        uint32_t width;
        uint32_t height;
        int32_t dataSpace;
        const native_handle_t* handle;
    };

    virtual int32_t process(int32_t token, const sp<IMemory>& paramMem,
                            cacao::SerializedData sd) = 0;
    virtual int32_t getConfig(int32_t id, const sp<IMemory>& mem,
                              cacao::SerializedData sd) = 0;

    virtual intptr_t processAsync(const sp<IMemory>& paramMem, cacao::SerializedData paramSd,
                                  Vector<cacao::ImageBuf*> images,
                                  const sp<IMemory>& resultMem, cacao::SerializedData resultSd,
                                  int32_t token, const sp<ICacaoProcessCallback>& callback) = 0;

    virtual intptr_t processAsyncWithBinder(const sp<IMemory>& paramMem,
                                            cacao::SerializedData paramSd,
                                            const sp<IBinder>& extra,
                                            const sp<IMemory>& resultMem,
                                            cacao::SerializedData resultSd, int32_t token,
                                            const sp<ICacaoProcessCallback>& callback) = 0;

    virtual int32_t cancel(intptr_t id) = 0;
};

class ICacaoClient : public IInterface {
public:
    DECLARE_META_INTERFACE(CacaoClient)
    virtual void notifyEvent(int32_t eventId, const sp<IMemory>& mem) = 0;
};

class ICacaoProcessCallback : public IInterface {
public:
    DECLARE_META_INTERFACE(CacaoProcessCallback)
    virtual void notifyProgress(const sp<IMemory>& mem) = 0;
    virtual void notifyResult(const sp<IMemory>& mem) = 0;
};

class ICacaoService : public IInterface {
public:
    DECLARE_META_INTERFACE(CacaoService)
    virtual sp<ICacao> connect(const sp<ICacaoClient>& client) = 0;
    virtual int32_t getServicePid() = 0;
    virtual int32_t allocMemory(const int32_t& value, const sp<IMemory>& mem,
                                cacao::SerializedData sd) = 0;
    virtual int32_t getJpegBufferSize(uint32_t width, uint32_t height) = 0;
};

class BnCacaoService : public BnInterface<ICacaoService> {
public:
    status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) override;
};

class BnCacao : public BnInterface<ICacao> {
public:
    status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) override;
};

void writeSerializedData(Parcel* out, const sp<IMemory>& mem, const int32_t* fds, int32_t numFds);

sp<ICacaoService> getCacaoService();

int32_t getCacaoServicePid();

}

#endif
