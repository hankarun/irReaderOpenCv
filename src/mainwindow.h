#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>

#include <opencv2/opencv.hpp>

#include <vector>

struct FrameData
{
    //BGR
    cv::Mat frameData;
    //RGB
    cv::Mat frameShortData;
    cv::Mat ushortData;
    QImage green;
    QImage gray;
    double minValue;
    double maxValue;


    void set(const cv::Mat &data);
    void resetMinMax();
    void setMinMax(double min, double max);
};

struct Project
{
    std::vector<FrameData> data;
    void loadFromAvi(const char* filenanme);
    void loadFromPng(const std::vector<QString>& filenames);
};

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class PictureLabel : public QLabel
{
    Q_OBJECT
public:
    PictureLabel(QWidget *parent = nullptr);

    void setPoiData(int x, int y, int value);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int x = -10;
    int y = -10;
    int value = 0;
};

class PlayerControl : public QObject
{
    Q_OBJECT
public:
    PlayerControl(QObject *parent = nullptr);

    int frameRate;
    int currentFrame;
    int maxFrame;
    int loop;

    void start();
    void stop();

private:
    void timerEvent(QTimerEvent *);
signals:
    void frameChanged(int index);
    void started();
    void stopped();

private:
    int timerId;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void onOpenAction();

    void openFile(const QString &filename);

    void updateList();

private:
    void updateGrayTexture(FrameData *frameData);
protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void setPosition(int x, int y);

    void setCurrentFrameData(FrameData *frameData);
    void setScaleFactor(float value);
private:
    Ui::MainWindow *ui;
    std::unique_ptr<Project> project;
    PictureLabel *green;
    PictureLabel *gray;
    int currentX = 0;
    int currentY = 0;
    FrameData *currentFrameData;
    PlayerControl playerControl;
    float scaleFactor = 1.0;
};

#endif // MAINWINDOW_H
