# Changelog

All notable changes to the NeuralMic project will be documented in this file.

---

## [0.1.0] — 2026-07-05

### Added

- **System tray integration** — The GUI now minimizes to the system tray instead of quitting. Includes:
  - Right-click context menu with Show/Hide, Start/Stop, and Quit actions
  - Double-click tray icon to restore/hide the window
  - Middle-click tray icon to quickly toggle denoising
  - Minimize-to-tray (window hides entirely when minimized)
  - Desktop notifications on denoising start/stop
  - Graceful fallback on systems without tray support (e.g., pure Wayland)
- **New application icon** (`icon.png`) used for the tray, taskbar/dock, and window decoration
- **Version label** displayed in the GUI footer for easy identification (`v0.1.0`)
- **Version propagated via CMake** — `NEURALMIC_VERSION` preprocessor define shared across all GUI sources
- **`.deb` packaging** includes the new `icon.png` asset

### Changed

- **Desktop entry** icon path updated from `logo.png` to `icon.png`
- **Version bumped** from `0.0.1` → `0.1.0`
- Application icon resolution uses the same 3-location search as other assets (dev → system → local)

### Fixed

- Application no longer exits when the window is closed — stays alive in tray for continuous denoising

---

## [0.0.1] — 2026-05-25

### Added

- Initial release with real-time AI noise suppression via DeepFilterNet ONNX model
- CLI tool (`NeuralMic`) for file batch processing and real-time denoising
- Qt GUI (`NeuralMicGUI`) with device selection, strength slider, and monitoring controls
- Virtual microphone support via PulseAudio for use with Discord, Zoom, and other apps
- Custom toggle switch and percentage slider widgets
- `.deb` packaging via CPack
- CI/CD workflow for automated builds
