/****************************************************************************
 Copyright (c) 2020-2023 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
****************************************************************************/

#include <jni.h>
#include "base/Macros.h"

#if CC_SURFACE_LESS_SERVICE
#include <android/asset_manager_jni.h>
#include "platform/interfaces/modules/ISystemWindowManager.h"
#include "platform/interfaces/modules/ISystemWindow.h"
#include "platform/java/modules/SystemWindowManager.h"
#else
#include "game-activity/native_app_glue/android_native_app_glue.h"
#endif

#include "platform/android/AndroidPlatform.h"
#include "platform/android/FileUtils-android.h"
#include "platform/java/jni/JniHelper.h"

extern int cocos_main(int argc, const char **argv); // NOLINT(readability-identifier-naming)

extern "C" {

#if CC_SURFACE_LESS_SERVICE
static void android_surface_less_main(int32_t width, int32_t height) {
    CC_LOG_INFO("android_surface_less_main: w=%d, h=%d", width, height);
    auto *platform = cc::BasePlatform::getPlatform();
    auto *androidPlatform = static_cast<cc::AndroidPlatform *>(platform);
    androidPlatform->init();

    CC_LOG_INFO("android_surface_less_main, 02");

    cc::ISystemWindowInfo info;
    info.width = width;
    info.height = height;
    info.externalHandle = nullptr;
    auto* window = androidPlatform->getInterface<cc::SystemWindowManager>()->createWindow(info);
    window->setViewSize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    CC_LOG_INFO("android_surface_less_main, 03");

    if (cocos_main(0, nullptr) != 0) {
        CC_LOG_ERROR("AndroidPlatform: Launch game failed!");
    }

    CC_LOG_INFO("android_surface_less_main, 04");
    // AndroidPlatform::run should be at the last since there is an infinite loop in it.
    androidPlatform->run(0, nullptr);
}
#else
void android_main(struct android_app *app) {
    auto *platform = cc::BasePlatform::getPlatform();
    auto *androidPlatform = static_cast<cc::AndroidPlatform *>(platform);
    androidPlatform->setAndroidApp(app);
    androidPlatform->init();
    androidPlatform->run(0, nullptr);
}
#endif

//NOLINTNEXTLINE
JNIEXPORT void JNICALL Java_com_cocos_lib_CocosActivity_onCreateNative(JNIEnv *env, jobject activity) {
    cc::JniHelper::init(env, activity);
}

JNIEXPORT jlong JNICALL Java_com_cocos_lib_CocosRemoteRenderService_nativeOnStartRender(JNIEnv *env, jobject service, jint clientId, jint width, jint height, jobject assetManager) {
    // TODO(cjh): Handle clientId parameter
    CC_LOG_INFO("nativeOnStartRender ...");
    cc::JniHelper::init(env, service);
    AAssetManager *aAssetManager = AAssetManager_fromJava(env, assetManager);
    cc::FileUtilsAndroid::setAssetManager(aAssetManager);

    auto *platform = cc::BasePlatform::getPlatform();
    auto *androidPlatform = static_cast<cc::AndroidPlatform *>(platform);
    androidPlatform->createGameThread([width, height](){
        android_surface_less_main(width, height);
    });
    return reinterpret_cast<jlong>(platform);
}

JNIEXPORT void JNICALL Java_com_cocos_lib_CocosRemoteRenderService_nativeOnStopRender(JNIEnv *env, jobject service, jlong handle, jint clientId) {
    // TODO(cjh): Handle clientId parameter
    CC_LOG_INFO("nativeOnStopRender ...");
    auto *androidPlatform = reinterpret_cast<cc::AndroidPlatform *>(handle);
    androidPlatform->requestExitGameThreadAndWait();
}

JNIEXPORT void JNICALL Java_com_cocos_lib_CocosRemoteRenderService_nativeOnRenderSizeChanged(JNIEnv *env, jobject service, jlong handle, jint clientId, jint width, jint height) {
    // TODO(cjh): Handle clientId parameter
    auto *androidPlatform = reinterpret_cast<cc::AndroidPlatform *>(handle);
    // TODO(cjh):
}

} // extern "C"
