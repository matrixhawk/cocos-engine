/****************************************************************************
 Copyright (c) 2017-2023 Xiamen Yaji Software Co., Ltd.

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

#pragma once

#include "base/Timer.h"
#include "platform/UniversalPlatform.h"

struct android_app;

namespace cc {
class GameInputProxy;

class CC_DLL AndroidPlatform : public UniversalPlatform {
public:
    AndroidPlatform() = default;

    ~AndroidPlatform() override;

    int init() override;

    void pollEvent() override;

    int32_t run(int argc, const char **argv) override;

    int getSdkVersion() const override;

    int32_t loop() override;

    void *getActivity();

    static void *getEnv();

    void onDestroy() override;

#if CC_SURFACE_LESS_SERVICE
    // Should be invoked in non-render thread.
    void createGameThread(const std::function<void()> &gameLoop);
    // Should be invoked in non-render thread.
    void requestExitGameThreadAndWait();
#else
    inline void setAndroidApp(android_app *app) {
        _app = app;
    }
#endif

    ISystemWindow *createNativeWindow(uint32_t windowId, void *externalHandle) override;

private:
#if CC_SURFACE_LESS_SERVICE
    std::thread *_gameThread{nullptr};
    bool _isDestroyRequested{false};
#else
    bool _isLowFrequencyLoopEnabled{false};
    utils::Timer _lowFrequencyTimer;
    int _loopTimeOut{-1};
    GameInputProxy *_inputProxy{nullptr};
    android_app *_app{nullptr};
    friend class GameInputProxy;
#endif
};
} // namespace cc
