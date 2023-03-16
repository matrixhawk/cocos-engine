package com.cocos.lib;

import android.app.Service;
import android.content.Intent;
import android.content.res.AssetManager;
import android.hardware.HardwareBuffer;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import java.util.HashMap;
import java.util.Map;

public abstract class CocosRemoteRenderServiceBase extends Service implements JsbBridge.ICallback {
    static {
        System.loadLibrary("cocos");
    }
    private static final String TAG = "CocosRemoteRenderServiceBase";
    private static int mClientIdCounter = 0;
    private static CocosRemoteRenderServiceBase sInstance;
    private final Map<Integer, CocosRemoteRenderInstance> mRemoteRenderInstanceMap = new HashMap<>();
    private long mPlatformHandle = 0L;
    private HardwareBuffer mDefaultHardwareBuffer;
    protected CocosRemoteRenderServiceBase() {
        sInstance = this;
    }

    @Override
    public void onCreate() {
        Log.i(TAG, "onCreate: " + this);
        super.onCreate();

        // GlobalObject.init should be initialized at first.
        GlobalObject.init(this, null);

        CocosHelper.registerBatteryLevelReceiver(this);
        CocosHelper.init();
        CocosAudioFocusManager.registerAudioFocusListener(this);
        CanvasRenderingContext2DImpl.init(this);

        JsbBridge.setCallback(this);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "onStartCommand: " + this);
        int clientId = intent.getIntExtra("clientId", 0);
        int width = intent.getIntExtra("width", 1);
        int height = intent.getIntExtra("height", 1);
        launchEngine(width, height, getAssets());
        return START_REDELIVER_INTENT;
    }
    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy:" + this);
        shutdownEngine();
        CocosHelper.unregisterBatteryLevelReceiver(this);
        CocosAudioFocusManager.unregisterAudioFocusListener(this);
        CanvasRenderingContext2DImpl.destroy();
        GlobalObject.destroy();
        JsbBridge.setCallback(null);

        super.onDestroy();
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
        Log.w(TAG, "onLowMemory");
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        Log.w(TAG, "onTrimMemory");
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        super.onTaskRemoved(rootIntent);
        Log.w(TAG, "onTaskRemoved");
    }
    @Override
    public IBinder onBind(Intent intent) {
        Log.w(TAG, "onBind: " + intent + ", this=" + this);
        int clientId = intent.getIntExtra("clientId", -1);
        int width = intent.getIntExtra("width", -1);
        int height = intent.getIntExtra("height", -1);
        if (clientId == -1 || width == -1 || height == -1) {
            Log.e(TAG, "onBind, intent is invalid, clientId: " + clientId + ", width=" + width + ", height=" + height);
            return null;
        }

        launchEngine(width, height, getAssets());

        CocosRemoteRenderInstance instance = new CocosRemoteRenderInstance(mPlatformHandle, clientId, width, height);
        synchronized (mRemoteRenderInstanceMap) {
            mRemoteRenderInstanceMap.put(clientId, instance);
        }
        // NOTE(cjh): ALl clients share the same hardware buffer now.
//        if (clientId == 0) {
            instance.setHardwareBuffer(mDefaultHardwareBuffer);
//        }

        return instance.asBinder();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        int clientId = intent.getIntExtra("clientId", -1);
        Log.w(TAG, "onUnbind: " + intent + ", clientId: " + clientId + ", map size: " + mRemoteRenderInstanceMap.size());
        Log.d(TAG, "onUnbind, this=" + this);
        CocosRemoteRenderInstance instance = getRenderInstance(clientId);
        if (instance != null) {
            try {
                instance.stop();
            } catch (RemoteException e) {
                e.printStackTrace();
            }
            // TODO(cjh): Whether need to remove the client from map?
            // If client is re-connected, onBind will not be invoked, onRebind is called instead.
            // Binder service will cache the instance internally, so we should not remove any clients.
//            finally {
//                synchronized (mRemoteRenderInstanceMap) {
//                    mRemoteRenderInstanceMap.remove(instance.getClientId());
//                }
//            }
        }
        return true;
    }

    @Override
    public void onRebind(Intent intent) {
        int clientId = intent.getIntExtra("clientId", -1);
        Log.w(TAG, "onRebind: " + intent + ", clientId: " + clientId + ",this=" + this);
        super.onRebind(intent);
    }

    private CocosRemoteRenderInstance getRenderInstance(int clientId) {
        CocosRemoteRenderInstance instance;
        synchronized (mRemoteRenderInstanceMap) {
            instance = mRemoteRenderInstanceMap.get(clientId);
        }
        if (instance == null) {
            Log.e(TAG, "setHardwareBuffer, could not find clientId: " + clientId);
        }
        return instance;
    }

    private void setHardwareBuffer(final int clientId, final HardwareBuffer buffer) {
        Log.d(TAG, "setHardwareBuffer, clientId: " + clientId);
        CocosRemoteRenderInstance instance = getRenderInstance(clientId);
        if (instance == null) {
            Log.e(TAG, "setHardwareBuffer(" + clientId + "), could not find instance");
            return;
        }

        HardwareBuffer oldBuffer = instance.getHardwareBuffer();
        Log.d(TAG, "setHardwareBuffer: new buffer: " + buffer + ", old buffer: " + oldBuffer);

        if (oldBuffer != null) {
            oldBuffer.close();
        }
        instance.setHardwareBuffer(buffer);
        instance.setBufferDirty(true);
        instance.updateHardwareBufferToClient();
    }

    public static void setHardwareBufferJNI(int clientId, final Object buffer) {
        GlobalObject.runOnUiThread(()->{
            sInstance.mDefaultHardwareBuffer = (HardwareBuffer)buffer;
            synchronized (sInstance.mRemoteRenderInstanceMap) {
                // NOTE(cjh): The current need is share one buffer for all clients, so iterate the clients
                for (Integer iClientId : sInstance.mRemoteRenderInstanceMap.keySet()) {
                    CocosRemoteRenderInstance instance = sInstance.getRenderInstance(iClientId);
                    sInstance.setHardwareBuffer(iClientId, (HardwareBuffer) buffer);
                }
            }
        });
    }

    @Override
    public void onScript(String arg0, String arg1) {
        for (Integer clientId : mRemoteRenderInstanceMap.keySet()) {
            CocosRemoteRenderInstance instance = getRenderInstance(clientId);
            if (instance == null) {
                continue;
            }
            try {
                if (instance.isActive()) {
                    instance.getCallback().onScript(arg0, arg1);
                }
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    }

    private void launchEngine(int width, int height, AssetManager manager) {
        if (mPlatformHandle == 0L) {
            Log.d(TAG, "launchEngine");
            mPlatformHandle = nativeOnLaunchEngine(width, height, manager);
        }
    }

    private void shutdownEngine() {
        if (mPlatformHandle != 0L) {
            Log.d(TAG, "shutdownEngine");
            nativeOnShutdownEngine(mPlatformHandle);
            mPlatformHandle = 0L;
        }
    }

    private native long nativeOnLaunchEngine(int width, int height, AssetManager manager);
    private native void nativeOnShutdownEngine(long handle);
    private native void nativeOnPauseEngine(long handle);
    private native void nativeOnResumeEngine(long handle);
}
