#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <QDebug>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QRgb>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), currentFrameData(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("IR Data Analyser");

    green = new PictureLabel(ui->grayScrollArea);
    ui->greenScrollArea->setWidget(green);

    gray = new PictureLabel(ui->grayScrollArea);
    ui->grayScrollArea->setWidget(gray);

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
                {
                    setCurrentFrameData(nullptr);
                    return;
                }
                if (!project)
                {
                    setCurrentFrameData(nullptr);
                    return;
                }
                setCurrentFrameData(&project->data.at(ui->listWidget->row(selectedItems.at(0))));
            });

    green->installEventFilter(this);
    gray->installEventFilter(this);
    setAcceptDrops(true);

    connect(ui->xCoordinate, qOverload<int>(&QSpinBox::valueChanged), this, [this]()
            { setPosition(ui->xCoordinate->value(), ui->yCoordinate->value()); });
    connect(ui->yCoordinate, qOverload<int>(&QSpinBox::valueChanged), this, [this]()
            { setPosition(ui->xCoordinate->value(), ui->yCoordinate->value()); });

    connect(&playerControl, &PlayerControl::started, [this]()
            {
                ui->playButton->setEnabled(false);
                ui->stopButton->setEnabled(true);
                ui->loop->setEnabled(false);
                ui->frameRate->setEnabled(false);
                ui->listWidget->setEnabled(false);
            });
    connect(&playerControl, &PlayerControl::stopped, [this]()
            {
                ui->playButton->setEnabled(true);
                ui->stopButton->setEnabled(false);
                ui->loop->setEnabled(true);
                ui->frameRate->setEnabled(true);
                ui->listWidget->setEnabled(true);
            });
    connect(ui->playButton, &QPushButton::clicked, &playerControl, &PlayerControl::start);
    connect(ui->stopButton, &QPushButton::clicked, &playerControl, &PlayerControl::stop);

    connect(&playerControl, &PlayerControl::frameChanged, this, [this](int index)
            { ui->listWidget->setCurrentRow(index); });

    connect(ui->loop, &QCheckBox::stateChanged, this, [this]()
            { playerControl.loop = ui->loop->isChecked(); });

    connect(ui->frameRate, qOverload<int>(&QSpinBox::valueChanged), this, [this]()
            { playerControl.frameRate = ui->frameRate->value(); });
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
    const int imgSize = 128;
    playerControl.stop();
    setCurrentFrameData(nullptr);
    ui->listWidget->clear();
    if (!project)
        return;
    int frameNumber = 1;
    for (auto &frameData : project->data)
    {
        QListWidgetItem *item = new QListWidgetItem(QString("Frame %1").arg(frameNumber++));
        item->setData(Qt::DecorationRole, QPixmap::fromImage(frameData.gray).scaled(imgSize, imgSize));
        ui->listWidget->addItem(item);
    }
    playerControl.maxFrame = project->data.size();
}

void FrameData::set(const cv::Mat &data)
{
    frameData = data;
    frameShortData = cv::Mat(cv::Size(frameData.cols, frameData.rows), CV_8U);
    ushortData = cv::Mat(cv::Size(frameData.cols, frameData.rows), CV_16U);

    for (int r = 0; r < frameData.rows; ++r)
    {
        for (int c = 0; c < frameData.cols; ++c)
        {
            int value = 0;
            value = frameData.at<cv::Vec3b>(r, c)[2] << 8;
            value |= frameData.at<cv::Vec3b>(r, c)[1];
            ushortData.at<unsigned short>(r, c) = value;
        }
    }

    double min, max;
    cv::minMaxLoc(ushortData, &min, &max);

    for (int r = 0; r < frameData.rows; ++r)
    {
        for (int c = 0; c < frameData.cols; ++c)
        {
            frameShortData.at<unsigned char>(r, c) = ((ushortData.at<unsigned short>(r, c) - min) / (max - min)) * 255.0f;
        }
    }

    gray = QImage((uchar *)frameShortData.data, frameShortData.cols, frameShortData.rows, QImage::Format_Grayscale8);
    green = QImage((uchar *)frameData.data, frameData.cols, frameData.rows, QImage::Format_BGR888);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->pos().x() < 0 || mouseEvent->pos().y() < 0)
            return false;
        int posX = mouseEvent->pos().x() - 5;
        int posY = mouseEvent->pos().y() - 5;
        setPosition(posX, posY);
    }else     if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->pos().x() < 0 || mouseEvent->pos().y() < 0)
            return false;
        int posX = mouseEvent->pos().x();
        int posY = mouseEvent->pos().y();
        setPosition(posX, posY);
    }
    return false;
}

void MainWindow::setPosition(int x, int y)
{
    if (!project)
        return;

    if (x == currentX && y == currentY)
        return;

    if (!currentFrameData)
        return;

    ui->xCoordinate->setValue(x);
    ui->yCoordinate->setValue(y);
    auto image = green->pixmap()->toImage();
    QRgb pixel = image.pixel(x, y);
    unsigned short value = 0;
    value = (qRed(pixel) << 8);
    value |= (qGreen(pixel) & 0x0F);
    ui->value->setText(QString::number(value));
    green->setPoiData(x, y, value);
    gray->setPoiData(x, y, value);

    currentX = x;
    currentY = y;
}

void MainWindow::setCurrentFrameData(FrameData *frameData)
{
    if (frameData)
    {
        auto image = frameData->green;
        green->setMaximumHeight(image.height());
        green->setMaximumWidth(image.width());
        green->setPixmap(QPixmap::fromImage(image));

        auto grayScale = frameData->gray;
        gray->setMaximumHeight(image.height());
        gray->setMaximumWidth(image.width());
        gray->setPixmap(QPixmap::fromImage(grayScale));
    }
    else
    {
        green->setPixmap(QPixmap());
        gray->setPixmap(QPixmap());
    }
    gray->setPoiData(-10, -10, 0);
    green->setPoiData(-10, -10, 0);
    currentFrameData = frameData;
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

PlayerControl::PlayerControl(QObject *parent)
    : QObject(parent), frameRate(1), currentFrame(0), maxFrame(0), loop(0)
{
}

void PlayerControl::start()
{
    if (currentFrame >= maxFrame)
    {
        currentFrame = 0;
    }
    timerId = startTimer(1.0 / frameRate * 1000.0);
    emit started();
    emit frameChanged(currentFrame);
}

void PlayerControl::stop()
{
    killTimer(timerId);
    emit stopped();
}

void PlayerControl::timerEvent(QTimerEvent *)
{
    currentFrame++;
    if (currentFrame >= maxFrame)
    {
        if (loop)
        {
            currentFrame = 0;
        }
        else
        {
            stop();
        }
    }
    else
    {
        emit frameChanged(currentFrame);
    }
}