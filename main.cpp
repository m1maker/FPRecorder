#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "gui/gui.h"
#include "Provider.h"
#include "user_config.h"
#include <cctype>
#include <deque>
#include<filesystem>
#include<mutex>
#include <ole2.h>
#include <shlobj_core.h>
#include <stacktrace>
#include <tlhelp32.h>
#include <Uiautomationcore.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <comdef.h>
#include <chrono>
#include<fstream>
#include <iomanip>
#include<sstream>
#include<thread>
#include <vector>
#include <array>
#include <functional>
#include <optional>
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <condition_variable>
#include <string_view>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <audioclientactivationparams.h>
#include <mfapi.h>


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
	HOTKEY_RESTART = 3,
	HOTKEY_HIDESHOW = 4
};


static int g_Retcode = 0;
static bool g_Running = false;

template <class T>
class CSingleton {
public:
	static T& GetInstance() {
		static T instance;
		return instance;
	}

private:
	CSingleton() = default;
	~CSingleton() = default;

	CSingleton(const CSingleton&) = delete;
	CSingleton& operator=(const CSingleton&) = delete;
	CSingleton(CSingleton&&) = delete;
	CSingleton& operator=(CSingleton&&) = delete;
};

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




// Command line arguments
class COptionSet {
public:
	bool start = false;
	std::string filename = "";
	bool useFilename = false;
	bool exitAfterStop = false;

	static bool ParseOptions(const wchar_t* lpCmdLine, std::vector<std::string>& options, std::vector<std::string>& values) {
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

};



static COptionSet g_CommandLineOptions;

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* exceptionInfo);


struct preset {
	std::string name;
	std::string command;
};
static preset g_CurrentPreset;

static std::vector<preset> g_Presets = {
	{ "Default", "ffmpeg.exe -i %I %i.%f" },
	{ "MP3 128k", "ffmpeg.exe -i %I -b:a 128k %i.mp3" },
	{ "MP3 192k", "ffmpeg.exe -i %I -b:a 192k %i.mp3" },
	{ "MP3 256k", "ffmpeg.exe -i %I -b:a 256k %i.mp3" },
	{ "MP3 320k", "ffmpeg.exe -i %I -b:a 320k %i.mp3" },
	{ "AAC 128k", "ffmpeg.exe -i %I -c:a aac -b:a 128k %i.m4a" },
	{ "AAC 192k", "ffmpeg.exe -i %I -c:a aac -b:a 192k %i.m4a" },
	{ "AAC 256k", "ffmpeg.exe -i %I -c:a aac -b:a 256k %i.m4a" },
	{ "AAC 320k", "ffmpeg.exe -i %I -c:a aac -b:a 320k %i.m4a" },
	{ "OGG Vorbis q5", "ffmpeg.exe -i %I -c:a libvorbis -qscale:a 5 %i.ogg" },
	{ "OGG Vorbis q7", "ffmpeg.exe -i %I -c:a libvorbis -qscale:a 7 %i.ogg" },
	{ "FLAC (Lossless)", "ffmpeg.exe -i %I -c:a flac %i.flac" }
};

static ma_uint32 sample_rate = 44100;
static ma_uint32 channels = 2;
static ma_uint32 buffer_size = 0;
static std::string filename_signature = "%Y %m %d %H %M %S";
static std::filesystem::path record_path = std::filesystem::current_path() / "recordings";
static std::string audio_format = "wav";
static ma_bool32 sound_events = MA_TRUE;
static ma_bool32 make_stems = MA_FALSE;
static ma_format buffer_format = ma_format_s16;
static constexpr ma_uint32 periods = 256;
static std::string hotkey_start_stop = "Windows+Shift+F1";
static std::string hotkey_pause_resume = "Windows+Shift+F2";
static std::string hotkey_restart = "Windows+Shift+F3";
static std::string hotkey_hide_show = "Windows+Shift+F4";

struct audio_device {
	std::wstring name;
	ma_device_id id;
};



enum SourceType {
	DEVICE_CAPTURE,
	DEVICE_LOOPBACK,
	APP_LOOPBACK
};

struct application {
	std::wstring name;
	ma_uint32 id;
};

struct ConfiguredSource {
	std::wstring custom_name;
	SourceType type;

	audio_device device;
	application app;
};

static std::vector<ConfiguredSource> g_ConfiguredSources;

static std::string current_preset_name = g_Presets[0].name;

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
		auto result = safeCallVal<IUIAutomation, HRESULT>(pAutomation, &IUIAutomation::CreatePropertyConditionEx, UIA_NamePropertyId, varName, PropertyConditionFlags_None, &pCondition);
		if (!result.has_value() || FAILED(*result)) {
			return;
		}
	}
	~CUIAutomationSpeech() {
		safeCall(pProvider, &Provider::Release);
		safeCall(pCondition, &IUIAutomationCondition::Release);
		safeCall(pAutomation, &IUIAutomation::Release);
		safeCall(pElement, &IUIAutomationElement::Release);
	}

	bool Speak(const wchar_t* text, bool interrupt = true) {
		safeCall(pProvider, &Provider::Release);
		safeCall(pElement, &IUIAutomationElement::Release);
		pProvider = new Provider(GetForegroundWindow());

		auto result = safeCallVal<IUIAutomation, HRESULT>(pAutomation, &IUIAutomation::ElementFromHandle, GetForegroundWindow(), &pElement);
		if (!result.has_value() || FAILED(*result)) {
			return false;
		}
		HRESULT hr = UiaRaiseNotificationEvent(pProvider, NotificationKind_ActionCompleted, interrupt ? NotificationProcessing_ImportantMostRecent : NotificationProcessing_ImportantAll, _bstr_t(text), _bstr_t(L""));
		if (FAILED(hr)) {
			return false;
		}
		return true;

	}
	inline bool Speak(const char* text, bool interrupt = true) {
		std::wstring str;
		CStringUtils::UnicodeConvert(text, str);
		return this->Speak(str.c_str(), interrupt);
	}
	inline bool Speak(const std::string& utf8str, bool interrupt = true) {
		return this->Speak(utf8str.c_str(), interrupt);
	}
	inline bool Speak(const std::wstring& utf16str, bool interrupt = true) {
		return this->Speak(utf16str.c_str(), interrupt);
	}

	template <typename... Args>
	bool Speakf(const char* format, Args... args) {
		const size_t bufferSize = 1024;
		char buffer[bufferSize];


		int ret = vsnprintf(buffer, bufferSize, format, args...);

		if (ret < 0 || ret >= bufferSize) {
			return false;
		}

		return Speak(buffer);
	}


	bool StopSpeech() {
		return Speak(L"", true);
	}
};


#define g_SpeechProvider CSingleton<CUIAutomationSpeech>::GetInstance()

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




static constexpr std::string_view ma_result_to_string(ma_result result) {
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

static void CheckIfError(const ma_result& result) {
	g_MaLastError = MA_SUCCESS;
	if (result == MA_SUCCESS) {
		return;
	}
	g_MaLastError = result;
	std::wstring error_u;
	std::stringstream ss;
	std::stacktrace st = std::stacktrace::current();
	ss << ma_result_to_string(result).data() << std::endl;
	ss << st;
	CStringUtils::UnicodeConvert(ss.str(), error_u);
	alert(L"FPRuntimeError", error_u, MB_ICONERROR);
	g_Retcode = result;
	g_Running = false;
}





class MINIAUDIO_IMPLEMENTATION CAudioContext {
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

	inline operator ma_context* () {
		return &*context;
	}
};

#define g_AudioContext CSingleton<CAudioContext>::GetInstance()





class MINIAUDIO_IMPLEMENTATION CSoundStream {
	std::unique_ptr<ma_engine> m_Engine;
	std::unique_ptr<ma_sound> m_Player;
	std::unique_ptr<ma_waveform> m_Waveform;
	std::wstring current_file;
public:
	enum ESoundEvent : std::int8_t {
		SOUND_EVENT_NONE = 0,
		SOUND_EVENT_START_RECORDING,
		SOUND_EVENT_STOP_RECORDING,
		SOUND_EVENT_PAUSE_RECORDING,
		SOUND_EVENT_RESUME_RECORDING,
		SOUND_EVENT_RESTART_RECORDING,
		SOUND_EVENT_RECORD_MANAGER,
		SOUND_EVENT_ERROR = -1,
	};
	CSoundStream() : m_Engine(nullptr), m_Player(nullptr), m_Waveform(nullptr) { Initialize(); }
	~CSoundStream() {
		Uninitialize();
	}

	bool Initialize() {
		if (m_Engine) {
			return true;
		}
		m_Engine = std::make_unique<ma_engine>();
		ma_engine_config cfg = ma_engine_config_init();
		cfg.sampleRate = sample_rate;
		cfg.channels = channels;
		CheckIfError(ma_engine_init(&cfg, &*m_Engine));
		return g_MaLastError == MA_SUCCESS;
	}

	bool Uninitialize() {
		Close();
		if (m_Engine) {
			ma_engine_uninit(&*m_Engine);
			m_Engine.reset();
		}
		return true;
	}

	inline bool Reinitialize() {
		return Uninitialize() && Initialize();
	}

	inline operator ma_sound* () {
		return &*m_Player;
	}

	inline operator ma_engine* () {
		return &*m_Engine;
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

	inline bool Play(const std::string& filename) {
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

#define g_SoundStream CSingleton<CSoundStream>::GetInstance()



static bool ma_format_convert(const std::string& format, ma_format& value)
{
	std::string str = CStringUtils::ToLowerCase(format);
	if (str == "u8") {
		value = ma_format_u8;
	}
	else if (str == "s16") {
		value = ma_format_s16;
	}
	else if (str == "s24") {
		value = ma_format_s24;
	}
	else if (str == "s32") {
		value = ma_format_s32;
	}
	else if (str == "f32") {
		value = ma_format_f32;
	}
	else {
		return false;
	}

	return true;
}

static bool ma_format_convert(const ma_format& format, std::string& value) {
	switch (format) {
	case ma_format_u8:
		value = "u8";
		break;
	case ma_format_s16:
		value = "s16";
		break;
	case ma_format_s24:
		value = "s24";
		break;
	case ma_format_s32:
		value = "s32";
		break;
	case ma_format_f32:
		value = "f32";
		break;
	default:
		return false;
	}
	return true;
}


static bool ma_device_type_convert(const std::string& type, ma_device_type& value)
{
	std::string str = CStringUtils::ToLowerCase(type);
	if (str == "mic") {
		value = ma_device_type_capture;
	}
	else if (str == "loopback") {
		value = ma_device_type_loopback;
	}
	else {
		return false;
	}

	return true;
}

static bool ma_device_type_convert(const ma_device_type& type, std::string& value) {
	switch (type) {
	case ma_device_type_capture:
		value = "mic";
		break;
	case ma_device_type_loopback:
		value = "loopback";
		break;
	default:
		return false;
	}
	return true;
}





static inline std::optional<bool> str_to_bool(const std::string& val) {
	std::string str = CStringUtils::ToLowerCase(val);
	if (str == "0" || str == "false") {
		return false;
	}
	else if (str == "1" || str == "true") {
		return true;
	}
	return std::nullopt;
}


using namespace std;









bool g_Recording = false;
bool g_RecordingPaused = false;

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


struct AudioData {
	float* buffer;
	ma_uint64 frameCount;
};

struct RecordingSource {
	std::deque<AudioData> queue;
	std::mutex mutex;
	std::condition_variable condition;
	std::wstring name;
};

struct CallbackUserData {
	size_t sourceIndex;
};


static std::vector<std::unique_ptr<RecordingSource>> g_RecordingSources;
static std::vector<std::unique_ptr<CallbackUserData>> g_CallbackUserData;
static std::vector<audio_device> g_SelectedCaptureDevices;
static std::vector<audio_device> g_SelectedLoopbackDevices;

static std::atomic<bool> thread_shutdown = false;
static std::atomic<bool> paused = false;

class CompletionHandler : public IActivateAudioInterfaceCompletionHandler, public IAgileObject {
public:
	CompletionHandler() : _refCount(1), activate_hr(E_FAIL), client(nullptr) {
		event_finished = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	}

	~CompletionHandler() {
		if (event_finished) CloseHandle(event_finished);
		if (client) client->Release();
	}

	// IUnknown
	ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&_refCount); }
	ULONG STDMETHODCALLTYPE Release() override {
		ULONG ulRef = InterlockedDecrement(&_refCount);
		if (0 == ulRef) {
			delete this;
		}
		return ulRef;
	}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
		if (riid == IID_IUnknown || riid == __uuidof(IActivateAudioInterfaceCompletionHandler)) {
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		else if (riid == __uuidof(IAgileObject)) {
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}

	// IActivateAudioInterfaceCompletionHandler
	STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) override {
		if (operation) {
			IUnknown* pUnknown = nullptr;
			HRESULT hr_activate_result = E_FAIL;
			operation->GetActivateResult(&hr_activate_result, &pUnknown);
			activate_hr = hr_activate_result;
			if (SUCCEEDED(activate_hr) && pUnknown) {
				pUnknown->QueryInterface(IID_PPV_ARGS(&client));
				pUnknown->Release();
			}
		}
		SetEvent(event_finished);
		return S_OK;
	}

	HRESULT activate_hr;
	IAudioClient* client;
	HANDLE event_finished;
private:
	LONG _refCount;
};


class AppLoopbackCapture {
private:
	std::thread m_thread;
	std::atomic<bool> m_shutdown_flag{ false };
	HANDLE m_shutdown_event = nullptr;
	HANDLE m_packet_ready_event = nullptr;
	DWORD m_pid;
	RecordingSource* m_target_queue;
	WAVEFORMATEX m_format;
	IAudioClient* m_client = nullptr;
	IAudioCaptureClient* m_capture_client = nullptr;

public:
	AppLoopbackCapture(DWORD pid, RecordingSource* target_queue, ma_uint32 sampleRate, ma_uint32 numChannels)
		: m_pid(pid), m_target_queue(target_queue) {
		m_format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		m_format.nChannels = numChannels;
		m_format.nSamplesPerSec = sampleRate;
		m_format.wBitsPerSample = sizeof(float) * 8;
		m_format.nBlockAlign = m_format.nChannels * sizeof(float);
		m_format.nAvgBytesPerSec = m_format.nSamplesPerSec * m_format.nBlockAlign;
		m_format.cbSize = 0;

		m_shutdown_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		m_packet_ready_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}

	~AppLoopbackCapture() {
		Stop();
		if (m_shutdown_event) CloseHandle(m_shutdown_event);
		if (m_packet_ready_event) CloseHandle(m_packet_ready_event);
	}

	void Start() {
		m_shutdown_flag = false;
		m_thread = std::thread(&AppLoopbackCapture::CaptureThread, this);
		m_thread.detach();
	}

	void Stop() {
		if (m_shutdown_flag.exchange(true)) return; // Already stopping

		if (m_shutdown_event) SetEvent(m_shutdown_event);
		if (m_thread.joinable()) m_thread.join();
	}

private:
	void CaptureThread() {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

		if (!InitClient()) {
			CoUninitialize();
			return;
		}

		m_client->Start();
		HANDLE wait_handles[] = { m_shutdown_event, m_packet_ready_event };

		while (!m_shutdown_flag) {
			DWORD wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);

			if (wait_result == WAIT_OBJECT_0) {
				break;
			}
			else if (wait_result == WAIT_OBJECT_0 + 1) {
				if (!m_shutdown_flag) {
					ProcessPacket();
				}
			}
			else {
				break;
			}
		}

		m_client->Stop();
		if (m_capture_client) m_capture_client->Release();
		if (m_client) m_client->Release();
		m_capture_client = nullptr;
		m_client = nullptr;
		CoUninitialize();
	}

	bool InitClient() {
		HRESULT hr;
		hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
		AUDIOCLIENT_ACTIVATION_PARAMS activationParams = {};
		activationParams.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
		activationParams.ProcessLoopbackParams.TargetProcessId = m_pid;
		activationParams.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

		PROPVARIANT propVariant = {};
		propVariant.vt = VT_BLOB;
		propVariant.blob.cbSize = sizeof(activationParams);
		propVariant.blob.pBlobData = (BYTE*)&activationParams;

		CompletionHandler* handler = new CompletionHandler();
		IActivateAudioInterfaceAsyncOperation* async_op = nullptr;
		hr = ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient), &propVariant, handler, &async_op);
		if (FAILED(hr)) {
			alert(L"YY", std::to_wstring(hr), 0);
			handler->Release();
			if (async_op) async_op->Release();
			return false;
		}
		if (async_op) async_op->Release();

		WaitForSingleObject(handler->event_finished, INFINITE);
		hr = handler->activate_hr;
		if (FAILED(hr)) {
			handler->Release();
			return false;
		}

		m_client = handler->client;
		handler->client = nullptr;
		handler->Release();

		hr = m_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM, 100 * 10000, 0, &m_format, NULL);
		if (FAILED(hr)) {
			m_client->Release();
			m_client = nullptr;
			return false;
		}

		hr = m_client->SetEventHandle(m_packet_ready_event);
		if (FAILED(hr)) { return false; }

		hr = m_client->GetService(__uuidof(IAudioCaptureClient), (void**)&m_capture_client);
		if (FAILED(hr)) { return false; }

		return true;
	}

	void ProcessPacket() {
		UINT32 num_frames = 0;
		HRESULT hr = m_capture_client->GetNextPacketSize(&num_frames);
		if (FAILED(hr)) return;

		while (num_frames > 0) {
			BYTE* pData;
			DWORD flags;
			UINT64 qpc_position;

			hr = m_capture_client->GetBuffer(&pData, &num_frames, &flags, NULL, &qpc_position);
			if (FAILED(hr)) break;

			if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && num_frames > 0) {
				float* buffer = new float[num_frames * m_format.nChannels];
				memcpy(buffer, pData, num_frames * m_format.nChannels * sizeof(float));

				AudioData data;
				data.buffer = buffer;
				data.frameCount = num_frames;

				{
					std::lock_guard<std::mutex> lock(m_target_queue->mutex);
					m_target_queue->queue.push_back(data);
				}
				m_target_queue->condition.notify_one();
			}

			m_capture_client->ReleaseBuffer(num_frames);
			hr = m_capture_client->GetNextPacketSize(&num_frames);
			if (FAILED(hr)) break;
		}
	}
};


class MINIAUDIO_IMPLEMENTATION CAudioRecorder {
	std::thread m_ProcessingThread;
	std::vector<std::unique_ptr<ma_device>> m_devices;
	std::vector<std::unique_ptr<ma_encoder>> m_encoders;
	std::unique_ptr<ma_data_converter> m_Converter;
	std::vector<std::unique_ptr<AppLoopbackCapture>> m_app_capturers;

public:
	std::string base_filename;

	CAudioRecorder() : m_Converter(nullptr) {}
	~CAudioRecorder() {
		if (g_Recording) {
			this->stop();
		}
	}

	static void GeneralDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
		auto* pUserData = static_cast<CallbackUserData*>(pDevice->pUserData);
		if (paused || !pUserData || pUserData->sourceIndex >= g_RecordingSources.size()) {
			return;
		}

		size_t index = pUserData->sourceIndex;

		float* buffer = new float[frameCount * channels];
		memcpy(buffer, pInput, frameCount * channels * sizeof(float));

		AudioData data;
		data.buffer = buffer;
		data.frameCount = frameCount;

		{
			std::lock_guard<std::mutex> lock(g_RecordingSources[index]->mutex);
			g_RecordingSources[index]->queue.push_back(data);
		}
		g_RecordingSources[index]->condition.notify_one();

		(void)pOutput;
	}

	static float* mix_f32_multi(const std::vector<AudioData>& sources, ma_uint64& outFrameCount) {
		if (sources.empty()) {
			outFrameCount = 0;
			return nullptr;
		}

		outFrameCount = sources[0].frameCount;
		if (outFrameCount == 0) return nullptr;

		float* mixedBuffer = new float[outFrameCount * channels];
		memset(mixedBuffer, 0, outFrameCount * channels * sizeof(float));

		for (const auto& source : sources) {
			ma_uint64 framesToMix = std::min(outFrameCount, source.frameCount);
			for (ma_uint32 i = 0; i < framesToMix * channels; ++i) {
				mixedBuffer[i] += source.buffer[i];
			}
		}
		return mixedBuffer;
	}

	static void ProcessingThread(CAudioRecorder* recorder) {
		while (!thread_shutdown) {
			std::vector<AudioData> dataToProcess;
			dataToProcess.resize(g_RecordingSources.size());
			bool allDataAvailable = true;

			for (size_t i = 0; i < g_RecordingSources.size(); ++i) {
				std::unique_lock<std::mutex> lock(g_RecordingSources[i]->mutex);
				if (g_RecordingSources[i]->condition.wait_for(lock, std::chrono::milliseconds(100), [&] { return !g_RecordingSources[i]->queue.empty() || thread_shutdown; })) {
					if (thread_shutdown) break;
					dataToProcess[i] = g_RecordingSources[i]->queue.front();
					g_RecordingSources[i]->queue.pop_front();
				}
				else {
					allDataAvailable = false;
					break;
				}
			}

			if (thread_shutdown) {
				for (auto& data : dataToProcess) if (data.buffer) delete[] data.buffer;
				break;
			}

			if (!allDataAvailable) {
				for (size_t i = 0; i < dataToProcess.size(); ++i) {
					if (dataToProcess[i].buffer) {
						std::lock_guard<std::mutex> lock(g_RecordingSources[i]->mutex);
						g_RecordingSources[i]->queue.push_front(dataToProcess[i]);
					}
				}
				continue;
			}

			if (make_stems) {
				for (size_t i = 0; i < dataToProcess.size(); ++i) {
					void* pOutput = dataToProcess[i].buffer;
					ma_uint64 frameCount = dataToProcess[i].frameCount;
					if (buffer_format != ma_format_f32) {
						ma_data_converter_process_pcm_frames__format_only(&*recorder->m_Converter, dataToProcess[i].buffer, &dataToProcess[i].frameCount, pOutput, &frameCount);
					}
					ma_encoder_write_pcm_frames(&*recorder->m_encoders[i], pOutput, frameCount, nullptr);
					delete[] dataToProcess[i].buffer;
				}
			}
			else {
				ma_uint64 frameCount = 0;
				float* mixedBuffer = mix_f32_multi(dataToProcess, frameCount);
				if (mixedBuffer) {
					void* pOutput = mixedBuffer;
					ma_uint64 frameCountOut = frameCount;
					if (buffer_format != ma_format_f32) {
						ma_data_converter_process_pcm_frames__format_only(&*recorder->m_Converter, mixedBuffer, &frameCount, pOutput, &frameCountOut);
					}

					ma_encoder_write_pcm_frames(&*recorder->m_encoders[0], pOutput, frameCountOut, nullptr);
					delete[] mixedBuffer;
				}
				for (auto& data : dataToProcess) delete[] data.buffer;
			}
		}
	}

	void start(const std::vector<ConfiguredSource>& sources) {
		stop();
		g_RecordingSources.clear();
		g_CallbackUserData.clear();
		m_app_capturers.clear();

		if (sources.empty()) {
			g_SpeechProvider.Speak("No devices configured for recording.", true);
			return;
		}

		for (const ConfiguredSource& source : sources) {
			g_RecordingSources.push_back(std::make_unique<RecordingSource>());
			g_RecordingSources.back()->name = source.custom_name;
		}

		base_filename = g_CommandLineOptions.useFilename ? (std::filesystem::path(record_path / g_CommandLineOptions.filename).generic_string()) : (record_path / get_now()).generic_string();
		ma_encoder_config encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, buffer_format, channels, sample_rate);

		if (make_stems) {
			m_encoders.resize(sources.size());
			for (size_t i = 0; i < sources.size(); ++i) {
				std::string custom_name_narrow;
				CStringUtils::UnicodeConvert(sources[i].custom_name, custom_name_narrow);
				CStringUtils::Replace(custom_name_narrow, "\"", "", true);
				CStringUtils::Replace(custom_name_narrow, "\\", "", true);
				std::string stem_filename = base_filename + " (" + custom_name_narrow + ").wav";

				m_encoders[i] = std::make_unique<ma_encoder>();
				CheckIfError(ma_encoder_init_file(stem_filename.c_str(), &encoderConfig, &*m_encoders[i]));
			}
		}
		else {
			m_encoders.resize(1);
			m_encoders[0] = std::make_unique<ma_encoder>();
			CheckIfError(ma_encoder_init_file(std::string(base_filename + ".wav").c_str(), &encoderConfig, &*m_encoders[0]));
		}

		m_devices.resize(sources.size());
		for (size_t i = 0; i < sources.size(); ++i) {
			const auto& source = sources[i];

			switch (source.type) {
			case APP_LOOPBACK: {
				auto capturer = std::make_unique<AppLoopbackCapture>(source.app.id, g_RecordingSources[i].get(), sample_rate, channels);
				capturer->Start();
				m_app_capturers.push_back(std::move(capturer));
				break;
			}
			case DEVICE_CAPTURE:
			case DEVICE_LOOPBACK: {
				g_CallbackUserData.push_back(std::make_unique<CallbackUserData>(CallbackUserData{ i }));

				ma_device_config deviceConfig = ma_device_config_init(source.type == DEVICE_CAPTURE ? ma_device_type_capture : ma_device_type_loopback);
				deviceConfig.capture.pDeviceID = (ma_device_id*)&source.device.id;
				deviceConfig.capture.format = ma_format_f32;
				deviceConfig.capture.channels = channels;
				deviceConfig.sampleRate = sample_rate;
				deviceConfig.dataCallback = CAudioRecorder::GeneralDataCallback;
				deviceConfig.pUserData = g_CallbackUserData.back().get();

				m_devices[i] = std::make_unique<ma_device>();
				CheckIfError(ma_device_init(CSingleton<CAudioContext>::GetInstance(), &deviceConfig, &*m_devices[i]));
				break;
			}
			}
		}


		ma_data_converter_config cfg = ma_data_converter_config_init(ma_format_f32, buffer_format, channels, channels, sample_rate, sample_rate);
		m_Converter = std::make_unique<ma_data_converter>();
		CheckIfError(ma_data_converter_init(&cfg, nullptr, &*m_Converter));

		for (auto& device : m_devices) {
			if (device) {
				CheckIfError(ma_device_start(&*device));
			}
		}
		thread_shutdown.store(false);
		m_ProcessingThread = std::thread(CAudioRecorder::ProcessingThread, this);
		g_Recording = true;
	}

	void stop() {
		if (!g_Recording && m_devices.empty()) return;
		g_Recording = false;
		for (auto& capturer : m_app_capturers) {
			if (capturer) capturer->Stop();
		}
		m_app_capturers.clear();

		if (m_Converter) {
			ma_data_converter_uninit(&*m_Converter, nullptr);
			m_Converter.reset();
		}
		for (auto& device : m_devices) {
			if (device) ma_device_uninit(&*device);
		}
		m_devices.clear();

		thread_shutdown.store(true);
		for (auto& source : g_RecordingSources) {
			source->condition.notify_all();
		}
		if (m_ProcessingThread.joinable()) {
			m_ProcessingThread.join();
		}

		for (auto& encoder : m_encoders) {
			if (encoder) ma_encoder_uninit(&*encoder);
		}
		m_encoders.clear();

		for (auto& source : g_RecordingSources) {
			std::lock_guard<std::mutex> lock(source->mutex);
			while (!source->queue.empty()) {
				delete[] source->queue.front().buffer;
				source->queue.pop_front();
			}
		}
		g_RecordingSources.clear();
		g_CallbackUserData.clear();
		g_Running = !g_CommandLineOptions.exitAfterStop;
	}

	void pause() {
		paused.store(true);
		for (auto& source : g_RecordingSources) {
			std::lock_guard<std::mutex> lock(source->mutex);
			while (!source->queue.empty()) {
				delete[] source->queue.front().buffer;
				source->queue.pop_front();
			}
		}
	}

	void resume() {
		paused.store(false);
	}
};

#define g_AudioRecorder CSingleton<CAudioRecorder>::GetInstance()


std::vector<audio_device> MA_API get_input_audio_devices()
{
	std::vector<audio_device> audioDevices;
	ma_device_info* pCaptureDeviceInfos;
	ma_uint32 captureDeviceCount;
	ma_uint32 iCaptureDevice;

	CheckIfError(ma_context_get_devices(CSingleton<CAudioContext>::GetInstance(), nullptr, nullptr, &pCaptureDeviceInfos, &captureDeviceCount));
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

	CheckIfError(ma_context_get_devices(CSingleton<CAudioContext>::GetInstance(), &pPlaybackDeviceInfos, &playbackDeviceCount, nullptr, nullptr));
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

	explicit IWindow(HWND parent = window) : m_Parent(parent) {
		this->reset();
		g_Windows.push_back(this);
		this->id = g_Windows.size() - 1;
	}
	~IWindow() {
		this->reset();
	}

	inline operator HWND() {
		return m_Parent;
	}
	virtual void build() {}



};

static void window_reset() {
	for (IWindow* w : g_Windows) {
		safeCall(w, &IWindow::reset);
	}
	g_Windows.clear();
}

static const std::wstring version = L"0.0.1 Beta";


static std::vector<audio_device> g_InAudioDevices;
static std::vector<audio_device> g_OutAudioDevices;


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


class CMainWindow final : public virtual IWindow {
public:
	HWND record_start = nullptr;
	HWND sources_list_text = nullptr;
	HWND sources_list = nullptr;
	HWND add_source_btn = nullptr;
	HWND remove_source_btn = nullptr;
	HWND record_manager = nullptr;
	HWND settings_button = nullptr;

	void build() override {
		this->reset();
		std::wstring wsHkStartStop; CStringUtils::UnicodeConvert(hotkey_start_stop, wsHkStartStop);

		record_start = create_button(window, std::wstring(L"Start recording (" + wsHkStartStop + L")").c_str(), 10, 10, 200, 50, 0);
		push(record_start);

		sources_list_text = create_text(window, L"Recording Sources:", 10, 70, 200, 20, 0);
		push(sources_list_text);

		sources_list = create_list(window, 10, 90, 300, 150, 0);
		push(sources_list);
		update_sources_list();

		add_source_btn = create_button(window, L"Add Source...", 10, 250, 145, 30, 0);
		push(add_source_btn);
		remove_source_btn = create_button(window, L"Remove Selected", 165, 250, 145, 30, 0);
		push(remove_source_btn);

		record_manager = create_button(window, L"Recordings manager", 10, 450, 200, 50, 0);
		push(record_manager);
		settings_button = create_button(window, L"Settings", 10, 450 + 50 + 10, 200, 50, 0);
		push(settings_button);

		focus(record_start);
	}

	void update_sources_list() {
		if (!sources_list) return;
		clear_list(sources_list);
		for (const auto& source : g_ConfiguredSources) {
			std::wstring type_str;
			switch (source.type) {
			case DEVICE_CAPTURE: type_str = L"[Mic]"; break;
			case DEVICE_LOOPBACK: type_str = L"[Loopback]"; break;
			case APP_LOOPBACK: type_str = L"[App]"; break;
			}
			std::wstring display_text = source.custom_name + L" " + type_str;
			add_list_item(sources_list, display_text.c_str());
		}
	}
};


class CRecordManagerWindow final : public virtual IWindow {
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
		play_button = create_button(window, L"Play", 10, 10, 10, 10, 0);
		push(play_button);
		pause_button = create_button(window, L"&Pause", 10, 10, 10, 10, 0);
		push(pause_button);
		stop_button = create_button(window, L"Stop", 10, 10, 10, 10, 0);
		push(stop_button);
		delete_button = create_button(window, L"Delete", 10, 10, 10, 10, 0);
		push(delete_button);
		close_button = create_button(window, L"Close", 10, 10, 10, 10, 0);
		push(close_button);
		std::vector<std::wstring> files = get_files(record_path.generic_wstring());
		for (unsigned int i = 0; i < files.size(); i++) {
			add_list_item(items_view_list, files[i].c_str());
		}

		focus(items_view_list);

	}



};


class CRecordingWindow final : public virtual IWindow {
public:
	HWND record_stop = nullptr;
	HWND record_pause = nullptr;
	HWND record_restart = nullptr;
	HWND hide = nullptr;

	void build()override {
		std::wstring wsHkStartStop; CStringUtils::UnicodeConvert(hotkey_start_stop, wsHkStartStop);
		std::wstring wsHkPauseResume; CStringUtils::UnicodeConvert(hotkey_pause_resume, wsHkPauseResume);
		std::wstring wsHkRestart; CStringUtils::UnicodeConvert(hotkey_restart, wsHkRestart);
		std::wstring wsHkHideShow; CStringUtils::UnicodeConvert(hotkey_hide_show, wsHkHideShow);

		record_stop = create_button(window, std::wstring(L"Stop recording (" + wsHkStartStop + L")").c_str(), 10, 10, 100, 30, 0);
		push(record_stop);
		record_pause = create_button(window, std::wstring(L"Pause recording (" + wsHkPauseResume + L")").c_str(), 120, 10, 100, 30, 0);
		push(record_pause);
		record_restart = create_button(window, std::wstring(L"Restart recording (" + wsHkRestart + L")").c_str(), 230, 10, 100, 30, 0);
		push(record_restart);
		hide = create_button(window, std::wstring(L"Hide window (" + wsHkHideShow + L")").c_str(), 250, 10, 100, 30, 0);
		push(hide);

		focus(record_stop);

	}







};


class CSettingsWindow final : public virtual IWindow {
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
	HWND lblHotkeyHideShow, editHotkeyHideShow;

	HWND lblCurrentPreset, listCurrentPreset;

	HWND btnSaveSettings;
	HWND btnCancelSettings;

	// Temp storage for hotkey parsing
	std::string new_hotkey_start_stop, new_hotkey_pause_resume, new_hotkey_restart, new_hotkey_hide_show;


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
		std::wstring wsRecordPath = record_path.generic_wstring();
		editRecordPath = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editRecordPath);
		set_text(editRecordPath, wsRecordPath.c_str());
		btnBrowseRecordPath = create_button(window, L"Browse...", x_button_browse, y_pos, 30, ctrl_height, 0); push(btnBrowseRecordPath);
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
		constexpr const char* formats[] = { "u8", "s16", "s24", "s32", "f32" };
		for (const char* fmt_str : formats) {
			std::wstring wfmt_str; CStringUtils::UnicodeConvert(std::string(fmt_str), wfmt_str);
			add_list_item(listBufferFormat, wfmt_str.c_str());
		}
		std::wstring wsBufferFormat;
		std::string s_buffer_format;
		ma_format_convert(buffer_format, s_buffer_format);
		CStringUtils::UnicodeConvert(s_buffer_format, wsBufferFormat);
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
		y_pos += ctrl_height + y_spacing;

		push(create_text(window, L"Hide/Show Hotkey:", x_label, y_pos, label_width, ctrl_height, 0));
		std::wstring wsHkHideShow; CStringUtils::UnicodeConvert(hotkey_hide_show, wsHkHideShow);
		editHotkeyHideShow = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editHotkeyHideShow);
		set_text(editHotkeyHideShow, wsHkHideShow.c_str());
		y_pos += ctrl_height + section_spacing;


		// --- Preset Settings ---
		push(create_text(window, L"Current FFmpeg Preset:", x_label, y_pos, label_width, ctrl_height, 0));
		listCurrentPreset = create_list(window, x_control, y_pos, control_width, list_height, 0); push(listCurrentPreset);
		for (const auto& p : g_Presets) {
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
		ws_val = get_text(editHotkeyHideShow); CStringUtils::UnicodeConvert(ws_val, new_hotkey_hide_show);
		if (!parse_hotkey(new_hotkey_hide_show, kmod_dummy, kcode_dummy)) { alert(L"Validation Error", L"Invalid Hide/Show Hotkey format.", MB_ICONERROR); focus(editHotkeyHideShow); return false; }


		ws_val = get_text(editRecordPath);
		if (ws_val.empty()) { alert(L"Validation Error", L"Record path cannot be empty.", MB_ICONERROR); focus(editRecordPath); return false; }

		// Audio Format vs Make Stems
		ws_val = get_text(editAudioFormat);
		if (ws_val.empty()) { alert(L"Validation Error", L"Audio format cannot be empty.", MB_ICONERROR); focus(editAudioFormat); return false; }
		if (is_checked(chkMakeStems) && ws_val != L"wav") {
			alert(L"Validation Error", L"'Make Stems' is only supported with 'wav' audio format, but you selected '" + ws_val + L"'. Please change audio format to 'wav' or disable 'Make Stems'.", MB_ICONERROR);
			return false;
		}


		return true;
	}


	void apply() {
		std::wstring ws_val; std::string s_val;

		ws_val = get_text(editSampleRate); CStringUtils::UnicodeConvert(ws_val, s_val);
		sample_rate = std::clamp(static_cast<ma_uint32>(std::stoul(s_val)), (ma_uint32)8000, (ma_uint32)384000);
		conf.write("General", "sample-rate", std::to_string(sample_rate));

		channels = (get_list_position(listChannels) == 0) ? 1 : 2;
		conf.write("General", "channels", std::to_string(channels));

		ws_val = get_text(editBufferSize); CStringUtils::UnicodeConvert(ws_val, s_val);
		buffer_size = std::clamp(static_cast<ma_uint32>(std::stoul(s_val)), (ma_uint32)0, (ma_uint32)3000);
		conf.write("General", "buffer-size", std::to_string(buffer_size));

		ws_val = get_text(editFilenameSignature); CStringUtils::UnicodeConvert(ws_val, filename_signature);
		conf.write("General", "filename-signature", filename_signature);
		std::string path;
		ws_val = get_text(editRecordPath); CStringUtils::UnicodeConvert(ws_val, path);
		record_path = path;
		conf.write("General", "record-path", record_path.generic_string());

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
		ma_format_convert(s_buffer_format_new.c_str(), buffer_format);
		conf.write("General", "sample-format", s_buffer_format_new);

		UnregisterHotKey(nullptr, HOTKEY_STARTSTOP);
		UnregisterHotKey(nullptr, HOTKEY_PAUSERESUME);
		UnregisterHotKey(nullptr, HOTKEY_RESTART);
		UnregisterHotKey(nullptr, HOTKEY_HIDESHOW);

		hotkey_start_stop = new_hotkey_start_stop;
		conf.write("General", "hotkey-start-stop", hotkey_start_stop);
		hotkey_pause_resume = new_hotkey_pause_resume;
		conf.write("General", "hotkey-pause-resume", hotkey_pause_resume);
		hotkey_restart = new_hotkey_restart;
		conf.write("General", "hotkey-restart", hotkey_restart);
		hotkey_hide_show = new_hotkey_hide_show;
		conf.write("General", "hotkey-hide-show", hotkey_hide_show);

		DWORD kmod; int kcode;
		if (parse_hotkey(hotkey_start_stop, kmod, kcode)) RegisterHotKey(nullptr, HOTKEY_STARTSTOP, kmod, kcode);
		if (parse_hotkey(hotkey_pause_resume, kmod, kcode)) RegisterHotKey(nullptr, HOTKEY_PAUSERESUME, kmod, kcode);
		if (parse_hotkey(hotkey_restart, kmod, kcode)) RegisterHotKey(nullptr, HOTKEY_RESTART, kmod, kcode);
		if (parse_hotkey(hotkey_hide_show, kmod, kcode)) RegisterHotKey(nullptr, HOTKEY_HIDESHOW, kmod, kcode);

		int preset_idx = get_list_position(listCurrentPreset);
		std::wstring ws_preset_name_new = get_list_item_text_by_index_internal(listCurrentPreset, preset_idx);
		CStringUtils::UnicodeConvert(ws_preset_name_new, current_preset_name);
		conf.write("General", "current-preset", current_preset_name);
		for (const auto& p : g_Presets) {
			if (p.name == current_preset_name) {
				g_CurrentPreset = p;
				break;
			}
		}
		// If the user has not changed the audio format, but has changed the preset.
		std::string command = g_CurrentPreset.command;
		size_t pos = command.find_last_of('.');
		command.erase(0, pos + 1);
		if (command != "%f" && audio_format != command) {
			audio_format = command;
			conf.write("General", "audio-format", audio_format);
		}
		CSingleton<CSoundStream>::GetInstance().Reinitialize(); // After a sample rate change
		conf.save();
	}
};


class CAddSourceWindow final : public virtual IWindow {
public:
	HWND lblName, editName;
	HWND lblDevice, listDevices;
	HWND btnOK, btnCancel;

	struct DiscoverableSource {
		std::wstring display_name;
		SourceType type;
		audio_device device;
		application app;
	};
	std::vector<DiscoverableSource> available_sources;


	void build() override {
		this->reset();
		available_sources.clear();

		int x_label = 10, x_control = 120;
		int y_pos = 10;
		int ctrl_height = 25, list_height = 200, label_width = 100, control_width = 300;
		int y_spacing = 10;

		lblName = create_text(window, L"Source Name:", x_label, y_pos, label_width, ctrl_height, 0); push(lblName);
		editName = create_input_box(window, false, false, x_control, y_pos, control_width, ctrl_height, 0); push(editName);
		y_pos += ctrl_height + y_spacing;

		lblDevice = create_text(window, L"Audio Source:", x_label, y_pos, label_width, ctrl_height, 0); push(lblDevice);
		listDevices = create_list(window, x_control, y_pos, control_width, list_height, 0);
		push(listDevices);
		y_pos += list_height + y_spacing * 2;

		btnOK = create_button(window, L"OK", x_control, y_pos, 80, 30, 0); push(btnOK);
		btnCancel = create_button(window, L"Cancel", x_control + 90, y_pos, 80, 30, 0); push(btnCancel);

		g_InAudioDevices = get_input_audio_devices();
		for (const auto& dev : g_InAudioDevices) {
			if (dev.name == L"Default" || dev.name == L"Not used") continue;
			DiscoverableSource src;
			src.display_name = dev.name + L" [Mic]";
			src.type = DEVICE_CAPTURE;
			src.device = dev;
			available_sources.push_back(src);
			add_list_item(listDevices, src.display_name.c_str());
		}

		g_OutAudioDevices = get_output_audio_devices();
		for (const auto& dev : g_OutAudioDevices) {
			if (dev.name == L"Not used") continue;
			DiscoverableSource src;
			src.display_name = dev.name + L" [Loopback]";
			src.type = DEVICE_LOOPBACK;
			src.device = dev;
			available_sources.push_back(src);
			add_list_item(listDevices, src.display_name.c_str());
		}

		auto apps = get_tasklist();
		for (const auto& app : apps) {
			if (app.id == 0) continue;
			DiscoverableSource src;
			src.display_name = app.name + L" (PID: " + std::to_wstring(app.id) + L") [App]";
			src.type = APP_LOOPBACK;
			src.app = app;
			available_sources.push_back(src);
			add_list_item(listDevices, src.display_name.c_str());
		}


		focus(editName);
	}
};



static CAddSourceWindow g_AddSourceWindow;
static bool g_AddingSourceMode = false;

static CMainWindow g_MainWindow;
static CRecordManagerWindow g_RecordManagerWindow;
static CRecordingWindow g_RecordingWindow;
static CSettingsWindow g_SettingsWindow;


bool g_RecordingsManager = false;


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, wchar_t* lpCmdLine, ma_int32       nShowCmd) {
	std::vector<std::string> options, values;
	if (g_CommandLineOptions.ParseOptions(lpCmdLine, options, values)) {
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
	options.clear();
	values.clear();
	g_Running = true; // Starting application
	SetUnhandledExceptionFilter(ExceptionHandler);
	timeBeginPeriod(1);
	DWORD kmod = -1;
	int kcode = -1;
	int result = conf.load();
	if (result == 0) {
		try {
			std::string srate = conf.read("General", "sample-rate");
			sample_rate = std::clamp(static_cast<ma_uint32>(std::stoi(srate)), (ma_uint32)8000, (ma_uint32)384000);
			std::string chann = conf.read("General", "channels");
			channels = std::clamp(static_cast<ma_uint32>(std::stoi(chann)), (ma_uint32)1, (ma_uint32)2);
			std::string bs = conf.read("General", "buffer-size");
			buffer_size = std::clamp(static_cast<ma_uint32>(std::stoi(bs)), (ma_uint32)0, (ma_uint32)3000);
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

			std::string sevents = conf.read("General", "sound-events");
			auto result = str_to_bool(sevents);
			if (!result.has_value()) {
				throw std::exception("Failed to parse bool. Either 'true' or 'false' can be used in uppercase or lowercase. Integers are also allowed (0 is false, 1 is true)");
			}
			sound_events = *result;
			std::string mstems = conf.read("General", "make-stems");
			result = str_to_bool(mstems);
			if (!result.has_value()) {
				throw std::exception("Failed to parse bool. Either 'true' or 'false' can be used in uppercase or lowercase. Integers are also allowed (0 is false, 1 is true)");
			}

			if (make_stems && audio_format != "wav") {
				throw std::exception("Can't make stems when using another format, built-in not supported by FPRecorder");
			}
			std::string sformat = CStringUtils::ToLowerCase(conf.read("General", "sample-format"));
			bool parse_result = ma_format_convert(sformat.c_str(), buffer_format);
			if (parse_result == false) {
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
			if (parse_hotkey(hotkey_hide_show, kmod, kcode) == false) {
				throw std::exception("Invalid hotkey");
			}
			RegisterHotKey(nullptr, HOTKEY_HIDESHOW, kmod, kcode);

			std::vector<std::string> presets_str = conf.get_keys("Presets");
			if (presets_str.size() > 0) {
				g_Presets.clear();
				for (unsigned int i = 0; i < presets_str.size(); ++i) {
					preset p;
					p.name = presets_str[i];
					p.command = conf.read("Presets", presets_str[i]);
					g_Presets.push_back(p);
				}
			}
			current_preset_name = conf.read("General", "current-preset");
			bool preset_found = false;
			unsigned int it;
			for (it = 0; it < g_Presets.size(); ++it) {
				if (current_preset_name == g_Presets[it].name) {
					preset_found = true;
					break;
				}
			}
			if (!preset_found) {
				throw std::exception("Invalid preset name");
			}
			g_CurrentPreset.name = g_Presets[it].name;
			g_CurrentPreset.command = g_Presets[it].command;
			// If the user has not changed the audio format, but has changed the preset.
			std::string command = g_CurrentPreset.command;
			size_t pos = command.find_last_of('.');
			command.erase(0, pos + 1);
			if (command != "%f" && audio_format != command) {
				audio_format = command;
				conf.write("General", "audio-format", audio_format);
			}

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
		conf.write("General", "record-path", record_path.generic_string());
		conf.write("General", "audio-format", audio_format);
		conf.write("General", "sound-events", std::to_string(sound_events));
		conf.write("General", "make-stems", std::to_string(make_stems));
		std::string s_format;
		ma_format_convert(buffer_format, s_format);
		conf.write("General", "sample-format", s_format);
		conf.write("General", "hotkey-start-stop", hotkey_start_stop);
		conf.write("General", "hotkey-pause-resume", hotkey_pause_resume);
		conf.write("General", "hotkey-restart", hotkey_restart);
		conf.write("General", "hotkey-hide-show", hotkey_hide_show);
		for (const preset& p : g_Presets) {
			conf.write("Presets", p.name, p.command);
		}
		conf.write("General", "current-preset", current_preset_name);
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
		hotkey_hide_show = conf.read("General", "hotkey-hide-show");
		if (parse_hotkey(hotkey_hide_show, kmod, kcode) == false) {
			throw std::exception("Invalid hotkey");
		}
		RegisterHotKey(nullptr, HOTKEY_HIDESHOW, kmod, kcode);

		g_CurrentPreset = g_Presets[0];
	}
	try {
		window = show_window(L"FPRecorder " + version + (IsUserAnAdmin() ? L" (Administrator)" : L""));
		g_MainWindow.build();
		key_pressed(VK_SPACE) || key_pressed(VK_RETURN); // Avoid click to start recording
		while (g_Running && window ? true : false) {
			wait(5);
			update_window(window);

			// Avoid invalid hotkey presses not in recording mode.
			if (!g_Recording || g_SettingsMode) {
				hotkey_pressed(HOTKEY_PAUSERESUME);
				hotkey_pressed(HOTKEY_RESTART);
				hotkey_pressed(HOTKEY_HIDESHOW);
			}
			if (g_RecordingsManager || g_SettingsMode) {
				hotkey_pressed(HOTKEY_STARTSTOP);
			}
			if (gui::try_close) {
				gui::try_close = false;
				if (g_Recording) {
					if (sound_events)g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
					g_SpeechProvider.Speak("Unable to exit, while recording.");
					continue;
				}
				g_Retcode = 0;
				g_Running = false;
				break;
			}
			if (!g_Recording && !g_RecordingsManager && !g_SettingsMode && is_pressed(g_MainWindow.settings_button)) {
				g_MainWindow.reset();
				g_SettingsWindow.build();
				g_SettingsMode = true;
				g_SpeechProvider.Speak("Settings", false);
				key_released(VK_RETURN);
			}

			if (g_SettingsMode) {
				if (is_pressed(g_SettingsWindow.btnCancelSettings) || key_down(VK_ESCAPE)) {
					g_SettingsWindow.reset();
					g_MainWindow.build();
					focus(g_MainWindow.settings_button);
					g_SettingsMode = false;
					g_SpeechProvider.Speak("Canceled.", false);
				}
				else if (is_pressed(g_SettingsWindow.btnSaveSettings) || key_pressed(VK_RETURN)) {
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
				g_MainWindow.reset();
				g_RecordManagerWindow.build();
				if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_RECORD_MANAGER);
				g_RecordingsManager = true;
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
				std::vector<wstring> files = get_files(record_path.generic_wstring());
				if (files.size() == 0) {
					if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
					g_SpeechProvider.Speak("There are no files in \"" + record_path.generic_string() + "\".");
					g_RecordManagerWindow.reset();
					g_MainWindow.build();
					focus(g_MainWindow.record_manager);
					g_RecordingsManager = false;
				}
				if ((key_pressed(VK_SPACE) && get_current_focus() == g_RecordManagerWindow.items_view_list) || is_pressed(g_RecordManagerWindow.play_button)) {
					std::filesystem::path file = record_path / get_focused_list_item_name(g_RecordManagerWindow.items_view_list);
					g_SoundStream.Play(file.generic_string());
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
						std::filesystem::path file = record_path / get_focused_list_item_name(g_RecordManagerWindow.items_view_list);
						g_SoundStream.Close();
						std::filesystem::remove(file);
						g_RecordManagerWindow.reset();
						g_RecordManagerWindow.build();
					}
				}
			}


			if (!g_Recording && !g_RecordingsManager && !g_SettingsMode && !g_AddingSourceMode) {
				if (is_pressed(g_MainWindow.add_source_btn)) {
					g_AddSourceWindow.build();
					g_AddingSourceMode = true;
				}

				if (is_pressed(g_MainWindow.remove_source_btn)) {
					int selection = get_list_position(g_MainWindow.sources_list);
					if (selection >= 0 && static_cast<size_t>(selection) < g_ConfiguredSources.size()) {
						g_ConfiguredSources.erase(g_ConfiguredSources.begin() + selection);
						g_MainWindow.update_sources_list();
					}
				}

				if (is_pressed(g_MainWindow.record_start) || hotkey_pressed(HOTKEY_STARTSTOP) || g_CommandLineOptions.start) {
					if (!g_ConfiguredSources.empty()) {
						if (sound_events) g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_START_RECORDING);

						g_MainWindow.reset();
						g_RecordingWindow.build();
						g_AudioRecorder.start(g_ConfiguredSources);
						g_RecordingPaused = false;
					}
					else {
						g_SpeechProvider.Speak("Please add at least one audio source before recording.", true);
					}
				}
			}

			if (g_AddingSourceMode) {
				if (is_pressed(g_AddSourceWindow.btnCancel) || key_down(VK_ESCAPE)) {
					g_AddSourceWindow.reset();
					focus(g_MainWindow.add_source_btn);
					g_AddingSourceMode = false;
				}
				else if (is_pressed(g_AddSourceWindow.btnOK)) {
					std::wstring name = get_text(g_AddSourceWindow.editName);
					int selection_idx = get_list_position(g_AddSourceWindow.listDevices);

					if (name.empty()) {
						alert(L"Input Error", L"Please provide a name for the source.", MB_ICONWARNING);
						focus(g_AddSourceWindow.editName);
					}
					else if (selection_idx < 0) {
						alert(L"Input Error", L"Please select a valid audio source.", MB_ICONWARNING);
						focus(g_AddSourceWindow.listDevices);
					}
					else {
						const auto& selected_source = g_AddSourceWindow.available_sources[selection_idx];
						ConfiguredSource new_source;
						new_source.custom_name = name;
						new_source.type = selected_source.type;
						new_source.device = selected_source.device;
						new_source.app.id = selected_source.app.id;
						new_source.app.name = selected_source.app.name;

						g_ConfiguredSources.push_back(new_source);

						g_MainWindow.update_sources_list();
						g_AddSourceWindow.reset();
						focus(g_MainWindow.add_source_btn);
						g_AddingSourceMode = false;
					}
				}
			}

			if (g_Recording && (is_pressed(g_RecordingWindow.record_stop) || hotkey_pressed(HOTKEY_STARTSTOP))) {
				g_AudioRecorder.stop();
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
					std::vector<std::string> split = CStringUtils::Split(".wav", g_AudioRecorder.base_filename);
					g_SpeechProvider.Speak("Converting...");
					std::string cmd = g_CurrentPreset.command;
					CStringUtils::Replace(cmd, "%I", "\"" + g_AudioRecorder.base_filename + ".wav\"", true);
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
						CStringUtils::UnicodeConvert(g_AudioRecorder.base_filename + ".wav", recording_name_u);
						DeleteFile(recording_name_u.c_str());
					}
				}
				g_MainWindow.build();

			}
			if (g_Recording && (is_pressed(g_RecordingWindow.record_pause) || hotkey_pressed(HOTKEY_PAUSERESUME))) {
				std::wstring wsHkPauseResume; CStringUtils::UnicodeConvert(hotkey_pause_resume, wsHkPauseResume);
				if (!g_RecordingPaused) {
					g_AudioRecorder.pause();
					if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_PAUSE_RECORDING);
					g_RecordingPaused = true;
					set_text(g_RecordingWindow.record_pause, std::wstring(L"Resume recording (" + wsHkPauseResume + L")").c_str());
				}
				else if (g_RecordingPaused) {
					if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_RESUME_RECORDING);
					g_AudioRecorder.resume();
					g_RecordingPaused = false;
					set_text(g_RecordingWindow.record_pause, std::wstring(L"Pause recording (" + wsHkPauseResume + L")").c_str());
				}
				wait(20);
			}
			if (g_Recording && (is_pressed(g_RecordingWindow.record_restart) || hotkey_pressed(HOTKEY_RESTART))) {
				wait(10);
				std::wstring recording_name_u;
				CStringUtils::UnicodeConvert(g_AudioRecorder.base_filename, recording_name_u);
				int result = alert(L"FPWarning", L"Are you sure you want to delete the recording \"" + recording_name_u + L"\" and rerecord it to new one? Old record can no longer be restored.", MB_YESNO | MB_ICONEXCLAMATION);
				if (result == IDNO)continue;
				else if (result == IDYES)
				{
					g_AudioRecorder.stop();
				}
				g_AudioRecorder.start(g_ConfiguredSources);
				g_Recording = true;
				g_RecordingPaused = false;
				window_reset();
				g_RecordingWindow.build();
				DeleteFile(recording_name_u.c_str());
				if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_RESTART_RECORDING);

			}
			if (g_Recording && (is_pressed(g_RecordingWindow.hide) || hotkey_pressed(HOTKEY_HIDESHOW))) {
				if (IsWindowVisible(window)) {
					hide_window(window);
				}
				else {
					show_window(L""); // Actually, it shouldn't be like that, I mean the window name will remain the same when hiding and showing and I didn't fix this bug in the GUI. But for now we'll use the bug.
					SetForegroundWindow(window);
				}
			}
		}
	}
	catch (const std::exception& ex) {
		std::wstring exception_u;
		std::stringstream ss;
		ss << ex.what();
		ss << std::stacktrace::current();
		CStringUtils::UnicodeConvert(ss.str(), exception_u);
		if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
		alert(L"FPRuntimeError", exception_u.c_str(), MB_ICONERROR);
		g_Running = false;
		g_Retcode = -100;
	}
	g_Running = false;
	if (!g_InAudioDevices.empty()) {
		g_InAudioDevices.clear();
	}
	if (!g_OutAudioDevices.empty()) {
		g_OutAudioDevices.clear();
	}
	window_reset();
	UnregisterHotKey(nullptr, HOTKEY_STARTSTOP);
	UnregisterHotKey(nullptr, HOTKEY_PAUSERESUME);
	UnregisterHotKey(nullptr, HOTKEY_RESTART);
	UnregisterHotKey(nullptr, HOTKEY_HIDESHOW);
	hide_window(window);
	timeEndPeriod(1);
	(void)hInstance;
	(void)hPrevInstance;
	g_MaLastError = MA_SUCCESS;
	return g_Retcode; // Application exit
}

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
	std::stringstream ss;
	std::stacktrace st = std::stacktrace::current();
	ss << st;
	std::wstring str_u;
	CStringUtils::UnicodeConvert(ss.str(), str_u);
	if (sound_events)		g_SoundStream.PlayEvent(CSoundStream::SOUND_EVENT_ERROR);
	alert(L"FPRuntimeError", str_u, MB_ICONERROR);
	g_Retcode = -100;
	g_Running = false; // In any situation, user should nott lost the record. Give application call audio recorder destructor to try uninitialize the encoder
	return 0;
}



