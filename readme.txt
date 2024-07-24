FPRecorder (Fast Portable Recorder)
Copyright © 2024 M_maker

FPRecorder is a portable recording  tool for Windows that allows you to record sound from a microphone and from speakers or headphones. It's called Loopback.

Key Features:
{
  - Supports configuration files, enabling you to customize various settings for FPRecorder.
  - Recording from two devices. Microphone, and loopback.
  - Fast and portable. FPRecorder is written in the compilable programming language C++, which makes it compact and fast.
  - Convenient and accessible interface. FPRecorder's GUI is also available for people using screen readers.
  - Adding hotkeys. Add global hotkeys to actions such as record, stop, pause. You can use them outside the FPRecorder window.
}
- Configurable settings:
{
  - sample-rate=44100: Sets the sample rate for the recorded audio, in this case, 44,100 Hz.
  - record-path=recordings: Specifies the directory where recorded files will be saved.
  - input-device=0: Sets the input device for recording audio. Default is 0.
  - channels=2: Determines the number of audio channels to be recorded, in this case, 2 (stereo).
  - buffer-size=8192: Sets the buffer size for audio recording. Default is 8192.
  - filename-signature=
  - audio-format=wav: Sets the audio format for the recorded files. Default is WAV.
  - loopback-device=0: Sets the loopback device for audio recording. Default is 0.
  - sound-events=0: Determines whether to play sound events during recording, pause recording, stop, etc. Default is 0 (disabled).
}
- Timestamp variables:
{
  - %Y: 4-digit year
  - %m: 2-digit month (01-12)
  - %d: 2-digit day of the month (01-31)
  - %H: 2-digit hour in 24-hour format (00-23)
  - %M: 2-digit minute (00-59)
  - %S: 2-digit second (00-59)
}

These timestamp variables allow you to create file names that include the date and time of the recording.

FPRecorder is designed to be a fast and portable solution for your audio recording needs. With its comprehensive features and user-friendly interface, it aims to streamline your audio recording workflow.