#include "buffdetector.h"
#include <algorithm>
#include <iostream>

namespace rm_buff
{

Detector::Detector(const std::string& model_path)
    : model_path_(model_path)
{
    core_ = ov::Core();
    model_ = core_.read_model(model_path_);

    ov::preprocess::PrePostProcessor ppp(model_);

    // 输入布局转换
    ppp.input().preprocess().convert_layout({0, 3, 1, 2}); // NHWC -> NCHW

    // 输出布局转换
    ppp.output().postprocess().convert_layout({0, 2, 1}); // [1,16,8400] -> [1,8400,16]

    model_ = ppp.build();

    // 编译模型 - 默认使用GPU，失败则使用CPU
    try {
        compiled_model_ = core_.compile_model(model_, "GPU");
        std::cout << "Model compiled on GPU" << std::endl;
    } catch (...) {
        compiled_model_ = core_.compile_model(model_, "CPU");
        std::cout << "Model compiled on CPU" << std::endl;
    }

    infer_request_ = compiled_model_.create_infer_request();
    input_tensor_ = infer_request_.get_input_tensor(0);
}

std::vector<Blade> Detector::Detect(cv::Mat& src_img)
{
    if (src_img.empty()) {
        std::cerr << "Empty image!" << std::endl;
        return {};
    }

    cv::Mat img;
    img = letterbox(src_img, buff_image_size, buff_image_size);

    // 归一化到[0,1]
    img.convertTo(img, CV_32FC3, 1.0 / 255.0);

    if (img.isContinuous()) {
        img = img.reshape(1, 1);
    } else {
        img = img.clone().reshape(1, 1);
    }

    input_tensor_ = ov::Tensor(
        input_tensor_.get_element_type(),
        input_tensor_.get_shape(),
        img.ptr<float>()
    );

    infer_request_.set_input_tensor(0, input_tensor_);

    // 执行推理
    infer_request_.infer();

    // 获取输出
    auto output = infer_request_.get_output_tensor(0);

    // 执行NMS和后处理
    non_max_suppression(output, conf_threshold_, nms_threshold_, src_img.size());

    return blade_array_;
}

cv::Mat Detector::letterbox(cv::Mat& src, int h, int w)
{
    int in_w = src.cols;
    int in_h = src.rows;
    int tar_w = w;
    int tar_h = h;
    float r = std::min(float(tar_h) / in_h, float(tar_w) / in_w);
    int inside_w = round(in_w * r);
    int inside_h = round(in_h * r);
    padd_w_ = tar_w - inside_w;
    padd_h_ = tar_h - inside_h;

    cv::Mat resize_img;
    cv::resize(src, resize_img, cv::Size(inside_w, inside_h));

    padd_w_ = padd_w_ / 2;
    padd_h_ = padd_h_ / 2;

    int top = int(round(padd_h_ - 0.1));
    int bottom = int(round(padd_h_ + 0.1));
    int left = int(round(padd_w_ - 0.1));
    int right = int(round(padd_w_ + 0.1));

    cv::copyMakeBorder(
        resize_img, resize_img, top, bottom, left, right,
        cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114)
    );

    return resize_img;
}

void Detector::non_max_suppression(
    ov::Tensor& output,
    float conf_thres,
    float iou_thres,
    cv::Size img_size)
{
    auto data = output.data<float>();

    int bs = output.get_shape()[0];
    int num_detections = output.get_shape()[1];
    int num_features = output.get_shape()[2];

    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    std::vector<std::vector<cv::Point2f>> kpts_list;
    std::vector<int> picked;
    std::vector<float> picked_useless;

    for (int i = 0; i < bs; i++) {
        for (int j = 0; j < num_detections; j++) {
            int offset = i * num_detections * num_features + j * num_features;

            float center_x_640 = data[offset + 0];
            float center_y_640 = data[offset + 1];
            float width_640 = data[offset + 2];
            float height_640 = data[offset + 3];

            // 类别置信度处理
            float class_scores[CLS_NUM];
            float max_class_score = -FLT_MAX;
            int classId = 0;
            for (int k = 0; k < CLS_NUM; k++) {
                class_scores[k] = data[offset + 4 + k];
                if (class_scores[k] > max_class_score) {
                    max_class_score = class_scores[k];
                    classId = k;
                }
            }

            if (max_class_score < conf_thres) {
                continue;
            }

            float x1_640 = center_x_640 - width_640 / 2.0f;
            float y1_640 = center_y_640 - height_640 / 2.0f;

            classIds.emplace_back(classId);
            confidences.emplace_back(max_class_score);
            boxes.emplace_back(cv::Rect(x1_640, y1_640, width_640, height_640));

            // 4个关键点处理
            std::vector<cv::Point2f> kpts;
            for (int k = 0; k < KPT_NUM; k++) {
                int kpt_x_idx = 8 + k * 2;
                int kpt_y_idx = 8 + k * 2 + 1;

                if (kpt_x_idx < num_features && kpt_y_idx < num_features) {
                    float kpt_x_640 = data[offset + kpt_x_idx];
                    float kpt_y_640 = data[offset + kpt_y_idx];

                    if (kpt_x_640 >= 0 && kpt_y_640 >= 0 &&
                        kpt_x_640 <= 640 && kpt_y_640 <= 640) {
                        kpts.emplace_back(cv::Point2f(kpt_x_640, kpt_y_640));
                    } else {
                        kpts.emplace_back(cv::Point2f(-1, -1));
                    }
                } else {
                    kpts.emplace_back(cv::Point2f(-1, -1));
                }
            }
            kpts_list.emplace_back(kpts);
        }
    }

    // 应用NMS
    cv::dnn::NMSBoxes(boxes, confidences, conf_thres, iou_thres, picked);

    // 构建最终结果
    blade_array_.clear();
    for (size_t i = 0; i < picked.size(); ++i) {
        Blade blade;
        int idx = picked[i];

        cv::Rect box_640 = boxes[idx];

        // 坐标转换函数
        auto convert_coord = [&](float coord_640, bool is_x) -> float {
            if (is_x) {
                return (coord_640 - padd_w_) * img_size.width /
                       (buff_image_size - 2 * padd_w_);
            } else {
                return (coord_640 - padd_h_) * img_size.height /
                       (buff_image_size - 2 * padd_h_);
            }
        };

        // 转换边界框坐标
        float x1_orig = convert_coord(box_640.x, true);
        float y1_orig = convert_coord(box_640.y, false);
        float x2_orig = convert_coord(box_640.x + box_640.width, true);
        float y2_orig = convert_coord(box_640.y + box_640.height, false);

        x1_orig = std::max(0.0f, std::min(x1_orig, float(img_size.width)));
        y1_orig = std::max(0.0f, std::min(y1_orig, float(img_size.height)));
        x2_orig = std::max(0.0f, std::min(x2_orig, float(img_size.width)));
        y2_orig = std::max(0.0f, std::min(y2_orig, float(img_size.height)));

        blade.rect = cv::Rect(x1_orig, y1_orig, x2_orig - x1_orig, y2_orig - y1_orig);

        // 转换关键点坐标
        std::vector<cv::Point2f> converted_kpts;
        for (const auto& kpt_640 : kpts_list[idx]) {
            if (kpt_640.x >= 0 && kpt_640.y >= 0) {
                float kpt_x_orig = convert_coord(kpt_640.x, true);
                float kpt_y_orig = convert_coord(kpt_640.y, false);

                kpt_x_orig = std::max(0.0f, std::min(kpt_x_orig, float(img_size.width)));
                kpt_y_orig = std::max(0.0f, std::min(kpt_y_orig, float(img_size.height)));

                converted_kpts.emplace_back(cv::Point2f(kpt_x_orig, kpt_y_orig));
            } else {
                converted_kpts.emplace_back(cv::Point2f(-1, -1));
            }
        }

        blade.label = class_names[classIds[idx]];
        blade.prob = confidences[idx];
        blade.kpt = converted_kpts;

        blade_array_.emplace_back(blade);
    }
}

void Detector::draw_blade(cv::Mat& img)
{
    for (size_t i = 0; i < blade_array_.size(); ++i) {
        if (blade_array_[i].label == "RW" || blade_array_[i].label == "BW")
            continue;
        // 绘制边界框
        cv::rectangle(img, blade_array_[i].rect, cv::Scalar(0, 255, 0), 2);

        // 绘制标签
        std::string label = blade_array_[i].label + ": " +
                           std::to_string(int(blade_array_[i].prob * 100)) + "%";
        cv::putText(img, label,
                    cv::Point(blade_array_[i].rect.x, blade_array_[i].rect.y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);

        // 绘制关键点
        std::vector<cv::Scalar> kpt_colors = {
            cv::Scalar(0, 255, 0),    // kpt0 - 绿色
            cv::Scalar(255, 0, 0),    // kpt1 - 蓝色
            cv::Scalar(0, 0, 255),    // kpt2 - 红色
            cv::Scalar(255, 255, 0)   // kpt3 - 青色
        };

        std::vector<std::string> kpt_names = {"kpt0", "kpt1", "kpt2", "kpt3"};

        for (size_t j = 0; j < blade_array_[i].kpt.size() && j < kpt_colors.size(); ++j) {
            cv::Point2f kpt = blade_array_[i].kpt[j];
            if (kpt.x >= 0 && kpt.y >= 0 && kpt.x != -1 && kpt.y != -1) {
                cv::circle(img, cv::Point(kpt.x, kpt.y), 5, kpt_colors[j], -1);

                cv::putText(img, kpt_names[j],
                            cv::Point(kpt.x + 7, kpt.y - 7),
                            cv::FONT_HERSHEY_SIMPLEX, 0.4, kpt_colors[j], 1);
            }
        }

        // 连接关键点形成四边形
        std::vector<cv::Point> valid_pts;
        for (size_t j = 0; j < blade_array_[i].kpt.size() && j < 4; ++j) {
            cv::Point2f kpt = blade_array_[i].kpt[j];
            if (kpt.x >= 0 && kpt.y >= 0 && kpt.x != -1 && kpt.y != -1) {
                valid_pts.push_back(cv::Point(kpt.x, kpt.y));
            }
        }

        if (valid_pts.size() == 4) {
            cv::polylines(img, valid_pts, true, cv::Scalar(255, 0, 255), 2);
        }
    }
}

} // namespace rm_buff
