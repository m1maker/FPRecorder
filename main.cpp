#pragma section("CONFIG", read, write)
#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
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
#include <cctype>
#include <codecvt>
#include<comdef.h>
#include<condition_variable>
#include <cstdarg>
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

#define SAFE_CALL_EX(obj, p, d) \
do {\
if (obj) {\
	obj->p;\
}\
else {\
d;\
}\
} while (0)



#define SAFE_CALL(obj, p) SAFE_CALL_EX(obj, p, void)



#define SAFE_CALL_VAL_EX(r, obj, p, d) \
do {\
	if (obj) {\
	r = obj->p;\
}\
else {\
d;\
}\
} while (0)



#define SAFE_CALL_VAL(r, obj, p) SAFE_CALL_VAL_EX(r, obj, p,  r)

#define confsec __declspec(allocate("CONFIG"))





using namespace gui;
enum EHotKey {
	HOTKEY_STARTSTOP = 1,
	HOTKEY_PAUSERESUME = 2,
	HOTKEY_RESTART = 3
};
int g_Retcode = 0;
bool g_Running = false;

class CStringUtils {
public:
	static std::string ToUpperCase(const std::string& str) {
		std::string result = str;
		std::transform(result.begin(), result.end(), result.begin(),
			[](unsigned char c) { return std::toupper(c); });
		return result;
	}

	static std::string ToLowerCase(const std::string& str) {
		std::string result = str;
		std::transform(result.begin(), result.end(), result.begin(),
			[](unsigned char c) { return std::tolower(c); });
		return result;
	}


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

	static bool _cdecl Replace(std::string& str, const std::string& from, const std::string& to, bool replace_all) {
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

	static std::vector<std::string> Split(const std::string& delim, const std::string& str)
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

};



static bool ParseCommandLineOptions(const wchar_t* lpCmdLine, std::vector<std::string>& options, std::vector<std::string>& values) {
	std::string str;
	CStringUtils::UnicodeConvert(lpCmdLine, str);
	std::vector<std::string> parsed = CStringUtils::Split(" ", str);
	if (parsed.empty()) return false;
	for (std::string& arg : parsed) {
		if (arg[0] == '-' || arg[0] == '/') {
			arg.erase(arg.begin() + 0);
			options.push_back(arg);
		}
		else {
			values.push_back(arg);
		}
	}
	return true;
}


// This class has values and flags built by command line arguments
class COptionSet {
public:
	bool start = false;
	std::string filename = "";
	bool useFilename = false;
	bool exitAfterStop = false;
};



static COptionSet g_CommandLineOptions;

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* exceptionInfo);


struct preset {
	std::string name;
	std::string command;
};
static preset g_CurrentPreset;
static std::vector<preset> presets;

confsec ma_uint32 sample_rate = 44100;
confsec ma_uint32 channels = 2;
confsec ma_uint32 buffer_size = 0;
confsec std::string filename_signature = "%Y %m %d %H %M %S";
confsec std::string record_path = "recordings";
confsec std::string audio_format = "wav";
confsec int input_device = 0;
confsec int loopback_device = 0;
confsec ma_bool32 sound_events = MA_TRUE;
confsec ma_bool32 make_stems = MA_FALSE;
confsec ma_format buffer_format = ma_format_s16;
confsec const ma_uint32 periods = 256;
confsec std::string hotkey_start_stop = "Windows+Shift+F1";
confsec std::string hotkey_pause_resume = "Windows+Shift+F2";
confsec std::string hotkey_restart = "Windows+Shift+F3";
confsec const preset g_DefaultPreset = { "Default", "ffmpeg.exe -i %I %i.%f" };
confsec std::string current_preset_name = "Default";
confsec static user_config conf("fp.ini");

class CUIAutomationSpeech {
	IUIAutomation* pAutomation = nullptr;
	IUIAutomationCondition* pCondition = nullptr;
	VARIANT varName;
	Provider* pProvider = nullptr;
	IUIAutomationElement* pElement = nullptr;
public:
	CUIAutomationSpeech() {
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
	~CUIAutomationSpeech() {
		SAFE_CALL(pProvider, Release());
		SAFE_CALL(pCondition, Release());
		SAFE_CALL(pAutomation, Release());
	}

	bool Speak(const wchar_t* text, bool interrupt = true) {
		NotificationProcessing flags = NotificationProcessing_ImportantAll;
		if (interrupt)
			flags = NotificationProcessing_ImportantMostRecent;
		pProvider = new Provider(GetForegroundWindow());

		HRESULT hr = 0;
		SAFE_CALL_VAL_EX(hr, pAutomation, ElementFromHandle(GetForegroundWindow(), &pElement), return false);
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
		CStringUtils::UnicodeConvert(text, str);
		return this->Speak(str.c_str(), interrupt);
	}
	bool Speak(const std::string& utf8str, bool interrupt = true) {
		return this->Speak(utf8str.c_str(), interrupt);
	}
	bool Speak(const std::wstring& utf16str, bool interrupt = true) {
		return this->Speak(utf16str.c_str(), interrupt);
	}

	bool Speakf(const char* format, ...) {
		const size_t bufferSize = 1024;
		char buffer[bufferSize];

		va_list args;
		va_start(args, format);

		int ret = vsnprintf(buffer, bufferSize, format, args);
		va_end(args);

		if (ret < 0 || ret >= bufferSize) {
			return false;
		}

		return Speak(buffer);
	}


	bool StopSpeech() {
		return Speak(L"", true);
	}
};

static CUIAutomationSpeech g_SpeechProvider;

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
		YieldProcessor();
		wait(5);
		void;
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





static std::vector<std::wstring> WINAPI get_files(std::wstring path) {
	std::vector<std::wstring> files;

	try {
		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			if (std::filesystem::is_regular_file(entry)) {
				files.push_back(entry.path().filename().wstring());
			}
		}
	}
	catch (...) {
		return files;
	}

	return files;
}






class CAudioContext {
	ma_context context;
public:
	CAudioContext() {
		ma_result result;
		if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
			alert(L"FPError", L"Failed to initialize context.");
		}
	}

	~CAudioContext() {
		ma_context_uninit(&context);
	}
	operator ma_context* () {
		return &context;
	}
};


static CAudioContext g_AudioContext;





class CSoundStream {
	ma_engine mixer;
	ma_sound player;
	ma_decoder* m_Decoder;
	bool m_EngineActive = false;
	bool m_SoundActive = false;
public:
	std::wstring current_file;
	~CSoundStream() {
		if (m_Decoder != nullptr) {
			ma_decoder_uninit(m_Decoder);
			delete m_Decoder;
			m_Decoder = nullptr;
		}

		if (m_SoundActive) {
			ma_sound_uninit(&player);
			m_SoundActive = false;
		}
		if (m_EngineActive) {
			ma_engine_uninit(&mixer);
			m_EngineActive = false;
		}
	}

	bool Play(const std::wstring& filename) {
		if (filename == current_file and m_SoundActive) {
			return ma_sound_start(&player) == MA_SUCCESS;
		}
		if (!m_EngineActive) {
			if (ma_engine_init(nullptr, &mixer) == MA_SUCCESS)m_EngineActive = true;
		}
		if (m_SoundActive) {
			ma_sound_uninit(&player);
			m_SoundActive = false;
		}
		if (!m_SoundActive) {
			ma_result result = ma_sound_init_from_file_w(&mixer, filename.c_str(), MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, &player);
			if (result == MA_SUCCESS)m_SoundActive = true;
			if (result == MA_SUCCESS)ma_sound_start(&player);
			current_file = filename;
			return result == MA_SUCCESS;
		}
		return false;
	}

	bool Play(const std::string& filename) {
		std::wstring filename_u;
		CStringUtils::UnicodeConvert(filename, filename_u);
		return Play(filename_u);
	}

	bool PlayFromMemory(const unsigned char* data, bool wait = true) {
		if (m_Decoder != nullptr) {
			ma_decoder_uninit(m_Decoder);
			delete m_Decoder;
			m_Decoder = nullptr;
		}
		if (!m_EngineActive) {
			if (ma_engine_init(nullptr, &mixer) == MA_SUCCESS)m_EngineActive = true;
		}
		if (m_SoundActive) {
			ma_sound_uninit(&player);
			m_SoundActive = false;
		}
		if (!m_SoundActive) {
			if (m_Decoder == nullptr)m_Decoder = new ma_decoder;
			ma_decoder_init_memory((void*)data, 13000, nullptr, m_Decoder); // For these are FPRecorder short sounds. It is normal allocation size for
			ma_result result = ma_sound_init_from_data_source(&mixer, m_Decoder, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &player);
			if (result == MA_SUCCESS)m_SoundActive = true;
			if (result == MA_SUCCESS)ma_sound_start(&player);
			if (wait) {
				while (ma_sound_is_playing(&player)) {
					gui::wait(5);
				}
			}
			return result == MA_SUCCESS;
		}
		return false;
	}

	void Close() {
		if (m_SoundActive) {
			ma_sound_uninit(&player);
			m_SoundActive = false;
		}
	}

	void Stop() {
		if (m_SoundActive) {
			ma_sound_seek_to_pcm_frame(&player, 0);
			ma_sound_stop(&player);
		}
	}

	void Pause() {
		if (m_SoundActive) {
			ma_sound_stop(&player);
		}
	}

	bool Play() {
		if (m_SoundActive)
			return ma_sound_start(&player) == MA_SUCCESS;
		return false;
	}
};

static CSoundStream g_SoundStream;

static inline ma_bool32 try_parse_format(const char* str, ma_format* pValue)
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

static inline const char* ma_format_to_string(ma_format format) {
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
	return NULL;
}


static inline std::string _cdecl get_now() {
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

static std::vector<application> WINAPI get_tasklist() {
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

static inline float* mix_f32(float* input1, float* input2, ma_uint32 frameCountFirst, ma_uint32 frameCountLast) {
	float* result = new float[frameCountFirst + frameCountLast];
	for (ma_uint32 i = 0; i < frameCountFirst + frameCountLast; i++) {
		result[i] = (input1[i] + input2[i]);
	}
	return result;
}

enum EProcess : uint8_t {
	PROCESS_NONE = 0,
	PROCESS_MICROPHONE = 1 << 0,
	PROCESS_LOOPBACK = 1 << 2
};

static std::atomic<uint8_t> g_Process = PROCESS_NONE;
static float* loopback_buffer = nullptr;
static float* microphone_buffer = nullptr;
static std::atomic<ma_uint32> loopback_frames;
static std::atomic<ma_uint32> microphone_frames;
static ma_event g_RecordThreadEvent;
static std::atomic<bool> thread_shutdown = false;
static std::atomic<bool> paused = false;
static std::atomic<ma_bool8> g_NullSamplesDestroyed = MA_FALSE;
static ma_data_converter g_Converter;
class MINIAUDIO_IMPLEMENTATION CAudioRecorder {
	ma_device_config deviceConfig;
	ma_device_config loopbackDeviceConfig;
	ma_encoder_config encoderConfig;
	ma_encoder encoder[2];
	ma_device recording_device;
	ma_device loopback_device;
public:
	std::string filename;
	CAudioRecorder() {}
	~CAudioRecorder() {
		if (g_Recording)
			this->stop();
	}
	static void MicrophoneCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
		if (paused || !(g_Process & PROCESS_MICROPHONE)) return;

		ma_encoder* encoder = reinterpret_cast<ma_encoder*>(pDevice->pUserData);

		if (g_NullSamplesDestroyed == MA_FALSE && buffer_format != ma_format_f32) {
			const float* pInput64 = static_cast<const float*>(pInput);
			for (ma_uint32 i = 0; i < frameCount; i++) {
				if (pInput64[i] != 0) {
					g_NullSamplesDestroyed.store(MA_TRUE);
					break; // Exit early on first non-zero sample
				}
			}
		}

		if ((g_Process & PROCESS_LOOPBACK && make_stems) || g_CurrentOutputDevice.name == L"Not used") {
			void* pInputOut = (void*)pInput;
			ma_uint64 frameCountToProcess = frameCount;
			ma_uint64 frameCountOut = frameCount;
			ma_data_converter_process_pcm_frames__format_only(&g_Converter, pInput, &frameCountToProcess, pInputOut, &frameCountOut);
			ma_encoder_write_pcm_frames(encoder, pInputOut, frameCountOut, nullptr);
		}
		else {
			microphone_buffer = (float*)pInput;
			microphone_frames.store(frameCount);
		}
		(void)pOutput; // Avoid unused parameter warning
	}

	static void LoopbackCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
		if (paused || !(g_Process & PROCESS_LOOPBACK)) return;

		ma_encoder* encoder = reinterpret_cast<ma_encoder*>(pDevice->pUserData);

		if (g_NullSamplesDestroyed == MA_FALSE && buffer_format != ma_format_f32) {
			const float* pInput64 = static_cast<const float*>(pInput);
			for (ma_uint32 i = 0; i < frameCount; i++) {
				if (pInput64[i] != 0) {
					g_NullSamplesDestroyed.store(MA_TRUE);
					break; // Exit early on first non-zero sample
				}
			}
		}

		if ((g_Process & PROCESS_MICROPHONE && make_stems) || g_CurrentInputDevice.name == L"Not used") {
			void* pInputOut = (void*)pInput;
			ma_uint64 frameCountToProcess = frameCount;
			ma_uint64 frameCountOut = frameCount;
			ma_data_converter_process_pcm_frames__format_only(&g_Converter, pInput, &frameCountToProcess, pInputOut, &frameCountOut);
			ma_encoder_write_pcm_frames(encoder, pInputOut, frameCountOut, nullptr);
		}
		else {
			loopback_buffer = (float*)pInput;
			loopback_frames.store(frameCount);
			ma_event_signal(&g_RecordThreadEvent);
		}
		(void)pOutput; // Avoid unused parameter warning
	}

	static void MixingThread(ma_encoder* encoder) {
		while (!thread_shutdown && g_Running) {
			ma_event_wait(&g_RecordThreadEvent);

			if (microphone_buffer != nullptr && loopback_buffer != nullptr) {
				void* result = mix_f32(microphone_buffer, loopback_buffer, microphone_frames.load(), loopback_frames.load());
				void* pInputOut = result;

				ma_uint64 frameCountToProcess = microphone_frames.load();
				ma_uint64 frameCountOut = microphone_frames.load();

				if (buffer_format != ma_format_f32) {
					ma_data_converter_process_pcm_frames__format_only(&g_Converter, result, &frameCountToProcess, pInputOut, &frameCountOut);
				}
				ma_encoder_write_pcm_frames(encoder, pInputOut, frameCountOut, nullptr);
			}
		}
	}

	void start() {
		loopback_buffer = nullptr;
		microphone_buffer = nullptr;
		loopback_frames.store(0);
		microphone_frames.store(0);
		ma_data_converter_config converter_config = ma_data_converter_config_init(ma_format_f32, buffer_format, channels, channels, sample_rate, sample_rate);
		ma_result init_result = ma_data_converter_init(&converter_config, nullptr, &g_Converter);
		if (init_result != MA_SUCCESS) {
			alert(L"FPConverterInitializerError", L"Error initializing data converter.", MB_ICONERROR);
			g_Retcode = -3;
			g_Running = false;
		}
		ma_event_init(&g_RecordThreadEvent);
		std::wstring record_path_u;
		CStringUtils::UnicodeConvert(record_path, record_path_u);
		CreateDirectory(record_path_u.c_str(), nullptr);
		std::string file = !g_CommandLineOptions.useFilename ? record_path + "/" + get_now() : record_path + "/" + g_CommandLineOptions.filename;
		if (g_CommandLineOptions.useFilename) {
			g_CommandLineOptions.useFilename = false;
			g_CommandLineOptions.filename = "";
		}
		filename = file;

		encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, buffer_format, channels, sample_rate);
		ma_result result = MA_SUCCESS;
		if (make_stems) {
			std::string stem;
			stem = file + " Mic.wav";
			result = ma_encoder_init_file(stem.c_str(), &encoderConfig, &encoder[0]);
			stem = file + " Loopback.wav";
			if (result == MA_SUCCESS)
				result = ma_encoder_init_file(stem.c_str(), &encoderConfig, &encoder[1]);
		}
		else {
			result = ma_encoder_init_file(std::string(file + ".wav").c_str(), &encoderConfig, &encoder[0]);
		}
		if (result != MA_SUCCESS) {
			std::wstring file_u;
			CStringUtils::UnicodeConvert(file, file_u);
			if (sound_events)		g_SoundStream.PlayFromMemory(Error_wav, false);
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
			deviceConfig.dataCallback = CAudioRecorder::MicrophoneCallback;
			deviceConfig.pUserData = &encoder[0];
			result = ma_device_init(g_AudioContext, &deviceConfig, &recording_device);
			if (result != MA_SUCCESS) {
				if (sound_events)		g_SoundStream.PlayFromMemory(Error_wav, false);
				alert(L"FPAudioDeviceInitializerError", L"Error initializing audio device for \"" + g_CurrentInputDevice.name + L"\" with retcode " + std::to_wstring(result) + L".", MB_ICONERROR);
				g_Retcode = result;
				g_Running = false;
			}
			ma_device_start(&recording_device);
			g_Process |= PROCESS_MICROPHONE;
		}
		if (g_CurrentOutputDevice.name != L"Not used") {
			loopbackDeviceConfig = ma_device_config_init(ma_device_type_loopback);
			loopbackDeviceConfig.capture.pDeviceID = &g_CurrentOutputDevice.id;;
			loopbackDeviceConfig.capture.format = ma_format_f32;
			loopbackDeviceConfig.capture.channels = channels;
			loopbackDeviceConfig.sampleRate = sample_rate;
			loopbackDeviceConfig.periodSizeInMilliseconds = buffer_size;
			loopbackDeviceConfig.dataCallback = CAudioRecorder::LoopbackCallback;
			loopbackDeviceConfig.pUserData = make_stems ? &encoder[1] : &encoder[0];

			result = ma_device_init(g_AudioContext, &loopbackDeviceConfig, &loopback_device);
			if (result != MA_SUCCESS) {
				if (sound_events)		g_SoundStream.PlayFromMemory(Error_wav, false);
				alert(L"FPAudioDeviceInitializerError", L"Error initializing audio device for \"" + g_CurrentOutputDevice.name + L"\" with retcode " + std::to_wstring(result) + L".", MB_ICONERROR);
				g_Retcode = result;
				g_Running = false;
			}
			ma_device_start(&loopback_device);
			g_Process |= PROCESS_LOOPBACK;
		}
		this->resume();
	}
	void stop() {
		if (g_Process & PROCESS_MICROPHONE) {
			ma_event_signal(&g_RecordThreadEvent);
			ma_device_uninit(&recording_device);
			g_Process &= ~PROCESS_MICROPHONE;
		}
		if (g_Process & PROCESS_LOOPBACK) {
			thread_shutdown = true;
			g_Process &= ~PROCESS_LOOPBACK;
			ma_device_uninit(&loopback_device);
		}
		if (make_stems)
			ma_encoder_uninit(&encoder[1]);
		ma_encoder_uninit(&encoder[0]);
		ma_event_uninit(&g_RecordThreadEvent);
		g_NullSamplesDestroyed = MA_FALSE;
		ma_data_converter_uninit(&g_Converter, nullptr);
		g_Running = !g_CommandLineOptions.exitAfterStop;
	}
	void pause() {
		thread_shutdown.store(true);
		paused.store(true);
		g_NullSamplesDestroyed.store(MA_FALSE);
	}
	void resume() {
		thread_shutdown.store(false);
		if (!make_stems && g_Process & PROCESS_MICROPHONE && g_Process & PROCESS_LOOPBACK) {
			std::thread t(CAudioRecorder::MixingThread, &encoder[0]);
			t.detach();
		}
		paused.store(false);
	}
};



std::vector<audio_device> MA_API get_input_audio_devices()
{
	ma_result result;
	std::vector<audio_device> audioDevices;
	ma_device_info* pCaptureDeviceInfos;
	ma_uint32 captureDeviceCount;
	ma_uint32 iCaptureDevice;

	result = ma_context_get_devices(g_AudioContext, nullptr, nullptr, &pCaptureDeviceInfos, &captureDeviceCount);
	if (result != MA_SUCCESS) {
		return audioDevices;
	}
	for (iCaptureDevice = 0; iCaptureDevice < captureDeviceCount; ++iCaptureDevice) {
		const char* name = pCaptureDeviceInfos[iCaptureDevice].name;
		std::string name_str(name);
		std::wstring name_str_u;
		CStringUtils::UnicodeConvert(name_str, name_str_u);
		audio_device ad;
		ad.id = pCaptureDeviceInfos[iCaptureDevice].id;
		ad.name = name_str_u;
		audioDevices.push_back(ad);
	}
	return audioDevices;
}

std::vector<audio_device> MA_API get_output_audio_devices()
{
	ma_result result;
	std::vector<audio_device> audioDevices;
	ma_device_info* pPlaybackDeviceInfos;
	ma_uint32 playbackDeviceCount;
	ma_uint32 iPlaybackDevice;

	result = ma_context_get_devices(g_AudioContext, &pPlaybackDeviceInfos, &playbackDeviceCount, nullptr, nullptr);
	if (result != MA_SUCCESS) {
		return audioDevices;
	}
	for (iPlaybackDevice = 0; iPlaybackDevice < playbackDeviceCount; ++iPlaybackDevice) {
		const char* name = pPlaybackDeviceInfos[iPlaybackDevice].name;
		std::string name_str(name);
		std::wstring name_str_u;
		CStringUtils::UnicodeConvert(name_str, name_str_u);
		audio_device ad;
		ad.id = pPlaybackDeviceInfos[iPlaybackDevice].id;
		ad.name = name_str_u;
		audioDevices.push_back(ad);
	}

	return audioDevices;
}



class IWindow;

static std::vector<IWindow*> g_Windows;
static HWND window = nullptr;


class IWindow {
	size_t id;
	HWND m_Parent = nullptr;
public:
	std::vector<HWND> items;
	void reset() {
		for (unsigned int i = 0; i < items.size(); ++i) {
			delete_control(items[i]);
		}
		items.clear();
	}

	size_t push(HWND window) {
		items.push_back(window);
		return items.size() - 1;
	}

	void remove(size_t index) {
		if (index > items.size() - 1 || index < 0)return;
		delete_control(items[index]);
		items.erase(items.begin() + index);
	}

	IWindow(HWND parent = window) {
		this->reset();
		this->m_Parent = parent;
		g_Windows.push_back(this);
		this->id = g_Windows.size() - 1;
	}
	~IWindow() {
		this->reset();
	}

	operator HWND() {
		return m_Parent;
	}
	virtual void build() {}

	void auto_size()
	{
		// Get the client rectangle of the window
		RECT clientRect;
		GetClientRect(window, &clientRect);

		// Iterate through all child windows (controls)
		HWND childWnd = GetWindow(window, GW_CHILD);
		while (childWnd != NULL)
		{
			// Get the control's rectangle
			RECT controlRect;
			GetWindowRect(childWnd, &controlRect);

			// Calculate the new position within the client area
			int newX = controlRect.left - clientRect.left;
			int newY = controlRect.top - clientRect.top;

			// Set the control's position within the window
			SetWindowPos(childWnd, HWND_TOP,
				newX, newY,
				controlRect.right - controlRect.left,
				controlRect.bottom - controlRect.top,
				SWP_NOSIZE | SWP_NOZORDER);

			// Move to the next child window
			childWnd = GetWindow(childWnd, GW_HWNDNEXT);
		}
	}



};

static void window_reset() {
	for (IWindow* w : g_Windows) {
		SAFE_CALL(w, reset());
	}
	g_Windows.clear();
}

static const std::wstring version = L"0.0.1 Alpha";
static CAudioRecorder rec;


static std::vector<audio_device> in_audio_devices;
static std::vector < audio_device> out_audio_devices;




class CMainWindow : public IWindow {
public:
	HWND record_start = nullptr;
	HWND input_devices_text = nullptr;
	HWND input_devices_list = nullptr;
	HWND output_devices_text = nullptr;
	HWND output_devices_list = nullptr;
	HWND record_manager = nullptr;


	void build()override {
		g_CurrentInputDevice.name = L"Default";
		g_CurrentOutputDevice.name = L"Not used";
		record_start = create_button(window, L"&Start recording", 10, 10, 200, 50, 0);
		push(record_start);

		input_devices_text = create_text(window, L"&Input devices", 10, 70, 200, 20, 0);

		push(input_devices_text);

		input_devices_list = create_list(window, 10, 90, 200, 150, 0);
		push(input_devices_list);
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
		push(output_devices_text);

		output_devices_list = create_list(window, 10, 270, 200, 150, 0);
		push(output_devices_list);

		out_audio_devices = get_output_audio_devices();
		out.name = L"Not used";
		out_audio_devices.push_back(out);

		for (unsigned int i = 0; i < out_audio_devices.size(); i++) {
			add_list_item(output_devices_list, out_audio_devices[i].name.c_str());
		}

		focus(output_devices_list);
		set_list_position(output_devices_list, loopback_device);
		record_manager = create_button(window, L"&Recordings manager", 10, 450, 200, 50, 0);
		push(record_manager);
		auto_size();

		focus(record_start);
	}

};




class CRecordManagerWindow : public IWindow {
public:
	HWND items_view_text = nullptr;
	HWND items_view_list = nullptr;
	HWND play_button = nullptr;
	HWND pause_button = nullptr;
	HWND stop_button = nullptr;
	HWND delete_button = nullptr;
	HWND close_button = nullptr;

	void build()override {
		g_SpeechProvider.Speak(record_path.c_str(), false);
		wait(50);
		items_view_text = create_text(window, L"Items view", 10, 10, 0, 10, 0);
		push(items_view_text);
		items_view_list = create_list(window, 10, 10, 0, 10, 0);
		push(items_view_list);
		play_button = create_button(window, L"&Play", 10, 10, 10, 10, 0);
		push(play_button);
		pause_button = create_button(window, L"&Pause", 10, 10, 10, 10, 0);
		push(pause_button);
		stop_button = create_button(window, L"&Stop", 10, 10, 10, 10, 0);
		push(stop_button);
		delete_button = create_button(window, L"&Delete", 10, 10, 10, 10, 0);
		push(delete_button);
		close_button = create_button(window, L"&Close", 10, 10, 10, 10, 0);
		push(close_button);
		std::vector<std::wstring> files = get_files(std::wstring(record_path.begin(), record_path.end()));
		for (unsigned int i = 0; i < files.size(); i++) {
			add_list_item(items_view_list, files[i].c_str());
		}
		auto_size();

		focus(items_view_list);

	}



};


class CRecordingWindow : public IWindow {
public:
	HWND record_stop = nullptr;
	HWND record_pause = nullptr;
	HWND record_restart = nullptr;

	void build()override {
		record_stop = create_button(window, L"&Stop recording", 10, 10, 100, 30, 0);
		push(record_stop);
		record_pause = create_button(window, L"&Pause recording", 120, 10, 100, 30, 0);
		push(record_pause);
		record_restart = create_button(window, L"&Restart recording", 230, 10, 100, 30, 0);
		push(record_restart);
		auto_size();

		focus(record_stop);

	}







};





class CSettingsWindow : public IWindow {
public:
	HWND sample_rate = nullptr;
	HWND record_path = nullptr;
	HWND channels = nullptr;
	HWND buffer_size = nullptr;
	HWND filename_signature = nullptr;
	HWND audio_format = nullptr;
	HWND sound_events = nullptr;
	HWND sample_format = nullptr;
	HWND hotkey_start_stop = nullptr;
	HWND hotkey_pause_resume = nullptr;
	HWND hotkey_restart = nullptr;
	HWND current_preset = nullptr;
	void build() override {}

};



static CMainWindow g_MainWindow;
static CRecordManagerWindow g_RecordManagerWindow;
static CRecordingWindow g_RecordingWindow;
static CSettingsWindow g_SettingsWindow;



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
	wchar_t* filename = new wchar_t[500];
	GetModuleFileNameW(nullptr, filename, 500);
	std::wstring result(filename);
	delete[] filename;
	return result;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, ma_int32       nShowCmd) {
	std::vector<std::string> options, values;
	if (ParseCommandLineOptions(lpCmdLine, options, values)) {
		for (size_t i = 0; i < options.size(); ++i) {
			if (options[i] == "-start") {
				g_CommandLineOptions.start = true;
			}
			else if (options[i] == "f") {
				g_CommandLineOptions.filename = values[i - 1];
				g_CommandLineOptions.useFilename = g_CommandLineOptions.start;
			}
			else if (options[i] == "-exit") {
				g_CommandLineOptions.exitAfterStop = true;
			}
		}
	}

	g_Running = true; // Starting application
	SetUnhandledExceptionFilter(ExceptionHandler);
	timeBeginPeriod(1);
	DWORD kmod = -1;
	int kcode = -1;
	int result = conf.load();
	if (result == 0) {
		try {
			std::string srate = conf.read("General", "sample-rate");
			sample_rate = std::stoi(srate);
			std::string chann = conf.read("General", "channels");
			channels = std::stoi(chann);
			std::string bs = conf.read("General", "buffer-size");
			buffer_size = std::stoi(bs);
			filename_signature = conf.read("General", "filename-signature");
			record_path = conf.read("General", "record-path");
			audio_format = CStringUtils::ToLowerCase(conf.read("General", "audio-format"));
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
			std::string mstems = conf.read("General", "make-stems");
			make_stems = std::stoi(mstems);
			if (make_stems && audio_format != "wav") {
				throw std::exception("Can't make stems when using another format, built-in not supported by FPRecorder");
			}
			std::string sformat = CStringUtils::ToLowerCase(conf.read("General", "sample-format"));
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
			CStringUtils::UnicodeConvert(what, what_u);
			std::wstring last_value_u;
			std::wstring last_name_u;
			CStringUtils::UnicodeConvert(conf.last_name, last_name_u);
			CStringUtils::UnicodeConvert(conf.last_value, last_value_u);
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
			alert(L"FPWelcomeDialogInitializerError", L"File: " + get_exe() + L"\\.rsrc\\DIALOG\\" + std::to_wstring(IDD_DIALOG1) + L" not found. Try to redownload the application", MB_ICONERROR);
			g_Retcode = -4;
			g_Running = false;
		}
		ShowWindow(window, SW_SHOW);
		std::wstring README_u;
		CStringUtils::UnicodeConvert(README, README_u);
		SetDlgItemTextW(window, IDC_EDIT1, README_u.c_str());
		while (g_Running && IsWindow(window)) {
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
		conf.write("General", "make-stems", std::to_string(make_stems));

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
	if (!g_Running)return g_Retcode; // When an error was triggered
	try {
		window = show_window(L"FPRecorder " + version + (IsUserAnAdmin() ? L" (Administrator)" : L"")); assert(window >= 0);
		g_MainWindow.build();
		presets.push_back(g_DefaultPreset);
		key_pressed(VK_SPACE) || key_pressed(VK_RETURN); // Avoid click to start recording
		while (g_Running) {
			wait(5);
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
					if (sound_events)		g_SoundStream.PlayFromMemory(Error_wav, false);
					g_SpeechProvider.Speak("Unable to exit, while recording.");
					continue;
				}
				g_Retcode = 0;
				g_Running = false;
				break;
			}
			if (is_pressed(g_MainWindow.record_manager) and !g_RecordingsManager) {
				loopback_device = get_list_position(g_MainWindow.output_devices_list);
				input_device = get_list_position(g_MainWindow.input_devices_list);
				conf.write("General", "input-device", std::to_string(input_device));
				conf.write("General", "loopback-device", std::to_string(loopback_device));
				conf.save();
				g_MainWindow.reset();
				g_RecordManagerWindow.build();
				if (sound_events)g_SoundStream.PlayFromMemory(Openmanager_wav);
				g_RecordingsManager = true;
			}
			if (g_RecordingsManager) {
				if (key_down(VK_ESCAPE) or is_pressed(g_RecordManagerWindow.close_button)) {
					g_SoundStream.Close();
					g_SpeechProvider.Speak("Closed.");
					g_RecordManagerWindow.reset();
					g_MainWindow.build();
					focus(g_MainWindow.record_manager);
					g_RecordingsManager = false;
				}
				std::wstring record_path_u;
				CStringUtils::UnicodeConvert(record_path, record_path_u);
				std::vector<wstring> files = get_files(record_path_u);
				if (files.size() == 0) {
					if (sound_events)		g_SoundStream.PlayFromMemory(Error_wav, false);
					g_SpeechProvider.Speak("There are no files in \"" + record_path + "\".");
					g_RecordManagerWindow.reset();
					g_MainWindow.build();
					focus(g_MainWindow.record_manager);
					g_RecordingsManager = false;
				}
				if ((key_pressed(VK_SPACE) && get_current_focus() == g_RecordManagerWindow.items_view_list) || is_pressed(g_RecordManagerWindow.play_button)) {
					std::wstring record_path_u;
					CStringUtils::UnicodeConvert(record_path, record_path_u);
					g_SoundStream.Play(record_path_u + L"/" + get_focused_list_item_name(g_RecordManagerWindow.items_view_list));
					if (get_current_focus() == g_RecordManagerWindow.play_button)focus(g_RecordManagerWindow.pause_button);
				}
				if (is_pressed(g_RecordManagerWindow.pause_button)) {
					g_SoundStream.Pause();
					if (get_current_focus() == g_RecordManagerWindow.pause_button)focus(g_RecordManagerWindow.play_button);

				}
				if (is_pressed(g_RecordManagerWindow.stop_button)) {
					g_SoundStream.Stop();
				}
				if (key_pressed(VK_DELETE) or is_pressed(g_RecordManagerWindow.delete_button)) {
					if (get_focused_list_item_name(g_RecordManagerWindow.items_view_list) == L"")continue;
					wait(10);
					int result = alert(L"FPWarning", L"Are you sure you want to delete the recording \"" + get_focused_list_item_name(g_RecordManagerWindow.items_view_list) + L"\"? It can no longer be restored.", MB_YESNO | MB_ICONEXCLAMATION);
					if (result == IDNO)continue;
					else if (result == IDYES)
					{
						std::wstring record_path_u;
						CStringUtils::UnicodeConvert(record_path, record_path_u);
						std::wstring file = record_path_u + L"/" + get_focused_list_item_name(g_RecordManagerWindow.items_view_list);
						g_SoundStream.Close();
						DeleteFile(file.c_str());
						g_RecordManagerWindow.reset();
						g_RecordManagerWindow.build();
					}
				}
			}
			if (!g_Recording && (is_pressed(g_MainWindow.record_start) || g_CommandLineOptions.start || hotkey_pressed(HOTKEY_STARTSTOP))) {
				g_CommandLineOptions.start = false;
				loopback_device = get_list_position(g_MainWindow.output_devices_list);
				input_device = get_list_position(g_MainWindow.input_devices_list);
				conf.write("General", "input-device", std::to_string(input_device));
				conf.write("General", "loopback-device", std::to_string(loopback_device));
				conf.save();
				if (sound_events)g_SoundStream.PlayFromMemory(Start_wav);
				g_CurrentInputDevice = in_audio_devices[input_device];
				g_CurrentOutputDevice = out_audio_devices[loopback_device];
				if (g_CurrentInputDevice.name == L"Not used" && g_CurrentOutputDevice.name == L"Not used") {
					if (sound_events)g_SoundStream.PlayFromMemory(Error_wav);
					g_SpeechProvider.Speak("Can't record silence", true);
					if (sound_events)g_SoundStream.PlayFromMemory(Stop_wav);

					continue;
				}
				g_MainWindow.reset();
				g_RecordingWindow.build();
				rec.start();
				g_Recording = true;
				g_RecordingPaused = false;
			}
			if (g_Recording && (is_pressed(g_RecordingWindow.record_stop) || hotkey_pressed(HOTKEY_STARTSTOP))) {
				rec.stop();
				if (sound_events)g_SoundStream.PlayFromMemory(Stop_wav);
				g_Recording = false;
				g_RecordingWindow.reset();
				if (audio_format == CStringUtils::ToLowerCase("jkm")) {
					alert(L"FPInteractiveAudioConverter", L"JKM - Jigsaw Kompression Media V 99.54.2. This audio format will be in 2015, October 95 at 3 hours -19 minutes.", MB_ICONHAND);
					g_Retcode = -256;
					g_Running = false;
				}

				else if (audio_format != CStringUtils::ToLowerCase("wav")) {
					string output;
					std::vector<std::string> split = CStringUtils::Split(".wav", rec.filename);
					g_SpeechProvider.Speak("Converting...");
					std::string cmd = g_CurrentPreset.command;
					CStringUtils::Replace(cmd, "%I", "\"" + rec.filename + "\"", true);
					CStringUtils::Replace(cmd, "%i", "\"" + split[0] + "\"", true);
					CStringUtils::Replace(cmd, "%f", audio_format, true);
					int result = ExecSystemCmd(cmd, output);
					if (result != 0) {
						if (sound_events)g_SoundStream.PlayFromMemory(Error_wav, false);
						wstring output_u;
						CStringUtils::UnicodeConvert(output, output_u);
						alert(L"FPError", L"Process exit failure!\nRetcode: " + std::to_wstring(result) + L"\nOutput: \"" + output_u + L"\".", MB_ICONERROR);
					}
					else {
						std::wstring recording_name_u;
						CStringUtils::UnicodeConvert(rec.filename, recording_name_u);
						DeleteFile(recording_name_u.c_str());
					}
				}
				g_MainWindow.build();

			}
			if (g_Recording && (is_pressed(g_RecordingWindow.record_pause) || hotkey_pressed(HOTKEY_PAUSERESUME))) {
				if (!g_RecordingPaused) {
					rec.pause();
					if (sound_events)g_SoundStream.PlayFromMemory(Pause_wav);
					g_RecordingPaused = true;
					set_text(g_RecordingWindow.record_pause, L"&Resume recording");
				}
				else if (g_RecordingPaused) {
					if (sound_events)g_SoundStream.PlayFromMemory(Unpause_wav);
					rec.resume();
					g_RecordingPaused = false;
					set_text(g_RecordingWindow.record_pause, L"&Pause recording");
				}
				ma_sleep(100);
			}
			if (g_Recording && (is_pressed(g_RecordingWindow.record_restart) || hotkey_pressed(HOTKEY_RESTART))) {
				wait(10);
				std::wstring recording_name_u;
				CStringUtils::UnicodeConvert(rec.filename, recording_name_u);
				int result = alert(L"FPWarning", L"Are you sure you want to delete the recording \"" + recording_name_u + L"\" and rerecord it to new one? Old record can no longer be restored.", MB_YESNO | MB_ICONEXCLAMATION);
				if (result == IDNO)continue;
				else if (result == IDYES)
				{
					rec.stop();
				}
				rec.start();
				g_Recording = true;
				g_RecordingPaused = false;
				set_text(g_RecordingWindow.record_pause, L"&Pause recording");
				DeleteFile(recording_name_u.c_str());
				if (sound_events)g_SoundStream.PlayFromMemory(Restart_wav);
			}
		}
	}
	catch (const std::exception& ex) {
		std::wstring exception_u;
		CStringUtils::UnicodeConvert(ex.what(), exception_u);
		if (sound_events)g_SoundStream.PlayFromMemory(Error_wav, false);
		alert(L"FPRuntimeError", exception_u.c_str(), MB_ICONERROR);
		g_Running = false;
		g_Retcode = -100;
	}
	g_Running = false;
	window_reset();
	UnregisterHotKey(nullptr, HOTKEY_STARTSTOP);
	UnregisterHotKey(nullptr, HOTKEY_PAUSERESUME);
	UnregisterHotKey(nullptr, HOTKEY_RESTART);
	hide_window(window);
	timeEndPeriod(1);
	(void)hInstance;
	(void)hPrevInstance;
	return g_Retcode; // Application exit
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
	CStringUtils::UnicodeConvert(ss.str(), str_u);
	if (sound_events)g_SoundStream.PlayFromMemory(Error_wav, false);
	alert(L"FPRuntimeError", str_u, MB_ICONERROR);
	g_Retcode = -100;
	g_Running = false; // In any situation, user should nott lost the record. Give application call audio recorder destructor to try uninitialize the encoder
	return 0;
}



