cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

# 프로젝트 이름과 C++ 표준 설정
project(ChatProgramQt LANGUAGES CXX)
#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1")


# Qt5 모듈 찾기
find_package(Qt5 COMPONENTS Core Gui Widgets Network Multimedia MultimediaWidgets REQUIRED)

# 소스 파일 및 헤더 파일 추가
set(SOURCES
    chatclient.cpp
    main.cpp
)

set(HEADERS
    chatclient.h
)

set(UI
    chatclient.ui
)

set(TRANSLATIONS
    ChatProgram_ko_KR.ts
)

# UI 파일 처리
qt5_wrap_ui(UIS_HEADERS ${UI})

# Meta-Object Compiler(MOC) 헤더 처리
qt5_wrap_cpp(MOC_HEADERS ${HEADERS})

# Qt 번역 파일 처리
# qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TRANSLATIONS})

# 실행 파일 생성
add_executable(ChatProgramQt ${SOURCES} ${MOC_HEADERS} ${UIS_HEADERS} ${QM_FILES})

# OpenCV 라이브러리 찾기
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Libtorch 라이브러리 설정
set(CMAKE_PREFIX_PATH "/usr/local/pytorch")
find_package(Torch REQUIRED)

# 컴파일 정의 추가 (cxx11 ABI 사용 설정)
add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)



# Qt5 라이브러리 링크
target_link_libraries(ChatProgramQt
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    ${OpenCV_LIBS}
    "${TORCH_LIBRARIES}"
)

# include 디렉터리 설정
target_include_directories(ChatProgramQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS} "${TORCH_INCLUDE_DIRS}")

# RPATH 설정
set_target_properties(ChatProgramQt PROPERTIES
    INSTALL_RPATH /usr/local/pytorch
)

# 설치 경로 설정 (이 부분은 QNX나 안드로이드 타겟일 때 추가적인 설정)
install(TARGETS ChatProgramQt DESTINATION /opt/ChatProgramQt/bin)
