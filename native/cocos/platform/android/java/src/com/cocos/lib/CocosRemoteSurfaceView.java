package com.cocos.lib;

import android.content.Context;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;

public class CocosRemoteSurfaceView{
    private CocosTouchHandler mTouchHandler;
    private long mNativeHandle;
    private int mWindowId;

    public CocosRemoteSurfaceView( int windowId) {
        mWindowId = windowId;
        mNativeHandle = constructNative(windowId);
        mTouchHandler = new CocosTouchHandler(mWindowId);
    }

    private native long constructNative(int windowId);
    private native void destructNative(long handle);
    private native void onSizeChangedNative(int windowId, int width, final int height);
    private native void onSurfaceRedrawNeededNative(long handle);
    private native void onSurfaceCreatedNative(long handle, Surface surface);
    private native void onSurfaceChangedNative(long handle, Surface surface, int format, int width, int height);
    private native void onSurfaceDestroyedNative(long handle);


    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        CocosHelper.runOnGameThreadAtForeground(new Runnable() {
            @Override
            public void run() {
                onSizeChangedNative(mWindowId, w, h);
            }
        });
    }

    public boolean onTouchEvent(MotionEvent event) {
        return mTouchHandler.onTouchEvent(event);
    }


    public void surfaceRedrawNeeded(Surface surface) {
        onSurfaceRedrawNeededNative(mNativeHandle);
    }

    public void surfaceCreated(Surface surface) {
        onSurfaceCreatedNative(mNativeHandle, surface);
    }

    public void surfaceChanged(Surface surface, int format, int width, int height) {
        onSurfaceChangedNative(mNativeHandle, surface, format, width, height);
    }

    public void surfaceDestroyed(Surface surface) {
        onSurfaceDestroyedNative(mNativeHandle);
    }

    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        destructNative(mNativeHandle);
        mNativeHandle = 0;
    }
}
