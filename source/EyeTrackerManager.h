#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_osc/juce_osc.h>

#include <atomic>

class EyeTrackerManager : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    enum class Action
    {
        none,
        decreaseMid,
        increaseMid,
        decreaseLow,
        increaseLow
    };

    EyeTrackerManager();
    ~EyeTrackerManager();

    void setEnabled (bool shouldBeEnabled);
    bool isEnabled() const;

    void update();

    bool hasValidGaze() const;
    bool isOscConnected() const;
    bool isReceivingOscGaze() const;
    juce::String getSourceStatus() const;
    float getNormalisedGazeX() const;
    float getNormalisedGazeY() const;
    Action getActiveAction() const;
    float getDwellProgress() const;
    juce::String getActionDescription() const;

private:
    void oscMessageReceived (const juce::OSCMessage& message) override;
    void oscBundleReceived (const juce::OSCBundle& bundle) override;
    void processOscBundle (const juce::OSCBundle& bundle);
    void updateFromOsc();
    void updateFromMousePosition();
    void updateGazePosition (float x, float y, bool valid);
    void ensureGazeOscRunning();
    void stopManagedGazeOsc();
    juce::File findGazeOscExecutable() const;
    float normaliseCoordinate (float value, bool horizontal) const;
    Action classifyGaze (float x, float y) const;

    juce::OSCReceiver oscReceiver;
    bool oscConnected = false;
    std::atomic<float> pendingOscX { 0.5f };
    std::atomic<float> pendingOscY { 0.5f };
    std::atomic<double> lastOscXTimeMs { 0.0 };
    std::atomic<double> lastOscYTimeMs { 0.0 };
    double enabledTimeMs = 0.0;
    bool autoLaunchAttempted = false;
    bool gazeOscAutoStarted = false;
    juce::String gazeOscLaunchStatus;

   #if JUCE_WINDOWS
    void* gazeOscProcessHandle = nullptr;
    unsigned long gazeOscProcessId = 0;
    double gazeOscLaunchTimeMs = 0.0;
   #endif

    std::atomic<bool> enabled { false };
    std::atomic<bool> gazeValid { false };
    std::atomic<float> gazeX { 0.5f };
    std::atomic<float> gazeY { 0.5f };
    std::atomic<Action> activeAction { Action::none };
    std::atomic<float> dwellProgress { 0.0f };
    float smoothedGazeX = 0.5f;
    float smoothedGazeY = 0.5f;
    Action candidateAction = Action::none;
    double candidateStartTimeMs = 0.0;
};
