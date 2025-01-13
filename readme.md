# FPRecorder (Fast Portable Recorder)

  **Copyright ©     2025 M_maker**

FPRecorder is a portable recording tool for Windows that allows you to record sound from a microphone and from speakers or headphones. It's called Loopback.

## Key Features

- Supports configuration files, enabling you to customize various settings for FPRecorder.
- Recording from two devices: Microphone and Loopback.
- Fast and portable. FPRecorder is written in the compilable programming language C++, which makes it compact and fast.
- Convenient and accessible interface. FPRecorder's GUI is accessible for people using screen readers.
- Adding hotkeys. Add global hotkeys to actions such as record, stop, and pause. You can use them outside the FPRecorder window.

## What Do I Need for FPRecorder?

- Windows 10 64 bit
- A sound card.

## Getting Started

1. You are now in the about program window. To close this window, press Enter or the "Start using FPRecorder" button.
2. To simply start recording, open the main program window and click the "Start recording" button.
3. By default, the input device selected in the system will be used while recording, but you can select it in the "Input devices" list.
4. To use the Loopback feature, select the device in the "Output loopback devices" list.
5. After clicking the record button, your selected devices will be used. During recording, you can stop, pause, or restart recording. **Warning:** When you restart recording, all previously recorded data will be lost. FPRecorder will warn you about this.
6. After recording is stopped, you can either open the "recordings" folder (it will be located in the same folder where you have FPRecorder executable) or listen to the recording directly from the application by clicking on the "Recordings manager" button.

### Using the Recordings Manager

1. In the main program window, find the "Recordings manager" button.
2. When you click on the manager button, a list of audio files will open, with buttons such as play, pause, stop, and delete.
3. Use the "Play" button to play the file, or simply press Space.
4. To close the Recordings Manager window, either press Escape or the "Close" button.

### Customizing Recording Settings

To customize the recording settings, edit the program configuration located in the folder with FPRecorder. Open the "fp.ini" file and start editing it.

#### Editing the Configuration File

1. Close the program.
2. Find the "fp.ini" file in the folder with FPRecorder. If it does not exist, try running the program again.
3. Open the configuration file in a text editor, such as Windows Notepad. FPRecorder uses the INI configuration format, so you will see sections with lines like "Key = Value". Section names are in square brackets.
4. **Warning:** Do not change parameter names, only their values.

### Example Configuration Fields

- **sample-rate = 44100**
Specifies the sample rate for audio recording, set to 44,100 Hz (common for high-quality audio).

- **record-path = recordings**
Specifies the directory for saved audio files (stored in the "recordings" directory).

- **input-device = 0**
Specifies the input device for audio recording; "0" indicates the default input device.

- **channels = 2**
Specifies the number of audio channels to be recorded (set to 2 for stereo).

- **buffer-size = 0**
Specifies the audio buffer size used during recording; "0" indicates the default buffer size.

- **filename-signature = %Y %m %d %H %M %S**
Specifies the format for recorded audio filenames, including year, month, day, hour, minute, and second.

- **audio-format = wav**
Specifies the audio format for recorded files (set to WAV).

- **loopback-device = 0**
Specifies the loopback device for audio recording; "0" indicates the default loopback device.

- **sound-events = 1**
Boolean value that determines whether sound events will be played during recording.

- **make-stems = 0**
Boolean value that determines whether separate loopback and microphone signals by two files.

- **sample-format = s16**
Specifies the sample format for audio recording (set to 16-bit signed integer).

- **hotkey-start-stop = Windows+Shift+F1**
Hotkey combination to start and stop audio recording.

- **hotkey-restart = Windows+Shift+F3**
Hotkey combination to restart audio recording.

- **hotkey-pause-resume = Windows+Shift+F2**
Hotkey combination to pause and resume audio recording.

- **current-preset = Default**
Specifies the current preset for audio recording.

### Presets

- **Default = ffmpeg.exe -i %I %i.%f**
Preset for audio conversion using FFmpeg.

### FPRecorder Constants

These are configuration file values; their keys must match one of the specified values. If not, a runtime error will trigger.

#### Sample Formats

- u8
- s16
- s24
- s32
- f32

**Note:** For hotkeys, use "+" as a delimiter. You cannot assign one key to multiple FPRecorder events, and key names must be capitalized.

Once you have edited the file, save it and run the program. If editing fails, a runtime error will be triggered.

## Additional Information

FPRecorder does not support encoding in other formats, but you can use FFmpeg for this. Download FFmpeg and place it next to `FPRecorder.exe`, or add `ffmpeg.exe` to the system Path variable.

If you find a bug in the program, you can report it in the Telegram channel's group: [Telegram Group](https://t.me/programms00001). The discussion group can be found by clicking on the profile, "More," "View Discussion."

If you do not have Telegram Messenger, contact me via email: [georgijbondarenko248@gmail.com](mailto:georgijbondarenko248@gmail.com).

I hope you enjoy this little project! Thanks for using!
