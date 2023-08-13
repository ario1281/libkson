﻿#pragma once
#include "kson/common/common.hpp"

namespace kson
{
	enum class AudioEffectType
	{
		Unspecified,
		Retrigger,
		Gate,
		Flanger,
		PitchShift,
		Bitcrusher,
		Phaser,
		Wobble,
		Tapestop,
		Echo,
		Sidechain,
		SwitchAudio,
		HighPassFilter,
		LowPassFilter,
		PeakingFilter,
	};

	AudioEffectType StrToAudioEffectType(std::string_view str);

	std::string_view AudioEffectTypeToStr(AudioEffectType type);

	using AudioEffectParams = Dict<std::string>;

	struct AudioEffectDef
	{
		AudioEffectType type = AudioEffectType::Unspecified;
		AudioEffectParams v;
	};

	using AudioEffectDefKVP = DefKeyValuePair<AudioEffectDef>;

	struct AudioEffectFXInfo
	{
		std::vector<AudioEffectDefKVP> def;
		Dict<Dict<ByPulse<std::string>>> paramChange;
		Dict<FXLane<AudioEffectParams>> longEvent;

		// Note: This is inefficient, so be careful when using it
		bool defContains(std::string_view name) const;

		// Note: This is inefficient, so be careful when using it
		const AudioEffectDef& defByName(std::string_view name) const;

		Dict<AudioEffectDef> defAsDict() const;
	};

	struct AudioEffectLaserInfo
	{
		std::vector<AudioEffectDefKVP> def;
		Dict<Dict<ByPulse<std::string>>> paramChange;
		Dict<std::set<Pulse>> pulseEvent;
		std::int32_t peakingFilterDelay = 0; // 0ms - 160ms

		// Note: If you call this function frequently, it's recommended to first call defAsDict to get the dictionary and use it,
		//       as this function uses linear search.
		bool defContains(std::string_view name) const;

		// Note: If you call this function frequently, it's recommended to first call defAsDict to get the dictionary and use it,
		//       as this function uses linear search.
		const AudioEffectDef& defByName(std::string_view name) const;

		Dict<AudioEffectDef> defAsDict() const;
	};

	struct AudioEffectInfo
	{
		AudioEffectFXInfo fx;
		AudioEffectLaserInfo laser;
	};
}
