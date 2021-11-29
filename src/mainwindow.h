#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>

#include <opencv2/opencv.hpp>

struct Project
{
    std::vector<cv::Mat> frameData;
    std::vector<QImage> frames;
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void onOpenAction();

    void openFile(const QString& filename);

protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    Ui::MainWindow *ui;
    Project* project = nullptr;
};
#endif // MAINWINDOW_H
