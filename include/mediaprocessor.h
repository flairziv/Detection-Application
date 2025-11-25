#ifndef MEDIAPROCESSOR_H
#define MEDIAPROCESSOR_H

#include <QObject>
#include <QTimer>
#include <QImage>
#include <opencv2/opencv.hpp>
#include <QDebug>

#include "buffdetector.h"

class MediaProcessor : public QObject
{
    Q_OBJECT

public:
    enum MediaType {
        NoMedia,
        ImageType,
        VideoType
    };

    enum DisplayMode {
        OriginalMode,
        DetectionMode,
        BinaryMode,
        ROIMode
    };

    explicit MediaProcessor(QObject *parent = nullptr);
    ~MediaProcessor();

    // 媒体加载
    bool loadImage(const QString& filePath);
    bool loadVideo(const QString& filePath);
    void closeMedia();

    // 视频控制
    void play();
    void pause();
    void stop();
    void seekToFrame(int frameNumber);

    // 模式设置
    void setDisplayMode(DisplayMode mode);
    void setConfidenceThreshold(double threshold);
    void setNMSThreshold(double threshold);
    void setROISize(int width, int height);
    void setPlaybackSpeed(double speed);

    // 模型设置
    bool loadDetectionModel(const QString& modelPath);

    // 获取信息
    MediaType getMediaType() const { return mediaType_; }
    int getTotalFrames() const { return totalFrames_; }
    int getCurrentFrame() const { return currentFrame_; }
    double getFPS() const { return fps_; }
    QSize getMediaSize() const { return mediaSize_; }
    QString getCurrentFilePath() const { return currentFilePath_; }

    void processCurrentImage();
    QImage getCurrentProcessedImage() const { return lastProcessedImage_; }

signals:
    void frameReady(const QImage &frame);
    void frameNumberChanged(int current, int total);
    void fpsChanged(double fps);
    void detectionCountChanged(int count);
    void detectionResults(const QList<QVariantMap> &results);
    void statusMessage(const QString &message);
    void mediaInfoChanged(const QString &type, const QSize &size, const QString &info);

private slots:
    void processNextFrame();

private:
    cv::Mat processFrame(const cv::Mat& frame);
    cv::Mat detectObjects(const cv::Mat& frame);
    cv::Mat applyBinary(const cv::Mat& frame);
    cv::Mat extractROI(const cv::Mat& frame);
    QImage matToQImage(const cv::Mat& mat);

    // 媒体数据
    MediaType mediaType_;
    cv::Mat currentImage_;
    cv::VideoCapture videoCapture_;

    // 检测器
    std::unique_ptr<rm_buff::Detector> detector_;

    QTimer *timer_;
    QString currentFilePath_;
    QSize mediaSize_;
    QImage lastProcessedImage_;

    // 显示参数
    DisplayMode displayMode_;
    double confidenceThreshold_;
    double nmsThreshold_;
    int roiWidth_;
    int roiHeight_;
    double playbackSpeed_;

    // 视频信息
    int totalFrames_;
    int currentFrame_;
    double fps_;
    bool isPlaying_;
};

#endif // MEDIAPROCESSOR_H
