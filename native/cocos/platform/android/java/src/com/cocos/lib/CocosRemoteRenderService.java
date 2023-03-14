package com.cocos.lib;

import android.app.Service;
import android.content.Intent;
import android.content.res.AssetManager;
import android.hardware.HardwareBuffer;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import com.cocos.aidl.ICocosRemoteRender;
import com.cocos.aidl.ICocosRemoteRenderCallback;

import java.util.HashMap;
import java.util.Map;

public class CocosRemoteRenderService extends Service {

    private static final String TAG = "CocosRemoteRenderService";
    private static int mClientIdCounter = 0;
    private static CocosRemoteRenderService sInstance;
    private Map<Integer, CocosRemoteRenderInstance> mRemoteRenderInstanceMap = new HashMap<>();
    private long mPlatformHandle = 0L;
    private HardwareBuffer mDefaultHardwareBuffer;

    public CocosRemoteRenderService() {
        sInstance = this;
    }

    @Override
    public void onCreate() {
        super.onCreate();

        // GlobalObject.init should be initialized at first.
        GlobalObject.init(this, null);

        CocosHelper.registerBatteryLevelReceiver(this);
        CocosHelper.init();
        CocosAudioFocusManager.registerAudioFocusListener(this);
        CanvasRenderingContext2DImpl.init(this);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        int clientId = intent.getIntExtra("clientId", 0);
        int width = intent.getIntExtra("width", 1);
        int height = intent.getIntExtra("height", 1);
        mPlatformHandle = nativeOnStartRender(clientId, width, height, getAssets());
        return START_REDELIVER_INTENT;
    }

    @Override
    public void onDestroy() {
        if (mPlatformHandle != 0L) {
            nativeOnStopRender(mPlatformHandle, -1); // TODO(cjh): Don't hardcode clientId
            mPlatformHandle = 0L;
        }

        CocosHelper.unregisterBatteryLevelReceiver(this);
        CocosAudioFocusManager.unregisterAudioFocusListener(this);
        CanvasRenderingContext2DImpl.destroy();
        GlobalObject.destroy();
        super.onDestroy();
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        super.onTaskRemoved(rootIntent);
    }

    private class CocosRemoteRenderInstance extends ICocosRemoteRender.Stub {

        private ICocosRemoteRenderCallback mCallback;
        private HardwareBuffer mBuffer;
        private boolean mIsBufferDirty = false;

        public HardwareBuffer getHardwareBuffer() { return mBuffer; }
        public void setHardwareBuffer(HardwareBuffer buffer) { mBuffer = buffer; }

        public boolean isBufferDirty() { return mIsBufferDirty; }
        public void setBufferDirty(boolean dirty) { mIsBufferDirty = dirty; }

        public ICocosRemoteRenderCallback getCallback() { return mCallback; }

        @Override
        public void initialize(int clientId, ICocosRemoteRenderCallback cb) throws RemoteException {
            mRemoteRenderInstanceMap.put(clientId, this);
            mCallback = cb;

            if (clientId == 0 && mDefaultHardwareBuffer != null) {
                GlobalObject.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        updateHardwareBufferToClient();
                    }
                });
            } else {
                GlobalObject.postDelay(new Runnable() {
                    @Override
                    public void run() {
                        updateHardwareBufferToClient();
                    }
                }, 200L);
            }
        }

        @Override
        public void updateClientWindowSize(int clientId, int width, int height) throws RemoteException {
            //TODO(cjh):
//            nativeOnRenderSizeChanged();
        }

        @Override
        public void notifyRenderFrameFinish(int clientId, int eglSyncFd) throws RemoteException {
            //TODO(cjh):
        }
    }

    private void updateHardwareBufferToClient() {
        synchronized (mRemoteRenderInstanceMap) {
            for (Integer clientId : mRemoteRenderInstanceMap.keySet()) {
                CocosRemoteRenderInstance instance = mRemoteRenderInstanceMap.get(clientId);
                if (instance == null || instance.getCallback() == null) {
                    continue;
                }

                if (!instance.isBufferDirty()) {
                    continue;
                }

                try {
                    instance.getCallback().onUpdateHardwareBuffer(instance.getHardwareBuffer());
                } catch(RemoteException e) {
                    e.printStackTrace();
                } finally {
                    instance.setBufferDirty(false);
                }
            }
        }
    }


    @Override
    public IBinder onBind(Intent intent) {
        ICocosRemoteRender renderInstance = new CocosRemoteRenderInstance();
        return renderInstance.asBinder();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        int clientId = intent.getIntExtra("clientId", -1);
        Log.w(TAG, "onUnbind: " + intent + ", clientId: " + clientId + ", map size: " + mRemoteRenderInstanceMap.size());
        synchronized (mRemoteRenderInstanceMap) {
            mRemoteRenderInstanceMap.remove(clientId);
        }
        return true;
    }

    @Override
    public void onRebind(Intent intent) {
        int clientId = intent.getIntExtra("clientId", -1);
        Log.w(TAG, "onRebind: " + intent + ", clientId: " + clientId);
        super.onRebind(intent);
    }

    private void setHardwareBuffer(int clientId, HardwareBuffer buffer) {
        if (clientId == 0) {
            mDefaultHardwareBuffer = buffer;
        }
        CocosRemoteRenderInstance instance;
        synchronized (mRemoteRenderInstanceMap) {
            instance = mRemoteRenderInstanceMap.get(clientId);
        }
        if (instance == null) {
            Log.e(TAG, "setHardwareBuffer, could not find clientId: " + clientId);
            return;
        }
        HardwareBuffer oldBuffer = instance.getHardwareBuffer();
        Log.d(TAG, "setHardwareBuffer: new buffer: " + buffer + ", old buffer: " + oldBuffer);

        if (oldBuffer != null) {
            oldBuffer.close();
        }
        instance.setHardwareBuffer(buffer);
        instance.setBufferDirty(true);

        updateHardwareBufferToClient();
    }

    public static void setHardwareBufferJNI(int clientId, Object buffer) {
        sInstance.setHardwareBuffer(clientId, (HardwareBuffer) buffer);
    }

    private native long nativeOnStartRender(int clientId, int width, int height, AssetManager manager);
    private native void nativeOnRenderSizeChanged(long handle, int clientId, int width, int height);
    private native void nativeOnStopRender(long handle, int clientId);
}
