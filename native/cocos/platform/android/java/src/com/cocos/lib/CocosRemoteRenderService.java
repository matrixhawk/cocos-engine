package com.cocos.lib;

import android.app.Service;
import android.content.Intent;
import android.content.res.AssetManager;
import android.hardware.HardwareBuffer;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.MotionEvent;

import com.cocos.aidl.ICocosRemoteRender;
import com.cocos.aidl.ICocosRemoteRenderCallback;

import java.util.HashMap;
import java.util.Map;

public class CocosRemoteRenderService extends Service {

    private static final String TAG = "CocosRemoteRenderService";
    private static int mClientIdCounter = 0;
    private static CocosRemoteRenderService sInstance;
    private final Map<Integer, CocosRemoteRenderInstance> mRemoteRenderInstanceMap = new HashMap<>();
    private long mPlatformHandle = 0L;
    private HardwareBuffer mDefaultHardwareBuffer;
    public CocosRemoteRenderService() {
        sInstance = this;
    }

    @Override
    public void onCreate() {
        Log.i(TAG, "onCreate");
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
        Log.i(TAG, "onStartCommand");
        int clientId = intent.getIntExtra("clientId", 0);
        int width = intent.getIntExtra("width", 1);
        int height = intent.getIntExtra("height", 1);
        mPlatformHandle = nativeOnLaunchEngine(width, height, getAssets());
        return START_REDELIVER_INTENT;
    }
    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy");
        if (mPlatformHandle != 0L) {
            nativeOnShutdownEngine(mPlatformHandle);
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
        Log.w(TAG, "onBind: " + intent);
        int clientId = intent.getIntExtra("clientId", -1);
        int width = intent.getIntExtra("width", -1);
        int height = intent.getIntExtra("height", -1);
        if (clientId == -1 || width == -1 || height == -1) {
            Log.e(TAG, "onBind, intent is invalid, clientId: " + clientId + ", width=" + width + ", height=" + height);
            return null;
        }

        CocosRemoteRenderInstance instance = new CocosRemoteRenderInstance(mPlatformHandle, clientId, width, height);
        synchronized (mRemoteRenderInstanceMap) {
            mRemoteRenderInstanceMap.put(clientId, instance);
        }
        if (clientId == 0) {
            instance.setHardwareBuffer(mDefaultHardwareBuffer);
        }

        return instance.asBinder();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        int clientId = intent.getIntExtra("clientId", -1);
        Log.w(TAG, "onUnbind: " + intent + ", clientId: " + clientId + ", map size: " + mRemoteRenderInstanceMap.size());
        CocosRemoteRenderInstance instance = getRenderInstance(clientId);
        if (instance != null) {
            try {
                instance.stop();
            } catch (RemoteException e) {
                e.printStackTrace();
            } finally {
//                synchronized (mRemoteRenderInstanceMap) {
//                    mRemoteRenderInstanceMap.remove(instance.getClientId());
//                }
            }
        }
        return true;
    }

    @Override
    public void onRebind(Intent intent) {
        int clientId = intent.getIntExtra("clientId", -1);
        Log.w(TAG, "onRebind: " + intent + ", clientId: " + clientId);
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

    private void setHardwareBuffer(int clientId, HardwareBuffer buffer) {
        if (clientId == 0) {
            mDefaultHardwareBuffer = buffer;
        }
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

    public static void setHardwareBufferJNI(int clientId, Object buffer) {
        sInstance.setHardwareBuffer(clientId, (HardwareBuffer) buffer);
    }

    private native long nativeOnLaunchEngine(int width, int height, AssetManager manager);
    private native void nativeOnShutdownEngine(long handle);
    private native void nativeOnPauseEngine(long handle);
    private native void nativeOnResumeEngine(long handle);



}
