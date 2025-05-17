#define _CRT_SECURE_NO_WARNINGS
#include "gui/gui.h"
#include "Provider.h"
#include "stdafx.h"
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
#include <array>
#include <functional>
#include <optional>

template<typename T, typename Func, typename ...Args>
inline void safeCall(T* obj, Func func, Args... args) {
	if (obj) {
		(obj->*func)(args...);
	}
	else {
	}
}

template<typename T, typename Func, typename ...Args>
inline void safeCall(T* obj, Func func, Args... args, std::function<void()> onNull) {
	if (obj) {
		(obj->*func)(args...);
	}
	else {
		onNull();
	}
}

template<typename T, typename R, typename Func, typename ...Args>
inline std::optional<R> safeCallVal(T* obj, Func func, Args ...args) {
	if (obj) {
		return (obj->*func)(args...);
	}
	else {
		return std::nullopt;
	}
}


using namespace gui;
enum EHotKey {
	HOTKEY_STARTSTOP = 1,
	HOTKEY_PAUSERESUME = 2,
	HOTKEY_RESTART = 3
};


static int g_Retcode = 0;
static bool g_Running = false;

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

static ma_uint32 sample_rate = 44100;
static ma_uint32 channels = 2;
static ma_uint32 buffer_size = 0;
static std::string filename_signature = "%Y %m %d %H %M %S";
static std::string record_path = "recordings";
static std::string audio_format = "wav";
static int input_device = 0;
static int loopback_device = 0;
static ma_bool32 sound_events = MA_TRUE;
static ma_bool32 make_stems = MA_FALSE;
static ma_format buffer_format = ma_format_s16;
static const ma_uint32 periods = 256;
static std::string hotkey_start_stop = "Windows+Shift+F1";
static std::string hotkey_pause_resume = "Windows+Shift+F2";
static std::string hotkey_restart = "Windows+Shift+F3";
static const preset g_DefaultPreset = { "Default", "ffmpeg.exe -i %I %i.%f" };
static std::string current_preset_name = "Default";
static user_config conf("fp.ini");

static inline std::string _cdecl get_now(bool filename = true) {
	auto now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);

	std::tm tm = *std::localtime(&now_c);
	std::stringstream oss;
	oss << std::put_time(&tm, filename ? filename_signature.c_str() : "%Y-%m-%d %H:%M:%S");
	return oss.str();
}

bool g_EnableLog = false;

class CLogger {
	std::ofstream ofs;
public:
	CLogger(const std::string& filename) {
		if (g_EnableLog)
			ofs.open(filename, std::ios::out | std::ios::app);
	}

	template <typename T>
	CLogger& operator<<(const T& info) {
		if (ofs.is_open()) {
			ofs << get_now() << " - " << info;
		}
		return *this;
	}

	template <typename T>
	CLogger& operator<<(T& info) {
		if (ofs.is_open()) {
			ofs << get_now() << " - " << info;
		}
		return *this;
	}

	~CLogger() {
		if (ofs.is_open()) {
			ofs.close();
		}
	}
};




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
		safeCall(pProvider, &Provider::Release);
		safeCall(pCondition, &IUIAutomationCondition::Release);
		safeCall(pAutomation, &IUIAutomation::Release);
	}

	bool Speak(const wchar_t* text, bool interrupt = true) {
		NotificationProcessing flags = NotificationProcessing_ImportantAll;
		if (interrupt)
			flags = NotificationProcessing_ImportantMostRecent;
		pProvider = new Provider(GetForegroundWindow());

		HRESULT hr = 0;
		auto result = safeCallVal<IUIAutomation, HRESULT>(pAutomation, &IUIAutomation::ElementFromHandle, GetForegroundWindow(), &pElement);
		if (!result.has_value()) {
			return false;
		}
		hr = *result;
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
	for (; g_Running;)
	{
		// Wait for a while to allow the process to work
		wait(5);
		void;
		DWORD ret = WaitForSingleObject(pi.hProcess, 50);
		if (gui::try_close) {
			int result = alert(L"FPWarning", L"You are attempting to close the program while it is converting a recording to another format. If the recording is lengthy, please wait a bit longer. If you believe the program has frozen and will not complete the conversion, click \"Yes\".", MB_YESNO | MB_ICONEXCLAMATION);
			if (result == IDYES) {
				g_Retcode = 0;
				g_Running = false;
			}
		}
		// Read from the stdout if there is any data
		for (; g_Running;)
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




static std::string ma_result_to_string(ma_result result) {
	switch (result) {
	case MA_SUCCESS:                        return "Success.";
	case MA_ERROR:                          return "A generic error occurred.";
	case MA_INVALID_ARGS:                   return "Invalid arguments were provided.";
	case MA_INVALID_OPERATION:              return "The operation is not valid.";

	case MA_OUT_OF_MEMORY:                  return "Out of memory.";
	case MA_OUT_OF_RANGE:                   return "Value is out of range.";
	case MA_ACCESS_DENIED:                  return "Access denied.";
	case MA_DOES_NOT_EXIST:                 return "The specified item does not exist.";
	case MA_ALREADY_EXISTS:                 return "The item already exists.";
	case MA_TOO_MANY_OPEN_FILES:            return "Too many open files.";
	case MA_INVALID_FILE:                   return "Invalid file format.";
	case MA_TOO_BIG:                        return "The item is too big.";
	case MA_PATH_TOO_LONG:                  return "The path is too long.";
	case MA_NAME_TOO_LONG:                  return "The name is too long.";
	case MA_NOT_DIRECTORY:                  return "Not a directory.";
	case MA_IS_DIRECTORY:                   return "Is a directory.";
	case MA_DIRECTORY_NOT_EMPTY:            return "Directory is not empty.";
	case MA_AT_END:                         return "Reached the end of the file.";
	case MA_NO_SPACE:                       return "No space left on device.";
	case MA_BUSY:                           return "Resource is busy.";
	case MA_IO_ERROR:                       return "An I/O error occurred.";
	case MA_INTERRUPT:                      return "Operation was interrupted.";
	case MA_UNAVAILABLE:                    return "Resource is unavailable.";
	case MA_ALREADY_IN_USE:                 return "Resource is already in use.";
	case MA_BAD_ADDRESS:                    return "Bad address.";
	case MA_BAD_SEEK:                       return "Bad seek operation.";
	case MA_BAD_PIPE:                       return "Bad pipe.";
	case MA_DEADLOCK:                       return "Deadlock detected.";
	case MA_TOO_MANY_LINKS:                 return "Too many links.";
	case MA_NOT_IMPLEMENTED:                return "Operation not implemented.";
	case MA_NO_MESSAGE:                     return "No message available.";
	case MA_BAD_MESSAGE:                    return "Bad message received.";
	case MA_NO_DATA_AVAILABLE:              return "No data available.";
	case MA_INVALID_DATA:                   return "Invalid data received.";
	case MA_TIMEOUT:                        return "Operation timed out.";
	case MA_NO_NETWORK:                     return "No network available.";
	case MA_NOT_UNIQUE:                     return "Not unique resource.";
	case MA_NOT_SOCKET:                     return "Not a socket.";
	case MA_NO_ADDRESS:                     return "No address found.";
	case MA_BAD_PROTOCOL:                   return "Bad protocol specified.";
	case MA_PROTOCOL_UNAVAILABLE:           return "Protocol is unavailable.";
	case MA_PROTOCOL_NOT_SUPPORTED:         return "Protocol not supported.";
	case MA_PROTOCOL_FAMILY_NOT_SUPPORTED:  return "Protocol family not supported.";
	case MA_ADDRESS_FAMILY_NOT_SUPPORTED:   return "Address family not supported.";
	case MA_SOCKET_NOT_SUPPORTED:           return "Socket type not supported.";
	case MA_CONNECTION_RESET:               return "Connection was reset.";
	case MA_ALREADY_CONNECTED:              return "Already connected.";
	case MA_NOT_CONNECTED:                  return "Not connected.";
	case MA_CONNECTION_REFUSED:             return "Connection refused.";
	case MA_NO_HOST:                        return "No host found.";
	case MA_IN_PROGRESS:                    return "Operation in progress.";
	case MA_CANCELLED:                      return "Operation was cancelled.";
	case MA_MEMORY_ALREADY_MAPPED:          return "Memory is already mapped.";

		/* General non-standard errors. */
	case MA_CRC_MISMATCH:                   return "CRC mismatch error.";

		/* General miniaudio-specific errors. */

	case MA_FORMAT_NOT_SUPPORTED:           return "Audio format not supported.";
	case MA_DEVICE_TYPE_NOT_SUPPORTED:      return "Device type not supported.";
	case MA_SHARE_MODE_NOT_SUPPORTED:       return "Share mode not supported for this device.";
	case MA_NO_BACKEND:                     return "No backend available for audio playback.";
	case MA_NO_DEVICE:                      return "No audio device available.";
	case MA_API_NOT_FOUND:                  return "API not found for audio backend.";
	case MA_INVALID_DEVICE_CONFIG:          return "Invalid device configuration specified.";
	case MA_LOOP:                           return "Looping detected in operation.";
	case MA_BACKEND_NOT_ENABLED:            return "Audio backend not enabled.";

		/* State errors. */
	case MA_DEVICE_NOT_INITIALIZED:         return "Audio device has not been initialized.";
	case MA_DEVICE_ALREADY_INITIALIZED:     return "Audio device has already been initialized.";
	case MA_DEVICE_NOT_STARTED:             return "Audio device has not been started.";
	case MA_DEVICE_NOT_STOPPED:             return "Audio device has not been stopped.";

		/* Operation errors. */
	case MA_FAILED_TO_INIT_BACKEND:         return "Failed to initialize audio backend.";
	case MA_FAILED_TO_OPEN_BACKEND_DEVICE:  return "Failed to open audio backend device.";
	case MA_FAILED_TO_START_BACKEND_DEVICE: return "Failed to start audio backend device.";
	case MA_FAILED_TO_STOP_BACKEND_DEVICE:  return "Failed to stop audio backend device.";

	default:
		return "Unknown error code.";
	}
	return "";
}


static ma_result g_MaLastError = MA_SUCCESS;

static void CheckIfError(ma_result result) {
	g_MaLastError = MA_SUCCESS;
	if (result == MA_SUCCESS) {
		return;
	}
	g_MaLastError = result;
	std::wstring error_u;
	CStringUtils::UnicodeConvert(ma_result_to_string(result), error_u);
	alert(L"FPRuntimeError", error_u, MB_ICONERROR);
	g_Retcode = result;
	g_Running = false;
}





class CAudioContext {
	std::unique_ptr<ma_context> context;
public:
	CAudioContext() : context(nullptr) {
		context = std::make_unique<ma_context>();
		CheckIfError(ma_context_init(NULL, 0, NULL, &*context));
	}

	~CAudioContext() {
		ma_context_uninit(&*context);
		context.reset();
	}
	operator ma_context* () {
		return &*context;
	}
};


static CAudioContext g_AudioContext;





class CSoundStream {
	std::shared_ptr<ma_engine> m_Engine;
	std::unique_ptr<ma_sound> m_Player;
	std::unique_ptr<ma_waveform> m_Waveform;
	std::wstring current_file;
public:
	enum ESoundEvent {
		SOUND_EVENT_NONE = 0,
		SOUND_EVENT_START_RECORDING,
		SOUND_EVENT_STOP_RECORDING,
		SOUND_EVENT_PAUSE_RECORDING,
		SOUND_EVENT_RESUME_RECORDING,
		SOUND_EVENT_RESTART_RECORDING,
		SOUND_EVENT_RECORD_MANAGER,
		SOUND_EVENT_ERROR = -1,
	};
	CSoundStream() : m_Engine(nullptr), m_Player(nullptr), m_Waveform(nullptr) {}
	~CSoundStream() {
		Close();
		if (m_Engine) {
			ma_engine_uninit(&*m_Engine);
			m_Engine.reset();
		}
	}

	bool Initialize() {
		if (m_Engine) {
			return true;
		}
		m_Engine = std::make_shared<ma_engine>();
		ma_engine_config cfg = ma_engine_config_init();
		cfg.sampleRate = sample_rate;
		cfg.channels = channels;
		CheckIfError(ma_engine_init(&cfg, &*m_Engine));
		return g_MaLastError == MA_SUCCESS;
	}

	bool Play(const std::wstring& filename) {
		if (!Initialize()) {
			return false;
		}
		if (filename == current_file and m_Player) {
			return ma_sound_start(&*m_Player) == MA_SUCCESS;
		}
		Close();
		if (!m_Player) {
			m_Player = std::make_unique<ma_sound>();
			g_MaLastError = ma_sound_init_from_file_w(&*m_Engine, filename.c_str(), MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH, nullptr, nullptr, &*m_Player);
			if (g_MaLastError == MA_SUCCESS) {
				g_MaLastError = ma_sound_start(&*m_Player);
				current_file = filename;
			}
			return g_MaLastError == MA_SUCCESS;
		}
		return false;
	}

	bool Play(const std::string& filename) {
		std::wstring filename_u;
		CStringUtils::UnicodeConvert(filename, filename_u);
		return Play(filename_u);
	}

	bool PlayEvent(const CSoundStream::ESoundEvent& evt) {
		if (!Initialize()) {
			return false;
		}
		Close();
		if (!m_Player) {
			ma_waveform_config cfg = ma_waveform_config_init(ma_format_f32, ma_engine_get_channels(&*m_Engine), ma_engine_get_sample_rate(&*m_Engine), ma_waveform_type_sine, 0.3, 1200);
			m_Waveform = std::make_unique<ma_waveform>();
			g_MaLastError = ma_waveform_init(&cfg, &*m_Waveform);
			if (g_MaLastError == MA_SUCCESS) {
				m_Player = std::make_unique<ma_sound>();
				g_MaLastError = ma_sound_init_from_data_source(&*m_Engine, (ma_data_source*)&*m_Waveform, MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH, nullptr, &*m_Player);
				if (g_MaLastError == MA_SUCCESS) {
					g_MaLastError = ma_sound_start(&*m_Player);
				}
			}
			gui::wait(20);
			ma_sound_stop(&*m_Player);
			gui::wait(20);
			double freq = cfg.frequency;
			switch (evt) {
			case SOUND_EVENT_RECORD_MANAGER:
				freq = freq + 50;
				break;
			case SOUND_EVENT_RESTART_RECORDING:
				freq = freq != 0 ? freq / 2 : freq + 16 * 2;
				break;
			case SOUND_EVENT_RESUME_RECORDING:
			case SOUND_EVENT_PAUSE_RECORDING:
				freq = evt == SOUND_EVENT_RESUME_RECORDING ? freq + 130 : freq - 130;
				break;
			case SOUND_EVENT_START_RECORDING:
			case SOUND_EVENT_STOP_RECORDING:
				freq = evt == SOUND_EVENT_START_RECORDING ? freq + 200 : freq - 200;
				break;
			default:
				freq = freq - 333;
				break;
			}
			ma_waveform_set_frequency(&*m_Waveform, freq);
			ma_sound_start(&*m_Player);
			gui::wait(30);
			Close();
			return g_MaLastError == MA_SUCCESS;
		}
		return false;
	}

	void Close() {
		if (m_Waveform) {
			ma_waveform_uninit(&*m_Waveform);
			m_Waveform.reset();
		}
		if (m_Player) {
			ma_sound_uninit(&*m_Player);
			m_Player.reset();
		}
	}

	void Stop() {
		if (m_Player) {
			ma_sound_seek_to_pcm_frame(&*m_Player, 0);
			ma_sound_stop(&*m_Player);
		}
	}

	void Pause() {
		if (m_Player) {
			ma_sound_stop(&*m_Player);
		}
	}

	bool Play() {
		if (m_Player)
			return ma_sound_start(&*m_Player) == MA_SUCCESS;
		return false;
	}
};

static CSoundStream g_SoundStream;

static inline ma_bool32 try_parse_format(const char* str, ma_format& value)
{

	/*  */ if (strcmp(str, "u8") == 0) {
		value = ma_format_u8;
	}
	else if (strcmp(str, "s16") == 0) {
		value = ma_format_s16;
	}
	else if (strcmp(str, "s24") == 0) {
		value = ma_format_s24;
	}
	else if (strcmp(str, "s32") == 0) {
		value = ma_format_s32;
	}
	else if (strcmp(str, "f32") == 0) {
		value = ma_format_f32;
	}
	else {
		return MA_FALSE;    /* Not a format. */
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



static bool str_to_bool(const std::string& val) {
	std::string str = CStringUtils::ToLowerCase(val);
	if (str == "0" || str == "false") {
		return false;
	}
	return str == "1" || str == "true" ? true : false;
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

struct AudioData {
	float* buffer;
	ma_uint32 frameCount;
};

std::deque<AudioData> microphoneQueue;
std::deque<AudioData> loopbackQueue;
std::mutex microphoneMutex;
std::mutex loopbackMutex;
std::condition_variable microphoneCondition;
std::condition_variable loopbackCondition;

static std::atomic<bool> thread_shutdown = false;
static std::atomic<bool> paused = false;
static ma_data_converter g_Converter;

class MINIAUDIO_IMPLEMENTATION CAudioRecorder {
	std::thread m_MixingThread;
	ma_device_config deviceConfig;
	ma_device_config loopbackDeviceConfig;
	ma_encoder_config encoderConfig;
	std::array<std::unique_ptr<ma_encoder>, 2> encoder;
	std::unique_ptr<ma_device> recording_device;
	std::unique_ptr<ma_device> loopback_device;
public:
	std::string filename;

	CAudioRecorder() : recording_device(nullptr), loopback_device(nullptr) { encoder[0].reset(); encoder[1].reset(); }
	~CAudioRecorder() {
		if (g_Recording)
			this->stop();
	}

	static void MicrophoneCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
		if (paused || !(g_Process & PROCESS_MICROPHONE)) return;

		float* buffer = new float[frameCount * channels];
		memcpy(buffer, pInput, frameCount * channels * sizeof(float));

		AudioData data;
		data.buffer = buffer;
		data.frameCount = frameCount;


		{
			std::lock_guard<std::mutex> lock(microphoneMutex);
			microphoneQueue.push_back(data);
		}

		microphoneCondition.notify_one();

		(void)pOutput;
	}

	static void LoopbackCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
		if (paused || !(g_Process & PROCESS_LOOPBACK)) return;

		float* buffer = new float[frameCount * channels];
		memcpy(buffer, pInput, frameCount * channels * sizeof(float));

		AudioData data;
		data.buffer = buffer;
		data.frameCount = frameCount;

		{
			std::lock_guard<std::mutex> lock(loopbackMutex);
			loopbackQueue.push_back(data);
		}

		loopbackCondition.notify_one();

		(void)pOutput;
	}

	static void MixingThread(CAudioRecorder* recorder) {
		while (!thread_shutdown && g_Running && recorder) {
			AudioData microphoneData;
			AudioData loopbackData;
			bool microphoneDataAvailable = false;
			bool loopbackDataAvailable = false;
			if (g_Process & PROCESS_MICROPHONE) {
				{
					std::unique_lock<std::mutex> lock(microphoneMutex);
					microphoneCondition.wait(lock, [&] { return !microphoneQueue.empty() || thread_shutdown; });
					if (thread_shutdown) break;
					microphoneData = microphoneQueue.front();
					microphoneQueue.pop_front();
					microphoneDataAvailable = true;
				}
			}
			if (g_Process & PROCESS_LOOPBACK) {
				{
					std::unique_lock<std::mutex> lock(loopbackMutex);
					loopbackCondition.wait(lock, [&] { return !loopbackQueue.empty() || thread_shutdown; });
					if (thread_shutdown) {
						if (microphoneDataAvailable) {
							delete[] microphoneData.buffer;
						}
						break;
					}
					loopbackData = loopbackQueue.front();
					loopbackQueue.pop_front();
					loopbackDataAvailable = true;
				}
			}

			if (microphoneDataAvailable && loopbackDataAvailable) {
				if (!make_stems) {
					float* mixedBuffer = mix_f32(microphoneData.buffer, loopbackData.buffer, microphoneData.frameCount, loopbackData.frameCount);

					void* pInputOut = mixedBuffer;
					ma_uint64 frameCountToProcess = microphoneData.frameCount;
					ma_uint64 frameCountOut = microphoneData.frameCount;
					if (buffer_format != ma_format_f32) {
						ma_data_converter_process_pcm_frames__format_only(&g_Converter, mixedBuffer, &frameCountToProcess, pInputOut, &frameCountOut);
					}

					ma_encoder_write_pcm_frames(&*recorder->encoder[0], pInputOut, frameCountOut, nullptr);

					delete[] microphoneData.buffer;
					delete[] loopbackData.buffer;
					delete[] mixedBuffer;
				}
				else {
					void* pInputOut = microphoneData.buffer;
					ma_uint64 frameCountToProcess = microphoneData.frameCount;
					ma_uint64 frameCountOut = microphoneData.frameCount;
					if (buffer_format != ma_format_f32) {
						ma_data_converter_process_pcm_frames__format_only(&g_Converter, microphoneData.buffer, &frameCountToProcess, pInputOut, &frameCountOut);
					}

					ma_encoder_write_pcm_frames(&*recorder->encoder[0], pInputOut, frameCountOut, nullptr);

					delete[] microphoneData.buffer;
					pInputOut = loopbackData.buffer;
					frameCountToProcess = loopbackData.frameCount;
					frameCountOut = loopbackData.frameCount;
					if (buffer_format != ma_format_f32) {
						ma_data_converter_process_pcm_frames__format_only(&g_Converter, loopbackData.buffer, &frameCountToProcess, pInputOut, &frameCountOut);
					}

					ma_encoder_write_pcm_frames(&*recorder->encoder[1], pInputOut, frameCountOut, nullptr);

					delete[] loopbackData.buffer;
				}
			}


			else if (microphoneDataAvailable) {
				void* pInputOut = microphoneData.buffer;
				ma_uint64 frameCountToProcess = microphoneData.frameCount;
				ma_uint64 frameCountOut = microphoneData.frameCount;
				if (buffer_format != ma_format_f32) {
					ma_data_converter_process_pcm_frames__format_only(&g_Converter, microphoneData.buffer, &frameCountToProcess, pInputOut, &frameCountOut);
				}

				ma_encoder_write_pcm_frames(&*recorder->encoder[0], pInputOut, frameCountOut, nullptr);

				delete[] microphoneData.buffer;
			}
			else if (loopbackDataAvailable) {
				void* pInputOut = loopbackData.buffer;
				ma_uint64 frameCountToProcess = loopbackData.frameCount;
				ma_uint64 frameCountOut = loopbackData.frameCount;
				if (buffer_format != ma_format_f32) {
					ma_data_converter_process_pcm_frames__format_only(&g_Converter, loopbackData.buffer, &frameCountToProcess, pInputOut, &frameCountOut);
				}

				ma_encoder_write_pcm_frames(&*recorder->encoder[0], pInputOut, frameCountOut, nullptr);

				delete[] loopbackData.buffer;
			}
		}
	}

	void start() {
		microphoneQueue.clear();
		loopbackQueue.clear();
		ma_data_converter_config converter_config = ma_data_converter_config_init(ma_format_f32, buffer_format, channels, channels, sample_rate, sample_rate);
		CheckIfError(ma_data_converter_init(&converter_config, nullptr, &g_Converter));
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
		if (make_stems) {
			std::string stem;
			stem = file + " Mic.wav";
			encoder[0] = std::make_unique<ma_encoder>();
			CheckIfError(ma_encoder_init_file(stem.c_str(), &encoderConfig, &*encoder[0]));
			stem = file + " Loopback.wav";
			if (g_MaLastError == MA_SUCCESS) {
				encoder[1] = std::make_unique<ma_encoder>();
				CheckIfError(ma_encoder_init_file(stem.c_str(), &encoderConfig, &*encoder[1]));
			}
		}
		else {
			encoder[0] = std::make_unique<ma_encoder>();
			CheckIfError(ma_encoder_init_file(std::string(file + ".wav").c_str(), &encoderConfig, &*encoder[0]));
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
			deviceConfig.pUserData = &*encoder[0];
			recording_device = std::make_unique<ma_device>();
			CheckIfError(ma_device_init(g_AudioContext, &deviceConfig, &*recording_device));
			CheckIfError(ma_device_start(&*recording_device));
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
			loopbackDeviceConfig.pUserData = make_stems ? &*encoder[1] : &*encoder[0];

			loopback_device = std::make_unique<ma_device>();

			CheckIfError(ma_device_init(g_AudioContext, &loopbackDeviceConfig, &*loopback_device));
			CheckIfError(ma_device_start(&*loopback_device));
			g_Process |= PROCESS_LOOPBACK;
		}
		thread_shutdown.store(false);
		m_MixingThread = std::thread(CAudioRecorder::MixingThread, this);
		m_MixingThread.detach();

	}
	void stop() {
		if (g_Process & PROCESS_MICROPHONE) {
			ma_device_uninit(&*recording_device);
			recording_device.reset();
			g_Process &= ~PROCESS_MICROPHONE;
		}
		if (g_Process & PROCESS_LOOPBACK) {
			g_Process &= ~PROCESS_LOOPBACK;
			ma_device_uninit(&*loopback_device);
			loopback_device.reset();
		}
		if (make_stems) {
			ma_encoder_uninit(&*encoder[1]);
			encoder[1].reset();
		}
		ma_encoder_uninit(&*encoder[0]);
		encoder[0].reset();
		ma_data_converter_uninit(&g_Converter, nullptr);
		thread_shutdown.store(true);
		microphoneCondition.notify_one();
		loopbackCondition.notify_one();
		if (m_MixingThread.joinable()) {
			m_MixingThread.join();
		}
		{
			std::lock_guard<std::mutex> lock(microphoneMutex);
			while (!microphoneQueue.empty()) {
				delete[] microphoneQueue.front().buffer;
				microphoneQueue.pop_front();
			}
		}
		{
			std::lock_guard<std::mutex> lock(loopbackMutex);
			while (!loopbackQueue.empty()) {
				delete[] loopbackQueue.front().buffer;
				loopbackQueue.pop_front();
			}
		}
		g_Running = !g_CommandLineOptions.exitAfterStop;
	}
	void pause() {
		paused.store(true);
		{
			std::lock_guard<std::mutex> lock(microphoneMutex);
			while (!microphoneQueue.empty()) {
				delete[] microphoneQueue.front().buffer;
				microphoneQueue.pop_front();
			}
		}
		{
			std::lock_guard<std::mutex> lock(loopbackMutex);
			while (!loopbackQueue.empty()) {
				delete[] loopbackQueue.front().buffer;
				loopbackQueue.pop_front();
			}
		}
	}

	void resume() {
		paused.store(false);
	}
};



std::vector<audio_device> MA_API get_input_audio_devices()
{
	std::vector<audio_device> audioDevices;
	ma_device_info* pCaptureDeviceInfos;
	ma_uint32 captureDeviceCount;
	ma_uint32 iCaptureDevice;

	CheckIfError(ma_context_get_devices(g_AudioContext, nullptr, nullptr, &pCaptureDeviceInfos, &captureDeviceCount));
	for (iCaptureDevice = 0; g_MaLastError == MA_SUCCESS && iCaptureDevice < captureDeviceCount; ++iCaptureDevice) {
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
	std::vector<audio_device> audioDevices;
	ma_device_info* pPlaybackDeviceInfos;
	ma_uint32 playbackDeviceCount;
	ma_uint32 iPlaybackDevice;

	CheckIfError(ma_context_get_devices(g_AudioContext, &pPlaybackDeviceInfos, &playbackDeviceCount, nullptr, nullptr));
	for (iPlaybackDevice = 0; g_MaLastError == MA_SUCCESS && iPlaybackDevice < playbackDeviceCount; ++iPlaybackDevice) {
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
		safeCall(w, &IWindow::reset);
	}
	g_Windows.clear();
}

static const std::wstring version = L"0.0.1 Alpha 2";
static CAudioRecorder rec;


static std::vector<audio_device> in_audio_devices;
static std::vector < audio_device> out_audio_devices;


static bool g_SettingsMode = false;

static std::wstring get_list_item_text_by_index_internal(HWND list_hwnd, int index) {
	if (!list_hwnd || index < 0) return L"";
	int len = (int)SendMessage(list_hwnd, LB_GETTEXTLEN, (WPARAM)index, 0);
	if (len == LB_ERR || len == 0) return L"";

	std::vector<wchar_t> buffer(len + 1);
	SendMessage(list_hwnd, LB_GETTEXT, (WPARAM)index, (LPARAM)buffer.data());
	return std::wstring(buffer.data());
}

static void set_list_selection_by_text_internal(HWND list_hwnd, const std::wstring& text) {
	if (!list_hwnd) return;
	int count = (int)SendMessage(list_hwnd, LB_GETCOUNT, 0, 0);
	for (int i = 0; i < count; ++i) {
		if (get_list_item_text_by_index_internal(list_hwnd, i) == text) {
			SendMessage(list_hwnd, LB_SETCURSEL, (WPARAM)i, 0);
			return;
		}
	}
}


class CMainWindow : public IWindow {
public:
	HWND record_start = nullptr;
	HWND input_devices_text = nullptr;
	HWND input_devices_list = nullptr;
	HWND output_devices_text = nullptr;
	HWND output_devices_list = nullptr;
	HWND record_manager = nullptr;
	HWND settings_button = nullptr;

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
		settings_button = create_button(window, L"&Settings", 10, 450 + 50 + 10, 200, 50, 0); // y = 510
		push(settings_button);

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

		focus(record_stop);

	}







};


class CSettingsWindow : public IWindow {
public:
	HWND lblSampleRate, editSampleRate;
	HWND lblChannels, listChannels;
	HWND lblBufferSize, editBufferSize;
	HWND lblFilenameSignature, editFilenameSignature;
	HWND lblRecordPath, editRecordPath, btnBrowseRecordPath;
	HWND lblAudioFormat, editAudioFormat;
	HWND chkSoundEvents;
	HWND chkMakeStems;
	HWND lblBufferFormat, listBufferFormat;

	HWND lblHotkeyStartStop, editHotkeyStartStop;
	HWND lblHotkeyPauseResume, editHotkeyPauseResume;
	HWND lblHotkeyRestart, editHotkeyRestart;

	HWND lblCurrentPreset, listCurrentPreset;

	HWND btnSaveSettings;
	HWND btnCancelSettings;

	// Temp storage for hotkey parsing
	std::string new_hotkey_start_stop, new_hotkey_pause_resume, new_hotkey_restart;


	void build() override {
		this->reset();

		int x_label = 10, x_control = 160, x_button_browse = x_control + 210;
		int y_pos = 10;
		int ctrl_height = 25, list_height = 75, label_width = 140, control_width = 200;
		int y_spacing = 5, section_spacing = 15;

		push(create_text(window, L"Sample Rate (Hz):", x_label, y_pos, label_width, ctrl_height, 0));
		editSampleRate = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editSampleRate);
		set_text(editSampleRate, std::to_wstring(sample_rate).c_str());
		y_pos += ctrl_height + y_spacing;

		push(create_text(window, L"Channels:", x_label, y_pos, label_width, ctrl_height, 0));
		listChannels = create_list(window, x_control, y_pos, control_width, list_height, 0); push(listChannels);
		add_list_item(listChannels, L"1 (Mono)");
		add_list_item(listChannels, L"2 (Stereo)");
		set_list_position(listChannels, channels == 1 ? 0 : 1);
		y_pos += list_height + y_spacing;

		push(create_text(window, L"Buffer Size (ms, 0=default):", x_label, y_pos, label_width, ctrl_height, 0));
		editBufferSize = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editBufferSize);
		set_text(editBufferSize, std::to_wstring(buffer_size).c_str());
		y_pos += ctrl_height + y_spacing;

		push(create_text(window, L"Filename Signature:", x_label, y_pos, label_width, ctrl_height, 0));
		std::wstring wsFilenameSig; CStringUtils::UnicodeConvert(filename_signature, wsFilenameSig);
		editFilenameSignature = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editFilenameSignature);
		set_text(editFilenameSignature, wsFilenameSig.c_str());
		y_pos += ctrl_height + y_spacing;

		push(create_text(window, L"Record Path:", x_label, y_pos, label_width, ctrl_height, 0));
		std::wstring wsRecordPath; CStringUtils::UnicodeConvert(record_path, wsRecordPath);
		editRecordPath = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editRecordPath);
		set_text(editRecordPath, wsRecordPath.c_str());
		btnBrowseRecordPath = create_button(window, L"...", x_button_browse, y_pos, 30, ctrl_height, 0); push(btnBrowseRecordPath);
		y_pos += ctrl_height + y_spacing;

		push(create_text(window, L"Audio Format:", x_label, y_pos, label_width, ctrl_height, 0));
		std::wstring wsAudioFormat; CStringUtils::UnicodeConvert(audio_format, wsAudioFormat);

		editAudioFormat = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editAudioFormat);
		set_text(editAudioFormat, wsAudioFormat.c_str());
		y_pos += ctrl_height + y_spacing;

		chkSoundEvents = create_checkbox(window, L"Enable Sound Events", x_label, y_pos, control_width + label_width, ctrl_height, 0); push(chkSoundEvents);
		set_checkboxMark(chkSoundEvents, sound_events == MA_TRUE ? true : false);
		y_pos += ctrl_height + y_spacing;

		chkMakeStems = create_checkbox(window, L"Make Stems (Mic/Loopback separate files, WAV only)", x_label, y_pos, control_width + label_width + 50, ctrl_height, 0); push(chkMakeStems);
		set_checkboxMark(chkMakeStems, make_stems == MA_TRUE ? true : false);
		y_pos += ctrl_height + y_spacing;

		push(create_text(window, L"Sample Format:", x_label, y_pos, label_width, ctrl_height, 0));
		listBufferFormat = create_list(window, x_control, y_pos, control_width, list_height + 20, 0); push(listBufferFormat);
		const char* formats[] = { "u8", "s16", "s24", "s32", "f32" };
		for (const char* fmt_str : formats) {
			std::wstring wfmt_str; CStringUtils::UnicodeConvert(std::string(fmt_str), wfmt_str);
			add_list_item(listBufferFormat, wfmt_str.c_str());
		}
		std::wstring wsBufferFormat; CStringUtils::UnicodeConvert(ma_format_to_string(buffer_format), wsBufferFormat);
		set_list_selection_by_text_internal(listBufferFormat, wsBufferFormat);
		y_pos += list_height + 20 + section_spacing;

		// --- Hotkey Settings ---
		push(create_text(window, L"Start/Stop Hotkey:", x_label, y_pos, label_width, ctrl_height, 0));
		std::wstring wsHkStartStop; CStringUtils::UnicodeConvert(hotkey_start_stop, wsHkStartStop);
		editHotkeyStartStop = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editHotkeyStartStop);
		set_text(editHotkeyStartStop, wsHkStartStop.c_str());
		y_pos += ctrl_height + y_spacing;

		push(create_text(window, L"Pause/Resume Hotkey:", x_label, y_pos, label_width, ctrl_height, 0));
		std::wstring wsHkPauseResume; CStringUtils::UnicodeConvert(hotkey_pause_resume, wsHkPauseResume);
		editHotkeyPauseResume = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editHotkeyPauseResume);
		set_text(editHotkeyPauseResume, wsHkPauseResume.c_str());
		y_pos += ctrl_height + y_spacing;

		push(create_text(window, L"Restart Hotkey:", x_label, y_pos, label_width, ctrl_height, 0));
		std::wstring wsHkRestart; CStringUtils::UnicodeConvert(hotkey_restart, wsHkRestart);
		editHotkeyRestart = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editHotkeyRestart);
		set_text(editHotkeyRestart, wsHkRestart.c_str());
		y_pos += ctrl_height + section_spacing;

		// --- Preset Settings ---
		push(create_text(window, L"Current FFmpeg Preset:", x_label, y_pos, label_width, ctrl_height, 0));
		listCurrentPreset = create_list(window, x_control, y_pos, control_width, list_height, 0); push(listCurrentPreset);
		for (const auto& p : presets) {
			std::wstring wp_name; CStringUtils::UnicodeConvert(p.name, wp_name);
			add_list_item(listCurrentPreset, wp_name.c_str());
		}
		std::wstring wsCurrentPresetName; CStringUtils::UnicodeConvert(current_preset_name, wsCurrentPresetName);
		set_list_selection_by_text_internal(listCurrentPreset, wsCurrentPresetName);
		y_pos += list_height + section_spacing;

		// --- Action Buttons ---
		btnSaveSettings = create_button(window, L"&Save and Apply", x_label, y_pos, 150, 30, 0); push(btnSaveSettings);
		btnCancelSettings = create_button(window, L"&Cancel", x_control + 60, y_pos, 100, 30, 0); push(btnCancelSettings);
		y_pos += 30 + y_spacing;



		focus(editSampleRate);
	}

	bool validate_and_prepare_settings() {
		std::wstring ws_val = get_text(editSampleRate); std::string s_val; CStringUtils::UnicodeConvert(ws_val, s_val);
		try { std::stoul(s_val); }
		catch (const std::exception&) { alert(L"Validation Error", L"Invalid Sample Rate. Must be a positive number.", MB_ICONERROR); focus(editSampleRate); return false; }

		ws_val = get_text(editBufferSize); CStringUtils::UnicodeConvert(ws_val, s_val);
		try { std::stoul(s_val); }
		catch (const std::exception&) { alert(L"Validation Error", L"Invalid Buffer Size. Must be a number.", MB_ICONERROR); focus(editBufferSize); return false; }

		DWORD kmod_dummy; int kcode_dummy;
		ws_val = get_text(editHotkeyStartStop); CStringUtils::UnicodeConvert(ws_val, new_hotkey_start_stop);
		if (!parse_hotkey(new_hotkey_start_stop, kmod_dummy, kcode_dummy)) { alert(L"Validation Error", L"Invalid Start/Stop Hotkey format.", MB_ICONERROR); focus(editHotkeyStartStop); return false; }

		ws_val = get_text(editHotkeyPauseResume); CStringUtils::UnicodeConvert(ws_val, new_hotkey_pause_resume);
		if (!parse_hotkey(new_hotkey_pause_resume, kmod_dummy, kcode_dummy)) { alert(L"Validation Error", L"Invalid Pause/Resume Hotkey format.", MB_ICONERROR); focus(editHotkeyPauseResume); return false; }

		ws_val = get_text(editHotkeyRestart); CStringUtils::UnicodeConvert(ws_val, new_hotkey_restart);
		if (!parse_hotkey(new_hotkey_restart, kmod_dummy, kcode_dummy)) { alert(L"Validation Error", L"Invalid Restart Hotkey format.", MB_ICONERROR); focus(editHotkeyRestart); return false; }

		ws_val = get_text(editRecordPath);
		if (ws_val.empty()) { alert(L"Validation Error", L"Record path cannot be empty.", MB_ICONERROR); focus(editRecordPath); return false; }

		// Audio Format vs Make Stems
		ws_val = get_text(editAudioFormat);
		if (ws_val.empty()) { alert(L"Validation Error", L"Audio format cannot be empty.", MB_ICONERROR); focus(editAudioFormat); return false; }
		if (is_checked(chkMakeStems) && ws_val != L"wav") {
			alert(L"Validation Error", L"'Make Stems' is only supported with 'wav' audio format. Please change audio format to 'wav' or disable 'Make Stems'.", MB_ICONERROR);
			return false;
		}


		return true;
	}


	void apply() {
		std::wstring ws_val; std::string s_val;

		ws_val = get_text(editSampleRate); CStringUtils::UnicodeConvert(ws_val, s_val);
		sample_rate = std::stoul(s_val);
		conf.write("General", "sample-rate", std::to_string(sample_rate));

		channels = (get_list_position(listChannels) == 0) ? 1 : 2;
		conf.write("General", "channels", std::to_string(channels));

		ws_val = get_text(editBufferSize); CStringUtils::UnicodeConvert(ws_val, s_val);
		buffer_size = std::stoul(s_val);
		conf.write("General", "buffer-size", std::to_string(buffer_size));

		ws_val = get_text(editFilenameSignature); CStringUtils::UnicodeConvert(ws_val, filename_signature);
		conf.write("General", "filename-signature", filename_signature);

		ws_val = get_text(editRecordPath); CStringUtils::UnicodeConvert(ws_val, record_path);
		conf.write("General", "record-path", record_path);

		ws_val = get_text(editAudioFormat); CStringUtils::UnicodeConvert(ws_val, audio_format);
		audio_format = CStringUtils::ToLowerCase(audio_format);
		conf.write("General", "audio-format", audio_format);
		if (audio_format != "wav") {
			std::string output_ffmpeg_check;
			if (ExecSystemCmd("ffmpeg.exe -h", output_ffmpeg_check) != 0) {
				alert(L"FPFFMPegInitializerError", L"ffmpeg.exe not found or inaccessible. Non-WAV formats require FFmpeg. Please install FFmpeg and ensure it's in your system's PATH, or choose WAV format. Reverting to WAV.", MB_ICONERROR);
				audio_format = "wav";
				set_text(editAudioFormat, L"wav");
				conf.write("General", "audio-format", audio_format);
			}
		}


		sound_events = is_checked(chkSoundEvents) ? MA_TRUE : MA_FALSE;
		conf.write("General", "sound-events", sound_events ? "true" : "false");

		make_stems = is_checked(chkMakeStems) ? MA_TRUE : MA_FALSE;
		conf.write("General", "make-stems", make_stems ? "true" : "false");

		int buffer_fmt_idx = get_list_position(listBufferFormat);
		std::wstring ws_buffer_format_new = get_list_item_text_by_index_internal(listBufferFormat, buffer_fmt_idx);
		std::string s_buffer_format_new; CStringUtils::UnicodeConvert(ws_buffer_format_new, s_buffer_format_new);
		try_parse_format(s_buffer_format_new.c_str(), buffer_format);
		conf.write("General", "sample-format", ma_format_to_string(buffer_format));

		UnregisterHotKey(nullptr, HOTKEY_STARTSTOP);
		UnregisterHotKey(nullptr, HOTKEY_PAUSERESUME);
		UnregisterHotKey(nullptr, HOTKEY_RESTART);

		hotkey_start_stop = new_hotkey_start_stop;
		conf.write("General", "hotkey-start-stop", hotkey_start_stop);
		hotkey_pause_resume = new_hotkey_pause_resume;
		conf.write("General", "hotkey-pause-resume", hotkey_pause_resume);
		hotkey_restart = new_hotkey_restart;
		conf.write("General", "hotkey-restart", hotkey_restart);

		DWORD kmod; int kcode; // Re-declare to avoid scope issues if any
		if (parse_hotkey(hotkey_start_stop, kmod, kcode)) RegisterHotKey(nullptr, HOTKEY_STARTSTOP, kmod, kcode);
		if (parse_hotkey(hotkey_pause_resume, kmod, kcode)) RegisterHotKey(nullptr, HOTKEY_PAUSERESUME, kmod, kcode);
		if (parse_hotkey(hotkey_restart, kmod, kcode)) RegisterHotKey(nullptr, HOTKEY_RESTART, kmod, kcode);

		int preset_idx = get_list_position(listCurrentPreset);
		std::wstring ws_preset_name_new = get_list_item_text_by_index_internal(listCurrentPreset, preset_idx);
		CStringUtils::UnicodeConvert(ws_preset_name_new, current_preset_name);
		conf.write("General", "current-preset", current_preset_name);
		for (const auto& p : presets) {
			if (p.name == current_preset_name) {
				g_CurrentPreset = p;
				break;
			}
		}

		conf.save();
	}
};





static CMainWindow g_MainWindow;
static CRecordManagerWindow g_RecordManagerWindow;
static CRecordingWindow g_RecordingWindow;
static CSettingsWindow g_SettingsWindow;



bool g_RecordingsManager = false;

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
			sound_events = str_to_bool(sevents);
			std::string mstems = conf.read("General", "make-stems");
			make_stems = str_to_bool(mstems);
			if (make_stems && audio_format != "wav") {
				throw std::exception("Can't make stems when using another format, built-in not supported by FPRecorder");
			}
			std::string sformat = CStringUtils::ToLowerCase(conf.read("General", "sample-format"));
			ma_bool32 parse_result = try_parse_format(sformat.c_str(), buffer_format);
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
				for (unsigned int i = 0; i < presets_str.size(); ++i) {
					preset p;
					p.name = presets_str[i];
					p.command = conf.read("Presets", presets_str[i]);
					presets.push_back(p);
				}
			}
			current_preset_name = conf.read("General", "current-preset");
			bool preset_found = false;
			unsigned int it;
			for (it = 0; it < presets.size(); ++it) {
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
		window = show_window(L"FPRecorder " + version + (IsUserAnAdmin() ? L" (Administrator)" : L"")); assert(window > 0);
		g_MainWindow.build();
		presets.push_back(g_DefaultPreset);
		key_pressed(VK_SPACE) || key_pressed(VK_RETURN); // Avoid click to start recording
		while (g_Running) {
			wait(5);
			update_window(window);

			// Avoid invalid hotkey presses not in recording mode.
			if (!g_Recording || g_SettingsMode) {
				hotkey_pressed(HOTKEY_PAUSERESUME);
				hotkey_pressed(HOTKEY_RESTART);
			}
			if (g_RecordingsManager || g_SettingsMode) {
				hotkey_pressed(HOTKEY_STARTSTOP);
			}
			if (gui::try_close) {
				gui::try_close = false;
				if (g_Recording) {
					if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
					g_SpeechProvider.Speak("Unable to exit, while recording.");
					continue;
				}
				g_Retcode = 0;
				g_Running = false;
				break;
			}
			if (!g_Recording && !g_RecordingsManager && !g_SettingsMode && is_pressed(g_MainWindow.settings_button)) {
				loopback_device = get_list_position(g_MainWindow.output_devices_list);
				input_device = get_list_position(g_MainWindow.input_devices_list);
				conf.write("General", "input-device", std::to_string(input_device));
				conf.write("General", "loopback-device", std::to_string(loopback_device));

				g_MainWindow.reset();
				g_SettingsWindow.build();
				g_SettingsMode = true;
				g_SpeechProvider.Speak("Settings", false);
			}

			if (g_SettingsMode) {
				if (is_pressed(g_SettingsWindow.btnCancelSettings) || key_down(VK_ESCAPE)) {
					g_SettingsWindow.reset();
					g_MainWindow.build();
					focus(g_MainWindow.settings_button);
					g_SettingsMode = false;
					g_SpeechProvider.Speak("Canceled.", false);
				}
				else if (is_pressed(g_SettingsWindow.btnSaveSettings)) {
					if (g_SettingsWindow.validate_and_prepare_settings()) {
						g_SettingsWindow.apply();

						g_SettingsWindow.reset();
						g_MainWindow.build();
						focus(g_MainWindow.settings_button);
						g_SettingsMode = false;
						g_SpeechProvider.Speak("Settings saved.", false);
					}
				}
				else if (is_pressed(g_SettingsWindow.btnBrowseRecordPath)) {
					OleInitialize(NULL);
					BROWSEINFOW bi = { 0 };
					bi.hwndOwner = window;
					bi.lpszTitle = L"Select Recording Folder";
					bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;

					wchar_t currentPathBuffer[MAX_PATH];
					std::wstring wsCurrentPath = get_text(g_SettingsWindow.editRecordPath);
					wcsncpy_s(currentPathBuffer, wsCurrentPath.c_str(), MAX_PATH - 1);

					auto browseCallback = [](HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) -> int {
						if (uMsg == BFFM_INITIALIZED) {
							SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
						}
						return 0;
						};
					bi.lpfn = browseCallback;
					bi.lParam = (LPARAM)currentPathBuffer;


					LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
					if (pidl != nullptr) {
						wchar_t path_buffer[MAX_PATH];
						if (SHGetPathFromIDListW(pidl, path_buffer)) {
							set_text(g_SettingsWindow.editRecordPath, path_buffer);
						}
						CoTaskMemFree(pidl);
					}
					OleUninitialize();
				}
			}

			if (is_pressed(g_MainWindow.record_manager) and !g_RecordingsManager) {
				loopback_device = get_list_position(g_MainWindow.output_devices_list);
				input_device = get_list_position(g_MainWindow.input_devices_list);
				conf.write("General", "input-device", std::to_string(input_device));
				conf.write("General", "loopback-device", std::to_string(loopback_device));
				conf.save();
				g_MainWindow.reset();
				g_RecordManagerWindow.build();
				if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_RECORD_MANAGER);
				g_RecordingsManager = true;
				g_SoundStream.Initialize();
				key_pressed(VK_SPACE);
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
					if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
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
				if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_START_RECORDING);
				g_CurrentInputDevice = in_audio_devices[input_device];
				g_CurrentOutputDevice = out_audio_devices[loopback_device];
				if (g_CurrentInputDevice.name == L"Not used" && g_CurrentOutputDevice.name == L"Not used") {
					if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
					g_SpeechProvider.Speak("Can't record silence", true);
					if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_STOP_RECORDING);

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
				if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_STOP_RECORDING);
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
					CStringUtils::Replace(cmd, "%I", "\"" + rec.filename + ".wav\"", true);
					CStringUtils::Replace(cmd, "%i", "\"" + split[0] + "\"", true);
					CStringUtils::Replace(cmd, "%f", audio_format, true);
					cmd.append(" -y"); // If ffmpeg will attempt to ask about overwrite, we add this flag to avoid stdin locks
					int result = ExecSystemCmd(cmd, output);
					if (result != 0) {
						if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
						wstring output_u;
						CStringUtils::UnicodeConvert(output, output_u);
						alert(L"FPError", L"Process exit failure!\nRetcode: " + std::to_wstring(result) + L"\nOutput: \"" + output_u + L"\".", MB_ICONERROR);
					}
					else {
						std::wstring recording_name_u;
						CStringUtils::UnicodeConvert(rec.filename + ".wav", recording_name_u);
						DeleteFile(recording_name_u.c_str());
					}
				}
				g_MainWindow.build();

			}
			if (g_Recording && (is_pressed(g_RecordingWindow.record_pause) || hotkey_pressed(HOTKEY_PAUSERESUME))) {
				if (!g_RecordingPaused) {
					rec.pause();
					if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_PAUSE_RECORDING);
					g_RecordingPaused = true;
					set_text(g_RecordingWindow.record_pause, L"&Resume recording");
				}
				else if (g_RecordingPaused) {
					if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_RESUME_RECORDING);
					rec.resume();
					g_RecordingPaused = false;
					set_text(g_RecordingWindow.record_pause, L"&Pause recording");
				}
				wait(20);
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
				if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_RESTART_RECORDING);

			}
		}
	}
	catch (const std::exception& ex) {
		std::wstring exception_u;
		CStringUtils::UnicodeConvert(ex.what(), exception_u);
		if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
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
	g_MaLastError = MA_SUCCESS;
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
	if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
	alert(L"FPRuntimeError", str_u, MB_ICONERROR);
	g_Retcode = -100;
	g_Running = false; // In any situation, user should nott lost the record. Give application call audio recorder destructor to try uninitialize the encoder
	return 0;
}



