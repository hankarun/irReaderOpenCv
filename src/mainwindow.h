#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>

#include <vector>

#include <opencv2/opencv.hpp>

struct FrameData
{
    cv::Mat frameData;
    cv::Mat frameShortData;
    QImage green;
    QImage gray;

    void set(const cv::Mat& data);
};

struct Project
{
    std::vector<FrameData> data;
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
    int x = 0;
    int y = 0;
    int value = 0;
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
protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    Ui::MainWindow *ui;
    Project *project = nullptr;
    PictureLabel* green;
    PictureLabel* gray;
};


#endif // MAINWINDOW_H
