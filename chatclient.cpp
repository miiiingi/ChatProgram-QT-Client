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
	cv::imwrite("frame.png", frame);
	cv::Mat modelInput = frame.clone();
	cv::imwrite("modelInput.png", modelInput);
	float x_factor = modelInput.cols / 640;
	float y_factor = modelInput.rows / 480;

	/*
	cv::Mat blob;
	cv::dnn::blobFromImage(modelInput, blob, 1.0/255.0, modelShape, cv::Scalar(), true, false);


	float x_factor = blob.cols / 640;
	float y_factor = blob.rows / 480;
	qDebug() << "blob cols:" << blob.cols; //640
	qDebug() << "blob rows:" << blob.rows; //480
	*/

        std::vector<float> inputTensorValues(modelInput.rows * modelInput.cols * 3);
        for (int y = 0; y < modelInput.rows; ++y) {
            for (int x = 0; x < modelInput.cols; ++x) {
                for (int c = 0; c < 3; ++c) {
                    inputTensorValues[(c * modelInput.rows + y) * modelInput.cols + x] =
                        modelInput.at<cv::Vec3b>(y, x)[c] / 255.0f;
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
	    float *classes_scores = outputData + 4;
	    cv::Mat scores(1, classes.size(), CV_32FC1, classes_scores);
	    cv::Point class_id;
            double maxClassScore;
            minMaxLoc(scores, 0, &maxClassScore, 0, &class_id);


	    if (maxClassScore > 0.7)
	    {
		confidences.push_back(maxClassScore);
		classIds.push_back(class_id.x);

		// YOLOv8의 출력 형식에 맞게 좌표 변환
		float x = outputData[0];     // 중심 x
		float y = outputData[1];     // 중심 y
		float w = outputData[2];     // 너비
		float h = outputData[3];     // 높이

		// 모델의 출력을 이미지 좌표계로 변환
		int left = int((x - w/2.0) * x_factor);
		int top = int((y - h/2.0) * y_factor);
		int width = int(w * x_factor);
		int height = int(h * y_factor);

		// 경계 좌표 클리핑 (이미지 경계를 벗어나지 않도록)
		left = std::max(0, left);
		top = std::max(0, top);
		width = std::min(width, modelInput.cols - left);
		height = std::min(height, modelInput.rows - top);

		boxes.push_back(cv::Rect(left, top, width, height));

		qDebug() << "x:" << x;
		qDebug() << "y:" << y;
		qDebug() << "w:" << w;
		qDebug() << "h:" << h;
		qDebug() << "left:" << left;
		qDebug() << "top:" << top;
		qDebug() << "width:" << width;
		qDebug() << "height:" << height;
	    }

	    std::vector<int> nms_result;
	    cv::dnn::NMSBoxes(boxes, confidences, 0.7, 0.7, nms_result);

	    for (unsigned long i = 0; i < nms_result.size(); ++i)
	    {
		int idx = nms_result[i];
		int class_id = classIds[idx];
		float confidence = confidences[idx];

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dis(100, 255);
		cv::Scalar color = cv::Scalar(dis(gen),
					  dis(gen),
					  dis(gen));

		std::string className = classes[class_id];
		cv::Rect box = boxes[idx];

                cv::rectangle(frame, box, color, 2);
                cv::putText(frame, className, cv::Point(box.x, box.y - 10), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
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
