#pragma once

// Music player tab support.
//
// Songs live in "Documents\OSGames Music" — the folder is created at
// runtime and users drop .mp3 / .wav / .m4a / .aac / .flac / .wma files there.
//
// Playback pipeline: Media Foundation decodes the whole track to PCM on a
// background thread, XAudio2 plays it (single-buffer design, so seek/loop are
// just re-submits of a sub-buffer). All calls are thread-safe.

#include <string>
#include <vector>

namespace music
{
	enum class state_t
	{
		idle,      // nothing loaded
		decoding,  // loading a track in the background
		playing,
		paused,
		stopped,   // user stopped playback
	};

	struct track_t
	{
		std::string path;
		std::string name;
	};

	// Path of the songs folder (Documents\OSGames Music).
	std::string song_folder();

	// Creates the folder if it does not exist yet.
	void ensure_song_folder();

	// Scan the folder for supported audio files (sorted by name).
	std::vector<track_t> scan_songs();

	// Player state accessors (thread-safe).
	state_t state();
	std::string current_name();
	std::string error_message();
	bool has_audio_device();

	// True once when the current track has finished playing naturally.
	// Cleared by play / stop / seek.
	bool finished();

	// Transport controls.
	void play(const std::string& path, const std::string& name);
	void toggle_play_pause();
	void stop();
	void seek_fraction(float fraction); // 0..1

	// Volume (0..1) and loop.
	float volume();
	void set_volume(float volume);
	bool loop_enabled();
	void set_loop(bool enabled);

	// Playback position helpers.
	float position_fraction(); // 0..1
	std::string position_text(); // "m:ss"
	std::string duration_text(); // "m:ss"

	void shutdown();
}
