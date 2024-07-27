#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include "Error.wav.h"
#include "gui.h"
#include "Openmanager.wav.h"
#include "Pause.wav.h"
#include "Provider.h"
#include "readme.h"
#include "resource1.h"
#include "Restart.wav.h"
#include "start.wav.h"
#include "stdafx.h"
#include "Stop.wav.h"
#include "Unpause.wav.h"
#include "user_config.h"
#include<assert.h>
#include <codecvt>
#include<condition_variable>
#include <deque>
#include<filesystem>
#include <locale>
#include<mutex>
#include <ole2.h>
#include <tlhelp32.h>
#include <UIAutomation.h>
#include <Uiautomationcore.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <chrono>
#include<fstream>
#include <wrl.h>
#include <wrl/client.h>
#include <iomanip>
#include <iostream>
#include<sstream>
#include<thread>
#include <vector>
using namespace gui;
using namespace Microsoft::WRL;
static bool _cdecl unicode_convert(const std::string& str, std::wstring& output) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	try {
		output = converter.from_bytes(str);
	}
	catch (const std::exception& e) { return false; }
	return true;
}
static bool _cdecl unicode_convert(const std::wstring& str, std::string& output) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	try {
		output = converter.to_bytes(str);
	}
	catch (const std::exception& e) { return false; }
	return true;
}
ma_uint32 sample_rate = 44100;
ma_uint32 channels = 2;
ma_uint32 buffer_size = 1024;
std::string filename_signature = "%Y %m %d %H %M %S";
std::string record_path = "recordings";
std::string audio_format = "wav";
int input_device = 0;
int loopback_device = 0;
ma_bool32 sound_events = MA_FALSE;
ma_format buffer_format = ma_format_s16;
const ma_uint32 periods = 0;
user_config conf("fp.ini");
static void WINAPI SendNotification(const std::wstring& message) {
	CoInitialize(NULL);

	IUIAutomation* pAutomation = NULL;
	HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&pAutomation);

	if (SUCCEEDED(hr)) {
		IUIAutomationCondition* pCondition = NULL;
		VARIANT varName;
		varName.vt = VT_BSTR;
		varName.bstrVal = SysAllocString(L"");
		hr = pAutomation->CreatePropertyConditionEx(UIA_NamePropertyId, varName, PropertyConditionFlags_None, &pCondition);

		if (SUCCEEDED(hr)) {
			Provider* pProvider = new Provider(GetForegroundWindow());
			IUIAutomationElement* pElement = NULL;
			hr = pAutomation->ElementFromHandle(GetForegroundWindow(), &pElement);

			if (SUCCEEDED(hr)) {
				hr = UiaRaiseNotificationEvent(pProvider, NotificationKind_ActionCompleted, NotificationProcessing_ImportantAll, SysAllocString(message.c_str()), SysAllocString(L""));

				if (SUCCEEDED(hr)) {
					pProvider->Release();
				}
			}

			pCondition->Release();
		}

		pAutomation->Release();
	}

	CoUninitialize();
}
static void WINAPI SendNotification(const std::string& message) {
	std::wstring message_u;
	unicode_convert(message, message_u);
	SendNotification(message_u);
}
static HHOOK g_KeyboardHook;
static LRESULT _stdcall KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0) {
		if (wParam == WM_KEYDOWN) {
			KBDLLHOOKSTRUCT* kbd = (KBDLLHOOKSTRUCT*)lParam;
			int key = kbd->vkCode;
		}

		return CallNextHookEx(g_KeyboardHook, nCode, wParam, lParam);
	}
}


static bool WINAPI run(const std::string& filename, const std::string& cmdline, bool wait_for_completion, bool background) {
	PROCESS_INFORMATION info;
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = (background ? SW_HIDE : SW_SHOW);
	char c_cmdline[32768];
	c_cmdline[0] = 0;
	if (cmdline.size() > 0) {
		std::string tmp = "\"";
		tmp += filename;
		tmp += "\" ";
		tmp += cmdline;
		strncpy(c_cmdline, tmp.c_str(), tmp.size());
		c_cmdline[tmp.size()] = 0;
	}
	std::wstring filename_u, cmdline_u;
	unicode_convert(filename, filename_u);
	unicode_convert(cmdline, cmdline_u);
	BOOL r = CreateProcess(filename_u.c_str(), &cmdline_u[0], NULL, NULL, FALSE, INHERIT_CALLER_PRIORITY, NULL, NULL, &si, &info);
	if (r == FALSE)
		return false;
	if (wait_for_completion) {
		while (WaitForSingleObject(info.hProcess, 0) == WAIT_TIMEOUT) {
			ma_sleep(5);
		}
	}
	CloseHandle(info.hProcess);
	CloseHandle(info.hThread);
	return true;
}
static std::vector<std::wstring> WINAPI get_files(const std::wstring& path) {
	std::vector<std::wstring> files;

	try {
		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			if (std::filesystem::is_regular_file(entry)) {
				files.push_back(entry.path().filename().wstring());
			}
		}
	}
	catch (const std::exception& e) {
		return files;
	}

	return files;
}
ma_engine mixer;
ma_device mixerDevice;
ma_sound player;
bool g_EngineActive = false;
bool g_SoundActive = false;
std::wstring current_file;
bool MA_API play(std::wstring filename) {
	if (filename == current_file and g_SoundActive) {
		return ma_sound_start(&player) == MA_SUCCESS;
	}
	if (!g_EngineActive) {
		ma_engine_config pMixerConfig = ma_engine_config_init();
		ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
		devConfig.noClip = MA_FALSE;
		devConfig.playback.format = buffer_format;
		ma_device_init(nullptr, &devConfig, &mixerDevice);
		pMixerConfig.pDevice = &mixerDevice;
		if (ma_engine_init(&pMixerConfig, &mixer) == MA_SUCCESS)g_EngineActive = true;
	}
	if (g_SoundActive) {
		ma_sound_uninit(&player);
		g_SoundActive = false;
	}
	if (!g_SoundActive) {
		ma_result result = ma_sound_init_from_file_w(&mixer, filename.c_str(), MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, &player);
		if (result == MA_SUCCESS)g_SoundActive = true;
		if (result == MA_SUCCESS)ma_sound_start(&player);
		current_file = filename;
		return result == MA_SUCCESS;
	}
	return false;
}
ma_decoder g_Decoder;
bool MA_API play(std::string filename) {
	std::wstring filename_u;
	unicode_convert(filename, filename_u);
	return play(filename_u);
}
bool MA_API play_from_memory(const unsigned char data[], size_t data_size = 0) {
	if (&g_Decoder != nullptr)ma_decoder_uninit(&g_Decoder);
	if (!g_EngineActive) {
		if (ma_engine_init(nullptr, &mixer) == MA_SUCCESS)g_EngineActive = true;
	}
	if (g_SoundActive) {
		ma_sound_uninit(&player);
		g_SoundActive = false;
	}
	if (!g_SoundActive) {
		ma_decoder_init_memory((char*)data, data_size, nullptr, &g_Decoder);
		ma_result result = ma_sound_init_from_data_source(&mixer, &g_Decoder, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &player);
		if (result == MA_SUCCESS)g_SoundActive = true;
		if (result == MA_SUCCESS)ma_sound_start(&player);
		return result == MA_SUCCESS;
	}
	return false;
}
MA_API ma_bool32 try_parse_format(const char* str, ma_format* pValue)
{
	ma_format format;

	/*  */ if (strcmp(str, "u8") == 0) {
		format = ma_format_u8;
	}
	else if (strcmp(str, "s16") == 0) {
		format = ma_format_s16;
	}
	else if (strcmp(str, "s24") == 0) {
		format = ma_format_s24;
	}
	else if (strcmp(str, "s32") == 0) {
		format = ma_format_s32;
	}
	else if (strcmp(str, "f32") == 0) {
		format = ma_format_f32;
	}
	else {
		return MA_FALSE;    /* Not a format. */
	}

	if (pValue != NULL) {
		*pValue = format;
	}

	return MA_TRUE;
}

MA_API const char* ma_format_to_string(ma_format format) {
	switch (format) {
	case ma_format_u8:
		return "u8";
	case ma_format_s16:
		return "s16";
	case ma_format_s24:
		return "s24";
	case ma_format_s32:
		return "s32";
	case ma_format_f32:
		return "f32";
	default:
		return NULL;
	}
}


std::string _cdecl get_now() {
	auto now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);

	std::tm tm = *std::localtime(&now_c);
	std::stringstream oss;
	oss << std::put_time(&tm, filename_signature.c_str());
	return oss.str();
}
using namespace std;
bool g_Recording = false;
bool g_RecordingPaused = false;
struct application {
	std::wstring name;
	ma_uint32 id;
};
const application g_LoopbackApplication{ L"", 0 };
std::vector<application> WINAPI get_tasklist() {
	std::vector<application> tasklist;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32W pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32W);
		if (Process32FirstW(hSnapshot, &pe32)) {
			do {
				application app;
				app.name = pe32.szExeFile;
				app.id = pe32.th32ProcessID;
				tasklist.push_back(app);
			} while (Process32NextW(hSnapshot, &pe32));
		}
		CloseHandle(hSnapshot);
	}
	return tasklist;
}
struct audio_device {
	std::wstring name;
	ma_device_id id;
};
audio_device g_CurrentInputDevice;
audio_device g_CurrentOutputDevice;
MA_API float* mix_f32(float* input1, float* input2, ma_uint32 frameCountFirst, ma_uint32 frameCountLast) {
	float* result = new float[frameCountFirst + frameCountLast];
	for (ma_uint32 i = 0; i < frameCountFirst + frameCountLast; i++) {
		result[i] = (input1[i] + input2[i]);
	}
	return result;
}
float* loopback_buffer = nullptr;
float* microphone_buffer = nullptr;
ma_uint32 loopback_frames;
ma_uint32 microphone_frames;
ma_event loopback_event;
ma_event microphone_event;
ma_bool8 g_LoopbackProcess = MA_TRUE;
bool thread_shutdown = false;
bool paused = false;
ma_bool8 g_NullSamplesDestroyed = MA_FALSE;
ma_data_converter g_Converter;
void MA_API audio_recorder_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	if (paused)return;
	ma_encoder* encoder = reinterpret_cast<ma_encoder*>(pDevice->pUserData);
	if (g_NullSamplesDestroyed == MA_FALSE) {
		if (buffer_format = ma_format_f32) {
			float* pInput64 = (float*)(pInput);
			for (ma_uint32 i = 0; i < frameCount; i++) {
				if (pInput64[i] == 0)return;
				else g_NullSamplesDestroyed = MA_TRUE;
			}
		}
	}
	if (g_CurrentOutputDevice.name == L"NO") {
		void* pInputOut = (void*)pInput;
		ma_uint64 frameCountToProcess = frameCount;
		ma_uint64 frameCountOut = frameCount * 2;
		ma_data_converter_process_pcm_frames(&g_Converter, pInput, &frameCountToProcess, pInputOut, &frameCountOut);
		ma_encoder_write_pcm_frames(encoder, pInputOut, frameCountOut, nullptr);
	}
	else {
		microphone_buffer = (float*)pInput;
		microphone_frames = frameCount;
		ma_event_signal(&microphone_event);
	}
	(void)pOutput;
}
void MA_API audio_recorder_callback_loopback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
	if (paused)return;
	if (!g_LoopbackProcess)return;
	loopback_buffer = (float*)pInput;
	loopback_frames = frameCount;
	ma_event_signal(&loopback_event);
	(void)pOutput;
	(void)pDevice;
}
void recording_thread(ma_encoder* encoder) {
	while (!thread_shutdown) {
		ma_event_wait(&microphone_event);
		ma_event_wait(&loopback_event);
		if (microphone_buffer != nullptr and loopback_buffer != nullptr) {
			void* result = nullptr;
			result = mix_f32((float*)microphone_buffer, (float*)loopback_buffer, microphone_frames, loopback_frames);
			void* pInputOut = (void*)result;
			ma_uint64 frameCountToProcess = microphone_frames;
			ma_uint64 frameCountOut = microphone_frames * 2;
			ma_data_converter_process_pcm_frames(&g_Converter, result, &frameCountToProcess, pInputOut, &frameCountOut);
			ma_encoder_write_pcm_frames(encoder, pInputOut, frameCountOut, nullptr);
		}
	}
}
class NamedMutex {
public:
	NamedMutex(const std::string& name) {
		std::wstring name_u;
		unicode_convert(name, name_u);
		mutex_ = CreateMutexW(nullptr, FALSE, name_u.c_str());
		if (mutex_ == nullptr) {
			exit(EXIT_FAILURE);
		}
	}

	~NamedMutex() {
		CloseHandle(mutex_);
	}

	void lock() {
		WaitForSingleObject(mutex_, INFINITE);
	}

	void unlock() {
		ReleaseMutex(mutex_);
	}
	bool try_lock() {
		return WaitForSingleObject(mutex_, 0) == WAIT_OBJECT_0; // Returns WAIT_OBJECT_0 on success
	}
private:
	HANDLE mutex_;
};

// FPRecorder must be one instance of recorder class
class MINIAUDIO_IMPLEMENTATION audio_recorder {
private:
	ma_device_config deviceConfig;
	ma_device_config loopbackDeviceConfig;
	ma_encoder_config encoderConfig;
	ma_encoder encoder;
	ma_device recording_device;
	ma_device loopback_device;
	NamedMutex* rec_mtx = nullptr;
public:
	std::string filename;
	audio_recorder() = default;  // Default constructor
	~audio_recorder() = default;  // Default destructor

	audio_recorder(const audio_recorder&) = delete;
	audio_recorder& operator=(const audio_recorder&) = delete;


	inline void start() {
		if (rec_mtx != nullptr and rec_mtx->try_lock() == false) {
			delete this;
			exit(EXIT_FAILURE);
		}
		ma_event_init(&loopback_event);
		ma_event_init(&microphone_event);
		std::wstring record_path_u;
		unicode_convert(record_path, record_path_u);
		CreateDirectory(record_path_u.c_str(), nullptr);
		std::string file = record_path + "/" + get_now() + ".wav";
		filename = file;
		encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, buffer_format, channels, sample_rate);
		ma_result result = ma_encoder_init_file(file.c_str(), &encoderConfig, &encoder);
		if (result != MA_SUCCESS) {
			std::wstring file_u;
			unicode_convert(file, file_u);
			if (sound_events == MA_TRUE)		play_from_memory(Error_wav, 15499);
			alert(L"FPEncoderInitializerError", L"Error initializing audio encoder for file \"" + file_u + L"\" with retcode " + std::to_wstring(result) + L".", MB_ICONERROR);
			exit(result);
		}
		deviceConfig = ma_device_config_init(ma_device_type_capture);
		if (g_CurrentInputDevice.name != L"NO")
			deviceConfig.capture.pDeviceID = &g_CurrentInputDevice.id;
		deviceConfig.capture.format = ma_format_f32;
		deviceConfig.capture.channels = channels;
		deviceConfig.sampleRate = sample_rate;
		deviceConfig.periodSizeInFrames = buffer_size;
		deviceConfig.periods = periods;
		deviceConfig.dataCallback = audio_recorder_callback;
		deviceConfig.pUserData = &encoder;
		result = ma_device_init(NULL, &deviceConfig, &recording_device);
		if (result != MA_SUCCESS) {
			if (sound_events == MA_TRUE)		play_from_memory(Error_wav, 15499);
			alert(L"FPAudioDeviceInitializerError", L"Error initializing audio device for \"" + g_CurrentInputDevice.name + L"\" with retcode " + std::to_wstring(result) + L".", MB_ICONERROR);
			exit(result);
		}
		ma_device_start(&recording_device);
		if (g_CurrentOutputDevice.name != L"NO") {
			loopbackDeviceConfig = ma_device_config_init(ma_device_type_loopback);
			loopbackDeviceConfig.capture.pDeviceID = &g_CurrentOutputDevice.id;
			loopbackDeviceConfig.capture.format = ma_format_f32;
			loopbackDeviceConfig.capture.channels = channels;
			loopbackDeviceConfig.sampleRate = sample_rate;
			loopbackDeviceConfig.periodSizeInFrames = buffer_size;
			loopbackDeviceConfig.periods = periods;
			loopbackDeviceConfig.dataCallback = audio_recorder_callback_loopback;
			loopbackDeviceConfig.pUserData = nullptr;
			ma_backend backends[] = {
				ma_backend_wasapi
			};


			result = ma_device_init_ex(backends, sizeof(backends) / sizeof(backends[0]), NULL, &loopbackDeviceConfig, &loopback_device);
			if (result != MA_SUCCESS) {
				if (sound_events == MA_TRUE)		play_from_memory(Error_wav, 15499);
				alert(L"FPAudioDeviceInitializerError", L"Error initializing audio device for \"" + g_CurrentOutputDevice.name + L"\" with retcode " + std::to_wstring(result) + L".", MB_ICONERROR);
				exit(result);
			}
			ma_device_start(&loopback_device);
		}
		g_LoopbackProcess = MA_TRUE;
		this->resume();
		rec_mtx = new NamedMutex("FPRecorderRecording");
		rec_mtx->lock();
	}
	inline void stop() {
		ma_event_signal(&microphone_event);
		ma_device_uninit(&recording_device);
		if (g_CurrentOutputDevice.name != L"NO") {
			thread_shutdown = true;
			g_LoopbackProcess = MA_FALSE;
			ma_event_signal(&loopback_event);
			ma_device_uninit(&loopback_device);
			rec_mtx->unlock();
			delete rec_mtx;
			rec_mtx = nullptr;
		}
		ma_encoder_uninit(&encoder);
		ma_event_uninit(&loopback_event);
		ma_event_uninit(&microphone_event);
		g_NullSamplesDestroyed = MA_FALSE;
	}
	inline void pause() {
		thread_shutdown = true;
		paused = true;
		g_NullSamplesDestroyed = MA_FALSE;
	}
	inline void resume() {
		thread_shutdown = false;
		std::thread t(recording_thread, &encoder);
		t.detach();
		paused = false;
	}
};
std::vector<audio_device> MA_API get_input_audio_devices()
{
	std::vector<audio_device> audioDevices;
	ma_result result;
	ma_context context;
	ma_device_info* pCaptureDeviceInfos;
	ma_uint32 captureDeviceCount;
	ma_uint32 iCaptureDevice;
	if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
		alert(L"FPError", L"Failed to initialize context.");
		return audioDevices;;
	}

	result = ma_context_get_devices(&context, nullptr, nullptr, &pCaptureDeviceInfos, &captureDeviceCount);
	if (result != MA_SUCCESS) {
		return audioDevices;
	}
	for (iCaptureDevice = 0; iCaptureDevice < captureDeviceCount; ++iCaptureDevice) {
		const char* name = pCaptureDeviceInfos[iCaptureDevice].name;
		std::string name_str(name);
		std::wstring name_str_u;
		unicode_convert(name_str, name_str_u);
		audio_device ad;
		ad.id = pCaptureDeviceInfos[iCaptureDevice].id;
		ad.name = name_str_u;
		audioDevices.push_back(ad);
	}
	ma_context_uninit(&context);
	return audioDevices;
}
std::vector<audio_device> MA_API get_output_audio_devices()
{
	std::vector<audio_device> audioDevices;
	ma_result result;
	ma_context context;
	ma_device_info* pPlaybackDeviceInfos;
	ma_uint32 playbackDeviceCount;
	ma_uint32 iPlaybackDevice;

	if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
		alert(L"FPError", L"Failed to initialize context.");
		return audioDevices;;
	}

	result = ma_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, nullptr, nullptr);
	if (result != MA_SUCCESS) {
		return audioDevices;
	}
	for (iPlaybackDevice = 0; iPlaybackDevice < playbackDeviceCount; ++iPlaybackDevice) {
		const char* name = pPlaybackDeviceInfos[iPlaybackDevice].name;
		std::string name_str(name);
		std::wstring name_str_u;
		unicode_convert(name_str, name_str_u);
		audio_device ad;
		ad.id = pPlaybackDeviceInfos[iPlaybackDevice].id;
		ad.name = name_str_u;
		audioDevices.push_back(ad);
	}

	ma_context_uninit(&context);
	return audioDevices;
}

HWND window;
const std::wstring version = L"0.0.0";
audio_recorder rec;
HWND record_start;
HWND input_devices_text;
HWND input_devices_list;
HWND output_devices_text;
HWND output_devices_list;
HWND record_manager;
HWND items_view_text;
HWND items_view_list;
HWND play_button;
HWND pause_button;
HWND stop_button;
HWND delete_button;
HWND close_button;
HWND record_stop;
HWND record_pause;
HWND record_restart;
std::vector<audio_device> in_audio_devices;
std::vector < audio_device> out_audio_devices;
std::vector<HWND> items;
// GUI functions
static inline void window_reset() {
	for (unsigned int i = 0; i < items.size(); i++) {
		delete_control(items[i]);
	}
	items.resize(0);
}

void main_items_construct() {
	g_CurrentInputDevice.name = L"NO";
	g_CurrentOutputDevice.name = L"NO";
	record_start = create_button(window, L"&Start recording", 10, 10, 200, 50, 0);
	items.push_back(record_start);

	input_devices_text = create_text(window, L"&Input devices", 10, 70, 200, 20, 0);

	items.push_back(input_devices_text);

	input_devices_list = create_list(window, 10, 90, 200, 150, 0);
	items.push_back(input_devices_list);
	add_list_item(input_devices_list, L"0(Default)");
	in_audio_devices = get_input_audio_devices();
	for (unsigned int i = 0; i < in_audio_devices.size(); i++) {
		add_list_item(input_devices_list, in_audio_devices[i].name.c_str());
	}
	focus(input_devices_list);
	set_list_position(input_devices_list, input_device);
	output_devices_text = create_text(window, L"&Output loopback devices", 10, 250, 200, 20, 0);
	items.push_back(output_devices_text);

	output_devices_list = create_list(window, 10, 270, 200, 150, 0);
	items.push_back(output_devices_list);
	add_list_item(output_devices_list, L"0(Not used)");
	out_audio_devices = get_output_audio_devices();
	for (unsigned int i = 0; i < out_audio_devices.size(); i++) {
		add_list_item(output_devices_list, out_audio_devices[i].name.c_str());
	}
	focus(output_devices_list);
	set_list_position(output_devices_list, loopback_device);
	record_manager = create_button(window, L"&Recordings manager", 10, 450, 200, 50, 0);
	items.push_back(record_manager);
	focus(record_start);
}
void record_manager_items_construct() {
	items_view_text = create_text(window, L"Items view", 10, 10, 0, 10, 0);
	items.push_back(items_view_text);
	items_view_list = create_list(window, 10, 10, 0, 10, 0);
	items.push_back(items_view_list);
	play_button = create_button(window, L"&Play", 10, 10, 10, 10, 0);
	items.push_back(play_button);
	pause_button = create_button(window, L"&Pause", 10, 10, 10, 10, 0);
	items.push_back(pause_button);
	stop_button = create_button(window, L"&Stop", 10, 10, 10, 10, 0);
	items.push_back(stop_button);
	delete_button = create_button(window, L"&Delete", 10, 10, 10, 10, 0);
	items.push_back(delete_button);
	close_button = create_button(window, L"&Close", 10, 10, 10, 10, 0);
	items.push_back(close_button);
	std::vector<std::wstring> files = get_files(std::wstring(record_path.begin(), record_path.end()));
	for (unsigned int i = 0; i < files.size(); i++) {
		add_list_item(items_view_list, files[i].c_str());
	}
	focus(items_view_list);
}
void record_items_construct() {
	record_stop = create_button(window, L"&Stop recording", 10, 10, 100, 30, 0);
	items.push_back(record_stop);
	record_pause = create_button(window, L"&Pause recording", 120, 10, 100, 30, 0);
	items.push_back(record_pause);
	record_restart = create_button(window, L"&Restart recording", 230, 10, 100, 30, 0);
	items.push_back(record_restart);
	focus(record_stop);
}
bool g_RecordingsManager = false;
static LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE: {
		MessageBeep(MB_ICONERROR);
		return TRUE;
	}
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			EndDialog(window, 0);
			DestroyWindow(window);
			return TRUE;
		}
		return FALSE;
	}
	return false;
}
std::wstring WINAPI get_exe() {
	vector<wchar_t> pathBuf;
	DWORD copied = 0;
	do {
		pathBuf.resize(pathBuf.size() + MAX_PATH);
		copied = GetModuleFileName(0, &pathBuf.at(0), pathBuf.size());
	} while (copied >= pathBuf.size());

	pathBuf.resize(copied);

	return std::wstring(pathBuf.begin(), pathBuf.end());
}
ma_int32 APIENTRY WINAPI _stdcall MINIAUDIO_IMPLEMENTATION wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, ma_int32       nShowCmd) {
	if (wcslen(lpCmdLine) != 0) {
		MessageBeep(MB_ICONERROR);
		play_from_memory(Error_wav, 15499);
		ma_sleep(1000);
		return MA_ERROR;
	}
	int result = conf.load();
	if (result == EXIT_SUCCESS) {
		try {
			std::string srate = conf.read("sample-rate");
			sample_rate = std::stoi(srate);
			std::string chann = conf.read("channels");
			channels = std::stoi(chann);
			std::string bs = conf.read("buffer-size");
			buffer_size = std::stoi(bs);
			filename_signature = conf.read("filename-signature");
			record_path = conf.read("record-path");
			audio_format = conf.read("audio-format");
			std::string ind = conf.read("input-device");
			input_device = std::stoi(ind);
			std::string oud = conf.read("loopback-device");
			loopback_device = std::stoi(oud);
			std::string sevents = conf.read("sound-events");
			sound_events = std::stoi(sevents);
			std::string sformat = conf.read("sample-format");
			ma_bool32 parse_result = try_parse_format(sformat.c_str(), &buffer_format);
			if (parse_result == MA_FALSE) {
				throw std::exception("Invalid sample format parameter");
			}
		}
		catch (const std::exception& e) {
			std::string what = e.what();
			std::wstring what_u;
			unicode_convert(what, what_u);
			std::wstring last_value_u;
			std::wstring last_name_u;
			unicode_convert(conf.last_name, last_name_u);
			unicode_convert(conf.last_value, last_value_u);
			if (last_name_u.empty())last_name_u = L"None";
			if (last_value_u.empty())last_value_u = L"Null";
			alert(L"FPConfigParsingError", L"An error occurred while loading the existing config file. Error details:\n\"" + what_u + L"\"\nParameter: \"" + last_name_u + L"\".\nValue: \"" + last_value_u + L"\".", MB_ICONERROR);
			exit(0 - 2);
		}
	}
	else if (result == -1) {
		play_from_memory(Restart_wav, 3563);
		MessageBeep(0xFFFFFFFF);
		window = CreateDialog(NULL, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc);
		if (!IsWindow(window)) {
			alert(L"FPWelcomeDialogInitializerError", L"File: " + get_exe() + L"\\.rsrc\\DIALOG\\" + std::to_wstring(IDD_DIALOG1) + L" not found.", MB_ICONERROR);
			exit(-4);
		}
		ShowWindow(window, SW_SHOW);
		SetDlgItemTextW(window, IDC_EDIT1, README.c_str());
		MSG msg;
		while (GetMessage(&msg, NULL, NULL, 0) && IsWindow(window)) {
			if (!IsDialogMessage(window, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}


		conf.write("sample-rate", std::to_string(sample_rate));
		conf.write("channels", std::to_string(channels));
		conf.write("buffer-size", std::to_string(buffer_size));
		conf.write("filename-signature", filename_signature);
		conf.write("record-path", record_path);
		conf.write("audio-format", audio_format);
		conf.write("input-device", std::to_string(input_device));
		conf.write("loopback-device", std::to_string(loopback_device));
		conf.write("sound-events", std::to_string(sound_events));
		conf.write("sample-format", string(ma_format_to_string(buffer_format)));
		conf.save();
	}
	ma_data_converter_config converter_config = ma_data_converter_config_init(ma_format_f32, buffer_format, channels, channels, sample_rate, sample_rate);
	ma_result init_result = ma_data_converter_init(&converter_config, nullptr, &g_Converter);
	if (init_result != MA_SUCCESS) {
		alert(L"FPConverterInitializerError", L"Error initializing data converter.", MB_ICONERROR);
		exit(-3);
	}
	window = show_window(L"FPRecorder " + version);
	MA_ASSERT(window != 0);
	g_KeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
	main_items_construct();
	while (true) {
		wait(5);
		update_window(window);
		if (gui::try_close) {
			gui::try_close = false;
			if (g_Recording) {
				if (sound_events == MA_TRUE)		play_from_memory(Error_wav, 15499);
				SendNotification(L"Unable to exit, while recording.");
				continue;
			}
			UnhookWindowsHookEx(g_KeyboardHook);
			exit(0);
		}
		if (is_pressed(record_manager) and !g_RecordingsManager) {
			loopback_device = get_list_position(output_devices_list);
			input_device = get_list_position(input_devices_list);
			conf.write("input-device", std::to_string(input_device));
			conf.write("loopback-device", std::to_string(loopback_device));
			conf.save();
			window_reset();
			record_manager_items_construct();
			if (sound_events == MA_TRUE)play_from_memory(Openmanager_wav, 20673);

			g_RecordingsManager = true;
		}
		if (g_RecordingsManager) {
			if (key_down(VK_ESCAPE) or is_pressed(close_button)) {
				if (g_SoundActive) {
					ma_sound_uninit(&player);
					g_SoundActive = false;
				}
				SendNotification(L"Closed.");
				window_reset();
				main_items_construct();
				focus(record_manager);
				g_RecordingsManager = false;
			}
			std::wstring record_path_u;
			unicode_convert(record_path, record_path_u);
			std::vector<wstring> files = get_files(record_path_u);
			if (files.size() == 0) {
				if (sound_events == MA_TRUE)		play_from_memory(Error_wav, 15499);
				SendNotification(L"There are no files in \"" + record_path_u + L"\".");
				window_reset();
				main_items_construct();
				focus(record_manager);
				g_RecordingsManager = false;
			}
			if ((key_down(VK_SPACE) && get_current_focus() == items_view_list) || is_pressed(play_button)) {
				std::wstring record_path_u;
				unicode_convert(record_path, record_path_u);
				play(record_path_u + L"/" + get_focused_list_item_name(items_view_list));
				if (get_current_focus() == play_button)focus(pause_button);
			}
			if (is_pressed(pause_button) and g_SoundActive) {
				ma_sound_stop(&player);
				if (get_current_focus() == pause_button)focus(play_button);

			}
			if (is_pressed(stop_button) and g_SoundActive) {
				ma_sound_seek_to_pcm_frame(&player, 0);
				ma_sound_stop(&player);
			}
			if (key_pressed(VK_DELETE) or is_pressed(delete_button)) {
				if (get_focused_list_item_name(items_view_list) == L"")continue;
				wait(10);
				int result = alert(L"FPWarning", L"Are you sure you want to delete the recording \"" + get_focused_list_item_name(items_view_list) + L"\"? It can no longer be restored.", MB_YESNO | MB_ICONEXCLAMATION);
				if (result == IDNO)continue;
				else if (result == IDYES)
				{
					std::wstring record_path_u;
					unicode_convert(record_path, record_path_u);
					std::wstring file = record_path_u + L"/" + get_focused_list_item_name(items_view_list);
					if (g_SoundActive) {
						ma_sound_uninit(&player);
						g_SoundActive = false;
					}
					DeleteFile(file.c_str());
					window_reset();
					record_manager_items_construct();
				}
			}
		}
		if (is_pressed(record_start) and !g_Recording) {
			loopback_device = get_list_position(output_devices_list);
			input_device = get_list_position(input_devices_list);
			conf.write("input-device", std::to_string(input_device));
			conf.write("loopback-device", std::to_string(loopback_device));
			conf.save();
			if (sound_events == MA_TRUE)play_from_memory(Start_wav, 6523);
			std::wstring in_device_name = get_focused_list_item_name(input_devices_list);
			for (unsigned int i = 0; i < in_audio_devices.size(); i++) {
				if (in_audio_devices[i].name == in_device_name) {
					g_CurrentInputDevice = in_audio_devices[i];
					break;
				}
			}
			std::wstring out_device_name = get_focused_list_item_name(output_devices_list);
			for (unsigned int i = 0; i < out_audio_devices.size(); i++) {
				if (out_audio_devices[i].name == out_device_name) {
					g_CurrentOutputDevice = out_audio_devices[i];
					break;
				}
			}

			window_reset();
			record_items_construct();
			rec.start();
			g_Recording = true;
			g_RecordingPaused = false;
		}
		if (is_pressed(record_stop) and g_Recording) {
			rec.stop();
			if (sound_events == MA_TRUE)play_from_memory(Stop_wav, 6533);
			g_Recording = false;
			window_reset();
			main_items_construct();
			if (audio_format != "wav") {
				bool result = run("ffmpeg.exe", "-i \"" + rec.filename + "\" \"" + rec.filename + "." + audio_format + "\"", false, true);
				if (result == false) {
					if (sound_events == MA_TRUE)play_from_memory(Error_wav, 15499);
					alert(L"FPError", L"\"ffmpeg.exe\" was not found. Can't convert audio file!", MB_ICONERROR);
				}
			}
		}
		if (is_pressed(record_pause) and g_Recording) {
			if (!g_RecordingPaused) {
				rec.pause();
				if (sound_events == MA_TRUE)play_from_memory(Pause_wav, 9545);
				g_RecordingPaused = true;
				set_text(record_pause, L"&Resume recording");
			}
			else if (g_RecordingPaused) {
				if (sound_events == MA_TRUE)play_from_memory(Unpause_wav, 12221);
				rec.resume();
				g_RecordingPaused = false;
				set_text(record_pause, L"&Pause recording");
			}
			ma_sleep(100);
		}
		if (is_pressed(record_restart) and g_Recording) {
			wait(10);
			std::wstring recording_name_u;
			unicode_convert(rec.filename, recording_name_u);
			int result = alert(L"FPWarning", L"Are you sure you want to delete the recording \"" + recording_name_u + L"\" and rerecord it to new one? Old record can no longer be restored.", MB_YESNO | MB_ICONEXCLAMATION);
			if (result == IDNO)continue;
			else if (result == IDYES)
			{
				rec.stop();
			}
			rec.start();
			g_Recording = true;
			g_RecordingPaused = false;
			set_text(record_pause, L"&Pause recording");
			DeleteFile(recording_name_u.c_str());
			if (sound_events == MA_TRUE)play_from_memory(Restart_wav, 3563);
		}
	}
	(void)hInstance;
	(void)hPrevInstance;
	return MA_SUCCESS;
}