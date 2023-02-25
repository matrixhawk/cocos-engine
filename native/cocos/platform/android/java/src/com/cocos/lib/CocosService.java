package com.cocos.lib;

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.graphics.PixelFormat;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import com.google.androidgamesdk.GameActivity;

import java.io.File;

import dalvik.system.BaseDexClassLoader;

public class CocosService extends Service {
    private static final String TAG = "CocosService";
    private static final String LIBNAME = "cocos";

    private WindowManager mWindowManager;
    private SparseArray<CocosRemoteSurfaceView> mSurfaceViewArray = new SparseArray<>();

    private boolean mDestroyed;
    private native void onCreateNative(Context context);

    private long mNativeHandle;
    protected native long loadNativeCode(Context context, String path, String funcname, String internalDataPath,
                                         String obbPath, String externalDataPath, AssetManager assetMgr, byte[] savedState);

    protected native String getDlError();

    protected native void unloadNativeCode(long handle);

    protected native void onStartNative(long handle);

    protected native void onResumeNative(long handle);

    protected native byte[] onSaveInstanceStateNative(long handle);

    protected native void onPauseNative(long handle);

    protected native void onStopNative(long handle);

    protected native void onConfigurationChangedNative(long handle);

    protected native void onTrimMemoryNative(long handle, int level);

    protected native void onWindowFocusChangedNative(long handle, boolean focused);

    protected native void onSurfaceCreatedNative(long handle, Surface surface);

    protected native void onSurfaceChangedNative(
        long handle, Surface surface, int format, int width, int height);

    protected native void onSurfaceRedrawNeededNative(long handle, Surface surface);

    protected native void onSurfaceDestroyedNative(long handle);

    protected native boolean onTouchEventNative(long handle, MotionEvent motionEvent);

    protected native boolean onKeyDownNative(long handle, KeyEvent keyEvent);

    protected native boolean onKeyUpNative(long handle, KeyEvent keyEvent);

    @Override
    public void onCreate() {
        super.onCreate();
        GlobalObject.setContext(getApplicationContext());

        onLoadNativeLibraries();
        onCreateNative(getApplicationContext());

        String libname = LIBNAME;
        String funcname = "GameActivity_onCreate";
        BaseDexClassLoader classLoader = (BaseDexClassLoader) getClassLoader();
        String path = classLoader.findLibrary(libname);
        if (path == null) {
            throw new IllegalArgumentException("Unable to find native library " + libname
                + " using classloader: " + classLoader.toString());
        }
        byte[] nativeSavedState = null;
        mNativeHandle = loadNativeCode(getApplicationContext(), path, funcname, getAbsolutePath(getFilesDir()),
            getAbsolutePath(getObbDir()), getAbsolutePath(getExternalFilesDir(null)),
            getAssets(), nativeSavedState);
        if (mNativeHandle == 0) {
            throw new UnsatisfiedLinkError(
                "Unable to load native library \"" + path + "\": " + getDlError());
        }

        CocosHelper.registerBatteryLevelReceiver(this);
        CocosHelper.init(this);
        CocosAudioFocusManager.registerAudioFocusListener(this);
        CanvasRenderingContext2DImpl.init(this);
//        mSensorHandler = new CocosSensorHandler(this);

        initView();

        onStartNative(mNativeHandle);

//        if (Build.VERSION.SDK_INT >= 23) {
//            if (!Settings.canDrawOverlays(this)) {
//                Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION);
//                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
//                startActivityForResult(intent, 1);
//            } else {
//                Log.e("TAG", "H");
//                //TODO do something you need
//            }
//        }
    }

    public void initView(){
        mWindowManager = (WindowManager) getApplicationContext().getSystemService(
            Context.WINDOW_SERVICE);
        WindowManager.LayoutParams params = new WindowManager.LayoutParams();
//        params.type = WindowManager.LayoutParams.TYPE_SYSTEM_ALERT;// 系统提示window
        //检查版本，注意当type为TYPE_APPLICATION_OVERLAY时，铺满活动窗口，但在关键的系统窗口下面，如状态栏或IME
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            params.type = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
        } else {
            params.type = WindowManager.LayoutParams.TYPE_SYSTEM_ALERT;
        }
        params.format = PixelFormat.TRANSLUCENT;// 支持透明
        //params.format = PixelFormat.RGBA_8888;
        params.flags |= WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;// 焦点
        params.width = 1;//窗口的宽和高
        params.height = 1;
        params.x = 0;//窗口位置的偏移量
        params.y = 0;
        SurfaceView surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback2() {
            @Override
            public void surfaceRedrawNeeded(SurfaceHolder holder) {
                CocosService.this.surfaceRedrawNeeded(0, holder.getSurface());
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                CocosService.this.surfaceCreated(0, holder.getSurface());
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                CocosService.this.surfaceChanged(0, holder.getSurface(), format, width, height);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                CocosService.this.surfaceDestroyed(0, holder.getSurface());
            }
        });
        mWindowManager.addView(surfaceView, params);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return new CocosSurfaceManager(this).asBinder();
    }

    @Override
    public boolean onUnbind(Intent intent) {
        return super.onUnbind(intent);
    }

    private static String getAbsolutePath(File file) {
        return (file != null) ? file.getAbsolutePath() : null;
    }

    @Override
    public void onDestroy() {
        this.mDestroyed = true;
        this.onSurfaceDestroyedNative(this.mNativeHandle);
        this.unloadNativeCode(this.mNativeHandle);
        CocosHelper.unregisterBatteryLevelReceiver(this);
        CocosAudioFocusManager.unregisterAudioFocusListener(this);
        CanvasRenderingContext2DImpl.destroy();
    }


    @Override
    public void onRebind(Intent intent) {
        super.onRebind(intent);
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (!this.mDestroyed) {
            this.onTrimMemoryNative(this.mNativeHandle, level);
        }
    }

    public void surfaceCreated(int appId, Surface surface) {
        if (!this.mDestroyed) {
            GlobalObject.getHandler().post(new Runnable() {
                @Override
                public void run() {
                    if (appId==0){
                        CocosService.this.onSurfaceCreatedNative(CocosService.this.mNativeHandle, surface);
                    }else {
                        CocosRemoteSurfaceView surfaceView = new CocosRemoteSurfaceView(appId);
                        mSurfaceViewArray.put(appId, surfaceView);
                        surfaceView.surfaceCreated(surface);
                    }
                }
            });

        }

    }

    public void surfaceChanged(int appId, Surface surface, int format, int width, int height) {
        if (!this.mDestroyed) {
            GlobalObject.getHandler().post(new Runnable() {
                @Override
                public void run() {
                    if (appId==0){
                        CocosService.this.onSurfaceChangedNative(CocosService.this.mNativeHandle, surface, format, width, height);
                    }else {
                        CocosRemoteSurfaceView surfaceView = mSurfaceViewArray.get(appId);
                        surfaceView.surfaceChanged(surface, format, width, height);
                    }
                }
            });
        }

    }

    public void surfaceRedrawNeeded(int appId, Surface surface) {
        if (!this.mDestroyed) {
            GlobalObject.getHandler().post(new Runnable() {
                @Override
                public void run() {
                    if (appId==0){
                        CocosService.this.onSurfaceRedrawNeededNative(CocosService.this.mNativeHandle, surface);
                    }else {
                        CocosRemoteSurfaceView surfaceView = mSurfaceViewArray.get(appId);
                        surfaceView.surfaceRedrawNeeded(surface);
                    }
                }
            });
        }

    }

    public void surfaceDestroyed(int appId, Surface surface) {
        if (!this.mDestroyed) {
            GlobalObject.getHandler().post(new Runnable() {
                @Override
                public void run() {
                    if (appId==0){
                        CocosService.this.onSurfaceDestroyedNative(CocosService.this.mNativeHandle);
                    }else {
                        CocosRemoteSurfaceView surfaceView = mSurfaceViewArray.get(appId);
                        if (surfaceView!=null){
                            surfaceView.surfaceDestroyed(surface);
                            mSurfaceViewArray.remove(appId);
                        }
                    }
                }
            });
        }

    }

    public void onSizeChanged(int appId, int w, int h, int oldw, int oldh){
        if (!this.mDestroyed) {
            GlobalObject.getHandler().post(new Runnable() {
                @Override
                public void run() {
                    CocosRemoteSurfaceView surfaceView = mSurfaceViewArray.get(appId);
                    surfaceView.onSizeChanged(w, h, oldw, oldh);
                }
            });
        }
    }

    private void onLoadNativeLibraries() {
        try {
            System.loadLibrary(LIBNAME);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

//    @Override
//    public boolean onTouchEvent(MotionEvent event) {
//        return onTouchEventNative(mNativeHandle, event);
//    }
//
//    @Override
//    public boolean onGenericMotionEvent(MotionEvent event) {
//        return super.onGenericMotionEvent(event);
////    return onTouchEventNative(mNativeHandle, event);
//    }
//
//    @Override
//    public boolean onKeyUp(final int keyCode, KeyEvent event) {
//        return onKeyUpNative(mNativeHandle, event);
//    }
//
//    @Override
//    public boolean onKeyDown(final int keyCode, KeyEvent event) {
//        return onKeyDownNative(mNativeHandle, event);
//    }
}

