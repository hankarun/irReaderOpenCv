#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QDebug>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QRgb>
#include <QMimeData>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenAction);

    connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, [this](){
        auto selectedItems = ui->listWidget->selectedItems();
        if (selectedItems.empty())
            return;
        if (!project)
            return;

        auto image = project->frames.at(ui->listWidget->row(selectedItems.at(0)));
        ui->green->setMaximumHeight(image.height());
        ui->green->setMaximumWidth(image.width());
        ui->green->setPixmap(QPixmap::fromImage(image));
    });

    ui->green->installEventFilter(this);
    setAcceptDrops(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onOpenAction()
{
    auto filename = QFileDialog::getOpenFileName(this, tr("Select Avi File"), "", "Avi files (*.avi);;PNG files (*.png)");
    if (filename.isEmpty())
        return;
    openFile(filename);
}

void MainWindow::openFile(const QString &filename)
{
    if (project)
    {
        delete project;
    }

    project = new Project();

    cv::VideoCapture cap(filename.toStdString().c_str());

    if(!cap.isOpened()){
        qDebug() << "Error opening video stream or file";
        return;
    }

    int frameNumber = 1;
    while(1){
        cv::Mat frame;
        cap >> frame;

        if (frame.empty())
            break;

        project->frameData.push_back(frame);

        QImage img((uchar*)frame.data, frame.cols, frame.rows, QImage::Format_RGB888);
        project->frames.push_back(img);
        QListWidgetItem *item = new QListWidgetItem(QString("%1 frame").arg(frameNumber++));
        item->setData(Qt::DecorationRole, QPixmap::fromImage(img).scaled(64,64));
        ui->listWidget->addItem(item);
    }
    cap.release();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->pos().x() < 0 || mouseEvent->pos().y() < 0)
            return false;
        ui->xCoordinate->setText(QString::number(mouseEvent->pos().x()));
        ui->yCoordinate->setText(QString::number(mouseEvent->pos().y()));
        auto image = ui->green->pixmap()->toImage();
        QRgb pixel = image.pixel(mouseEvent->pos().x(), mouseEvent->pos().y());
        unsigned short value = 0;
        value = (qBlue(pixel) << 8);
        value |= (qGreen(pixel) & 0x0F);
        ui->value->setText(QString::number(value));
    }
    return false;
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();

    if (mimeData->hasUrls())
    {
        QStringList pathList;
        QList<QUrl> urlList = mimeData->urls();

        auto filename = urlList.at(0).toLocalFile();
        QFileInfo fileInfo(filename);
        if (fileInfo.suffix() == "avi")
        {
            openFile(filename);
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

