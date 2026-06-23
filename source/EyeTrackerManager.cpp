#include "EyeTrackerManager.h"

#include <cmath>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
constexpr auto dwellTimeMs = 400.0;
constexpr auto horizontalDeadZone = 0.08f;
constexpr auto verticalDeadZone = 0.08f;
constexpr auto oscPort = 4242;
constexpr auto oscTimeoutMs = 500.0;
constexpr auto autoLaunchDelayMs = 1000.0;

#if JUCE_WINDOWS
BOOL CALLBACK setGazeOscWindowVisibility (HWND window, LPARAM parameter)
{
    DWORD windowProcessId = 0;
    GetWindowThreadProcessId (window, &windowProcessId);

    if (windowProcessId == static_cast<DWORD> (parameter))
        ShowWindow (window, SW_HIDE);

    return TRUE;
}
#endif
}

EyeTrackerManager::EyeTrackerManager()
{
    oscReceiver.addListener (this);
    oscConnected = oscReceiver.connect (oscPort);
}

EyeTrackerManager::~EyeTrackerManager()
{
    oscReceiver.removeListener (this);
    oscReceiver.disconnect();
    stopManagedGazeOsc();
}

void EyeTrackerManager::setEnabled (bool shouldBeEnabled)
{
    enabled.store (shouldBeEnabled);

    if (shouldBeEnabled)
    {
        enabledTimeMs = juce::Time::getMillisecondCounterHiRes();
        autoLaunchAttempted = false;
        gazeOscLaunchStatus.clear();
    }
    else
    {
        gazeValid.store (false);
        activeAction.store (Action::none);
        dwellProgress.store (0.0f);
        candidateAction = Action::none;
    }
}

bool EyeTrackerManager::isEnabled() const
{
    return enabled.load();
}

void EyeTrackerManager::update()
{
    if (! isEnabled())
        return;

    ensureGazeOscRunning();

    if (isReceivingOscGaze())
        updateFromOsc();
    else
        updateFromMousePosition();
}

bool EyeTrackerManager::isOscConnected() const
{
    return oscConnected;
}

bool EyeTrackerManager::isReceivingOscGaze() const
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto xAge = nowMs - lastOscXTimeMs.load();
    const auto yAge = nowMs - lastOscYTimeMs.load();
    return oscConnected && xAge >= 0.0 && yAge >= 0.0
        && xAge < oscTimeoutMs && yAge < oscTimeoutMs;
}

juce::String EyeTrackerManager::getSourceStatus() const
{
    if (isReceivingOscGaze())
        return gazeOscAutoStarted ? "Tobii OSC (auto-started)" : "Tobii OSC";

    if (gazeOscLaunchStatus.isNotEmpty())
        return gazeOscLaunchStatus;

    return "Waiting for Tobii OSC";
}

void EyeTrackerManager::ensureGazeOscRunning()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();

   #if JUCE_WINDOWS
    if (gazeOscProcessHandle != nullptr)
    {
        if (WaitForSingleObject (static_cast<HANDLE> (gazeOscProcessHandle), 0) == WAIT_TIMEOUT)
        {
            if (nowMs - gazeOscLaunchTimeMs < 5000.0)
                EnumWindows (setGazeOscWindowVisibility, static_cast<LPARAM> (gazeOscProcessId));

            return;
        }

        CloseHandle (static_cast<HANDLE> (gazeOscProcessHandle));
        gazeOscProcessHandle = nullptr;
        gazeOscProcessId = 0;
    }
   #endif

    if (isReceivingOscGaze() || autoLaunchAttempted || nowMs - enabledTimeMs < autoLaunchDelayMs)
        return;

    autoLaunchAttempted = true;
    const auto executable = findGazeOscExecutable();

    if (! executable.existsAsFile())
    {
        gazeOscLaunchStatus = "GazeOSC not found | Mouse fallback";
        return;
    }

   #if JUCE_WINDOWS
    STARTUPINFOW startupInfo {};
    startupInfo.cb = sizeof (startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION processInfo {};
    std::wstring command = L"\"";
    command += executable.getFullPathName().toWideCharPointer();
    command += L"\"";
    std::vector<wchar_t> commandBuffer (command.begin(), command.end());
    commandBuffer.push_back (L'\0');

    const auto started = CreateProcessW (
        executable.getFullPathName().toWideCharPointer(),
        commandBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_UNICODE_ENVIRONMENT,
        nullptr,
        executable.getParentDirectory().getFullPathName().toWideCharPointer(),
        &startupInfo,
        &processInfo);

    if (started != FALSE)
    {
        CloseHandle (processInfo.hThread);
        gazeOscProcessHandle = processInfo.hProcess;
        gazeOscProcessId = processInfo.dwProcessId;
        gazeOscLaunchTimeMs = nowMs;
        gazeOscAutoStarted = true;
        gazeOscLaunchStatus = "GazeOSC starting | Mouse fallback";
        EnumWindows (setGazeOscWindowVisibility, static_cast<LPARAM> (gazeOscProcessId));
    }
    else
    {
        gazeOscLaunchStatus = "GazeOSC failed to start | Mouse fallback";
    }
   #else
    gazeOscLaunchStatus = "Start GazeOSC manually | Mouse fallback";
   #endif
}

void EyeTrackerManager::stopManagedGazeOsc()
{
   #if JUCE_WINDOWS
    if (gazeOscProcessHandle == nullptr)
        return;

    EnumWindows ([] (HWND window, LPARAM parameter) -> BOOL {
        DWORD windowProcessId = 0;
        GetWindowThreadProcessId (window, &windowProcessId);

        if (windowProcessId == static_cast<DWORD> (parameter))
            PostMessageW (window, WM_CLOSE, 0, 0);

        return TRUE;
    }, static_cast<LPARAM> (gazeOscProcessId));

    if (WaitForSingleObject (static_cast<HANDLE> (gazeOscProcessHandle), 1000) == WAIT_TIMEOUT)
        TerminateProcess (static_cast<HANDLE> (gazeOscProcessHandle), 0);

    CloseHandle (static_cast<HANDLE> (gazeOscProcessHandle));
    gazeOscProcessHandle = nullptr;
    gazeOscProcessId = 0;
   #endif
}

juce::File EyeTrackerManager::findGazeOscExecutable() const
{
    const auto environmentPath = juce::SystemStats::getEnvironmentVariable ("GAZEOSC_PATH", {});

    if (environmentPath.isNotEmpty())
    {
        const juce::File configuredPath (environmentPath);
        const auto configuredExecutable = configuredPath.isDirectory()
                                            ? configuredPath.getChildFile ("gazeOSC.exe")
                                            : configuredPath;

        if (configuredExecutable.existsAsFile())
            return configuredExecutable;
    }

    const auto appExecutable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    const auto currentDirectory = juce::File::getCurrentWorkingDirectory();
    const std::array candidates {
        appExecutable.getSiblingFile ("GazeOSC").getChildFile ("gazeOSC.exe"),
        appExecutable.getSiblingFile ("gazeOSC.exe"),
        currentDirectory.getChildFile ("GazeOSC").getChildFile ("gazeOSC.exe")
    };

    for (const auto& candidate : candidates)
        if (candidate.existsAsFile())
            return candidate;

    return {};
}

void EyeTrackerManager::oscMessageReceived (const juce::OSCMessage& message)
{
    if (message.size() < 1)
        return;

    const auto& argument = message[0];
    float value = 0.0f;

    if (argument.isFloat32())
        value = argument.getFloat32();
    else if (argument.isInt32())
        value = static_cast<float> (argument.getInt32());
    else
        return;

    const auto address = message.getAddressPattern().toString();
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();

    if (address == "/gaze/x")
    {
        pendingOscX.store (value);
        lastOscXTimeMs.store (nowMs);
    }
    else if (address == "/gaze/y")
    {
        pendingOscY.store (value);
        lastOscYTimeMs.store (nowMs);
    }
}

void EyeTrackerManager::oscBundleReceived (const juce::OSCBundle& bundle)
{
    processOscBundle (bundle);
}

void EyeTrackerManager::processOscBundle (const juce::OSCBundle& bundle)
{
    for (const auto& element : bundle)
    {
        if (element.isMessage())
            oscMessageReceived (element.getMessage());
        else if (element.isBundle())
            processOscBundle (element.getBundle());
    }
}

void EyeTrackerManager::updateFromOsc()
{
    const auto x = normaliseCoordinate (pendingOscX.load(), true);
    const auto y = normaliseCoordinate (pendingOscY.load(), false);
    updateGazePosition (x, y, true);
}

void EyeTrackerManager::updateFromMousePosition()
{
    const auto mousePosition = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
    const auto display = juce::Desktop::getInstance().getDisplays().getDisplayForPoint (mousePosition);

    if (display == nullptr
        || display->logicalBounds.getWidth() <= 0
        || display->logicalBounds.getHeight() <= 0)
    {
        gazeValid.store (false);
        activeAction.store (Action::none);
        return;
    }

    const auto normalisedX = juce::jlimit (
        0.0f,
        1.0f,
        static_cast<float> (mousePosition.x - display->logicalBounds.getX())
            / static_cast<float> (display->logicalBounds.getWidth()));

    const auto normalisedY = juce::jlimit (
        0.0f,
        1.0f,
        static_cast<float> (mousePosition.y - display->logicalBounds.getY())
            / static_cast<float> (display->logicalBounds.getHeight()));

    updateGazePosition (normalisedX, normalisedY, true);
}

void EyeTrackerManager::updateGazePosition (float x, float y, bool valid)
{
    if (! valid)
    {
        gazeValid.store (false);
        activeAction.store (Action::none);
        return;
    }

    // Eye data is naturally noisy, so smooth both real gaze and the mouse fallback.
    smoothedGazeX += 0.12f * (juce::jlimit (0.0f, 1.0f, x) - smoothedGazeX);
    smoothedGazeY += 0.12f * (juce::jlimit (0.0f, 1.0f, y) - smoothedGazeY);
    gazeX.store (smoothedGazeX);
    gazeY.store (smoothedGazeY);
    gazeValid.store (true);

    const auto newCandidate = classifyGaze (smoothedGazeX, smoothedGazeY);
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();

    if (newCandidate != candidateAction)
    {
        candidateAction = newCandidate;
        candidateStartTimeMs = nowMs;
        activeAction.store (Action::none);
        dwellProgress.store (0.0f);
        return;
    }

    if (candidateAction == Action::none)
    {
        activeAction.store (Action::none);
        dwellProgress.store (0.0f);
        return;
    }

    const auto progress = static_cast<float> ((nowMs - candidateStartTimeMs) / dwellTimeMs);
    dwellProgress.store (juce::jlimit (0.0f, 1.0f, progress));
    activeAction.store (progress >= 1.0f ? candidateAction : Action::none);
}

float EyeTrackerManager::normaliseCoordinate (float value, bool horizontal) const
{
    if (value >= 0.0f && value <= 1.0f)
        return value;

    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();

    if (display == nullptr)
        return 0.5f;

    const auto dimension = horizontal ? display->logicalBounds.getWidth()
                                      : display->logicalBounds.getHeight();
    return dimension > 0 ? juce::jlimit (0.0f, 1.0f, value / static_cast<float> (dimension))
                         : 0.5f;
}

bool EyeTrackerManager::hasValidGaze() const
{
    return gazeValid.load();
}

float EyeTrackerManager::getNormalisedGazeX() const
{
    return gazeX.load();
}

float EyeTrackerManager::getNormalisedGazeY() const
{
    return gazeY.load();
}

EyeTrackerManager::Action EyeTrackerManager::getActiveAction() const
{
    return activeAction.load();
}

float EyeTrackerManager::getDwellProgress() const
{
    return dwellProgress.load();
}

juce::String EyeTrackerManager::getActionDescription() const
{
    const auto action = activeAction.load() != Action::none ? activeAction.load() : candidateAction;

    switch (action)
    {
        case Action::decreaseMid: return "Upper-left: decrease Mid Xover";
        case Action::increaseMid: return "Upper-right: increase Mid Xover";
        case Action::decreaseLow: return "Lower-left: decrease Low Xover";
        case Action::increaseLow: return "Lower-right: increase Low Xover";
        case Action::none: break;
    }

    return "Centre dead zone";
}

EyeTrackerManager::Action EyeTrackerManager::classifyGaze (float x, float y) const
{
    const auto horizontalDistance = x - 0.5f;
    const auto verticalDistance = y - 0.5f;

    if (std::abs (horizontalDistance) < horizontalDeadZone
        || std::abs (verticalDistance) < verticalDeadZone)
        return Action::none;

    const auto lookingLeft = horizontalDistance < 0.0f;
    const auto lookingUp = verticalDistance < 0.0f;

    if (lookingUp)
        return lookingLeft ? Action::decreaseMid : Action::increaseMid;

    return lookingLeft ? Action::decreaseLow : Action::increaseLow;
}
