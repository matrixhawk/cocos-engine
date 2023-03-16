package com.cocos.lib;

import android.hardware.HardwareBuffer;
import android.os.RemoteException;
import android.util.Log;
import android.view.MotionEvent;

import com.cocos.aidl.ICocosRemoteRender;
import com.cocos.aidl.ICocosRemoteRenderCallback;

class CocosRemoteRenderInstance extends ICocosRemoteRender.Stub {
    private static final String TAG = "CocosRemoteRenderInstance" ;
    private long mPlatformHandle = 0L;
    private int mClientId = -1;
    private int mWidth = 0;
    private int mHeight = 0;
    private ICocosRemoteRenderCallback mCallback;
    private HardwareBuffer mBuffer;
    private boolean mIsBufferDirty = true;
    private boolean mIsActive = false;
    private CocosTouchHandler mTouchHandler;

    CocosRemoteRenderInstance(long platformHandle, int clientId, int width, int height) {
        mPlatformHandle = platformHandle;
        mClientId = clientId;
        mWidth = width;
        mHeight = height;
        // FIXME(cjh): Don't hardcode windowId to 1
        mTouchHandler = new CocosTouchHandler(1); // mainWindowId = 1
    }

    public void setActive(boolean v) {
        mIsActive = v;
    }

    public boolean isActive() {
        return mIsActive;
    }
    public HardwareBuffer getHardwareBuffer() {
        return mBuffer;
    }

    public void setHardwareBuffer(HardwareBuffer buffer) {
        mBuffer = buffer;
    }

    public boolean isBufferDirty() {
        return mIsBufferDirty;
    }

    public void setBufferDirty(boolean dirty) {
        mIsBufferDirty = dirty;
    }

    public int getClientId() {
        return mClientId;
    }

    public ICocosRemoteRenderCallback getCallback() { return mCallback; }

    public void updateHardwareBufferToClient() {
        Log.d(TAG, "updateHardwareBufferToClient: mClientId=" + mClientId);
        if (mCallback == null) {
            Log.e(TAG, "The callback of render instance is null");
            return;
        }

        if (!isBufferDirty()) {
            Log.w(TAG, ">>> clientId: " + mClientId + ", instance buffer is not dirty, this=" + this);
            return;
        }

        try {
            Log.i(TAG, ">> Notify client (" + mClientId + ") to update buffer");
            mCallback.onUpdateHardwareBuffer(mBuffer);
            setBufferDirty(false);
        } catch(RemoteException e) {
            e.printStackTrace();
            setBufferDirty(true);
        }
    }

    @Override
    public void start() throws RemoteException {
        Log.i(TAG, "CocosRemoteRenderInstance.start: " + mClientId + ", this=" + this);
        setActive(true);
        setBufferDirty(true);
        // NOTE: The current need is share the same buffer for all clients
        // Therefore, just synchronize the default hardware buffer to client.
//        if (mClientId == 0) {
            Runnable r = new Runnable() {
                @Override
                public void run() {
                    if (mBuffer == null) {
                        GlobalObject.postDelay(this, 200);
                        return;
                    }
                    updateHardwareBufferToClient();
                }
            };

            GlobalObject.runOnUiThread(r);
//        } else {
//            CocosHelper.runOnGameThread(() -> {
//                nativeOnCreateClient(mPlatformHandle, mClientId, mWidth, mHeight);
//            });
//        }
    }

    @Override
    public void stop() throws RemoteException {
        Log.i(TAG, "CocosRemoteRenderInstance.stop: " + mClientId + ", this=" + this);
        setActive(false);
        setBufferDirty(false);

        if (mClientId == -1) {
            return;
        }

        final int clientId = mClientId;
        if (clientId > 0) {
            CocosHelper.runOnGameThread(() -> {
                nativeOnDestroyClient(mPlatformHandle, clientId);
            });
        }
    }

    @Override
    public void setCallback(ICocosRemoteRenderCallback cb) throws RemoteException {
        mCallback = cb;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) throws RemoteException {
        return mTouchHandler.onTouchEvent(event);
    }

    @Override
    public void pause() {
        nativeOnPauseClient(mPlatformHandle, mClientId);
    }

    @Override
    public void resume() {
        nativeOnResumeClient(mPlatformHandle, mClientId);
    }

    @Override
    public void updateClientWindowSize(int width, int height) throws RemoteException {
        //TODO(cjh):
        nativeOnRenderSizeChanged(mPlatformHandle, mClientId, width, height);
    }

    @Override
    public void notifyRenderFrameFinish(int eglSyncFd) throws RemoteException {
        //TODO(cjh):
    }

    @Override
    public void sendToScript(String arg0, String arg1) throws RemoteException {
        JsbBridge.sendToScript(arg0, arg1);
    }

    private native void nativeOnCreateClient(long handle, int clientId, int width, int height);
    private native void nativeOnDestroyClient(long handle, int clientId);
    private native void nativeOnPauseClient(long handle, int clientId);
    private native void nativeOnResumeClient(long handle, int clientId);

    private native void nativeOnRenderSizeChanged(long handle, int clientId, int width, int height);
}
