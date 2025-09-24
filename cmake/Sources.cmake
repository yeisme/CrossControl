# Source Files
set(TS_FILES CrossControl_yue_CN.ts)

set(PROJECT_SOURCES
    src/main.cpp
    src/widgets/crosscontrolwidget.cpp
    include/widgets/crosscontrolwidget.h
    src/widgets/crosscontrolwidget.ui
    src/widgets/loginwidget.cpp
    include/widgets/loginwidget.h
    src/widgets/loginwidget.ui
    src/widgets/mainwidget.cpp
    include/widgets/mainwidget.h
    src/widgets/mainwidget.ui
    src/widgets/monitorwidget.cpp
    include/widgets/monitorwidget.h
    src/widgets/monitorwidget.ui
    src/widgets/visitrecordwidget.cpp
    include/widgets/visitrecordwidget.h
    src/widgets/visitrecordwidget.ui
    src/widgets/messagewidget.cpp
    include/widgets/messagewidget.h
    src/widgets/messagewidget.ui
    src/widgets/settingwidget.cpp
    include/widgets/settingwidget.h
    src/widgets/settingwidget.ui
    src/widgets/logwidget.cpp
    include/widgets/logwidget.h
    src/widgets/logwidget.ui
    src/widgets/unlockwidget.cpp
    include/widgets/unlockwidget.h
    src/widgets/unlockwidget.ui
    src/widgets/weatherwidget.cpp
    include/widgets/weatherwidget.h
    src/widgets/weatherwidget.ui
    ${TS_FILES})

# Module source files (for extensibility)
if(BUILD_HUMAN_RECOGNITION)
    set(HUMAN_RECOGNITION_SOURCES
        src/modules/HumanRecognition/humanrecognition.cpp
        include/modules/HumanRecognition/humanrecognition.h
    )
endif()

if(BUILD_AUDIO_VIDEO)
    set(AUDIO_VIDEO_SOURCES
        src/modules/AudioVideo/audiovideo.cpp
        include/modules/AudioVideo/audiovideo.h
    )
endif()
