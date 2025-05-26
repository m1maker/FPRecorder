# FPRecorder (Fast Portable Recorder)

## 1. What is FPRecorder?

FPRecorder is a Windows application designed for capturing audio from your microphone and/or your system's audio output (loopback). It provides a graphical user interface (GUI) for easy operation, along with hotkey support for quick access to recording functions. Recordings are saved locally, and the application can leverage FFmpeg (if installed and configured) to convert recordings to various audio formats beyond the default WAV.

**Key Features:**

*   **Dual Audio Source Recording:** Record from your microphone, system audio (what you hear), or both simultaneously.
*   **Stem Recording:** Option to save microphone and loopback audio into separate files when recording both.
*   **GUI Interface:** Easy-to-use windows for starting/stopping recordings, managing devices, accessing settings, and managing recorded files.
*   **Recordings Manager:** Built-in tool to browse, play, and delete your recordings.
*   **Settings Window:** Configure various aspects of recording, output, and hotkeys through a dedicated GUI.
*   **Hotkey Support:** Control recording (start/stop, pause/resume, restart, hiding/showing window) without needing the application window in focus.
*   **Configurable Output:**
    *   Set sample rate, channels, and buffer size.
    *   Customize filename patterns using date/time.
    *   Choose the recording path.
    *   Select audio bit depth/format (e.g., 16-bit, 24-bit, 32-bit float).
*   **FFmpeg Integration:** Convert recordings from WAV to other formats (MP3, OGG, FLAC, etc.) using custom FFmpeg commands via presets.
*   **Audio & Speech Feedback:** Optional sound cues and screen reader announcements for important actions.
*   **Command-Line Options:** Start recording automatically or specify a filename on launch.
*   **Configuration File:** Fine-tune settings via an `fp.ini` file; settings made in the GUI are saved here.

## 2. System Requirements

*   **Operating System:** Windows (developed with Windows-specific APIs).
*   **Audio Devices:** At least one microphone and/or playback device.
*   **FFmpeg (Optional):** Required if you want to save recordings in formats other than WAV. FFmpeg must be downloadable and its `ffmpeg.exe` should be in your system's PATH or in the same directory as FPRecorder.exe.

## 3. Getting Started: First Run

When you run `FPRecorder.exe` for the first time, it will detect that no configuration file (`fp.ini`) exists.

1.  FPRecorder will create a default `fp.ini` file in the same directory as `FPRecorder.exe`.
2.  The main application window will open.

## 4. Main Window Interface

The main window is your central hub for starting recordings and managing devices.

*   **Start recording Button:** Initiates a new recording session.
*   **Input devices List:**
    *   Shows available microphone/input devices.
    *   `Default`: Uses your system's default input device.
    *   `Not used`: Disables microphone recording for this session.
    *   Select your desired microphone here.
*   **Output loopback devices List:**
    *   Shows available playback devices for capturing system audio.
    *   `Not used`: Disables system audio (loopback) recording for this session.
    *   Select the device whose output you want to capture (e.g., your speakers or headphones).
*   **Recordings manager Button:** Opens a window to manage your saved recordings.
*   **Settings Button:** Opens the Settings window to configure application behavior.

## 5. Recording Audio

### Starting a Recording

1.  In the main window, select your desired **Input device** (microphone).
2.  Select your desired **Output loopback device** if you want to record system audio.
    *   To record *only* microphone: Set Output device to "Not used".
    *   To record *only* system audio: Set Input device to "Not used".
    *   To record *both*: Select active devices for both.
3.  Click the **Start recording** button.
    *   The main window will be replaced by the "Recording Window".
    *   If sound events are enabled, you'll hear a beep.

### Recording Window

Once recording starts, this window appears:

*   **Stop recording Button:** Stops the current recording and saves the file.
*   **Pause recording / Resume recording Button:** Toggles between pausing and resuming the recording.
*   **Restart recording Button:** Stops the current recording, *deletes it*, and immediately starts a new one with the same settings. You'll be asked for confirmation.
*   **Hide window Button:** Hides the recording window. To show it, please press hide/show hotkey.

### Hotkeys for Recording

You can control recording even when FPRecorder is not the active window using global hotkeys:

*   **Start/Stop Recording:** `Windows + Shift + F1` (Default)
*   **Pause/Resume Recording:** `Windows + Shift + F2` (Default)
*   **Restart Recording:** `Windows + Shift + F3` (Default)
*   **Hide/Show Window:** `Windows + Shift + F4` (Default)

*(These can be changed in the Settings window or `fp.ini`)*

### File Naming and Location

*   By default, recordings are saved in a folder named `recordings` created in the same directory as `FPRecorder.exe`.
*   The default filename pattern is based on the current date and time (e.g., `YYYY MM DD HH MM SS.wav`).
*   If FFmpeg is used for conversion (e.g., to MP3), the original WAV file is typically deleted after successful conversion, leaving you with the converted file (e.g., `YYYY MM DD HH MM SS.mp3`).
*   If "Make Stems" is enabled and you record both mic and loopback, you'll get two files, e.g., `YYYY MM DD HH MM SS Mic.wav` and `YYYY MM DD HH MM SS Loopback.wav`.

## 6. Managing Recordings (Recordings Manager)

Click the **Recordings manager** button on the main window to access your recordings.

*   **Items view List:** Displays all recorded files in your `record-path`.
*   **Play Button:** Plays the selected recording.
*   **Pause Button:** Pauses playback of the current recording.
*   **Stop Button:** Stops playback and resets to the beginning.
*   **Delete Button:** Permanently deletes the selected recording. You'll be asked for confirmation.
*   **Close Button:** Closes the Recordings Manager and returns to the main window.
*   You can also use `Spacebar` to play/pause when a file is selected in the list, and `Delete` key to delete. Press `Escape` to close the manager.

## 7. Settings Window

Click the **Settings** button on the main window to open the Settings window. Here you can configure various aspects of FPRecorder. Changes made here are saved to the `fp.ini` file when you click "Save and Apply".

*   **Sample Rate (Hz):**
    *   Sets the audio sample rate for recordings (e.g., `44100`, `48000`).
    *   Higher values mean better quality but larger file sizes.
    *   Default: `44100`
*   **Channels:**
    *   Sets the number of audio channels: `1 (Mono)` or `2 (Stereo)`.
    *   Default: `2 (Stereo)`
*   **Buffer Size (ms, 0=default):**
    *   Audio buffer size in milliseconds. Lower values can reduce latency but might cause issues (like glitches or dropouts) on slower systems.
    *   Setting to `0` lets the system/driver choose a default buffer size.
    *   Default: `0`
*   **Filename Signature:**
    *   Defines the pattern for naming recorded files. It uses `strftime` format codes (e.g., `%Y` for year, `%m` for month, `%d` for day, `%H` for hour, `%M` for minute, `%S` for second).
    *   Default: `%Y %m %d %H %M %S` (e.g., "2023 10 27 14 30 55")
*   **Record Path:**
    *   The folder where recordings will be saved.
    *   Can be a relative path (e.g., `recordings`) or an absolute path (e.g., `C:\MyRecordings`).
    *   Click the "Browse..." button to browse for a folder.
    *   Default: `recordings`
*   **Audio Format:**
    *   Select the desired *final* audio format for your recordings.
    *   `wav`: Saves directly as a WAV file.
    *   Other formats (e.g., `mp3`, `flac`): Records as a temporary WAV file first, then uses FFmpeg (via the "Current FFmpeg Preset") to convert to the selected format. The original WAV is then deleted if conversion is successful. FFmpeg must be installed and accessible.
    *   Default: `wav`
*   **Enable Sound Events (Checkbox):**
    *   If checked, FPRecorder will play short sound cues (beeps) for actions like starting/stopping recording.
    *   Default: `Enabled`
*   **Make Stems (Checkbox):**
    *   If checked, and you are recording both microphone and loopback audio simultaneously, they will be saved as two separate WAV files (e.g., `filename Mic.wav` and `filename Loopback.wav`).
    *   If unchecked, they are mixed into a single file.
    *   This option only applies if "Audio Format" is set to `wav`.
    *   Default: `Disabled`
*   **Sample Format:**
    *   Determines the bit depth and data type of the recorded WAV file.
    *   Options: `u8` (unsigned 8-bit), `s16` (signed 16-bit), `s24` (signed 24-bit), `s32` (signed 32-bit), `f32` (32-bit floating point).
    *   Higher bit depths offer greater dynamic range. `s16` is common for general use, `f32` is often preferred for professional audio work due to its headroom.
    *   Default: `s16`
*   **Hotkey Settings:**
    *   **Start/Stop Hotkey:** Defines the global hotkey to start or stop recording.
        *   Format: `Modifier+Key` (e.g., `Windows+Shift+F1`, `Control+Alt+R`).
        *   Modifiers: `Control`, `Alt`, `Shift`, `Windows`.
        *   Default: `Windows+Shift+F1`
    *   **Pause/Resume Hotkey:** Defines the global hotkey to pause or resume recording.
        *   Default: `Windows+Shift+F2`
    *   **Restart Hotkey:** Defines the global hotkey to restart a recording (stops current, deletes it, and starts new).
        *   Default: `Windows+Shift+F3`
    *   **Hide/Show Hotkey:** Defines the global hotkey to hide or show FPRecorder window.
        *   Default: `Windows+Shift+F4`
*   **Current FFmpeg Preset:**
    *   Selects the FFmpeg preset to use for conversion if "Audio Format" is not `wav`.
    *   Presets define the FFmpeg command-line arguments for conversion. They are configured in the `[Presets]` section of `fp.ini`.
    *   Default: `Default` (which typically performs a basic conversion to the target format).

*   **Save and Apply Button:** Saves all changes made in the Settings window to `fp.ini` and applies them immediately (including re-registering hotkeys).
*   **Cancel Button:** Closes the Settings window without saving any changes made in the current session.

## 8. Advanced: Configuration (`fp.ini`)

FPRecorder's behavior can be customized by editing the `fp.ini` file located in the same directory as `FPRecorder.exe`. Open it with a text editor. Many of these settings can also be configured through the **Settings Window** (see section 7).

The file is organized into sections (e.g., `[General]`, `[Presets]`).

### `[General]` Settings:

*   `sample-rate`: Audio sample rate in Hz (e.g., `44100`, `48000`).
    *   Default: `44100`
*   `channels`: Number of audio channels (e.g., `1` for mono, `2` for stereo).
    *   Default: `2`
*   `buffer-size`: Audio buffer size in milliseconds. Lower values can mean lower latency but may cause issues on slower systems. `0` often means a default chosen by the system/driver.
    *   Default: `0`
*   `filename-signature`: Pattern for naming recorded files. Uses `strftime` format codes (e.g., `%Y` for year, `%m` for month, `%d` for day, `%H` for hour, `%M` for minute, `%S` for second).
    *   Default: `%Y %m %d %H %M %S` (results in "2023 10 27 14 30 55")
*   `record-path`: Folder where recordings are saved. Can be a relative path (e.g., `recordings`) or an absolute path (e.g., `D:\MyAudioCaptures`).
    *   Default: `recordings`
*   `audio-format`: The desired *final* audio format.
    *   `wav`: Saves directly as a WAV file.
    *   Other formats (e.g., `mp3`, `ogg`, `flac`): Records as WAV first, then uses FFmpeg (via the `current-preset`) to convert to this format. The original WAV is then deleted.
    *   Default: `wav`
*   `input-device`: Index of the default input device selected in the list (0-based). *This is primarily for remembering the last selection and not typically manually edited.*
    *   Default: `0`
*   `loopback-device`: Index of the default loopback device selected in thelist (0-based). *This is primarily for remembering the last selection and not typically manually edited.*
    *   Default: `0` (often "Not used" initially or if only one output)
*   `sound-events`: Enable (`true`) or disable (`false`) beeps for actions like start/stop recording.
    *   Default: `true`
*   `make-stems`: If `true` and recording both microphone and loopback, saves them as two separate WAV files (e.g., `filename Mic.wav` and `filename Loopback.wav`). If `false`, they are mixed into one file. This only applies if `audio-format` is `wav`.
    *   Default: `false`
*   `sample-format`: The bit depth and format of the recorded WAV file.
    *   Options: `u8` (unsigned 8-bit), `s16` (signed 16-bit), `s24` (signed 24-bit), `s32` (signed 32-bit), `f32` (32-bit floating point).
    *   Default: `s16`
*   `hotkey-start-stop`: Hotkey for starting/stopping recording. Format: `Modifier+Key` (e.g., `Windows+Shift+F1`, `Control+Alt+R`).
    *   Modifiers: `Control`, `Alt`, `Shift`, `Windows`.
    *   Default: `Windows+Shift+F1`
*   `hotkey-pause-resume`: Hotkey for pausing/resuming.
    *   Default: `Windows+Shift+F2`
*   `hotkey-restart`: Hotkey for restarting recording.
    *   Default: `Windows+Shift+F3`
*   `hotkey-hide-show`: Hotkey for hide or show FPRecorder window.
    *   Default: `Windows+Shift+F4`
*   `current-preset`: Name of the FFmpeg preset to use if `audio-format` is not `wav`. Must match a preset name in the `[Presets]` section.
    *   Default: `Default`

### `[Presets]` Settings:

This section defines FFmpeg commands for converting audio. Each line is a `PresetName = CommandString`.

*   **Command Placeholders:**
    *   `%I`: Full path to the input WAV file (e.g., `"recordings/2023 10 27 14 30 55.wav"`). **Use quotes around it: `"%I"`**.
    *   `%i`: Full path to the input file *without* the `.wav` extension (e.g., `"recordings/2023 10 27 14 30 55"`). **Use quotes around it: `"%i"`**.
    *   `%f`: The target audio format string from `audio-format` (e.g., `mp3`).

*   **Default Preset:**
    *   `Default = ffmpeg.exe -i "%I" "%i.%f"`
    *   This command takes the input WAV (`%I`) and outputs a file with the same name but the new extension (`%i.%f`). For example, if `audio-format=mp3`, it becomes `ffmpeg.exe -i "input.wav" "input.mp3"`.

*   **Example: Creating an MP3 Preset with specific quality:**
    ```ini
    [Presets]
    Default = ffmpeg.exe -i "%I" "%i.%f"
    MP3_HighQuality = ffmpeg.exe -i "%I" -q:a 2 "%i.mp3"
    ```
    To use `MP3_HighQuality`, you would set `current-preset = MP3_HighQuality` and `audio-format = mp3` in the `[General]` section (or select it in the Settings window).

## 9. Command-Line Options

You can launch `FPRecorder.exe` with these options:

*   `--start`: Automatically starts recording when the program launches, using the settings from `fp.ini`.
*   `-f <filename>`: Specifies a filename (without extension) for the recording. If used with `--start`, this filename will be used. The `.wav` or other extension will be added automatically.
    *   Example: `FPRecorder.exe --start -f "MyMeetingAudio"`
*   `--exit`: If specified, the program will automatically exit after a recording (started via `--start` or a hotkey) is stopped.

## 10. FFmpeg Integration (Detailed)

FPRecorder records audio to a temporary WAV file first. If `audio-format` (configured in the Settings window or `fp.ini`) is set to something other than `wav` (e.g., `mp3`), FPRecorder will then:

1.  Look up the command associated with `current-preset` in the `[Presets]` section of `fp.ini`.
2.  Replace the placeholders (`%I`, `%i`, `%f`) in that command.
3.  Execute the resulting FFmpeg command.
4.  If FFmpeg completes successfully (returns exit code 0), the original temporary WAV file is deleted.
5.  If FFmpeg fails, an error message is shown, and the original WAV file might remain.

**Example: Converting to Ogg Vorbis at ~128kbps**

1.  **Open the Settings Window:**
    *   Set **Audio Format** to `ogg`.
    *   Set **Current FFmpeg Preset** to `OggVorbis_128k` (you'll need to define this preset in `fp.ini` first).
    *   Click "Save and Apply".
2.  **Or, Edit `fp.ini`:**
    ```ini
    [General]
    ; ... other settings ...
    audio-format = ogg
    current-preset = OggVorbis_128k
    ; ... other settings ...

    [Presets]
    Default = ffmpeg.exe -i "%I" "%i.%f"
    OggVorbis_128k = ffmpeg.exe -i "%I" -c:a libvorbis -qscale:a 4 "%i.ogg"
    ```
3.  Ensure `ffmpeg.exe` is accessible.
4.  Start a recording. When you stop it, FPRecorder will run:
    `ffmpeg.exe -i "path/to/your/temp_file.wav" -c:a libvorbis -qscale:a 4 "path/to/your/temp_file.ogg"`
    And then delete `temp_file.wav`.

## 11. Troubleshooting

*   **"FFmpeg ... Get exit code 0 failed" or "Process exit failure!":**
    *   FFmpeg is not found: Ensure `ffmpeg.exe` is in your system PATH or in the same directory as `FPRecorder.exe`.
    *   Incorrect FFmpeg command: Your preset command in `fp.ini` might be invalid. Test the command manually in a command prompt.
    *   FFmpeg doesn't support the conversion: The codecs or parameters might be wrong.
*   **Config Parsing Error on Startup:**
    *   There's a syntax error in your `fp.ini` file, or a value is invalid (e.g., text where a number is expected). The error message will usually point to the problematic parameter and value.
    *   Delete `fp.ini` to reset to defaults if you can't fix it.
*   **No Audio Devices Listed / Recording Fails:**
    *   Ensure your microphone/playback devices are enabled in Windows Sound settings and properly connected.
    *   Try selecting "Default" if specific devices aren't working.
    *   Restart FPRecorder after changing audio hardware.
*   **"FPRuntimeError ... Access violation (segmentation fault)":**
    *   This is a program crash. The error message includes a stack trace which can be helpful for debugging if you report the issue to the developer. Try to note what you were doing when it happened.
*   **Hotkeys Not Working:**
    *   Ensure the hotkey definition in the Settings window or `fp.ini` is correct.
    *   Another application might be using the same global hotkey. Try a different combination.
    *   Run FPRecorder as Administrator if other applications with higher privileges are intercepting hotkeys (though generally not recommended unless necessary).
*   **Settings window changes not taking effect:**
    *   Ensure you clicked "Save and Apply" in the Settings window. Changes are not applied if you only click "Cancel" or close the window.

---
