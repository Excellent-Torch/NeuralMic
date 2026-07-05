#include "MainWindow.h"
#include "Core/RealtimeDenoiser.h"
#include "SwitchToggle.h"
#include "PercentSlider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>

static QString findAsset(const QString& relativePath) {
    // 1. Development path: <exe>/../assets/...
    QString devPath = QApplication::applicationDirPath() + "/../assets/" + relativePath;
    if (QFileInfo::exists(devPath)) return devPath;

    // 2. Installed path: /usr/share/neuralmic/...
    QString sysPath = "/usr/share/neuralmic/" + relativePath;
    if (QFileInfo::exists(sysPath)) return sysPath;

    // 3. Local share path: /usr/local/share/neuralmic/...
    QString localPath = "/usr/local/share/neuralmic/" + relativePath;
    if (QFileInfo::exists(localPath)) return localPath;

    return devPath; // fallback (will fail gracefully)
}

// Base AudioWorker Implementation
AudioWorker::AudioWorker(RealtimeDenoiser* denoiser) 
    : denoiser_(denoiser) {}

void AudioWorker::start() {
    if (!denoiser_->initialize()) {
        emit error("Failed to initialize audio devices");
        return;
    }
    
    // Emit started BEFORE blocking call
    emit started();
    
    // This blocks until stop() is called from main thread
    denoiser_->start();
    
    // When we exit the loop, emit stopped
    emit stopped();
}

// A Simple MainWindow Implementation
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , denoiser_(std::make_unique<RealtimeDenoiser>())
{
    setupUi();
    setupConnections();
    loadModel();
    onRefreshDevices();
}

MainWindow::~MainWindow() {
    if (running_) {
        denoiser_->stop();
    }
    if (audioThread_) {
        audioThread_->quit();
        audioThread_->wait();
    }
}

void MainWindow::setupUi() {
    setWindowTitle("NeuralMic");
    setMinimumSize(440, 480);
    setMaximumSize(640, 560);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setSpacing(6);
    layout->setContentsMargins(16, 14, 16, 14);

    // === Logo ===
    auto* logoLabel = new QLabel(this);
    QPixmap logo(findAsset("images/logo.png"));
    if (!logo.isNull()) {
        logoLabel->setPixmap(logo.scaledToWidth(280, Qt::SmoothTransformation));
        logoLabel->setAlignment(Qt::AlignCenter);
        logoLabel->setContentsMargins(0, 4, 0, 8);
        layout->addWidget(logoLabel);
    }

    // === Device Selection ===
    auto* deviceGroup = new QGroupBox("Audio Devices", this);
    auto* deviceLayout = new QVBoxLayout(deviceGroup);
    deviceLayout->setSpacing(6);

    // Microphone row
    auto* micRow = new QHBoxLayout();
    micRow->addWidget(new QLabel("Input:", this));
    ui_.micCombo = new QComboBox(this);
    ui_.micCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    micRow->addWidget(ui_.micCombo, 1);
    deviceLayout->addLayout(micRow);

    // Speaker row
    auto* speakerRow = new QHBoxLayout();
    speakerRow->addWidget(new QLabel("Output:", this));
    ui_.speakerCombo = new QComboBox(this);
    ui_.speakerCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    speakerRow->addWidget(ui_.speakerCombo, 1);
    deviceLayout->addLayout(speakerRow);

    layout->addWidget(deviceGroup);

    // === Noise Suppression ===
    auto* noiseGroup = new QGroupBox("Noise Suppression", this);
    auto* noiseLayout = new QHBoxLayout(noiseGroup);

    noiseLayout->addWidget(new QLabel("Strength:", this));
    
    ui_.strengthSlider = new PercentSlider(this);
    ui_.strengthSlider->setValue(0);
    noiseLayout->addWidget(ui_.strengthSlider, 1);
    
    ui_.strengthValue = new QLabel("0%", this);
    ui_.strengthValue->setMinimumWidth(50);
    ui_.strengthValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    noiseLayout->addWidget(ui_.strengthValue);

    layout->addWidget(noiseGroup);

    // === Options ===
    auto* optionsGroup = new QGroupBox("Options", this);
    auto* optionsLayout = new QHBoxLayout(optionsGroup);
    optionsLayout->setSpacing(24);

    // Monitor toggle
    auto* monitorRow = new QHBoxLayout();
    monitorRow->addWidget(new QLabel("Monitor:", this));
    ui_.monitorToggle = new SwitchToggle(this);
    ui_.monitorToggle->setToolTip("Hear processed audio through speakers");
    monitorRow->addWidget(ui_.monitorToggle);
    monitorRow->addStretch();
    optionsLayout->addLayout(monitorRow);

    // Virtual mic toggle
    auto* virtualMicRow = new QHBoxLayout();
    virtualMicRow->addWidget(new QLabel("Virtual Mic:", this));
    ui_.virtualMicToggle = new SwitchToggle(this);
    ui_.virtualMicToggle->setChecked(true);
    ui_.virtualMicToggle->setToolTip("Use in Discord, games, etc.");
    virtualMicRow->addWidget(ui_.virtualMicToggle);
    virtualMicRow->addStretch();
    optionsLayout->addLayout(virtualMicRow);

    layout->addWidget(optionsGroup);

    // === Controls ===
    auto* controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(8);
    
    ui_.refreshBtn = new QPushButton("Refresh", this);
    ui_.refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    controlLayout->addWidget(ui_.refreshBtn);
    
    controlLayout->addStretch();
    
    ui_.startBtn = new QPushButton("Start", this);
    ui_.startBtn->setMinimumWidth(120);
    ui_.startBtn->setMinimumHeight(36);
    ui_.startBtn->setStyleSheet(
        "QPushButton { font-weight: bold; padding: 8px 16px; }"
        "QPushButton:disabled { background: #ccc; }"
    );
    controlLayout->addWidget(ui_.startBtn);

    layout->addLayout(controlLayout);

    // === Status ===
    ui_.status = new QLabel("Ready", this);
    ui_.status->setStyleSheet("QLabel { color: #666; padding: 4px; }");
    ui_.status->setAlignment(Qt::AlignCenter);
    layout->addWidget(ui_.status);

    layout->addStretch();
    setCentralWidget(central);
}

void MainWindow::setupConnections() {
    connect(ui_.startBtn, &QPushButton::clicked, this, &MainWindow::onStartStop);
    connect(ui_.refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshDevices);
    connect(ui_.strengthSlider, &PercentSlider::valueChanged, this, &MainWindow::onStrengthChanged);
    connect(ui_.monitorToggle, &SwitchToggle::toggled, this, &MainWindow::onMonitorToggled);
    connect(ui_.virtualMicToggle, &SwitchToggle::toggled, this, &MainWindow::onVirtualMicToggled);
}

void MainWindow::loadModel() {
    const QString modelPath = findAsset("models/DeepFilterNetV3.onnx");
    
    if (denoiser_->loadModel(modelPath.toStdString())) 
    {
        updateStatus("Model loaded");
    } else {
        updateStatus("Failed to load model", true);
        ui_.startBtn->setEnabled(false);
    }
}

void MainWindow::onRefreshDevices() {
    ui_.micCombo->clear();
    ui_.speakerCombo->clear();

    for (const auto& mic : denoiser_->listMicrophones()) 
    {
        ui_.micCombo->addItem(QString::fromStdString(mic));
    }
    for (const auto& speaker : denoiser_->listSpeakers()) 
    {
        ui_.speakerCombo->addItem(QString::fromStdString(speaker));
    }
}

void MainWindow::onStartStop() {
    if (!running_) 
    {
        // Configure denoiser
        denoiser_->selectMicrophone(ui_.micCombo->currentIndex());
        denoiser_->selectSpeaker(ui_.speakerCombo->currentIndex());
        denoiser_->enableMonitoring(ui_.monitorToggle->isChecked());
        denoiser_->enableVirtualMic(ui_.virtualMicToggle->isChecked());
        denoiser_->setNoiseSuppressionStrength(ui_.strengthSlider->value());

        // Setup worker thread
        audioThread_ = new QThread(this);
        audioWorker_ = new AudioWorker(denoiser_.get());
        audioWorker_->moveToThread(audioThread_);

        connect(audioThread_, &QThread::started, audioWorker_, &AudioWorker::start);
        connect(audioWorker_, &AudioWorker::started, this, &MainWindow::onAudioStarted);
        connect(audioWorker_, &AudioWorker::stopped, this, &MainWindow::onAudioStopped);
        connect(audioWorker_, &AudioWorker::error, this, &MainWindow::onAudioError);
        connect(audioThread_, &QThread::finished, audioWorker_, &QObject::deleteLater);

        ui_.startBtn->setEnabled(false);
        updateStatus("Starting...");
        audioThread_->start();
    } else {
        ui_.startBtn->setEnabled(false);
        updateStatus("Stopping...");
        
        // Call stop directly - it just sets atomic flags, safe from any thread
        // The blocking loop in start() will exit and emit stopped()
        denoiser_->stop();
    }
}

void MainWindow::onAudioStarted() {
    running_ = true;
    setRunningState(true);
    updateStatus("● Running");
}

void MainWindow::onAudioStopped() {
    running_ = false;
    
    if (audioThread_) 
    {
        audioThread_->quit();
        audioThread_->wait();
        audioThread_->deleteLater();
        audioThread_ = nullptr;
        audioWorker_ = nullptr;
    }
    
    setRunningState(false);
    updateStatus("Stopped");
    
    // Refresh devices since mic_reader was reset
    onRefreshDevices();
}

void MainWindow::onAudioError(const QString& msg) {
    running_ = false;
    
    if (audioThread_) 
    {
        audioThread_->quit();
        audioThread_->wait();
        audioThread_->deleteLater();
        audioThread_ = nullptr;
        audioWorker_ = nullptr;
    }
    
    setRunningState(false);
    updateStatus(msg, true);
    
    // Refresh devices since mic_reader was reset
    onRefreshDevices();
    
    QMessageBox::warning(this, "Audio Error", msg);
}

void MainWindow::setRunningState(bool running) {
    ui_.startBtn->setEnabled(true);
    ui_.startBtn->setText(running ? "Stop" : "Start");
    ui_.startBtn->setStyleSheet(running
        ? "QPushButton { font-weight: bold; padding: 8px 16px; background: #c0392b; color: white; }"
        : "QPushButton { font-weight: bold; padding: 8px 16px; }"
    );
    
    ui_.micCombo->setEnabled(!running);
    ui_.speakerCombo->setEnabled(!running);
    ui_.refreshBtn->setEnabled(!running);
    ui_.virtualMicToggle->setEnabled(!running);
    ui_.monitorToggle->setEnabled(!running);
}

void MainWindow::onStrengthChanged(int value) {
    ui_.strengthValue->setText(QString("%1%").arg(value));
    if (denoiser_) 
    {
        // Convert 0-100% to 0 to -30 dB range
        float db = -static_cast<float>(value) * 0.3f;
        denoiser_->setNoiseSuppressionStrength(db);
    }
}

void MainWindow::onMonitorToggled(bool checked) {
    if (denoiser_) 
    {
        denoiser_->enableMonitoring(checked);
    }
}

void MainWindow::onVirtualMicToggled(bool checked) {
    if (denoiser_) 
    {
        denoiser_->enableVirtualMic(checked);
    }
}

void MainWindow::updateStatus(const QString& status, bool isError) {
    ui_.status->setText(status);

    if (running_) 
    {
        ui_.status->setStyleSheet("QLabel { color: #27ae60; padding: 4px; font-weight: bold; }");
        return;
    }

    ui_.status->setStyleSheet(isError 
        ? "QLabel { color: #c0392b; padding: 4px; font-weight: bold; }"
        : "QLabel { color: #666; padding: 4px; }"
    );
}
