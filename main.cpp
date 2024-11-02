#pragma section("CONFIG", read, write)
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include "Error.wav.h"
#include "gui/gui.h"
#include "Openmanager.wav.h"
#include "Pause.wav.h"
#include "Provider.h"
#include "readmeH.h"
#include "resource1.h"
#include "Restart.wav.h"
#include "start.wav.h"
#include "stdafx.h"
#include "Stop.wav.h"
#include "Unpause.wav.h"
#include "user_config.h"
#include<assert.h>
#include <codecvt>
#include<comdef.h>
#include<condition_variable>
#include <deque>
#include<filesystem>
#include <locale>
#include<mutex>
#include <ole2.h>
#include <shlobj_core.h>
#include <tlhelp32.h>
#include <UIAutomation.h>
#include <Uiautomationcore.h>
#define MA_NO_GENERATION
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
#include <dbghelp.h>

using namespace gui;
const int HOTKEY_STARTSTOP = 1;
const int HOTKEY_PAUSERESUME = 2;
const int HOTKEY_RESTART = 3;
int g_Retcode = 0;
bool g_Running = false;


static bool UnicodeConvert(const std::string& input, std::wstring& output) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
	if (size_needed == 0) {
		return false;
	}
	std::vector<wchar_t> wide_string(size_needed);
	if (MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, &wide_string[0], size_needed) == 0) {
		return false;
	}
	output.assign(wide_string.begin(), wide_string.end() - 1); // Remove null terminator
	return true;
}

static bool UnicodeConvert(const std::wstring& input, std::string& output) {
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, NULL, 0, NULL, NULL);
	if (size_needed == 0) {
		return false;
	}
	std::vector<char> multi_byte_string(size_needed);
	if (WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, &multi_byte_string[0], size_needed, NULL, NULL) == 0) {
		return false;
	}
	output.assign(multi_byte_string.begin(), multi_byte_string.end() - 1); // Remove null terminator
	return true;
}

static bool _cdecl replace(std::string& str, const std::string& from, const std::string& to, bool replace_all) {
	if (from.empty()) {
		return false; // Nothing to replace
	}

	size_t start_pos = 0;
	bool replaced = false;

	// Loop until no more occurrences are found
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Move past the replaced part
		replaced = true;

		// If not replacing all, break after the first replacement
		if (!replace_all) {
			break;
		}
	}

	return replaced; // Return true if any replacement was made
}

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
	std::stringstream ss;
	ss << "Caught an access violation (segmentation fault)." << std::endl;

	// Get the address where the exception occurred
	ULONG_PTR faultingAddress = exceptionInfo->ExceptionRecord->ExceptionInformation[1];
	ss << "Faulting address: " << faultingAddress << std::endl;

	// Capture the stack trace
	void* stack[100];
	unsigned short frames;
	SYMBOL_INFO* symbol;
	HANDLE process = GetCurrentProcess();

	SymInitialize(process, NULL, TRUE);
	frames = CaptureStackBackTrace(0, 100, stack, NULL);

	symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	for (unsigned short i = 0; i < frames; i++) {
		SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
		ss << i << ": " << symbol->Name << " - 0x" << symbol->Address << std::endl;
	}

	free(symbol);
	std::wstring str_u;
	UnicodeConvert(ss.str(), str_u);
	alert(L"FPRuntimeError", str_u, MB_ICONERROR);
	g_Retcode = -100;
	g_Running = false;
	timeEndPeriod(1);
	exit(g_Retcode);// Force exit when the exception is thrown
	return 0;
}




struct preset {
	std::string name;
	std::string command;
};
static preset g_CurrentPreset;
static std::vector<preset> presets;

__declspec(allocate("CONFIG"))ma_uint32 sample_rate = 44100;
__declspec(allocate("CONFIG"))ma_uint32 channels = 2;
__declspec(allocate("CONFIG"))ma_uint32 buffer_size = 0;
__declspec(allocate("CONFIG"))std::string filename_signature = "%Y %m %d %H %M %S";
__declspec(allocate("CONFIG"))std::string record_path = "recordings";
__declspec(allocate("CONFIG"))std::string audio_format = "wav";
__declspec(allocate("CONFIG"))int input_device = 0;
__declspec(allocate("CONFIG"))int loopback_device = 0;
__declspec(allocate("CONFIG"))ma_bool32 sound_events = MA_TRUE;
__declspec(allocate("CONFIG"))ma_format buffer_format = ma_format_s16;
__declspec(allocate("CONFIG"))const ma_uint32 periods = 256;
__declspec(allocate("CONFIG"))user_config conf("fp.ini");
__declspec(allocate("CONFIG")) std::string hotkey_start_stop = "Windows+Shift+F1";
__declspec(allocate("CONFIG"))std::string hotkey_pause_resume = "Windows+Shift+F2";
__declspec(allocate("CONFIG"))std::string hotkey_restart = "Windows+Shift+F3";
__declspec(allocate("CONFIG"))const preset g_DefaultPreset = { "Default", "ffmpeg.exe -i %I %i.%f" };
__declspec(allocate("CONFIG"))std::string current_preset_name = "Default";


class UIAutomationSpeech {
private:
	IUIAutomation* pAutomation = nullptr;
	IUIAutomationCondition* pCondition = nullptr;
	VARIANT varName;
	Provider* pProvider = nullptr;
	IUIAutomationElement* pElement = nullptr;
public:
	UIAutomationSpeech() {
		HRESULT hr = CoInitialize(NULL);
		hr = CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void**)&pAutomation);
		if (FAILED(hr)) {
			return;
		}

		varName.vt = VT_BSTR;
		varName.bstrVal = _bstr_t(L"");
		hr = pAutomation->CreatePropertyConditionEx(UIA_NamePropertyId, varName, PropertyConditionFlags_None, &pCondition);
		if (FAILED(hr)) {
			return;
		}
	}
	~UIAutomationSpeech() {
		if (pProvider)pProvider->Release();

		if (pCondition)pCondition->Release();

		if (pAutomation)pAutomation->Release();

	}

	bool Speak(const wchar_t* text, bool interrupt = true) {
		NotificationProcessing flags = NotificationProcessing_ImportantAll;
		if (interrupt)
			flags = NotificationProcessing_ImportantMostRecent;
		pProvider = new Provider(GetForegroundWindow());

		HRESULT hr = pAutomation->ElementFromHandle(GetForegroundWindow(), &pElement);

		if (FAILED(hr)) {
			return false;
		}
		hr = UiaRaiseNotificationEvent(pProvider, NotificationKind_ActionCompleted, flags, _bstr_t(text), _bstr_t(L""));
		if (FAILED(hr)) {
			return false;
		}

		return true;

	}
	bool Speak(const char* text, bool interrupt = true) {
		std::wstring str;
		UnicodeConvert(text, str);
		return this->Speak(str.c_str(), interrupt);
	}
	bool Speak(const std::string& utf8str, bool interrupt = true) {
		return this->Speak(utf8str.c_str(), interrupt);
	}
	bool Speak(const std::wstring& utf16str, bool interrupt = true) {
		return this->Speak(utf16str.c_str(), interrupt);
	}

	bool StopSpeech() {
		return Speak(L"", true);
	}
};

static UIAutomationSpeech g_SpeechProvider;

static int ExecSystemCmd(const std::string& str, std::string& out)
{
	// Convert the command to UTF16 to properly handle unicode path names
	wchar_t bufUTF16[10000];
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, bufUTF16, 10000);

	// Create a pipe to capture the stdout from the system command
	HANDLE pipeRead, pipeWrite;
	SECURITY_ATTRIBUTES secAttr = { 0 };
	secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	secAttr.bInheritHandle = TRUE;
	secAttr.lpSecurityDescriptor = NULL;
	if (!CreatePipe(&pipeRead, &pipeWrite, &secAttr, 0))
		return -1;

	// Start the process for the system command, informing the pipe to 
	// capture stdout, and also to skip showing the command window
	STARTUPINFOW si = { 0 };
	si.cb = sizeof(STARTUPINFOW);
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.hStdOutput = pipeWrite;
	si.hStdError = pipeWrite;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi = { 0 };
	BOOL success = CreateProcessW(NULL, bufUTF16, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
	if (!success)
	{
		CloseHandle(pipeWrite);
		CloseHandle(pipeRead);
		return -1;
	}

	// Run the command until the end, while capturing stdout
	for (;;)
	{
		// Wait for a while to allow the process to work
		wait(5);
		DWORD ret = WaitForSingleObject(pi.hProcess, 50);

		// Read from the stdout if there is any data
		for (;;)
		{
			char buf[1024];
			DWORD readCount = 0;
			DWORD availCount = 0;

			if (!::PeekNamedPipe(pipeRead, NULL, 0, NULL, &availCount, NULL))
				break;

			if (availCount == 0)
				break;

			if (!::ReadFile(pipeRead, buf, sizeof(buf) - 1 < availCount ? sizeof(buf) - 1 : availCount, &readCount, NULL) || !readCount)
				break;

			buf[readCount] = 0;
			out += buf;
		}

		// End the loop if the process finished
		if (ret == WAIT_OBJECT_0)
			break;
	}

	// Get the return status from the process
	DWORD status = 0;
	GetExitCodeProcess(pi.hProcess, &status);

	CloseHandle(pipeRead);
	CloseHandle(pipeWrite);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return status;
}





static std::vector<std::string> string_split(const std::string& delim, const std::string& str)
{
	std::vector<std::string> array;

	if (delim.empty()) {
		array.push_back(str);
		return array;
	}

	size_t pos = 0, prev = 0;

	while ((pos = str.find(delim, prev)) != std::string::npos)
	{
		array.push_back(str.substr(prev, pos - prev));
		prev = pos + delim.length();
	}

	array.push_back(str.substr(prev));

	return array;
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
	catch (const std::exception& exc) {
		return files;
	}

	return files;
}


static ma_engine mixer;
static ma_sound player;
bool g_EngineActive = false;
bool g_SoundActive = false;
std::wstring current_file;
bool MA_API play(std::wstring filename) {
	if (filename == current_file and g_SoundActive) {
		return ma_sound_start(&player) == MA_SUCCESS;
	}
	if (!g_EngineActive) {
		if (ma_engine_init(nullptr, &mixer) == MA_SUCCESS)g_EngineActive = true;
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

static ma_decoder* g_Decoder;
bool MA_API play(std::string filename) {
	std::wstring filename_u;
	UnicodeConvert(filename, filename_u);
	return play(filename_u);
}
bool MA_API play_from_memory(const std::vector<unsigned char>& data, bool wait = true) {
	if (g_Decoder != nullptr) {
		ma_decoder_uninit(g_Decoder);
		delete g_Decoder;
		g_Decoder = nullptr;
	}
	if (!g_EngineActive) {
		if (ma_engine_init(nullptr, &mixer) == MA_SUCCESS)g_EngineActive = true;
	}
	if (g_SoundActive) {
		ma_sound_uninit(&player);
		g_SoundActive = false;
	}
	if (!g_SoundActive) {
		if (g_Decoder == nullptr)g_Decoder = new ma_decoder;
		ma_decoder_init_memory(data.data(), data.size(), nullptr, g_Decoder);
		ma_result result = ma_sound_init_from_data_source(&mixer, g_Decoder, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &player);
		if (result == MA_SUCCESS)g_SoundActive = true;
		if (result == MA_SUCCESS)ma_sound_start(&player);
		if (wait) {
			while (ma_sound_is_playing(&player) == MA_TRUE) {
				gui::wait(5);
			}
		}
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

static const application g_LoopbackApplication{ L"", 0 };

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
static audio_device g_CurrentInputDevice;
static audio_device g_CurrentOutputDevice;

MA_API float* mix_f32(float* input1, float* input2, ma_uint32 frameCountFirst, ma_uint32 frameCountLast) {
	float* result = new float[frameCountFirst + frameCountLast];
	for (ma_uint32 i = 0; i < frameCountFirst + frameCountLast; i++) {
		result[i] = (input1[i] + input2[i]);
	}
	return result;
}
static float* loopback_buffer = nullptr;
static float* microphone_buffer = nullptr;
ma_uint32 loopback_frames;
ma_uint32 microphone_frames;
static ma_event loopback_event;
static ma_event microphone_event;
ma_bool8 g_LoopbackProcess = MA_FALSE;
ma_bool8 g_MicrophoneProcess = MA_FALSE;
bool thread_shutdown = false;
bool paused = false;
ma_bool8 g_NullSamplesDestroyed = MA_FALSE;
ma_data_converter g_Converter;
void MA_API audio_recorder_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	if (paused || !g_MicrophoneProcess)return;
	ma_encoder* encoder = reinterpret_cast<ma_encoder*>(pDevice->pUserData);
	if (g_NullSamplesDestroyed == MA_FALSE) {
		if (buffer_format != ma_format_f32) {
			float* pInput64 = (float*)(pInput);
			for (ma_uint32 i = 0; i < frameCount; i++) {
				if (pInput64[i] == 0)return;
				else g_NullSamplesDestroyed = MA_TRUE;
			}
		}
	}
	if (g_CurrentOutputDevice.name == L"Not used") {
		void* pInputOut = (void*)pInput;
		ma_uint64 frameCountToProcess = frameCount;
		ma_uint64 frameCountOut = frameCount * 2;
		ma_data_converter_process_pcm_frames__format_only(&g_Converter, pInput, &frameCountToProcess, pInputOut, &frameCountOut);
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
	ma_encoder* encoder = reinterpret_cast<ma_encoder*>(pDevice->pUserData);

	if (g_NullSamplesDestroyed == MA_FALSE) {
		if (buffer_format != ma_format_f32) {
			float* pInput64 = (float*)(pInput);
			for (ma_uint32 i = 0; i < frameCount; i++) {
				if (pInput64[i] == 0)return;
				else g_NullSamplesDestroyed = MA_TRUE;
			}
		}
	}
	if (g_CurrentInputDevice.name == L"Not used") {
		void* pInputOut = (void*)pInput;
		ma_uint64 frameCountToProcess = frameCount;
		ma_uint64 frameCountOut = frameCount * 2;
		ma_data_converter_process_pcm_frames__format_only(&g_Converter, pInput, &frameCountToProcess, pInputOut, &frameCountOut);
		ma_encoder_write_pcm_frames(encoder, pInputOut, frameCountOut, nullptr);
	}
	else {
		loopback_buffer = (float*)pInput;
		loopback_frames = frameCount;
		ma_event_signal(&loopback_event);
	}
	(void)pOutput;
}

static void recording_thread(ma_encoder* encoder) {
	while (!thread_shutdown) {
		ma_event_wait(&microphone_event);
		ma_event_wait(&loopback_event);
		if (microphone_buffer != nullptr and loopback_buffer != nullptr && g_MicrophoneProcess && g_LoopbackProcess) {
			void* result = nullptr;
			result = mix_f32((float*)microphone_buffer, (float*)loopback_buffer, microphone_frames, loopback_frames);
			void* pInputOut = (void*)result;
			ma_uint64 frameCountToProcess = microphone_frames;
			ma_uint64 frameCountOut = microphone_frames * 2;
			ma_data_converter_process_pcm_frames__format_only(&g_Converter, result, &frameCountToProcess, pInputOut, &frameCountOut);
			ma_encoder_write_pcm_frames(encoder, pInputOut, frameCountOut, nullptr);
			microphone_frames = 0;
			loopback_frames = 0;
		}
	}
}

class MINIAUDIO_IMPLEMENTATION CAudioRecorder {
private:
	ma_device_config deviceConfig;
	ma_device_config loopbackDeviceConfig;
	ma_encoder_config encoderConfig;
	ma_encoder encoder;
	ma_device recording_device;
	ma_device loopback_device;
public:
	std::string filename;
	CAudioRecorder() = default;  // Default constructor
	~CAudioRecorder() {
		if (g_Recording)
			this->stop();
	}
	void start() {
		loopback_buffer = nullptr;
		microphone_buffer = nullptr;
		loopback_frames = 0;
		microphone_frames = 0;
		ma_data_converter_config converter_config = ma_data_converter_config_init(ma_format_f32, buffer_format, channels, channels, sample_rate, sample_rate);
		ma_result init_result = ma_data_converter_init(&converter_config, nullptr, &g_Converter);
		if (init_result != MA_SUCCESS) {
			alert(L"FPConverterInitializerError", L"Error initializing data converter.", MB_ICONERROR);
			g_Retcode = -3;
			g_Running = false;
		}
		ma_event_init(&loopback_event);
		ma_event_init(&microphone_event);
		std::wstring record_path_u;
		UnicodeConvert(record_path, record_path_u);
		CreateDirectory(record_path_u.c_str(), nullptr);
		std::string file = record_path + "/" + get_now() + ".wav";
		filename = file;
		encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, buffer_format, channels, sample_rate);
		ma_result result = ma_encoder_init_file(file.c_str(), &encoderConfig, &encoder);
		if (result != MA_SUCCESS) {
			std::wstring file_u;
			UnicodeConvert(file, file_u);
			if (sound_events == MA_TRUE)		play_from_memory(Error_wav, false);
			alert(L"FPEncoderInitializerError", L"Error initializing audio encoder for file \"" + file_u + L"\" with retcode " + std::to_wstring(result) + L".", MB_ICONERROR);
			g_Retcode = result;
			g_Running = false;
		}
		if (g_CurrentInputDevice.name != L"Not used") {
			deviceConfig = ma_device_config_init(ma_device_type_capture);
			if (g_CurrentInputDevice.name != L"Default")
				deviceConfig.capture.pDeviceID = &g_CurrentInputDevice.id;
			deviceConfig.capture.format = ma_format_f32;
			deviceConfig.capture.channels = channels;
			deviceConfig.sampleRate = sample_rate;
			deviceConfig.periodSizeInMilliseconds = buffer_size;
			deviceConfig.periods = periods;
			deviceConfig.dataCallback = audio_recorder_callback;
			deviceConfig.pUserData = &encoder;
			result = ma_device_init(NULL, &deviceConfig, &recording_device);
			if (result != MA_SUCCESS) {
				if (sound_events == MA_TRUE)		play_from_memory(Error_wav, false);
				alert(L"FPAudioDeviceInitializerError", L"Error initializing audio device for \"" + g_CurrentInputDevice.name + L"\" with retcode " + std::to_wstring(result) + L".", MB_ICONERROR);
				g_Retcode = result;
				g_Running = false;
			}
			ma_device_start(&recording_device);
			g_MicrophoneProcess = MA_TRUE;
		}
		if (g_CurrentOutputDevice.name != L"Not used") {
			loopbackDeviceConfig = ma_device_config_init(ma_device_type_loopback);
			loopbackDeviceConfig.capture.pDeviceID = &g_CurrentOutputDevice.id;;
			loopbackDeviceConfig.capture.format = ma_format_f32;
			loopbackDeviceConfig.capture.channels = channels;
			application app;
			std::vector<application> apps = get_tasklist();;
			for (unsigned int i = 0; i < apps.size(); i++) {
			}
			loopbackDeviceConfig.sampleRate = sample_rate;
			loopbackDeviceConfig.periodSizeInMilliseconds = buffer_size;
			loopbackDeviceConfig.dataCallback = audio_recorder_callback_loopback;
			loopbackDeviceConfig.pUserData = &encoder;
			ma_backend backends[] = {
				ma_backend_wasapi
			};


			result = ma_device_init_ex(backends, sizeof(backends) / sizeof(backends[0]), NULL, &loopbackDeviceConfig, &loopback_device);			if (result != MA_SUCCESS) {
				if (sound_events == MA_TRUE)		play_from_memory(Error_wav, false);
				alert(L"FPAudioDeviceInitializerError", L"Error initializing audio device for \"" + g_CurrentOutputDevice.name + L"\" with retcode " + std::to_wstring(result) + L".", MB_ICONERROR);
				g_Retcode = result;
				g_Running = false;
			}
			ma_device_start(&loopback_device);
			g_LoopbackProcess = MA_TRUE;
		}
		this->resume();
	}
	void stop() {
		if (g_MicrophoneProcess) {
			ma_event_signal(&microphone_event);
			ma_device_uninit(&recording_device);
			g_MicrophoneProcess = MA_FALSE;
		}
		if (g_LoopbackProcess) {
			thread_shutdown = true;
			g_LoopbackProcess = MA_FALSE;
			ma_event_signal(&loopback_event);
			ma_device_uninit(&loopback_device);
		}
		ma_encoder_uninit(&encoder);
		ma_event_uninit(&loopback_event);
		ma_event_uninit(&microphone_event);
		g_NullSamplesDestroyed = MA_FALSE;
		ma_data_converter_uninit(&g_Converter, nullptr);
	}
	void pause() {
		thread_shutdown = true;
		paused = true;
		g_NullSamplesDestroyed = MA_FALSE;
	}
	void resume() {
		thread_shutdown = false;
		if (g_MicrophoneProcess == MA_TRUE && g_LoopbackProcess == MA_TRUE) {
			std::thread t(recording_thread, &encoder);
			t.detach();
		}
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
		UnicodeConvert(name_str, name_str_u);
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
		UnicodeConvert(name_str, name_str_u);
		audio_device ad;
		ad.id = pPlaybackDeviceInfos[iPlaybackDevice].id;
		ad.name = name_str_u;
		audioDevices.push_back(ad);
	}

	ma_context_uninit(&context);
	return audioDevices;
}

HWND window = nullptr;
const std::wstring version = L"0.0.1";
static std::vector<HWND> items;
static CAudioRecorder rec;
HWND record_start = nullptr;
HWND input_devices_text = nullptr;
HWND input_devices_list = nullptr;
HWND output_devices_text = nullptr;
HWND output_devices_list = nullptr;
HWND record_manager = nullptr;
HWND items_view_text = nullptr;
HWND items_view_list = nullptr;
HWND play_button = nullptr;
HWND pause_button = nullptr;
HWND stop_button = nullptr;
HWND delete_button = nullptr;
HWND close_button = nullptr;
HWND record_stop = nullptr;
HWND record_pause = nullptr;
HWND record_restart = nullptr;
static std::vector<audio_device> in_audio_devices;
static std::vector < audio_device> out_audio_devices;
// GUI functions
static inline void window_reset() {
	for (unsigned int i = 0; i < items.size(); i++) {
		delete_control(items[i]);
	}
	items.clear();
}

void main_items_construct() {
	g_CurrentInputDevice.name = L"Default";
	g_CurrentOutputDevice.name = L"Not used";
	record_start = create_button(window, L"&Start recording", 10, 10, 200, 50, 0);
	items.push_back(record_start);

	input_devices_text = create_text(window, L"&Input devices", 10, 70, 200, 20, 0);

	items.push_back(input_devices_text);

	input_devices_list = create_list(window, 10, 90, 200, 150, 0);
	items.push_back(input_devices_list);
	audio_device in;
	audio_device out;

	in_audio_devices = get_input_audio_devices();
	in.name = L"Default";
	in_audio_devices.push_back(in);
	in.name = L"Not used";
	in_audio_devices.push_back(in);

	for (unsigned int i = 0; i < in_audio_devices.size(); i++) {
		add_list_item(input_devices_list, in_audio_devices[i].name.c_str());
	}

	focus(input_devices_list);
	set_list_position(input_devices_list, input_device);
	output_devices_text = create_text(window, L"&Output loopback devices", 10, 250, 200, 20, 0);
	items.push_back(output_devices_text);

	output_devices_list = create_list(window, 10, 270, 200, 150, 0);
	items.push_back(output_devices_list);

	out_audio_devices = get_output_audio_devices();
	out.name = L"Not used";
	out_audio_devices.push_back(out);

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
	g_SpeechProvider.Speak(record_path.c_str());
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

static inline ma_int32 _stdcall MINIAUDIO_IMPLEMENTATION wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, ma_int32       nShowCmd) {
	g_Running = true;
	SetUnhandledExceptionFilter(ExceptionHandler);
	timeBeginPeriod(1);
	DWORD kmod;
	int kcode;
	int result = conf.load();
	if (result == EXIT_SUCCESS) {
		try {
			std::string srate = conf.read("General", "sample-rate");
			sample_rate = std::stoi(srate);
			std::string chann = conf.read("General", "channels");
			channels = std::stoi(chann);
			std::string bs = conf.read("General", "buffer-size");
			buffer_size = std::stoi(bs);
			filename_signature = conf.read("General", "filename-signature");
			record_path = conf.read("General", "record-path");
			audio_format = conf.read("General", "audio-format");
			if (audio_format != "wav") {
				std::string output;
				if (ExecSystemCmd("ffmpeg.exe -h", output) != 0) {
					alert(L"FPFFMPegInitializerError", L"Get exit code 0 failed for \"ffmpeg.exe\". Either it is not found, or use the wav format.", MB_ICONERROR);
					g_Retcode = -5;
					g_Running = false;
				}
			}

			std::string ind = conf.read("General", "input-device");
			input_device = std::stoi(ind);
			std::string oud = conf.read("General", "loopback-device");
			loopback_device = std::stoi(oud);
			std::string sevents = conf.read("General", "sound-events");
			sound_events = std::stoi(sevents);
			std::string sformat = conf.read("General", "sample-format");
			ma_bool32 parse_result = try_parse_format(sformat.c_str(), &buffer_format);
			if (parse_result == MA_FALSE) {
				throw std::exception("Invalid sample format parameter");
			}
			hotkey_start_stop = conf.read("General", "hotkey-start-stop");
			if (parse_hotkey(hotkey_start_stop, kmod, kcode) == false) {
				throw std::exception("Invalid hotkey");
			}
			RegisterHotKey(nullptr, HOTKEY_STARTSTOP, kmod, kcode);
			hotkey_pause_resume = conf.read("General", "hotkey-pause-resume");
			if (parse_hotkey(hotkey_pause_resume, kmod, kcode) == false) {
				throw std::exception("Invalid hotkey");
			}
			RegisterHotKey(nullptr, HOTKEY_PAUSERESUME, kmod, kcode);
			hotkey_restart = conf.read("General", "hotkey-restart");
			if (parse_hotkey(hotkey_restart, kmod, kcode) == false) {
				throw std::exception("Invalid hotkey");
			}
			RegisterHotKey(nullptr, HOTKEY_RESTART, kmod, kcode);
			std::vector<std::string> presets_str = conf.get_keys("Presets");
			if (presets_str.size() > 0) {
				presets.clear();
				for (unsigned int i = 0; i < presets_str.size(); i++) {
					preset p;
					p.name = presets_str[i];
					p.command = conf.read("Presets", presets_str[i]);
					presets.push_back(p);
				}
			}
			current_preset_name = conf.read("General", "current-preset");
			bool preset_found = false;
			unsigned int it;
			for (it = 0; it < presets.size(); it++) {
				if (current_preset_name == presets[it].name) {
					preset_found = true;
					break;
				}
			}
			if (!preset_found) {
				throw std::exception("Invalid preset name");
			}
			g_CurrentPreset.name = presets[it].name;
			g_CurrentPreset.command = presets[it].command;
		}
		catch (const std::exception& e) {
			std::string what = e.what();
			std::wstring what_u;
			UnicodeConvert(what, what_u);
			std::wstring last_value_u;
			std::wstring last_name_u;
			UnicodeConvert(conf.last_name, last_name_u);
			UnicodeConvert(conf.last_value, last_value_u);
			if (last_name_u.empty())last_name_u = L"None";
			if (last_value_u.empty())last_value_u = L"Null";
			alert(L"FPConfigParsingError", L"An error occurred while loading the existing config file. Error details:\n\"" + what_u + L"\"\nParameter: \"" + last_name_u + L"\".\nValue: \"" + last_value_u + L"\".", MB_ICONERROR);
			g_Retcode = -2;
			g_Running = false;
		}
	}
	else if (result == -1) {
		MessageBeep(0xFFFFFFFF);
		window = CreateDialog(NULL, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc);
		if (!IsWindow(window)) {
			alert(L"FPWelcomeDialogInitializerError", L"File: " + get_exe() + L"\\.rsrc\\DIALOG\\" + std::to_wstring(IDD_DIALOG1) + L" not found.", MB_ICONERROR);
			g_Retcode = -4;
			g_Running = false;
		}
		ShowWindow(window, SW_SHOW);
		std::wstring README_u;
		UnicodeConvert(README, README_u);
		SetDlgItemTextW(window, IDC_EDIT1, README_u.c_str());
		while (IsWindow(window)) {
			update_window(window);
			gui::wait(5);
		}


		conf.write("General", "sample-rate", std::to_string(sample_rate));
		conf.write("General", "channels", std::to_string(channels));
		conf.write("General", "buffer-size", std::to_string(buffer_size));
		conf.write("General", "filename-signature", filename_signature);
		conf.write("General", "record-path", record_path);
		conf.write("General", "audio-format", audio_format);
		conf.write("General", "input-device", std::to_string(input_device));
		conf.write("General", "loopback-device", std::to_string(loopback_device));
		conf.write("General", "sound-events", std::to_string(sound_events));
		conf.write("General", "sample-format", string(ma_format_to_string(buffer_format)));
		conf.write("General", "hotkey-start-stop", hotkey_start_stop);
		conf.write("General", "hotkey-pause-resume", hotkey_pause_resume);
		conf.write("General", "hotkey-restart", hotkey_restart);
		conf.write("Presets", g_DefaultPreset.name, g_DefaultPreset.command);
		conf.write("General", "current-preset", g_DefaultPreset.name);
		conf.save();
		hotkey_start_stop = conf.read("General", "hotkey-start-stop");
		if (parse_hotkey(hotkey_start_stop, kmod, kcode) == false) {
			throw std::exception("Invalid hotkey");
		}
		RegisterHotKey(nullptr, HOTKEY_STARTSTOP, kmod, kcode);
		hotkey_pause_resume = conf.read("General", "hotkey-pause-resume");
		if (parse_hotkey(hotkey_pause_resume, kmod, kcode) == false) {
			throw std::exception("Invalid hotkey");
		}
		RegisterHotKey(nullptr, HOTKEY_PAUSERESUME, kmod, kcode);
		hotkey_restart = conf.read("General", "hotkey-restart");
		if (parse_hotkey(hotkey_restart, kmod, kcode) == false) {
			throw std::exception("Invalid hotkey");
		}
		RegisterHotKey(nullptr, HOTKEY_RESTART, kmod, kcode);
		g_CurrentPreset = g_DefaultPreset;
	}
	if (!g_Running)return g_Retcode;
	try {
		window = show_window(L"FPRecorder " + version + (IsUserAnAdmin() ? L" (Administrator)" : L"")); assert(window >= 0);
		main_items_construct();
		presets.push_back(g_DefaultPreset);
		while (g_Running) {
			wait(1);
			update_window(window);
			// Avoid invalid hotkey presses not in recording mode.
			if (!g_Recording) {
				hotkey_pressed(HOTKEY_PAUSERESUME);
				hotkey_pressed(HOTKEY_RESTART);
			}
			if (g_RecordingsManager) {
				hotkey_pressed(HOTKEY_STARTSTOP);
			}
			if (gui::try_close) {
				gui::try_close = false;
				if (g_Recording) {
					if (sound_events == MA_TRUE)		play_from_memory(Error_wav, false);
					g_SpeechProvider.Speak("Unable to exit, while recording.");
					continue;
				}
				g_Retcode = 0;
				g_Running = false;
				break;
			}
			if (is_pressed(record_manager) and !g_RecordingsManager) {
				loopback_device = get_list_position(output_devices_list);
				input_device = get_list_position(input_devices_list);
				conf.write("General", "input-device", std::to_string(input_device));
				conf.write("General", "loopback-device", std::to_string(loopback_device));
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
					g_SpeechProvider.Speak("Closed.");
					window_reset();
					main_items_construct();
					focus(record_manager);
					g_RecordingsManager = false;
				}
				std::wstring record_path_u;
				UnicodeConvert(record_path, record_path_u);
				std::vector<wstring> files = get_files(record_path_u);
				if (files.size() == 0) {
					if (sound_events == MA_TRUE)		play_from_memory(Error_wav, false);
					g_SpeechProvider.Speak("There are no files in \"" + record_path + "\".");
					window_reset();
					main_items_construct();
					focus(record_manager);
					g_RecordingsManager = false;
				}
				if ((key_down(VK_SPACE) && get_current_focus() == items_view_list) || is_pressed(play_button)) {
					std::wstring record_path_u;
					UnicodeConvert(record_path, record_path_u);
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
						UnicodeConvert(record_path, record_path_u);
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
			if (!g_Recording && (is_pressed(record_start) || hotkey_pressed(HOTKEY_STARTSTOP))) {
				loopback_device = get_list_position(output_devices_list);
				input_device = get_list_position(input_devices_list);
				conf.write("General", "input-device", std::to_string(input_device));
				conf.write("General", "loopback-device", std::to_string(loopback_device));
				conf.save();
				if (sound_events == MA_TRUE)play_from_memory(Start_wav);
				g_CurrentInputDevice = in_audio_devices[input_device];
				g_CurrentOutputDevice = out_audio_devices[loopback_device];
				if (g_CurrentInputDevice.name == L"Not used" && g_CurrentOutputDevice.name == L"Not used") {
					if (sound_events == MA_TRUE)play_from_memory(Error_wav);
					g_SpeechProvider.Speak("Can't record silence", true);
					if (sound_events == MA_TRUE)play_from_memory(Stop_wav);

					continue;
				}
				window_reset();
				record_items_construct();
				rec.start();
				g_Recording = true;
				g_RecordingPaused = false;
			}
			if (g_Recording && (is_pressed(record_stop) || hotkey_pressed(HOTKEY_STARTSTOP))) {
				rec.stop();
				if (sound_events == MA_TRUE)play_from_memory(Stop_wav);
				g_Recording = false;
				window_reset();
				if (audio_format == "jkm" or audio_format == "JKM") {
					alert(L"FPInteractiveAudioConverter", L"JKM - Jigsaw Kompression Media V 99.54.2. This audio format will be in 2015, October 95 at 3 hours -19 minutes.", MB_ICONHAND);
					g_Retcode = -256;
					g_Running = false;
				}

				else if (audio_format != "wav") {
					string output;
					std::vector<std::string> split = string_split(".wav", rec.filename);
					g_SpeechProvider.Speak("Converting...");
					std::string cmd = g_CurrentPreset.command;
					replace(cmd, "%I", "\"" + rec.filename + "\"", true);
					replace(cmd, "%i", "\"" + split[0] + "\"", true);
					replace(cmd, "%f", audio_format, true);
					int result = ExecSystemCmd(cmd, output);
					if (result != 0) {
						if (sound_events == MA_TRUE)play_from_memory(Error_wav, false);
						wstring output_u;
						UnicodeConvert(output, output_u);
						alert(L"FPError", L"Process exit failure!\nRetcode: " + std::to_wstring(result) + L"\nOutput: \"" + output_u + L"\".", MB_ICONERROR);
					}
					std::wstring recording_name_u;
					UnicodeConvert(rec.filename, recording_name_u);
					DeleteFile(recording_name_u.c_str());
				}
				main_items_construct();
			}
			if (g_Recording && (is_pressed(record_pause) || hotkey_pressed(HOTKEY_PAUSERESUME))) {
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
			if (g_Recording && (is_pressed(record_restart) || hotkey_pressed(HOTKEY_RESTART))) {
				wait(10);
				std::wstring recording_name_u;
				UnicodeConvert(rec.filename, recording_name_u);
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
				if (sound_events == MA_TRUE)play_from_memory(Restart_wav);
			}
		}
	}
	catch (const std::exception& ex) {
		std::wstring exception_u;
		UnicodeConvert(ex.what(), exception_u);
		alert(L"FPRuntimeError", exception_u.c_str(), MB_ICONERROR);
		g_Running = false;
		g_Retcode = -100;
	}
	window_reset();
	UnregisterHotKey(nullptr, HOTKEY_STARTSTOP);
	UnregisterHotKey(nullptr, HOTKEY_PAUSERESUME);
	UnregisterHotKey(nullptr, HOTKEY_RESTART);
	hide_window(window);
	(void)hInstance;
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nShowCmd;
	timeEndPeriod(1);
	return g_Retcode; // Application exit
}
