修改 native/external/sources/CMakeLists.txt 文件，添加 SURFACE_LESS_SERVICE 的判断

```cmake
if(ANDROID AND (NOT SURFACE_LESS_SERVICE))
    set(CC_GAME_ACTIVITY_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/android-gamesdk/GameActivity/game-activity/include/game-activity/GameActivity.cpp
        ${CMAKE_CURRENT_LIST_DIR}/android-gamesdk/GameActivity/game-activity/include/game-activity/native_app_glue/android_native_app_glue.c
        ${CMAKE_CURRENT_LIST_DIR}/android-gamesdk/GameActivity/game-activity/include/game-text-input/gametextinput.cpp
    )

```

