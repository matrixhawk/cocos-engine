package com.cocos.lib;

import android.content.Context;
import android.os.RemoteException;
import android.view.MotionEvent;
import android.view.Surface;

public class CocosSurfaceManager extends ISurfaceViewManager.Stub {
    private CocosService mContext;
    public CocosSurfaceManager(CocosService context){
        this.mContext = context;
    }


    @Override
    public void onSizeChanged(int appId, int w, int h, int oldw, int oldh) throws RemoteException {
        if (mContext!=null){
            mContext.onSizeChanged(appId, w, h, oldw, oldw);
        }
    }

    @Override
    public void surfaceCreated(int appId, Surface surface) throws RemoteException {
        if (mContext!=null){
            mContext.surfaceCreated(appId, surface);
        }
    }

    @Override
    public void surfaceChanged(int appId, Surface surface,int format, int width, int height) throws RemoteException {
        if (mContext!=null){
            mContext.surfaceChanged(appId, surface, format, width, height);
        }
    }

    @Override
    public void surfaceDestroyed(int appId, Surface surface) throws RemoteException {
        if (mContext!=null){
            mContext.surfaceDestroyed(appId, surface);
        }
    }

    @Override
    public void surfaceRedrawNeeded(int appId, Surface surface) throws RemoteException {
        if (mContext!=null){
            mContext.surfaceRedrawNeeded(appId, surface);
        }
    }

    @Override
    public void sendTouchEvent(int appId, MotionEvent event) throws RemoteException {

    }

}
