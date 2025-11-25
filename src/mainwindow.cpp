#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QCloseEvent>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , currentZoom_(1.0)
    , currentTheme_("light")
{
    ui->setupUi(this);

    mediaProcessor = new MediaProcessor(this);

    setupUI();
    setupConnections();
    loadSettings();

    setWindowTitle(tr("目标检测系统"));
    resize(1400, 900);

    // 加载模型
    QString modelPath = ":/models/buff.xml";
    if (QFile::exists(modelPath)) {
        if (mediaProcessor->loadDetectionModel(modelPath)) {
            statusBar()->showMessage(tr("成功加载模型"), 3000);
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    // 设置 Splitter 初始大小
    ui->mainSplitter->setSizes({220, 1180});
    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);

    // 默认隐藏视频控制
    ui->progressSlider->setVisible(false);
    ui->frameLabel->setVisible(false);
    ui->fpsLabel->setVisible(false);

    // 设置显示模式按钮组
    displayModeGroup = new QButtonGroup(this);
    displayModeGroup->addButton(ui->originalRadio, MediaProcessor::OriginalMode);
    displayModeGroup->addButton(ui->detectionRadio, MediaProcessor::DetectionMode);
    displayModeGroup->addButton(ui->binaryRadio, MediaProcessor::BinaryMode);
    displayModeGroup->addButton(ui->roiRadio, MediaProcessor::ROIMode);
    // 设置为互斥模式
    displayModeGroup->setExclusive(true);

    // 主题 Action 组（互斥）
    themeActionGroup = new QActionGroup(this);
    themeActionGroup->addAction(ui->actionLightTheme);
    themeActionGroup->addAction(ui->actionDarkTheme);
    themeActionGroup->setExclusive(true);

    // 设置初始状态
    ui->originalRadio->setChecked(true);

    // 状态栏初始消息
    statusBar()->showMessage(tr("就绪 - 请打开图片或视频文件"));
}

void MainWindow::setupConnections()
{
    // ========== 文件菜单 ==========
    connect(ui->actionOpenImage, &QAction::triggered,
            this, &MainWindow::openImage);
    connect(ui->actionOpenVideo, &QAction::triggered,
            this, &MainWindow::openVideo);
    connect(ui->actionSaveFrame, &QAction::triggered,
            this, &MainWindow::saveCurrentFrame);
    connect(ui->actionExport, &QAction::triggered,
            this, &MainWindow::exportResults);
    connect(ui->actionExit, &QAction::triggered,
            this, &MainWindow::exitApp);

    // ========== 视图菜单 ==========
    connect(ui->actionZoomIn, &QAction::triggered,
            this, &MainWindow::zoomIn);
    connect(ui->actionZoomOut, &QAction::triggered,
            this, &MainWindow::zoomOut);
    connect(ui->actionFitWindow, &QAction::triggered,
            this, &MainWindow::fitWindow);
    connect(ui->actionActualSize, &QAction::triggered,
            this, &MainWindow::actualSize);
    connect(ui->actionToggleLeftPanel, &QAction::toggled,
            this, &MainWindow::toggleLeftPanel);

    // ========== 主题切换 ==========
    connect(ui->actionLightTheme, &QAction::triggered,
                this, &MainWindow::switchToLightTheme);
    connect(ui->actionDarkTheme, &QAction::triggered,
                this, &MainWindow::switchToDarkTheme);

    // ========== 工具菜单 ==========
    connect(ui->actionLoadModel, &QAction::triggered,
            this, &MainWindow::loadModel);
    connect(ui->actionSettings, &QAction::triggered,
            this, &MainWindow::showSettings);
    connect(ui->actionAbout, &QAction::triggered,
            this, &MainWindow::showAbout);

    // ========== 播放控制 ==========
    connect(ui->actionPlay, &QAction::triggered,
            this, &MainWindow::playMedia);
    connect(ui->actionPause, &QAction::triggered,
            this, &MainWindow::pauseMedia);
    connect(ui->actionStop, &QAction::triggered,
            this, &MainWindow::stopMedia);

    // ========== 媒体处理器信号 ==========
    connect(mediaProcessor, &MediaProcessor::frameReady,
            this, &MainWindow::onFrameReady);

    connect(mediaProcessor, &MediaProcessor::frameNumberChanged,
            this, &MainWindow::onFrameNumberChanged);

    connect(mediaProcessor, &MediaProcessor::fpsChanged,
            this, &MainWindow::onFPSChanged);

    connect(mediaProcessor, &MediaProcessor::detectionCountChanged,
            this, &MainWindow::onDetectionCountChanged);

    connect(mediaProcessor, &MediaProcessor::detectionResults,
            this, &MainWindow::onDetectionResults);

    connect(mediaProcessor, &MediaProcessor::statusMessage,
            this, [this](const QString &msg) {
                statusBar()->showMessage(msg);
            });

    connect(mediaProcessor, &MediaProcessor::mediaInfoChanged,
            this, &MainWindow::onMediaInfoChanged);

    // ========== 显示模式 ==========
    connect(displayModeGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(onDisplayModeChanged(int)));

    // ========== 参数调整 ==========
    connect(ui->confidenceSlider, &QSlider::valueChanged,
            this, &MainWindow::onConfidenceChanged);

    connect(ui->nmsSlider, &QSlider::valueChanged,
            this, &MainWindow::onNMSChanged);

    connect(ui->roiSizeSpinBox, SIGNAL(valueChanged(int)),
            this, SLOT(onROISizeChanged(int)));

    connect(ui->progressSlider, &QSlider::sliderMoved,
            this, &MainWindow::onProgressSliderMoved);
}

// 加载主题文件
void MainWindow::loadTheme(const QString &themePath)
{
    QFile file(themePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qWarning() << "无法打开主题文件:" << themePath;
        statusBar()->showMessage(tr("主题加载失败"), 3000);
        return;
    }

    QTextStream in(&file);
    QString styleSheet = in.readAll();
    file.close();

    qApp->setStyleSheet(styleSheet);
    qDebug() << "已加载主题:" << themePath;
}

// 切换到浅色主题
void MainWindow::switchToLightTheme()
{
    loadTheme(":/themes/light.qss");
    currentTheme_ = "light";
    ui->actionLightTheme->setChecked(true);
    ui->actionDarkTheme->setChecked(false);
    statusBar()->showMessage(tr("已切换到浅色主题"), 2000);
}

// 切换到深色主题
void MainWindow::switchToDarkTheme()
{
    loadTheme(":/themes/moonlight.qss");
    currentTheme_ = "dark";
    ui->actionDarkTheme->setChecked(true);
    ui->actionLightTheme->setChecked(false);
    statusBar()->showMessage(tr("已切换到深色主题"), 2000);
}

void MainWindow::loadSettings()
{
    QSettings settings("JulyJolly", "DetectionSystem");

    // 恢复窗口
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());

    // 恢复 Splitter
    QByteArray splitterState = settings.value("splitterState").toByteArray();
    if (!splitterState.isEmpty()) {
        ui->mainSplitter->restoreState(splitterState);
    }

    // 恢复左侧面板
    bool leftPanelVisible = settings.value("leftPanelVisible", true).toBool();
    ui->leftPanel->setVisible(leftPanelVisible);
    ui->actionToggleLeftPanel->setChecked(leftPanelVisible);

    // 恢复参数
    int confidence = settings.value("confidence", 50).toInt();
    int nms = settings.value("nms", 40).toInt();
    int roiSize = settings.value("roiSize", 640).toInt();

    ui->confidenceSlider->setValue(confidence);
    ui->nmsSlider->setValue(nms);
    ui->roiSizeSpinBox->setValue(roiSize);

    // 恢复主题
    QString theme = settings.value("theme", "light").toString();
    currentTheme_ = theme;

    if (theme == "dark") {
        ui->actionDarkTheme->setChecked(true);
        switchToDarkTheme();
    } else {
        ui->actionLightTheme->setChecked(true);
        switchToLightTheme();
    }

    qDebug() << "已加载设置 - 主题:" << theme;
}

void MainWindow::saveSettings()
{
    // 保存于~/.config/JulyJolly/BuffDetection.conf
    QSettings settings("JulyJolly", "DetectionSystem");

    // 保存窗口
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    // 保存 Splitter
    settings.setValue("splitterState", ui->mainSplitter->saveState());

    // 保存左侧面板
    settings.setValue("leftPanelVisible", ui->leftPanel->isVisible());

    // 保存参数
    settings.setValue("confidence", ui->confidenceSlider->value());
    settings.setValue("nms", ui->nmsSlider->value());
    settings.setValue("roiSize", ui->roiSizeSpinBox->value());

    // 保存主题
    settings.setValue("theme", currentTheme_);

    qDebug() << "已保存设置 - 主题:" << currentTheme_;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateDisplayImage();
}

// ========== 文件操作 ==========

void MainWindow::openImage()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("打开图片"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif);;所有文件 (*.*)")
    );

    if (fileName.isEmpty()) return;

    if (mediaProcessor->loadImage(fileName)) {
        updateUIForMediaType(MediaProcessor::ImageType);
        fitWindow();
    }
}

void MainWindow::openVideo()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("打开视频"),
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
        tr("视频文件 (*.mp4 *.avi *.mkv *.mov);;所有文件 (*.*)")
    );

    if (fileName.isEmpty()) return;

    if (mediaProcessor->loadVideo(fileName)) {
        updateUIForMediaType(MediaProcessor::VideoType);
        fitWindow();
    }
}

void MainWindow::saveCurrentFrame()
{
    if (currentDisplayImage_.isNull()) {
        QMessageBox::warning(this, tr("保存失败"), tr("当前没有可保存的图像"));
        return;
    }

    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                         + "/frame_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png";

    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("保存当前帧"),
        defaultPath,
        tr("PNG 图片 (*.png);;JPEG 图片 (*.jpg);;所有文件 (*.*)")
    );

    if (fileName.isEmpty()) return;

    if (currentDisplayImage_.save(fileName)) {
        statusBar()->showMessage(tr("已保存: %1").arg(fileName), 3000);
    } else {
        QMessageBox::warning(this, tr("保存失败"), tr("无法保存图像"));
    }
}

void MainWindow::exportResults()
{
    if (mediaProcessor->getMediaType() == MediaProcessor::NoMedia) {
        QMessageBox::information(this, tr("导出"), tr("当前没有检测结果"));
        return;
    }

    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                         + "/detections_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt";

    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("导出检测结果"),
        defaultPath,
        tr("文本文件 (*.txt);;JSON 文件 (*.json);;所有文件 (*.*)")
    );

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法创建文件"));
        return;
    }

    QTextStream out(&file);
    out << "目标检测结果导出\n";
    out << "=" << QString("").fill('=', 50) << "\n";
    out << "时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
    out << "文件: " << mediaProcessor->getCurrentFilePath() << "\n";
    out << "分辨率: " << mediaProcessor->getMediaSize().width() << "x"
        << mediaProcessor->getMediaSize().height() << "\n";
    out << "\n检测到 " << ui->detectionList->count() << " 个目标\n";
    out << "=" << QString("").fill('=', 50) << "\n\n";

    for (int i = 0; i < ui->detectionList->count(); ++i) {
        out << ui->detectionList->item(i)->text() << "\n";
    }

    file.close();
    statusBar()->showMessage(tr("已导出: %1").arg(fileName), 3000);
}

void MainWindow::exitApp()
{
    close();
}

// ========== 媒体控制 ==========

void MainWindow::playMedia()
{
    mediaProcessor->play();
}

void MainWindow::pauseMedia()
{
    mediaProcessor->pause();
}

void MainWindow::stopMedia()
{
    mediaProcessor->stop();
}

// ========== 视图操作 ==========

void MainWindow::toggleLeftPanel(bool visible)
{
    ui->leftPanel->setVisible(visible);
    statusBar()->showMessage(visible ? tr("左侧面板已显示") : tr("左侧面板已隐藏"), 2000);
}

void MainWindow::zoomIn()
{
    currentZoom_ *= 1.2;
    updateDisplayImage();
    statusBar()->showMessage(tr("缩放: %1%").arg(int(currentZoom_ * 100)), 2000);
}

void MainWindow::zoomOut()
{
    currentZoom_ /= 1.2;
    updateDisplayImage();
    statusBar()->showMessage(tr("缩放: %1%").arg(int(currentZoom_ * 100)), 2000);
}

void MainWindow::fitWindow()
{
    currentZoom_ = 1.0;
    updateDisplayImage();
    statusBar()->showMessage(tr("适应窗口"), 2000);
}

void MainWindow::actualSize()
{
    currentZoom_ = 1.0;
    updateDisplayImage();
    statusBar()->showMessage(tr("实际大小"), 2000);
}

// ========== 工具操作 ==========

void MainWindow::loadModel()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("加载 OpenVINO 模型"),
        "model",
        tr("OpenVINO 模型 (*.xml);;所有文件 (*.*)")
    );

    if (fileName.isEmpty()) return;

    mediaProcessor->loadDetectionModel(fileName);
}

void MainWindow::showSettings()
{
    QMessageBox::information(this, tr("设置"),
        tr("参数设置功能开发中...\n"
           "\n当前可通过左侧面板调整：\n"
           "- 置信度阈值\n"
           "- NMS 阈值\n"
           "- ROI 大小"));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("关于"),
        tr("<h2>目标检测系统 v1.0</h2>"
           "<p>基于 OpenVINO 的目标检测系统</p>"
           "<hr>"
           "<p><b>作者：</b>JulyJolly</p>"
           "<p><b>日期：</b>2025-11-20</p>"
           "<p><b>功能特性：</b></p>"
           "<ul>"
           "<li>✓ 图片/视频加载与显示</li>"
           "<li>✓ buff检测</li>"
           "<li>✓ 关键点检测与可视化</li>"
           "<li>✓ 多种显示模式</li>"
           "<li>✓ 实时参数调整</li>"
           "<li>✓ 检测结果导出</li>"
           "</ul>"
           "<hr>"
           "<p><b>技术栈：</b></p>"
           "<p>Qt 5 + OpenCV + OpenVINO</p>"));
}

// ========== 信号响应 ==========

void MainWindow::onFrameReady(const QImage &frame)
{
    if (!frame.isNull()) {
        currentDisplayImage_ = frame;
        updateDisplayImage();
    }
}

void MainWindow::updateDisplayImage()
{
    if (currentDisplayImage_.isNull()) return;

    QSize labelSize = ui->displayLabel->size();
    QImage scaledImage;

    if (currentZoom_ == 1.0) {
        // 适应窗口
        scaledImage = currentDisplayImage_.scaled(
            labelSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
    } else {
        // 按缩放比例
        QSize targetSize = currentDisplayImage_.size() * currentZoom_;
        scaledImage = currentDisplayImage_.scaled(
            targetSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
    }

    ui->displayLabel->setPixmap(QPixmap::fromImage(scaledImage));
}

void MainWindow::onFrameNumberChanged(int current, int total)
{
    ui->frameLabel->setText(tr("帧：%1/%2").arg(current).arg(total));
    ui->progressSlider->setMaximum(total);
    ui->progressSlider->setValue(current);
}

void MainWindow::onFPSChanged(double fps)
{
    ui->fpsLabel->setText(tr("FPS：%1").arg(static_cast<int>(fps)));
}

void MainWindow::onDetectionCountChanged(int count)
{
    ui->detectionCountLabel->setText(tr("检测：%1 个目标").arg(count));
}

void MainWindow::onDetectionResults(const QList<QVariantMap> &results)
{
    ui->detectionList->clear();

    for (int i = 0; i < results.size(); ++i) {
        QVariantMap det = results[i];
        QString text = tr("%1 - 置信度: %2%")
            .arg(det["label"].toString())
            .arg(static_cast<int>(det["confidence"].toDouble() * 100));

        QListWidgetItem *item = new QListWidgetItem(text);
        item->setCheckState(Qt::Checked);
        ui->detectionList->addItem(item);
    }
}

void MainWindow::onMediaInfoChanged(const QString &type, const QSize &size, const QString &info)
{
    ui->mediaTypeLabel->setText(tr("类型：%1").arg(type));
    ui->mediaSizeLabel->setText(tr("分辨率：%1x%2").arg(size.width()).arg(size.height()));
    ui->mediaInfoLabel->setText(tr("详细信息：%1").arg(info));
}

// ========== 参数调整 ==========

void MainWindow::onDisplayModeChanged(int id)
{
    mediaProcessor->setDisplayMode(static_cast<MediaProcessor::DisplayMode>(id));
}

void MainWindow::onConfidenceChanged(int value)
{
    double confidence = value / 100.0;
    mediaProcessor->setConfidenceThreshold(confidence);
    ui->confidenceValueLabel->setText(QString::number(confidence, 'f', 2));
}

void MainWindow::onNMSChanged(int value)
{
    double nms = value / 100.0;
    mediaProcessor->setNMSThreshold(nms);
    ui->nmsValueLabel->setText(QString::number(nms, 'f', 2));
}

void MainWindow::onROISizeChanged(int value)
{
    mediaProcessor->setROISize(value, value);
}

void MainWindow::onProgressSliderMoved(int value)
{
    mediaProcessor->seekToFrame(value);
}

void MainWindow::updateUIForMediaType(MediaProcessor::MediaType type)
{
    bool isVideo = (type == MediaProcessor::VideoType);

    // 显示/隐藏视频控制
    ui->actionPlay->setVisible(isVideo);
    ui->actionPause->setVisible(isVideo);
    ui->actionStop->setVisible(isVideo);
    ui->progressSlider->setVisible(isVideo);
    ui->frameLabel->setVisible(isVideo);
    ui->fpsLabel->setVisible(isVideo);
}
