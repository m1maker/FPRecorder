FPRecorder(Fast Portable Recorder)
Copyright © 2024 M_maker
FPRecorder is a portable recording  tool for Windows that allows you to record sound from a microphone and from speakers or headphones. It's called Loopback.

Key Features :
{
	-Supports configuration files, enabling you to customize various settings for FPRecorder.
	- Recording from two devices. Microphone, and loopback.
	- Fast and portable. FPRecorder is written in the compilable programming language C++, which makes it compact and fast.
	- Convenient and accessible interface. FPRecorder's GUI is accessible for people using screen readers.
	- Adding hotkeys. Add global hotkeys to actions such as record, stop, pause. You can use them outside the FPRecorder window.
}

What I need for FPRecorder?
Windows 10 64 bit and a sound card.
Getting started:
You are now in the about program window. To close this window, press Enter or the "Start using FPRecorder" button.
To simply start recording, open the main program window and click the "Start recording" button. By default, the input device selected in the system will be used while recording, but you can select it in the "Input devices" list.
Also, to use the Loopback feature, you must select the device in the "Output loopback devices" list.
Now, when you clicked on the record button, your selected devices will be used.
During recording, you can stop, pause or restart recording. When you restart recording, all previously recorded data will be lost, so if you press the restart button carelessly, FPRecorder will warn you about this.
After recordingwill be stopped, you can either open the "recordings" folder (It will be located in the same folder where you have FPRecorder executable), or listen to the recording directly from the application by clicking on the "Recordings manager" button.
Let's use the recordings manager to listen your recorded audio!
In the main program window, you can find the "Recordings manager" button. This is what you need now.
When you click on the manager button, a list of audio files will open, with buttons such as play, pause, stop and delete. 
Either you can use the "Play" button to play the file, or simply press Space.
To close the Recordings Manager window, either press Escape or the "Close" button.
But if you want to customize the recording settings, you must edit the program configuration, which is also located in the folder with FPRecorder. Of course, I will definitely make the settings in the future, but now you can open the "fp.ini" file and start editing it.
To edit this configuration file, close the program, find the file "fp.ini", which should be in the folder with FPRecorder. If it does not exist, try running the program again.
Now you need to open the configuration file in a text editor, for example you can use Windows Notepad.
FPRecorder uses the INI configuration format, so you can see sections in the file that have the lines "Key = Value". Section names are in square brackets.
Now that you know the structure of the configuration, you can start editing the configuration file. Warning! You should not change the parameter names. You can only change their values.
General
sample-rate = 44100
This field specifies the sample rate for the audio recording, which is set to 44,100 Hz. This is a common sample rate for high-quality audio.
record-path = recordings
This field specifies the directory where the recorded audio files will be saved. In this case, the recordings will be saved in the "recordings" directory.
input-device = 0
This field specifies the input device to be used for the audio recording. The value "0" indicates the default input device.
channels = 2
This field specifies the number of audio channels to be recorded. In this case, it is set to 2, which corresponds to stereo audio.
buffer-size = 0
This field specifies the size of the audio buffer used during the recording process. A value of 0 indicates that the default buffer size will be used.
filename-signature = %Y %m %d %H %M %S
This field specifies the format of the filename for the recorded audio files. The format includes the year, month, day, hour, minute, and second of the recording time.
audio-format = wav
This field specifies the audio format for the recorded files, which is set to WAV.
loopback-device = 0
This field specifies the loopback device to be used for the audio recording. The value "0" indicates the default loopback device.
sound-events = 1
This field is a boolean value that determines whether sound events will be played during the recording process. When set to 1 (true), sounds will be played for events such as Start, Stop, Pause, and Restart recording.
sample-format = s16
This field specifies the sample format for the audio recording, which is set to 16-bit signed integer (s16).
hotkey-start-stop = Windows+Shift+F1
This field specifies the hotkey combination to start and stop the audio recording. In this case, the hotkey is Windows+Shift+F1.
hotkey-restart = Windows+Shift+F3
This field specifies the hotkey combination to restart the audio recording. In this case, the hotkey is Windows+Shift+F3.
hotkey-pause-resume = Windows+Shift+F2
This field specifies the hotkey combination to pause and resume the audio recording. In this case, the hotkey is Windows+Shift+F2.
current-preset = Default
This field specifies the current preset to be used for the audio recording. In this case, the preset is set to "Default".

Presets
Default = ffmpeg.exe -i %I %i.%f
This field specifies the preset for the audio conversion using the FFmpeg tool. The preset includes the following placeholders:
- %I: The full name and path of the input file.
- %i: The filename of the input file without the extension.
- %f: The audio format of the output file (e.g., mp3).

FPRecorder constants:
These are also configuration file values, but the keys related to these values, their values must be one of the values below. If this does not happen, a runtime error with details will be triggered.
Sample formats:
u8,
s16,
s24,
s32,
f32

For hotkeys, the delimiter "+" is used, you cannot assign one key to FPRecorder events. Key names must be capitalized.

Once you have edited the file, you can save it and run the program. In case of failure to edit the configuration, a runtime error with details will be triggered.

Additional information
FPRecorder does not support encoding in other formats, but you can use ffmpeg for this. To do this, you need to download ffmpeg and put it next to FPRecorder.exe, or simply add ffmpeg.exe to the system Path variable.
If you find a bug in the program, which may be typical for this version, you can write about it in the Telegram channel's group : 
https://t.me/programms00001
The discussion group can be found by clicking on the profile, "More", "View Discussion"
If you do not have a Telegram Messenger, you can contact me by mail:
georgijbondarenko248@gmail.com

I hope you enjoy this little project! Thanks for using!
