package com.cocos.lib;

import android.app.Service;
import android.content.Intent;
import android.hardware.HardwareBuffer;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import com.cocos.aidl.ICocosRemoteRender;
import com.cocos.aidl.ICocosRemoteRenderCallback;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicBoolean;

public class CocosRemoteRenderService extends Service {

    private static final String TAG = "RemoteRenderService";
    //TODO(cjh): Should we consider thread race issues?
    private Map<Integer, CocosRemoteRenderInstance> mRemoteRenderInstanceMap = new ConcurrentHashMap<>();

    public CocosRemoteRenderService() {

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
        }

        @Override
        public void notifyRenderFrameFinish(int eglSyncFd) throws RemoteException {
            //TODO(cjh):
        }
    }

    private void notifyHardwareBufferUpdatedToClient() {

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
        mRemoteRenderInstanceMap.remove(clientId);
        return true;
    }

    @Override
    public void onRebind(Intent intent) {
        int clientId = intent.getIntExtra("clientId", -1);
        Log.w(TAG, "onRebind: " + intent + ", clientId: " + clientId);
        super.onRebind(intent);
    }

    public void setHardwareBuffer(int clientId, HardwareBuffer buffer) {
        CocosRemoteRenderInstance instance = mRemoteRenderInstanceMap.get(clientId);
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

        notifyHardwareBufferUpdatedToClient();
    }
}
