#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QThread>
#include <memory>
#include <atomic>

class SwitchToggle;
class PercentSlider;

class RealtimeDenoiser;

// Worker for audio processing on separate thread
class AudioWorker : public QObject {
    Q_OBJECT
public:
    explicit AudioWorker(RealtimeDenoiser* denoiser);

public slots:
    void start();

signals:
    void started();
    void stopped();
    void error(const QString& msg);

private:
    RealtimeDenoiser* denoiser_;
};

// Main application window
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onStartStop();
    void onStrengthChanged(int value);
    void onMonitorToggled(bool checked);
    void onVirtualMicToggled(bool checked);
    void onRefreshDevices();
    
    void onAudioStarted();
    void onAudioStopped();
    void onAudioError(const QString& msg);

private:
    void setupUi();
    void setupConnections();
    void loadModel();
    void setRunningState(bool running);
    void updateStatus(const QString& status, bool isError = false);

    // Audio
    std::unique_ptr<RealtimeDenoiser> denoiser_;
    QThread* audioThread_ = nullptr;
    AudioWorker* audioWorker_ = nullptr;
    std::atomic<bool> running_{false};

    // UI Components
    struct {
        QComboBox* micCombo;
        QComboBox* speakerCombo;
        PercentSlider* strengthSlider;
        QLabel* strengthValue;
        SwitchToggle* monitorToggle;
        SwitchToggle* virtualMicToggle;
        QPushButton* startBtn;
        QPushButton* refreshBtn;
        QLabel* status;
    } ui_{};
};
