# Source Files
set(TS_FILES src/i18n/CrossControl_zh_CN.ts)

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
  resources/icons.qrc
    include/logging/logging.h
    ${TS_FILES})

# Module source files (for extensibility)
if(BUILD_HUMAN_RECOGNITION)
  set(HUMAN_RECOGNITION_SOURCES
      src/modules/HumanRecognition/humanrecognition.cpp
    src/modules/HumanRecognition/opencv_backend.cpp
    include/modules/HumanRecognition/humanrecognition.h
    include/modules/HumanRecognition/iface_human_recognition.h
    include/modules/HumanRecognition/opencv_backend.h)
endif()

if(BUILD_AUDIO_VIDEO)
  set(AUDIO_VIDEO_SOURCES src/modules/AudioVideo/audiovideo.cpp
                          include/modules/AudioVideo/audiovideo.h)
endif()

set(STORAGE_SOURCES
  src/modules/Storage/storage.cpp
  include/modules/Storage/storage.h
  src/modules/Storage/dbmanager.cpp
  include/modules/Storage/dbmanager.h)

set(CONFIG_SOURCES src/modules/Config/config.cpp
                   include/modules/Config/config.h)
