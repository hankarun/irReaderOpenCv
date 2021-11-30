#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QDebug>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QRgb>
#include <QMimeData>
#include <QPainter>
#include <QVBoxLayout>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("IR Data Analyser");

    green = new PictureLabel(this);
    QWidget *greenScrollWidget = new QWidget();
    QVBoxLayout *layout1 = new QVBoxLayout();
    layout1->addWidget(green);
    greenScrollWidget->setLayout(layout1);
    ui->greenScrollArea->setWidget(green);

    gray = new PictureLabel(this);
    QWidget *grayScrollWidget = new QWidget();
    QVBoxLayout *layout2 = new QVBoxLayout();
    layout2->addWidget(gray);
    grayScrollWidget->setLayout(layout2);
    ui->grayScrollArea->setWidget(grayScrollWidget);

    connect(ui->greenScrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, ui->grayScrollArea->horizontalScrollBar(), &QScrollBar::setValue);
    connect(ui->greenScrollArea->verticalScrollBar(), &QScrollBar::valueChanged, ui->grayScrollArea->verticalScrollBar(), &QScrollBar::setValue);

    connect(ui->grayScrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, ui->greenScrollArea->horizontalScrollBar(), &QScrollBar::setValue);
    connect(ui->grayScrollArea->verticalScrollBar(), &QScrollBar::valueChanged, ui->greenScrollArea->verticalScrollBar(), &QScrollBar::setValue);

    ui->gridLayout->setColumnStretch(1, 1);
    ui->gridLayout->setColumnStretch(2, 1);

    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenAction);

    connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, [this]()
            {
                auto selectedItems = ui->listWidget->selectedItems();
                if (selectedItems.empty())
                    return;
                if (!project)
                    return;
                const auto &frameData = project->data.at(ui->listWidget->row(selectedItems.at(0)));
                auto image = frameData.green;
                green->setMaximumHeight(image.height());
                green->setMaximumWidth(image.width());
                green->setPixmap(QPixmap::fromImage(image));

                auto grayScale = frameData.gray;
                gray->setMaximumHeight(grayScale.height());
                gray->setMaximumWidth(grayScale.width());
                gray->setPixmap(QPixmap::fromImage(grayScale));

                gray->setPoiData(0, 0, 0);
                green->setPoiData(0, 0, 0);
            });

    green->installEventFilter(this);
    gray->installEventFilter(this);
    setAcceptDrops(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onOpenAction()
{
    auto filename = QFileDialog::getOpenFileName(this, tr("Select Avi File"), "", "Avi files (*.avi)");
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

    if (!cap.isOpened())
    {
        qDebug() << "Error opening video stream or file";
        return;
    }

    int frameNumber = 1;
    while (1)
    {
        FrameData frameData;
        cv::Mat data;
        cap >> data;

        if (data.empty())
            break;

        frameData.set(data);

        project->data.push_back(frameData);
    }
    cap.release();
    updateList();
}

void MainWindow::updateList()
{
    ui->listWidget->clear();
    if (!project)
        return;
    int frameNumber = 1;
    for (auto &frameData : project->data)
    {
        QListWidgetItem *item = new QListWidgetItem(QString("%1 frame").arg(frameNumber++));
        item->setData(Qt::DecorationRole, QPixmap::fromImage(frameData.green).scaled(64, 64));
        ui->listWidget->addItem(item);
    }
}

void FrameData::set(const cv::Mat &data)
{
    frameData = data;
    frameShortData = cv::Mat(cv::Size(frameData.cols, frameData.rows), CV_8U);
    for (int r = 0; r < frameData.rows; ++r)
    {
        for (int c = 0; c < frameData.cols; ++c)
        {
            int value = 0;
            value = frameData.at<cv::Vec3b>(r, c)[2] << 8;
            value |= frameData.at<cv::Vec3b>(r, c)[1];
            unsigned char charValue = (value / 15000.0f) * 255.0f;
            frameShortData.at<unsigned char>(r, c) = charValue;
        }
    }

    gray = QImage((uchar *)frameShortData.data, frameShortData.cols, frameShortData.rows, QImage::Format_Grayscale8);
    green = QImage((uchar *)frameData.data, frameData.cols, frameData.rows, QImage::Format_RGB888);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!project)
        return false;
    if (event->type() == QEvent::MouseMove)
    {

        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->pos().x() < 0 || mouseEvent->pos().y() < 0)
            return false;
        int posX = mouseEvent->pos().x() - 5;
        int posY = mouseEvent->pos().y() - 5;
        ui->xCoordinate->setText(QString::number(posX));
        ui->yCoordinate->setText(QString::number(posY));
        auto image = green->pixmap()->toImage();
        QRgb pixel = image.pixel(posX, posY);
        unsigned short value = 0;
        value = (qBlue(pixel) << 8);
        value |= (qGreen(pixel) & 0x0F);
        ui->value->setText(QString::number(value));

        green->setPoiData(posX, posY, value);
        gray->setPoiData(posX, posY, value);
    }
    return false;
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();

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
        else if (fileInfo.suffix() == "png")
        {
            if (project)
            {
                delete project;
            }

            project = new Project();

            for (auto &file : urlList)
            {
                cv::Mat data = cv::imread(file.toLocalFile().toStdString().c_str(), cv::IMREAD_COLOR);
                FrameData frameData;
                frameData.set(data);
                project->data.push_back(frameData);
            }

            updateList();
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

PictureLabel::PictureLabel(QWidget *parent)
    : QLabel(parent)
{
}

void PictureLabel::setPoiData(int x, int y, int value)
{
    this->x = x;
    this->y = y;
    this->value = value;
    this->update();
}

void PictureLabel::paintEvent(QPaintEvent *e)
{
    QLabel::paintEvent(e);
    QPainter painter(this);
    painter.setPen(Qt::red);
    painter.drawPoint(x, y);
    QFont font = painter.font();
    font.setPointSize(font.pointSize() * 2);
    painter.setFont(font);
    painter.drawText(x + 15, y + 5, QString::number(value));
    painter.drawEllipse(x - 5, y - 5, 10, 10);
}