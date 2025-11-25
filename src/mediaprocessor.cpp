#include "mediaprocessor.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDateTime>

MediaProcessor::MediaProcessor(QObject *parent)
    : QObject(parent)
    , mediaType_(NoMedia)
    , displayMode_(OriginalMode)
    , confidenceThreshold_(0.5)
    , nmsThreshold_(0.4)
    , roiWidth_(640)
    , roiHeight_(480)
    , playbackSpeed_(1.0)
    , totalFrames_(0)
    , currentFrame_(0)
    , fps_(0)
    , isPlaying_(false)
{
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MediaProcessor::processNextFrame);
}

MediaProcessor::~MediaProcessor()
{
    closeMedia();
}

bool MediaProcessor::loadDetectionModel(const QString& modelPath)
{
    try {
        QString actualXmlPath;
        QString actualBinPath;

        // Lambda: 提取文件到可写路径
        auto extractFile = [](const QString& src, const QString& dst) -> bool {
            if (QFile::exists(dst)) QFile::remove(dst);
            QFile fileSrc(src);
            if (!fileSrc.open(QIODevice::ReadOnly)) return false;
            QFile fileDst(dst);
            if (!fileDst.open(QIODevice::WriteOnly)) return false;
            fileDst.write(fileSrc.readAll());
            fileDst.close();
            fileSrc.close();
            QFile::setPermissions(dst,
                QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
            return true;
        };

        // 判断是否是资源路径 (AppImage 内部)
        if (modelPath.startsWith(":/") || modelPath.startsWith("qrc:/")) {
            // 尝试多个可写目录
            QString modelDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/model";
            if (!QDir().mkpath(modelDir)) {
                modelDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/model";
                if (!QDir().mkpath(modelDir)) {
                    modelDir = QDir::tempPath() + "/VideoDetection/model";
                    if (!QDir().mkpath(modelDir)) {
                        qDebug() << "无法创建可写模型目录";
                        emit statusMessage(tr("无法创建模型目录"));
                        return false;
                    }
                }
            }

            // 构建目标文件路径
            actualXmlPath = modelDir + "/buff_model.xml";
            actualBinPath = modelDir + "/buff_model.bin";

            QString xmlRes = modelPath;
            QString binRes = modelPath;
            binRes.replace(".xml", ".bin");

            qDebug() << "准备提取模型到:" << modelDir;
            qDebug() << "XML 资源:" << xmlRes << "BIN 资源:" << binRes;

            if (!extractFile(xmlRes, actualXmlPath)) {
                emit statusMessage(tr("无法写入模型文件 .xml"));
                return false;
            }
            if (!extractFile(binRes, actualBinPath)) {
                emit statusMessage(tr("无法写入模型文件 .bin"));
                return false;
            }

            qDebug() << "模型提取完成";
            qDebug() << "XML 大小:" << QFileInfo(actualXmlPath).size();
            qDebug() << "BIN 大小:" << QFileInfo(actualBinPath).size();

        } else {
            // 外部路径，直接使用
            actualXmlPath = modelPath;
            actualBinPath = modelPath;
            actualBinPath.replace(".xml", ".bin");

            qDebug() << "使用外部模型路径:" << actualXmlPath;
        }

        // 验证文件存在
        if (!QFile::exists(actualXmlPath)) {
            qDebug() << "XML 文件不存在:" << actualXmlPath;
            emit statusMessage(tr("模型文件不存在: %1").arg(actualXmlPath));
            return false;
        }
        if (!QFile::exists(actualBinPath)) {
            qDebug() << "BIN 文件不存在:" << actualBinPath;
            emit statusMessage(tr("模型权重文件不存在: %1").arg(actualBinPath));
            return false;
        }

        // 加载 OpenVINO 模型
        qDebug() << "开始加载 OpenVINO 模型:" << actualXmlPath;
        detector_ = std::make_unique<rm_buff::Detector>(actualXmlPath.toStdString());
        detector_->setConfThreshold(confidenceThreshold_);
        detector_->setNMSThreshold(nmsThreshold_);
        qDebug() << "模型加载成功";
        emit statusMessage(tr("模型加载成功: %1").arg(actualXmlPath));

        return true;

    } catch (const std::exception& e) {
        qDebug() << "模型加载异常:" << e.what();
        emit statusMessage(tr("模型加载失败: %1").arg(e.what()));
        return false;
    }
}


bool MediaProcessor::loadImage(const QString& filePath)
{
    closeMedia();

    currentImage_ = cv::imread(filePath.toStdString());

    if (currentImage_.empty()) {
        emit statusMessage(tr("无法加载图片: %1").arg(filePath));
        return false;
    }

    mediaType_ = ImageType;
    currentFilePath_ = filePath;
    mediaSize_ = QSize(currentImage_.cols, currentImage_.rows);

    QFileInfo fileInfo(filePath);
    qint64 fileSize = fileInfo.size();
    QString sizeStr = fileSize > 1024 * 1024
        ? QString("%1 MB").arg(fileSize / (1024.0 * 1024.0), 0, 'f', 2)
        : QString("%1 KB").arg(fileSize / 1024.0, 0, 'f', 2);

    emit mediaInfoChanged("图片", mediaSize_, sizeStr);
    emit statusMessage(tr("已加载图片: %1 (%2x%3, %4)")
                      .arg(fileInfo.fileName())
                      .arg(mediaSize_.width())
                      .arg(mediaSize_.height())
                      .arg(sizeStr));

    processCurrentImage();

    return true;
}

bool MediaProcessor::loadVideo(const QString& filePath)
{
    closeMedia();

    videoCapture_.open(filePath.toStdString());

    if (!videoCapture_.isOpened()) {
        emit statusMessage(tr("无法打开视频文件: %1").arg(filePath));
        return false;
    }

    mediaType_ = VideoType;
    currentFilePath_ = filePath;

    totalFrames_ = static_cast<int>(videoCapture_.get(cv::CAP_PROP_FRAME_COUNT));
    fps_ = videoCapture_.get(cv::CAP_PROP_FPS);
    int width = static_cast<int>(videoCapture_.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(videoCapture_.get(cv::CAP_PROP_FRAME_HEIGHT));
    mediaSize_ = QSize(width, height);
    currentFrame_ = 0;

    emit frameNumberChanged(0, totalFrames_);
    emit fpsChanged(fps_);

    QFileInfo fileInfo(filePath);
    QString info = QString("%1 帧, %2 FPS")
                      .arg(totalFrames_)
                      .arg(static_cast<int>(fps_));

    emit mediaInfoChanged("视频", mediaSize_, info);
    emit statusMessage(tr("已加载视频: %1").arg(fileInfo.fileName()));

    processNextFrame();

    return true;
}

void MediaProcessor::closeMedia()
{
    stop();

    if (videoCapture_.isOpened()) {
        videoCapture_.release();
    }

    currentImage_ = cv::Mat();
    mediaType_ = NoMedia;
    currentFilePath_.clear();
}

void MediaProcessor::play()
{
    if (mediaType_ != VideoType || !videoCapture_.isOpened()) {
        emit statusMessage(tr("没有加载视频"));
        return;
    }

    isPlaying_ = true;
    int interval = static_cast<int>(1000.0 / (fps_ * playbackSpeed_));
    timer_->start(interval);
    emit statusMessage(tr("播放中..."));
}

void MediaProcessor::pause()
{
    isPlaying_ = false;
    timer_->stop();
    emit statusMessage(tr("已暂停"));
}

void MediaProcessor::stop()
{
    isPlaying_ = false;
    timer_->stop();

    if (mediaType_ == VideoType && videoCapture_.isOpened()) {
        videoCapture_.set(cv::CAP_PROP_POS_FRAMES, 0);
        currentFrame_ = 0;
        processNextFrame();
    }

    emit statusMessage(tr("已停止"));
}

void MediaProcessor::seekToFrame(int frameNumber)
{
    if (mediaType_ != VideoType || !videoCapture_.isOpened()) return;

    frameNumber = qBound(0, frameNumber, totalFrames_ - 1);
    videoCapture_.set(cv::CAP_PROP_POS_FRAMES, frameNumber);
    currentFrame_ = frameNumber;

    if (!isPlaying_) {
        processNextFrame();
    }
}

void MediaProcessor::setDisplayMode(DisplayMode mode)
{
    displayMode_ = mode;

    if (mediaType_ == ImageType) {
        processCurrentImage();
    }
}

void MediaProcessor::setConfidenceThreshold(double threshold)
{
    confidenceThreshold_ = threshold;
    if (detector_) {
        detector_->setConfThreshold(threshold);
    }

    if (mediaType_ == ImageType && displayMode_ == DetectionMode) {
        processCurrentImage();
    }
}

void MediaProcessor::setNMSThreshold(double threshold)
{
    nmsThreshold_ = threshold;
    if (detector_) {
        detector_->setNMSThreshold(threshold);
    }

    if (mediaType_ == ImageType && displayMode_ == DetectionMode) {
        processCurrentImage();
    }
}

void MediaProcessor::setROISize(int width, int height)
{
    roiWidth_ = width;
    roiHeight_ = height;

    if (mediaType_ == ImageType && displayMode_ == ROIMode) {
        processCurrentImage();
    }
}

void MediaProcessor::setPlaybackSpeed(double speed)
{
    playbackSpeed_ = speed;

    if (isPlaying_) {
        int interval = static_cast<int>(1000.0 / (fps_ * playbackSpeed_));
        timer_->setInterval(interval);
    }
}

void MediaProcessor::processCurrentImage()
{
    if (mediaType_ != ImageType || currentImage_.empty()) return;

    cv::Mat processed = processFrame(currentImage_);
    QImage qImage = matToQImage(processed);
    lastProcessedImage_ = qImage;
    emit frameReady(qImage);
}

void MediaProcessor::processNextFrame()
{
    if (mediaType_ != VideoType || !videoCapture_.isOpened()) return;

    cv::Mat frame;
    videoCapture_ >> frame;

    if (frame.empty()) {
        stop();
        emit statusMessage(tr("视频播放完毕"));
        return;
    }

    currentFrame_ = static_cast<int>(videoCapture_.get(cv::CAP_PROP_POS_FRAMES));
    emit frameNumberChanged(currentFrame_, totalFrames_);

    cv::Mat processed = processFrame(frame);
    QImage qImage = matToQImage(processed);
    lastProcessedImage_ = qImage;
    emit frameReady(qImage);
}

cv::Mat MediaProcessor::processFrame(const cv::Mat& frame)
{
    cv::Mat result;

    switch (displayMode_) {
        case OriginalMode:
            result = frame.clone();
            break;

        case DetectionMode:
            result = detectObjects(frame);
            break;

        case BinaryMode:
            result = applyBinary(frame);
            break;

        case ROIMode:
            result = extractROI(frame);
            break;
    }

    return result;
}

cv::Mat MediaProcessor::detectObjects(const cv::Mat& frame)
{
    cv::Mat result = frame.clone();

    if (!detector_) {
        cv::putText(result, "模型未能正确加载", cv::Point(50, 50),
                   cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
        return result;
    }

    // 使用你的检测器
    try {
        auto blades = detector_->Detect(result);
        detector_->draw_blade(result);

        // 发送检测结果
        emit detectionCountChanged(blades.size());

        QList<QVariantMap> detections;
        for (const auto& blade : blades) {
            QVariantMap det;
            det["label"] = QString::fromStdString(blade.label);
            det["confidence"] = blade.prob;
            det["x"] = blade.rect.x;
            det["y"] = blade.rect.y;
            det["width"] = blade.rect.width;
            det["height"] = blade.rect.height;
            detections.append(det);
        }
        emit detectionResults(detections);

    } catch (const std::exception& e) {
        cv::putText(result, "识别系统出错", cv::Point(50, 50),
                   cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
    }

    return result;
}

cv::Mat MediaProcessor::applyBinary(const cv::Mat& frame)
{
    cv::Mat gray, binary;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::Mat result;
    cv::cvtColor(binary, result, cv::COLOR_GRAY2BGR);

    return result;
}

cv::Mat MediaProcessor::extractROI(const cv::Mat& frame)
{
    cv::Mat result = frame.clone();

    int centerX = frame.cols / 2;
    int centerY = frame.rows / 2;
    int roiX = std::max(0, centerX - roiWidth_ / 2);
    int roiY = std::max(0, centerY - roiHeight_ / 2);

    cv::Rect roiRect(roiX, roiY, roiWidth_, roiHeight_);
    roiRect = roiRect & cv::Rect(0, 0, frame.cols, frame.rows);

    if (!roiRect.empty()) {
        cv::rectangle(result, roiRect, cv::Scalar(255, 0, 0), 2);

        cv::Mat roi = frame(roiRect);
        int scaledWidth = std::min(300, frame.cols / 3);
        int scaledHeight = static_cast<int>(roi.rows * (static_cast<double>(scaledWidth) / roi.cols));

        cv::Mat scaledROI;
        cv::resize(roi, scaledROI, cv::Size(scaledWidth, scaledHeight));

        cv::Rect overlayRect(10, 10, scaledWidth, scaledHeight);
        if (overlayRect.br().x < result.cols && overlayRect.br().y < result.rows) {
            scaledROI.copyTo(result(overlayRect));
            cv::rectangle(result, overlayRect, cv::Scalar(0, 255, 255), 2);
            cv::putText(result, "ROI Preview", cv::Point(15, 35),
                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);
        }
    }

    return result;
}

QImage MediaProcessor::matToQImage(const cv::Mat& mat)
{
    if (mat.empty()) {
        return QImage();
    }

    cv::Mat rgb;
    if (mat.channels() == 3) {
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    } else if (mat.channels() == 1) {
        cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
    } else {
        rgb = mat;
    }

    QImage qImage(rgb.data, rgb.cols, rgb.rows, rgb.step,
                  QImage::Format_RGB888);

    return qImage.copy();
}
