cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
# 프로젝트 이름과 C++ 표준 설정
project(ChatProgramQt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O1")
# Qt5 모듈 찾기
find_package(Qt5 COMPONENTS Core Gui Widgets Network Multimedia MultimediaWidgets REQUIRED)
# GSL 패키지 찾기
find_package(GSL REQUIRED)
# Boost 패키지 찾기
find_package(Boost REQUIRED)
find_package(nsync REQUIRED)
# Abseil 패키지 찾기 추가
find_package(absl REQUIRED)
# 소스 파일 및 헤더 파일 추가

find_package(OpenCV 4.5.4 REQUIRED COMPONENTS
	core
	highgui
	imgproc
	dnn
)
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
# ONNX Runtime 설치 경로
set(ONNX_RUNTIME_ROOT "/usr/local/onnxruntime")
set(ONNX_RUNTIME_INCLUDE_DIR "${ONNX_RUNTIME_ROOT}/include")
set(ONNX_RUNTIME_CONFIG_PATH "${ONNX_RUNTIME_ROOT}/build_new/Release/_deps/onnx-src/onnx")
set(ONNX_RUNTIME_BUILD_DIR "${ONNX_RUNTIME_ROOT}/build_new/Release/_deps/onnx-build")
set(ONNX_RUNTIME_LIB_DIR "${ONNX_RUNTIME_ROOT}/build_new/Release")
set(ONNX_RUNTIME_GOOGLE_PROTOC_DIR "${ONNX_RUNTIME_ROOT}/build_new/Release/_deps/protobuf-src/src")
# CUDA 설치 경로
set(CUDA_INCLUDE_DIR "/usr/local/cuda/include")
set(CUDA_LIB_DIR "/usr/local/cuda/lib64")
# TensorRT 설치 경로
set(TENSORRT_INCLUDE_DIR "/usr/include")
set(TENSORRT_LIB_DIR "/usr/lib/aarch64-linux-gnu")
# 추가적인 ONNX Runtime 헤더 경로
set(ONNX_RUNTIME_ADDITIONAL_INCLUDE_DIR "${ONNX_RUNTIME_ROOT}/onnxruntime")
set(MS_GSL_INCLUDE_DIR "/usr/local/include")  # Microsoft GSL이 설치된 경로
# UI 파일 처리
qt5_wrap_ui(UIS_HEADERS ${UI})
# Meta-Object Compiler(MOC) 헤더 처리
qt5_wrap_cpp(MOC_HEADERS ${HEADERS})
# Qt 번역 파일 처리
# qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TRANSLATIONS})
# 실행 파일 생성
add_executable(ChatProgramQt ${SOURCES} ${MOC_HEADERS} ${UIS_HEADERS} ${QM_FILES})
# include 디렉터리 설정을 정리
target_include_directories(ChatProgramQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    ${ONNX_RUNTIME_ROOT}
    ${ONNX_RUNTIME_LIB_DIR}
    ${ONNX_RUNTIME_GOOGLE_PROTOC_DIR}
    ${ONNX_RUNTIME_BUILD_DIR}
    ${ONNX_RUNTIME_CONFIG_PATH}
    ${ONNX_RUNTIME_INCLUDE_DIR}
    ${ONNX_RUNTIME_INCLUDE_DIR}/onnxruntime
    ${ONNX_RUNTIME_ADDITIONAL_INCLUDE_DIR}
    ${ONNX_RUNTIME_INCLUDE_DIR}/onnxruntime/core/session
    ${CUDA_INCLUDE_DIR}
    ${TENSORRT_INCLUDE_DIR}
    ${ONNX_RUNTIME_INCLUDE_DIR}/onnxruntime/include
    ${ONNX_RUNTIME_INCLUDE_DIR}/include
    ${MS_GSL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)
# Qt5 라이브러리 링크
target_link_libraries(ChatProgramQt
    ${OpenCV_LIBS}
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    onnxruntime
    cuda
    nvinfer
    GSL::gsl  # GSL 라이브러리 링크
    absl::inlined_vector  # Abseil 라이브러리 추가
    protobuf
)
# include 디렉터리 설정
target_include_directories(ChatProgramQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS} "${TORCH_INCLUDE_DIRS}")
