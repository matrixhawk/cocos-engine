// ISurfaceViewManager.aidl
package com.cocos.lib;

import android.view.Surface;
import android.view.MotionEvent;
// Declare any non-default types here with import statements

interface ISurfaceViewManager {
        void onSizeChanged(int appId, int w, int h, int oldw, int oldh);
        // SurfaceView onCreate时调用
        void surfaceCreated(int appId, in Surface surface);
        // SurfaceView onChange时调用
        void surfaceChanged(int appId, in Surface surface,int format, int width, int height);
        // SurfaceView onDestroy时调用
        void surfaceDestroyed(int appId, in Surface surface);
        void surfaceRedrawNeeded(int appId, in Surface surface);
        // 发送SurfaceView的Touch事件
        void sendTouchEvent(int appId, in MotionEvent event);


}
