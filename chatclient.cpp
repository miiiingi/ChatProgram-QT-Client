#include "chatclient.h"
#include <random>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCloseEvent>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <onnxruntime/core/providers/cuda/cuda_provider_factory.h>
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

ChatClient::ChatClient(QWidget *parent) : QWidget(parent) {
    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    streamButton = new QPushButton("Stream", this);
    modelInferenceButton = new QPushButton("Model Inference", this);
    modelInferenceButton->setEnabled(false);
    videoWidget = new QLabel(this);

    videoWidget->setFixedSize(480, 640);  // 초기 크기를 640x640으로 설정
    videoWidget->setAlignment(Qt::AlignCenter); // 텍스트나 이미지 정렬

    QVBoxLayout *videoLayout = new QVBoxLayout();
    videoLayout->addWidget(videoWidget);

    QHBoxLayout *videoButtonLayout = new QHBoxLayout();
    videoButtonLayout->addWidget(streamButton);
    videoButtonLayout->addWidget(modelInferenceButton);
    videoLayout->addLayout(videoButtonLayout);

    mainLayout->addLayout(videoLayout);
    setLayout(mainLayout);

    // Connections
    connect(streamButton, &QPushButton::clicked, this, &ChatClient::streamVideoToServer);
    // Initialize timer for video processing
    frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, &ChatClient::runModelInference);  // 매 프레임 처리

    // Model Loading
    loadModel();
}

void ChatClient::streamVideoToServer() {
    if (!isWebcamStreaming) {
        if (!webCam.open(0, cv::CAP_V4L2)) {
            QMessageBox::warning(this, "Error", "Failed to open webcam");
            return;
        }
        isWebcamStreaming = true;
        streamButton->setText("Stop Stream");
        modelInferenceButton->setEnabled(true);
        frameTimer->start(16);
    } else {
        isWebcamStreaming = false;
        streamButton->setText("Stream");
        modelInferenceButton->setEnabled(false);
        frameTimer->stop();
        webCam.release();
    }
}

void ChatClient::runModelInference() {
    if (!isWebcamStreaming) return;
    cv::Mat frame;
    if (webCam.read(frame)) {
	cv::Mat modelInput;
	cv::cvtColor(frame, modelInput, cv::COLOR_BGR2RGB);
	cv::resize(modelInput, modelInput, cv::Size(640, 480));
	cv::imwrite("modelInput.png", modelInput);

        cv::Scalar image_mean = cv::Scalar(127, 127, 127);
        modelInput.convertTo(modelInput, CV_32F);
        modelInput = (modelInput - image_mean) / 128.0;

        // 채널 순서 변경 (HWC -> CHW)
        std::vector<float> inputTensorValues(modelInput.rows * modelInput.cols * 3);
        for (int c = 0; c < 3; ++c) {
            for (int y = 0; y < modelInput.rows; ++y) {
                for (int x = 0; x < modelInput.cols; ++x) {
                    inputTensorValues[(c * modelInput.rows + y) * modelInput.cols + x] = 
                        modelInput.at<cv::Vec3f>(y, x)[c];
                }
            }
        }

        try {
            auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            std::array<int64_t, 4> inputShape = {1, 3, modelInput.rows, modelInput.cols};

            Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
                memory_info, inputTensorValues.data(), inputTensorValues.size(),
                inputShape.data(), inputShape.size()
            );

            Ort::AllocatorWithDefaultOptions allocator;
            Ort::AllocatedStringPtr inputName = model->GetInputNameAllocated(0, allocator);
            const char* inputNames[] = {inputName.get()};

            Ort::AllocatedStringPtr outputName1 = model->GetOutputNameAllocated(0, allocator);
            Ort::AllocatedStringPtr outputName2 = model->GetOutputNameAllocated(1, allocator);

            const char* outputNames[] = {outputName1.get(), outputName2.get()};

	    // Run model inference
	    std::vector<Ort::Value> outputTensor = model->Run(
		Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 2
	    );

	    float* confidences = outputTensor[0].GetTensorMutableData<float>();
	    float* boxes = outputTensor[1].GetTensorMutableData<float>();

	    Ort::TensorTypeAndShapeInfo confidenceShape = outputTensor[0].GetTensorTypeAndShapeInfo();
	    Ort::TensorTypeAndShapeInfo boxShape = outputTensor[1].GetTensorTypeAndShapeInfo();

	    std::vector<int64_t> confidenceDims = confidenceShape.GetShape();
	    std::vector<int64_t> boxDims = boxShape.GetShape();

	    // 메인 코드에서 getBoxes 호출
	    std::vector<cv::Rect> boxesOut = getBoxes(boxes, boxDims, confidences, 0.99, 0.99); // confThreshold=0.5, nmsThreshold=0.4
	    for (const auto& box : boxesOut) {
		// 랜덤한 색상 생성
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dis(100, 255);
		cv::Scalar color = cv::Scalar(dis(gen), dis(gen), dis(gen));

		// 박스 그리기

		cv::rectangle(frame, box, color, 2);

		// 라벨 추가 (여기서는 예제라 "label" 사용)
		cv::putText(frame, "label", cv::Point(box.x, box.y - 10),
			    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
	    }

            cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
            QImage qimg(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
            videoWidget->setPixmap(QPixmap::fromImage(qimg).scaled(
                videoWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } catch (const Ort::Exception& e) {
            qDebug() << "Model inference failed:" << e.what();
        }
    }
}

void ChatClient::loadModel() {
    try {
        // ONNX Runtime 환경 초기화
        static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "ONNXRuntimeTest");  // Env를 static으로 유지
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
        session_options.DisableMemPattern();
        session_options.SetExecutionMode(ORT_SEQUENTIAL); // 순차 실행 설정

        // CUDA 프로바이더 설정
        OrtCUDAProviderOptions cuda_options;
        cuda_options.device_id = 0;                     // GPU ID
        cuda_options.gpu_mem_limit = SIZE_MAX;          // 메모리 제한 해제
        cuda_options.arena_extend_strategy = 0;         // 기본 값
        cuda_options.do_copy_in_default_stream = 0;     // 기본 스트림 사용
        cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearch::OrtCudnnConvAlgoSearchHeuristic;

        // CUDA 프로바이더 추가
        session_options.AppendExecutionProvider_CUDA(cuda_options);
        qDebug() << "CUDA Provider initialized successfully.";

        // ONNX 모델 로드
        model = std::make_unique<Ort::Session>(
            env, "/home/mingi/workspace/ChatProgram-QT/Client/version-RFB-640-18.onnx", session_options
        );
        qDebug() << "ONNX 모델 로드 성공!";
    } catch (const Ort::Exception &e) {
        qDebug() << "ONNX 모델 로드 실패: " << e.what();
    }
}

// NMS를 추가한 getBoxes 함수
std::vector<cv::Rect> ChatClient::getBoxes(float* boxes, const std::vector<int64_t>& boxDims, float* confidences, float confThreshold, float nmsThreshold) {
    int numDetections = static_cast<int>(boxDims[1]); // 검출된 박스의 수
    int boxInfoCount = static_cast<int>(boxDims[2]);  // 박스 정보 (일반적으로 4)
    std::vector<cv::Rect> boxesList;
    std::vector<float> scores; // 신뢰도 저장

    // 박스와 신뢰도 추출
    for (int i = 0; i < numDetections; ++i) {
        float confidence = confidences[i];
	std::cout << "confidence: " << confidence;
        if (confidence > confThreshold) {
            // 박스 좌표 계산
            std::vector<float> boxCoords;
            for (int j = 0; j < boxInfoCount; ++j) {
                boxCoords.push_back(boxes[i * boxInfoCount + j]);
            }
            cv::Rect rect(
                cv::Point(boxCoords[0] * 640, boxCoords[1] * 480),
                cv::Point(boxCoords[2] * 640, boxCoords[3] * 480)
            );
            boxesList.push_back(rect);
            scores.push_back(confidence);
        }
    }

    // NMS 적용
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxesList, scores, confThreshold, nmsThreshold, indices);

    // NMS 이후의 박스 반환
    std::vector<cv::Rect> nmsBoxes;
    for (int idx : indices) {
        nmsBoxes.push_back(boxesList[idx]);
    }

    return nmsBoxes;
}
