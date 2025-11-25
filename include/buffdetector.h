#ifndef BUFFDETECTOR_H
#define BUFFDETECTOR_H

#include <opencv2/opencv.hpp>
#include <openvino/openvino.hpp>
#include <vector>
#include <string>

namespace rm_buff
{

// 检测结果结构
struct Blade {
    cv::Rect rect;                  // 边界框
    std::string label;              // 类别标签
    float prob;                     // 置信度
    std::vector<cv::Point2f> kpt;   // 关键点
};

// 检测器类
class Detector
{
public:
    explicit Detector(const std::string& model_path);
    ~Detector() = default;

    // 执行检测
    std::vector<Blade> Detect(cv::Mat& src_img);

    // 绘制检测结果
    void draw_blade(cv::Mat& img);

    // 获取最新的检测结果
    const std::vector<Blade>& getBladeArray() const { return blade_array_; }

    // 设置检测参数
    void setConfThreshold(float conf) { conf_threshold_ = conf; }
    void setNMSThreshold(float nms) { nms_threshold_ = nms; }

    float getConfThreshold() const { return conf_threshold_; }
    float getNMSThreshold() const { return nms_threshold_; }

private:
    // Letterbox 图像预处理
    cv::Mat letterbox(cv::Mat& src, int h, int w);

    // NMS 后处理
    void non_max_suppression(
        ov::Tensor& output,
        float conf_thres,
        float iou_thres,
        cv::Size img_size
    );

    // OpenVINO 相关
    std::string model_path_;
    ov::Core core_;
    std::shared_ptr<ov::Model> model_;
    ov::CompiledModel compiled_model_;
    ov::InferRequest infer_request_;
    ov::Tensor input_tensor_;

    // 图像处理参数
    static constexpr int buff_image_size = 640;
    float padd_w_ = 0.0f;
    float padd_h_ = 0.0f;

    // 检测参数
    float conf_threshold_ = 0.5f;
    float nms_threshold_ = 0.4f;

    // 类别定义
    static constexpr int CLS_NUM = 4;
    static constexpr int KPT_NUM = 4;
    const std::vector<std::string> class_names = {"RR", "RW", "BR", "BW"};

    // 检测结果
    std::vector<Blade> blade_array_;
};

} // namespace rm_buff

#endif // BUFFDETECTOR_H
