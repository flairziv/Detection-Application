#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QButtonGroup>
#include <QActionGroup>
#include "mediaprocessor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // 文件操作
    void openImage();
    void openVideo();
    void saveCurrentFrame();
    void exportResults();
    void exitApp();

    // 媒体控制
    void playMedia();
    void pauseMedia();
    void stopMedia();

    // 视图操作
    void toggleLeftPanel(bool visible);
    void zoomIn();
    void zoomOut();
    void fitWindow();
    void actualSize();

    // 主题切换
    void switchToLightTheme();
    void switchToDarkTheme();
    void loadTheme(const QString &themePath);

    // 工具操作
    void loadModel();
    void showSettings();
    void showAbout();

    // 信号响应
    void onFrameReady(const QImage &frame);
    void onFrameNumberChanged(int current, int total);
    void onFPSChanged(double fps);
    void onDetectionCountChanged(int count);
    void onDetectionResults(const QList<QVariantMap> &results);
    void onMediaInfoChanged(const QString &type, const QSize &size, const QString &info);

    // 参数调整
    void onDisplayModeChanged(int id);         // QButtonGroup::buttonClicked(int)
    void onConfidenceChanged(int value);       // QSlider::valueChanged(int)
    void onNMSChanged(int value);              // QSlider::valueChanged(int)
    void onROISizeChanged(int value);          // QSpinBox::valueChanged(int)
    void onProgressSliderMoved(int value);     // QSlider::sliderMoved(int)

private:
    Ui::MainWindow *ui;
    MediaProcessor *mediaProcessor;
    QButtonGroup *displayModeGroup;
    QActionGroup *themeActionGroup;

    double currentZoom_;
    QImage currentDisplayImage_;
    QString currentTheme_;

    void setupUI();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void updateUIForMediaType(MediaProcessor::MediaType type);
    void updateDisplayImage();
};

#endif // MAINWINDOW_H
