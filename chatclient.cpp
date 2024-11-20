#include "chatclient.h"
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
    // UI Elements
    /*
    classNames = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
        "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
        "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
        "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
        "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
        "hair drier", "toothbrush"
    };

    // 클래스 색상 벡터 (랜덤 색상 생성)
    for (size_t i = 0; i < classNames.size(); ++i) {
        classColors.push_back(cv::Scalar(rand() % 256, rand() % 256, rand() % 256));
    }
    */

    // Layouts
    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    streamButton = new QPushButton("Stream", this);
    modelInferenceButton = new QPushButton("Model Inference", this);
    modelInferenceButton->setEnabled(false);
    videoWidget = new QLabel(this);

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
        cv::Mat inputFrame;
        cv::cvtColor(frame, inputFrame, cv::COLOR_BGR2RGB);
        cv::resize(inputFrame, inputFrame, cv::Size(640, 640));

        std::vector<float> inputTensorValues(inputFrame.rows * inputFrame.cols * 3);
        for (int y = 0; y < inputFrame.rows; ++y) {
            for (int x = 0; x < inputFrame.cols; ++x) {
                for (int c = 0; c < 3; ++c) {
                    inputTensorValues[(c * inputFrame.rows + y) * inputFrame.cols + x] =
                        inputFrame.at<cv::Vec3b>(y, x)[c] / 255.0f;
                }
            }
        }

        try {
            auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            std::array<int64_t, 4> inputShape = {1, 3, 640, 640};

            Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
                memory_info, inputTensorValues.data(), inputTensorValues.size(),
                inputShape.data(), inputShape.size()
            );

            Ort::AllocatorWithDefaultOptions allocator;
            Ort::AllocatedStringPtr inputName = model->GetInputNameAllocated(0, allocator);
            Ort::AllocatedStringPtr outputName = model->GetOutputNameAllocated(0, allocator);

            const char* inputNames[] = {inputName.get()};
            const char* outputNames[] = {outputName.get()};

            std::vector<Ort::Value> outputTensor = model->Run(
                Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1
            );

            float* outputData = outputTensor[0].GetTensorMutableData<float>();
            size_t outputSize = outputTensor[0].GetTensorTypeAndShapeInfo().GetElementCount();

            std::vector<cv::Rect> boxes;
            std::vector<float> confidences;
            std::vector<int> classIds;
            qDebug() << "outputSize:" << outputSize;
            for (size_t i = 0; i < outputSize; i += 6) {
                float x1 = outputData[i] * frame.cols;
                float y1 = outputData[i + 1] * frame.rows;
                float x2 = outputData[i + 2] * frame.cols;
                float y2 = outputData[i + 3] * frame.rows;
                float confidence = outputData[i + 4];
                int classId = static_cast<int>(outputData[i + 5]);

                if (confidence > 0.5) {
                    boxes.emplace_back(cv::Rect(cv::Point(x1, y1), cv::Point(x2, y2)));
                    confidences.push_back(confidence);
                    classIds.push_back(classId);
                }
            }

	    std::vector<int> indices;
	    if (!boxes.empty()) {
		cv::dnn::NMSBoxes(
		    boxes,                  // bboxes
		    confidences,           // scores
		    0.5f,                  // score_threshold
		    0.4f,                  // nms_threshold
		    indices                // indices
		    // eta와 top_k는 기본값 사용
		);
	    }

            for (int idx : indices) {
                const cv::Rect& box = boxes[idx];
                int classId = classIds[idx];
                float confidence = confidences[idx];

                cv::rectangle(frame, box, (0, 255, 0), 2);
                // cv::rectangle(frame, box, classColors[classId], 2);
                // std::string label = classNames[classId] + ": " + std::to_string(confidence);
                // cv::putText(frame, label, cv::Point(box.x, box.y - 10), cv::FONT_HERSHEY_SIMPLEX, 0.5, classColors[classId], 1);
                cv::putText(frame, "label", cv::Point(box.x, box.y - 10), cv::FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1);
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
            env, "/home/mingi/workspace/ChatProgram-QT/Client/yolov8n.onnx", session_options
        );
        qDebug() << "ONNX 모델 로드 성공!";
    } catch (const Ort::Exception &e) {
        qDebug() << "ONNX 모델 로드 실패: " << e.what();
    }
}
