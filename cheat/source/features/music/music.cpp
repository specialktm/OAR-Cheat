#include "pch/pch.h"
#include "features/music/music.h"
#include "logger/logger.hpp"
#include "util/files.hpp"

#include <windows.h>
#include <shlobj.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <xaudio2.h>

#include <atomic>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <cwctype>

namespace
{
	std::atomic<bool> g_finished{ false };
	std::atomic<bool> g_decoding{ false };
	struct decode_guard {
		decode_guard() { g_decoding.store(true); }
		~decode_guard() { g_decoding.store(false); }
	};

	struct voice_callback final : public IXAudio2VoiceCallback
	{
		void OnVoiceProcessingPassStart(UINT32) override {}
		void OnVoiceProcessingPassEnd() override {}
		void OnStreamEnd() override {}
		void OnBufferStart(void*) override {}
		void OnBufferEnd(void*) override { g_finished = true; }
		void OnLoopEnd(void*) override {}
		void OnVoiceError(void*, HRESULT) override {}
	};

	voice_callback g_voice_callback;

	std::string wide_to_utf8(const std::wstring& wide)
	{
		if (wide.empty())
		{
			return {};
		}
		const int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (len <= 1)
		{
			return {};
		}
		std::string out(static_cast<std::size_t>(len - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, out.data(), len, nullptr, nullptr);
		return out;
	}

	std::wstring utf8_to_wide(const std::string& utf8)
	{
		if (utf8.empty())
		{
			return {};
		}
		const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
		if (len <= 1)
		{
			return {};
		}
		std::wstring out(static_cast<std::size_t>(len - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, out.data(), len);
		return out;
	}

	std::string format_time(std::uint64_t seconds)
	{
		char buffer[32] = {};
		if (seconds >= 3600)
		{
			std::snprintf(buffer, sizeof(buffer), "%llu:%02llu:%02llu",
				static_cast<unsigned long long>(seconds / 3600),
				static_cast<unsigned long long>((seconds % 3600) / 60),
				static_cast<unsigned long long>(seconds % 60));
		}
		else
		{
			std::snprintf(buffer, sizeof(buffer), "%llu:%02llu",
				static_cast<unsigned long long>(seconds / 60),
				static_cast<unsigned long long>(seconds % 60));
		}
		return buffer;
	}
}

namespace music
{
	struct player
	{
		std::mutex mtx;

		state_t state = state_t::idle;
		std::string current_path;
		std::string current_name;
		std::string error;
		std::uint64_t generation = 0;

		std::vector<BYTE> pcm;
		WAVEFORMATEX format = {};
		UINT32 total_samples = 0;
		UINT32 seek_base = 0;
		bool loop = false;
		float volume = 1.0f;

		IXAudio2* engine = nullptr;
		IXAudio2MasteringVoice* mastering = nullptr;
		IXAudio2SourceVoice* voice = nullptr;
		bool audio_ready = false;
	};

	static player g_player;

	namespace detail
	{
		bool decode_to_pcm(const std::string& path, std::vector<BYTE>& pcm, WAVEFORMATEX& format, std::string& error)
		{
			IMFByteStream* byte_stream = nullptr;
			IMFSourceReader* reader = nullptr;
			IMFMediaType* partial_type = nullptr;

			auto cleanup = [&]()
			{
				if (partial_type) partial_type->Release();
				if (reader) reader->Release();
				if (byte_stream) byte_stream->Release();
			};

			const std::wstring wide_path = utf8_to_wide(path);
			if (wide_path.empty() ||
				FAILED(MFCreateFile(MF_ACCESSMODE_READ, MF_OPENMODE_FAIL_IF_NOT_EXIST, MF_FILEFLAGS_NONE, wide_path.c_str(), &byte_stream)))
			{
				error = "failed to open file";
				cleanup();
				return false;
			}

			if (FAILED(MFCreateSourceReaderFromByteStream(byte_stream, nullptr, &reader)))
			{
				error = "unsupported file format";
				cleanup();
				return false;
			}

			bool format_ok = false;
			if (SUCCEEDED(MFCreateMediaType(&partial_type)))
			{
				partial_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
				partial_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
				format_ok = SUCCEEDED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, partial_type));
			}
			if (!format_ok)
			{
				error = "no supported audio stream";
				cleanup();
				return false;
			}

			IMFMediaType* current_type = nullptr;
			if (FAILED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &current_type)))
			{
				error = "no supported audio stream";
				cleanup();
				return false;
			}

			UINT32 rate = 0;
			UINT32 channels = 0;
			UINT32 bits = 0;
			current_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
			current_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
			current_type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
			current_type->Release();

			if (rate == 0 || channels == 0 || (bits != 16 && bits != 32))
			{
				error = "unsupported audio format";
				cleanup();
				return false;
			}

			format.wFormatTag = (bits == 32) ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
			format.nChannels = static_cast<WORD>(channels);
			format.nSamplesPerSec = rate;
			format.wBitsPerSample = static_cast<WORD>(bits);
			format.nBlockAlign = static_cast<WORD>(channels * (bits / 8));
			format.nAvgBytesPerSec = rate * format.nBlockAlign;
			format.cbSize = 0;

			pcm.clear();
			pcm.reserve(16 * 1024 * 1024);

			constexpr std::size_t max_bytes = 256ull * 1024 * 1024;

			while (true)
			{
				DWORD flags = 0;
				IMFSample* sample = nullptr;
				const HRESULT hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
				if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
				{
					if (sample) sample->Release();
					break;
				}

				if (sample)
				{
					IMFMediaBuffer* media_buffer = nullptr;
					if (SUCCEEDED(sample->ConvertToContiguousBuffer(&media_buffer)))
					{
						BYTE* data = nullptr;
						DWORD length = 0;
						if (SUCCEEDED(media_buffer->Lock(&data, nullptr, &length)) && length > 0)
						{
							pcm.insert(pcm.end(), data, data + length);
							media_buffer->Unlock();
						}
						media_buffer->Release();
					}
					sample->Release();
				}

				if (pcm.size() > max_bytes)
				{
					error = "file is too large";
					cleanup();
					return false;
				}
			}

			const std::size_t block = format.nBlockAlign;
			const std::size_t remainder = pcm.size() % block;
			if (remainder)
			{
				pcm.resize(pcm.size() - remainder);
			}

			if (pcm.empty())
			{
				error = "no audio data in file";
				cleanup();
				return false;
			}

			// 45 minute cap keeps the in-memory buffer sane.
			if (pcm.size() / block > static_cast<std::size_t>(rate) * 45ull * 60ull)
			{
				error = "file is too long";
				cleanup();
				return false;
			}

			cleanup();
			return true;
		}

		void teardown_voice(player& p)
		{
			if (p.voice)
			{
				p.voice->Stop(0);
				p.voice->FlushSourceBuffers();
				p.voice->DestroyVoice();
				p.voice = nullptr;
			}
			p.pcm.clear();
			p.pcm.shrink_to_fit();
			p.total_samples = 0;
			p.seek_base = 0;
		}

		void ensure_audio(player& p)
		{
			if (p.audio_ready)
			{
				return;
			}

			if (SUCCEEDED(XAudio2Create(&p.engine, 0, XAUDIO2_DEFAULT_PROCESSOR)))
			{
				if (SUCCEEDED(p.engine->CreateMasteringVoice(&p.mastering)))
				{
					p.audio_ready = true;
				}
				else
				{
					p.engine->Release();
					p.engine = nullptr;
				}
			}
		}

		void submit_and_start(player& p, UINT32 start_sample)
		{
			if (!p.audio_ready || p.pcm.empty() || p.format.nBlockAlign == 0)
			{
				return;
			}

			const UINT32 total = p.total_samples;
			if (total == 0)
			{
				return;
			}
			if (start_sample >= total)
			{
				start_sample = total - 1;
			}

			HRESULT hr = p.engine->CreateSourceVoice(&p.voice, &p.format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &g_voice_callback, nullptr, 0);
			if (FAILED(hr))
			{
				return;
			}

			p.seek_base = start_sample;

			const BYTE* data = p.pcm.data() + static_cast<std::size_t>(start_sample) * p.format.nBlockAlign;
			const UINT32 bytes = (total - start_sample) * p.format.nBlockAlign;

			XAUDIO2_BUFFER buffer = {};
			buffer.AudioBytes = bytes;
			buffer.pAudioData = const_cast<BYTE*>(data);
			if (p.loop)
			{
				buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
			}

			if (FAILED(p.voice->SubmitSourceBuffer(&buffer)))
			{
				p.voice->DestroyVoice();
				p.voice = nullptr;
				return;
			}

			p.voice->SetVolume(p.volume);
			p.voice->Start(0);
		}

		std::uint64_t played_samples_locked(const player& p)
		{
			if (!p.voice || p.total_samples == 0)
			{
				return 0;
			}

			XAUDIO2_VOICE_STATE vs = {};
			p.voice->GetState(&vs);

			std::uint64_t played = static_cast<std::uint64_t>(p.seek_base) + vs.SamplesPlayed;

			if (p.loop)
			{
				const std::uint64_t window = static_cast<std::uint64_t>(p.total_samples) - p.seek_base;
				if (window > 0)
				{
					played = static_cast<std::uint64_t>(p.seek_base) + (played - p.seek_base) % window;
				}
			}

			if (played > p.total_samples)
			{
				played = p.total_samples;
			}
			return played;
		}
	}

	std::string song_folder()
	{
		std::string base = fileManager::baseDir();
		if (base.empty())
			return {};
		return base + "\\Music";
	}

	void ensure_song_folder()
	{
		const std::string folder = song_folder();
		if (folder.empty())
		{
			return;
		}
		CreateDirectoryA(folder.c_str(), nullptr);
	}

	std::vector<track_t> scan_songs()
	{
		std::vector<track_t> tracks;
		const std::string folder = song_folder();
		if (folder.empty())
		{
			static bool warned = false;
			if (!warned)
			{
				warned = true;
				g_logger->send(levels::warn, "music: could not locate songs folder, song list unavailable");
			}
			return tracks;
		}

		const std::wstring pattern = utf8_to_wide(folder) + L"\\*";
		WIN32_FIND_DATAW fd = {};
		HANDLE find = FindFirstFileW(pattern.c_str(), &fd);
		if (find == INVALID_HANDLE_VALUE)
		{
			return tracks;
		}

		static const wchar_t* extensions[] = { L".mp3", L".wav", L".m4a", L".aac", L".flac", L".wma" };

		auto ends_with_ci = [](const std::wstring& name, const wchar_t* ext)
		{
			std::wstring lower = name;
			std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
			const std::size_t ext_len = std::wcslen(ext);
			return lower.size() >= ext_len && lower.compare(lower.size() - ext_len, ext_len, ext) == 0;
		};

		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				continue;
			}

			const std::wstring name = fd.cFileName;

			bool supported = false;
			for (const wchar_t* ext : extensions)
			{
				if (ends_with_ci(name, ext))
				{
					supported = true;
					break;
				}
			}
			if (!supported)
			{
				continue;
			}

			track_t track;
			track.path = wide_to_utf8(utf8_to_wide(folder) + L"\\" + name);
			const std::size_t dot = name.find_last_of(L'.');
			track.name = wide_to_utf8(dot == std::wstring::npos ? name : name.substr(0, dot));
			tracks.push_back(std::move(track));
		} while (FindNextFileW(find, &fd));

		FindClose(find);

		std::sort(tracks.begin(), tracks.end(), [](const track_t& a, const track_t& b)
		{
			std::string an = a.name;
			std::string bn = b.name;
			std::transform(an.begin(), an.end(), an.begin(), ::tolower);
			std::transform(bn.begin(), bn.end(), bn.begin(), ::tolower);
			return an < bn;
		});

		return tracks;
	}

	state_t state()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		return g_player.state;
	}

	std::string current_name()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		return g_player.current_name;
	}

	std::string error_message()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		return g_player.error;
	}

	bool has_audio_device()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		return g_player.audio_ready;
	}

	bool finished()
	{
		return g_finished.load();
	}

	void play(const std::string& path, const std::string& name)
	{
		std::uint64_t my_gen = 0;
		{
			std::lock_guard<std::mutex> lock(g_player.mtx);
			++g_player.generation;
			my_gen = g_player.generation;
			g_player.state = state_t::decoding;
			g_player.current_path = path;
			g_player.current_name = name;
			g_player.error.clear();
			g_finished = false;
			detail::teardown_voice(g_player);
		}

		std::thread([path, name, my_gen]()
		{
			decode_guard dg;
			// Serialize decodes: MFStartup/MFShutdown are process-global and
			// must not overlap between two decode threads.
			static std::mutex decode_mtx;
			std::lock_guard<std::mutex> decode_lock(decode_mtx);
			try
			{
				CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

				std::vector<BYTE> pcm;
				WAVEFORMATEX format = {};
				std::string error;
				const bool ok = detail::decode_to_pcm(path, pcm, format, error);

				MFShutdown();
				CoUninitialize();

				std::lock_guard<std::mutex> lock(g_player.mtx);
				if (g_player.generation != my_gen)
				{
					return; // superseded by another play/stop (same file counts too)
				}

				g_finished = false;

				if (ok && !pcm.empty())
				{
					detail::ensure_audio(g_player);
					if (!g_player.audio_ready)
					{
						g_player.error = "audio device unavailable";
						g_player.state = state_t::idle;
						return;
					}

					g_player.pcm = std::move(pcm);
					g_player.format = format;
					g_player.total_samples = static_cast<UINT32>(g_player.pcm.size() / format.nBlockAlign);
					detail::submit_and_start(g_player, 0);
					g_player.state = state_t::playing;
				}
				else
				{
					g_player.error = error.empty() ? "unsupported or corrupted file" : error;
					g_player.state = state_t::idle;
				}
			}
			catch (...)
			{
				std::lock_guard<std::mutex> lock(g_player.mtx);
				if (g_player.generation == my_gen)
				{
					g_player.error = "failed to decode file";
					g_player.state = state_t::idle;
				}
			}
		}).detach();
	}

	void toggle_play_pause()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);

		if (g_player.state == state_t::playing && g_player.voice)
		{
			g_player.voice->Stop(0);
			g_player.state = state_t::paused;
		}
		else if (g_player.state == state_t::paused && g_player.voice)
		{
			g_player.voice->Start(0);
			g_player.state = state_t::playing;
		}
		else if (g_player.voice && g_finished.load())
		{
			// Track already finished - replay from the start.
			g_finished = false;
			g_player.voice->DestroyVoice();
			g_player.voice = nullptr;
			detail::submit_and_start(g_player, 0);
			g_player.state = state_t::playing;
		}
	}

	void stop()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		++g_player.generation;
		g_finished = false;
		detail::teardown_voice(g_player);
		g_player.error.clear();
		g_player.state = state_t::stopped;
	}

	void seek_fraction(float fraction)
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		if (!g_player.voice || g_player.total_samples == 0)
		{
			return;
		}

		fraction = std::clamp(fraction, 0.0f, 1.0f);
		const UINT32 target = static_cast<UINT32>(static_cast<double>(fraction) * g_player.total_samples);
		const bool was_playing = g_player.state == state_t::playing;

		g_finished = false;
		g_player.voice->DestroyVoice();
		g_player.voice = nullptr;

		detail::submit_and_start(g_player, target);
		if (!was_playing && g_player.voice)
		{
			g_player.voice->Stop(0);
		}
		g_player.state = was_playing ? state_t::playing : state_t::paused;
	}

	float volume()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		return g_player.volume;
	}

	void set_volume(float new_volume)
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		g_player.volume = std::clamp(new_volume, 0.0f, 1.0f);
		if (g_player.voice)
		{
			g_player.voice->SetVolume(g_player.volume);
		}
	}

	bool loop_enabled()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		return g_player.loop;
	}

	void set_loop(bool enabled)
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		if (g_player.loop == enabled)
		{
			return;
		}

		g_player.loop = enabled;
		if (g_player.voice && g_player.total_samples > 0)
		{
			// Rebuild the voice so LoopCount takes effect, keeping position.
			XAUDIO2_VOICE_STATE vs = {};
			g_player.voice->GetState(&vs);
			const std::uint64_t played = static_cast<std::uint64_t>(g_player.seek_base) + vs.SamplesPlayed;
			const bool was_playing = g_player.state == state_t::playing;

			g_finished = false;
			g_player.voice->DestroyVoice();
			g_player.voice = nullptr;

			const UINT32 target = played >= g_player.total_samples ? g_player.total_samples - 1 : static_cast<UINT32>(played);
			detail::submit_and_start(g_player, target);
			if (!was_playing && g_player.voice)
			{
				g_player.voice->Stop(0);
			}
		}
	}

	float position_fraction()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		if (g_player.total_samples == 0)
		{
			return 0.0f;
		}
		return static_cast<float>(static_cast<double>(detail::played_samples_locked(g_player)) / g_player.total_samples);
	}

	std::string position_text()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		if (g_player.format.nSamplesPerSec == 0)
		{
			return "0:00";
		}
		return format_time(detail::played_samples_locked(g_player) / g_player.format.nSamplesPerSec);
	}

	std::string duration_text()
	{
		std::lock_guard<std::mutex> lock(g_player.mtx);
		if (g_player.format.nSamplesPerSec == 0)
		{
			return "0:00";
		}
		return format_time(static_cast<std::uint64_t>(g_player.total_samples) / g_player.format.nSamplesPerSec);
	}

	void shutdown()
	{
		{
			std::lock_guard<std::mutex> lock(g_player.mtx);
			++g_player.generation;
			g_finished = false;
			detail::teardown_voice(g_player);
			g_player.error.clear();
			g_player.state = state_t::idle;
		}
		for (int i = 0; i < 200 && g_decoding.load(); ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
		std::lock_guard<std::mutex> lock(g_player.mtx);
		detail::teardown_voice(g_player);
		if (g_player.mastering) { g_player.mastering->DestroyVoice(); g_player.mastering = nullptr; }
		if (g_player.engine) { g_player.engine->Release(); g_player.engine = nullptr; }
		g_player.audio_ready = false;
	}
}
