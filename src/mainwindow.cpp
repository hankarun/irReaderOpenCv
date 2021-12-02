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

    connect(ui->scaleSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value)
            { this->setScaleFactor(value); });

    connect(ui->minRadians, qOverload<int>(&QSpinBox::valueChanged), this, [this]()
            {
                if (!currentFrameData)
                    return;
                currentFrameData->setMinMax(ui->minRadians->value(), ui->maxRadians->value());
                updateGrayTexture(currentFrameData);
            });
    connect(ui->maxRadians, qOverload<int>(&QSpinBox::valueChanged), this, [this]()
            {
                if (!currentFrameData)
                    return;
                currentFrameData->setMinMax(ui->minRadians->value(), ui->maxRadians->value());
                updateGrayTexture(currentFrameData);
            });
    connect(ui->reset, &QToolButton::clicked, this, [this]()
            {
                if (!currentFrameData)
                    return;
                currentFrameData->resetMinMax();
                ui->minRadians->setValue(currentFrameData->minValue);
                ui->maxRadians->setValue(currentFrameData->maxValue);
            });
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
    project = std::make_unique<Project>();
    project->loadFromAvi(filename.toStdString().c_str());
    updateList();
    setWindowTitle(tr("IR Data Analyser (%1)").arg(QFileInfo(filename).fileName()));
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

void MainWindow::updateGrayTexture(FrameData *frameData)
{
    if (!frameData)
        return;
    auto grayScale = frameData->gray;
    int height = grayScale.height() * scaleFactor;
    int width = grayScale.width() * scaleFactor;
    gray->setMaximumHeight(height);
    gray->setMaximumWidth(width);
    gray->setPixmap(QPixmap::fromImage(grayScale).scaled(width, height));
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

    resetMinMax();

    gray = QImage((uchar *)frameShortData.data, frameShortData.cols, frameShortData.rows, QImage::Format_Grayscale8);
    green = QImage((uchar *)frameData.data, frameData.cols, frameData.rows, QImage::Format_BGR888);
}

void FrameData::resetMinMax()
{
    cv::minMaxLoc(ushortData, &minValue, &maxValue);
    setMinMax(minValue, maxValue);
}
void FrameData::setMinMax(double min, double max)
{
    minValue = min;
    maxValue = max;
    for (int r = 0; r < frameData.rows; ++r)
    {
        for (int c = 0; c < frameData.cols; ++c)
        {
            frameShortData.at<unsigned char>(r, c) = ((ushortData.at<unsigned short>(r, c) - minValue) / (maxValue - minValue)) * 255.0f;
        }
    }
}

void Project::loadFromAvi(const char *filename)
{
    cv::VideoCapture cap(filename);

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

        this->data.push_back(frameData);
    }
    cap.release();
}

void Project::loadFromPng(const std::vector<QString> &filenames)
{
    for (auto &file : filenames)
    {
        cv::Mat data = cv::imread(file.toStdString().c_str(), cv::IMREAD_COLOR);
        FrameData frameData;
        frameData.set(data);
        this->data.push_back(frameData);
    }
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
    }
    else if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->pos().x() < 0 || mouseEvent->pos().y() < 0)
            return false;
        int posX = mouseEvent->pos().x();
        int posY = mouseEvent->pos().y();
        setPosition(posX, posY);
    }
    else if (event->type() == QEvent::Wheel)
    {
        if (QApplication::keyboardModifiers() & Qt::ControlModifier)
        {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
            const int delta = wheelEvent->delta();
            if (delta > 0)
            {
                scaleFactor += 0.2;
            }
            else
            {
                scaleFactor -= 0.2;
            }
            scaleFactor = std::max(1.0f, scaleFactor);
            scaleFactor = std::min(5.0f, scaleFactor);
            setScaleFactor(scaleFactor);
        }
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

void MainWindow::setScaleFactor(float value)
{
    ui->scaleSpin->setValue(value);
    this->scaleFactor = value;
    setCurrentFrameData(currentFrameData);
}

void MainWindow::setCurrentFrameData(FrameData *frameData)
{
    if (frameData)
    {
        auto image = frameData->green;

        int height = image.height() * scaleFactor;
        int width = image.width() * scaleFactor;

        green->setMaximumHeight(height);
        green->setMaximumWidth(width);
        green->setPixmap(QPixmap::fromImage(image).scaled(width, height));

        updateGrayTexture(frameData);

        currentFrameData = nullptr;
        ui->minRadians->setValue(frameData->minValue);
        ui->maxRadians->setValue(frameData->maxValue);
        currentFrameData = frameData;
    }
    else
    {
        green->setPixmap(QPixmap());
        gray->setPixmap(QPixmap());
        ui->minRadians->setValue(0);
        ui->maxRadians->setValue(65535);
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
            project = std::make_unique<Project>();

            std::vector<QString> files;
            for (auto &file : urlList)
            {
                files.push_back(file.toLocalFile());
            }
            project->loadFromPng(files);
            updateList();
            setWindowTitle(tr("IR Data Analyser (PNG)"));
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