#ifndef KSON_WITHOUT_JSON_DEPENDENCY
#include "kson/IO/KsonIO.hpp"
#include <fstream>
#include <optional>
#include <limits>
#include <cmath>

namespace
{
	using namespace kson;

	// ==================== Reading/Loading Implementation ====================

	template<typename T>
	T GetWithDefault(const nlohmann::json& j, const std::string& key, const T& defaultValue)
	{
		if (j.contains(key) && !j[key].is_null())
		{
			return j[key].get<T>();
		}
		return defaultValue;
	}

	template<typename T>
	std::optional<T> GetOptional(const nlohmann::json& j, const std::string& key)
	{
		if (j.contains(key) && !j[key].is_null())
		{
			return j[key].get<T>();
		}
		return std::nullopt;
	}

	GraphValue ParseGraphValue(const nlohmann::json& j, ChartData& chartData)
	{
		if (j.is_number())
		{
			return GraphValue{ j.get<double>() };
		}
		else if (j.is_array() && j.size() >= 2)
		{
			return GraphValue{ j[0].get<double>(), j[1].get<double>() };
		}
		else
		{
			chartData.warnings.push_back("Invalid graph value format");
			return GraphValue{ 0.0 };
		}
	}

	GraphPoint ParseGraphPoint(const nlohmann::json& j, ChartData& chartData)
	{
		// Parse v (GraphValue)
		GraphValue v = ParseGraphValue(j, chartData);

		// Parse curve (GraphCurveValue) if present (3rd element)
		GraphCurveValue curve{ 0.0, 0.0 };
		if (j.is_array() && j.size() >= 3 && j[2].is_array() && j[2].size() >= 2)
		{
			curve = GraphCurveValue{ j[2][0].get<double>(), j[2][1].get<double>() };
		}

		return GraphPoint{ v, curve };
	}

	// Parse GraphPoint from an array item where item[valueIdx] is the value and item[curveIdx] is the curve
	GraphPoint ParseGraphPointFromArrayItem(
		const nlohmann::json& item,
		std::size_t valueIdx,
		std::size_t curveIdx,
		ChartData& chartData)
	{
		// Parse v (GraphValue)
		GraphValue v{ 0.0 };
		if (item.size() > valueIdx)
		{
			v = ParseGraphValue(item[valueIdx], chartData);
		}

		// Parse curve (GraphCurveValue) if present
		GraphCurveValue curve{ 0.0, 0.0 };
		if (item.size() > curveIdx && item[curveIdx].is_array() && item[curveIdx].size() >= 2)
		{
			curve = GraphCurveValue{ item[curveIdx][0].get<double>(), item[curveIdx][1].get<double>() };
		}

		return GraphPoint{ v, curve };
	}

	template<typename T>
	ByPulse<T> ParseByPulse(const nlohmann::json& j, ChartData& chartData)
	{
		ByPulse<T> result;
		if (!j.is_array())
		{
			return result;
		}

		for (const auto& item : j)
		{
			if (item.is_array() && item.size() >= 2)
			{
				Pulse pulse = item[0].get<Pulse>();
				T value = item[1].get<T>();
				result[pulse] = value;
			}
			else
			{
				chartData.warnings.push_back("Invalid ByPulse entry format");
			}
		}
		return result;
	}

	Graph ParseGraph(const nlohmann::json& j, ChartData& chartData)
	{
		Graph result;
		if (!j.is_array())
		{
			return result;
		}

		for (const auto& item : j)
		{
			if (item.is_array() && item.size() >= 2)
			{
				Pulse pulse = item[0].get<Pulse>();
				GraphPoint point = ParseGraphPointFromArrayItem(item, 1, 2, chartData);
				result[pulse] = point;
			}
			else
			{
				chartData.warnings.push_back("Invalid graph entry format");
			}
		}
		return result;
	}

	template<typename T>
	ByMeasureIdx<T> ParseByMeasureIdx(const nlohmann::json& j, ChartData& chartData)
	{
		ByMeasureIdx<T> result;
		if (!j.is_array())
		{
			return result;
		}

		for (const auto& item : j)
		{
			if (item.is_array() && item.size() >= 2)
			{
				std::int64_t idx = item[0].get<std::int64_t>();
				T value = item[1].get<T>();
				result[idx] = value;
			}
			else
			{
				chartData.warnings.push_back("Invalid ByMeasureIdx entry format");
			}
		}
		return result;
	}

	template <typename ChartDataType>
	MetaInfo ParseMetaInfo(const nlohmann::json& j, ChartDataType& chartData)
#ifdef __cpp_concepts
		requires std::is_same_v<ChartDataType, kson::ChartData> || std::is_same_v<ChartDataType, kson::MetaChartData>
#endif
	{
		MetaInfo meta;
		
		meta.title = GetWithDefault<std::string>(j, "title", "");
		meta.titleTranslit = GetWithDefault<std::string>(j, "title_translit", "");
		meta.titleImgFilename = GetWithDefault<std::string>(j, "title_img_filename", "");
		meta.artist = GetWithDefault<std::string>(j, "artist", "");
		meta.artistTranslit = GetWithDefault<std::string>(j, "artist_translit", "");
		meta.artistImgFilename = GetWithDefault<std::string>(j, "artist_img_filename", "");
		meta.chartAuthor = GetWithDefault<std::string>(j, "chart_author", "");
		if (j.contains("difficulty"))
		{
			const auto& diff = j["difficulty"];
			if (diff.is_number_integer())
			{
				meta.difficulty.idx = diff.get<std::int32_t>();
			}
			else if (diff.is_string())
			{
				meta.difficulty.idx = 3; // String difficulty is always recognized as infinity
				meta.difficulty.name = diff.get<std::string>();
			}
		}
		meta.level = GetWithDefault<std::int32_t>(j, "level", 1);
		meta.dispBPM = GetWithDefault<std::string>(j, "disp_bpm", "");
		auto stdBPM = GetOptional<double>(j, "std_bpm");
		if (stdBPM.has_value())
		{
			meta.stdBPM = stdBPM.value();
		}
		meta.jacketFilename = GetWithDefault<std::string>(j, "jacket_filename", "");
		meta.jacketAuthor = GetWithDefault<std::string>(j, "jacket_author", "");
		meta.iconFilename = GetWithDefault<std::string>(j, "icon_filename", "");
		meta.information = GetWithDefault<std::string>(j, "information", "");

		return meta;
	}

	BeatInfo ParseBeatInfo(const nlohmann::json& j, ChartData& chartData)
	{
		BeatInfo beat;
		
		if (j.contains("bpm"))
		{
			beat.bpm = ParseByPulse<double>(j["bpm"], chartData);
		}
		
		if (j.contains("time_sig"))
		{
			const auto& timeSigArray = j["time_sig"];
			if (timeSigArray.is_array())
			{
				for (const auto& item : timeSigArray)
				{
					if (item.is_array() && item.size() >= 2)
					{
						std::size_t idx = item[0].get<std::size_t>();
						const auto& tsData = item[1];
						if (tsData.is_array() && tsData.size() >= 2)
						{
							beat.timeSig[idx] = TimeSig{ tsData[0].get<std::int32_t>(), tsData[1].get<std::int32_t>() };
						}
					}
				}
			}
		}
		
		if (j.contains("v")) 
		{
			// resolution field handled separately
		}
		
		// Parse scroll_speed
		if (j.contains("scroll_speed"))
		{
			beat.scrollSpeed = ParseGraph(j["scroll_speed"], chartData);
		}
		else
		{
			// Apply default value [[0, 1.0]]
			beat.scrollSpeed[0] = GraphValue{1.0, 1.0};
		}

		// Parse stop
		if (j.contains("stop"))
		{
			beat.stop = ParseByPulse<RelPulse>(j["stop"], chartData);
		}

		return beat;
	}

	GaugeInfo ParseGaugeInfo(const nlohmann::json& j, ChartData& chartData)
	{
		GaugeInfo gauge;
		
		gauge.total = GetWithDefault<std::uint32_t>(j, "total", 0);
		
		return gauge;
	}

	void ParseLaneNotes(const nlohmann::json& j, ByPulse<Interval>& lane, ChartData& chartData)
	{
		if (!j.is_array())
		{
			return;
		}

		for (const auto& item : j)
		{
			if (item.is_array() && item.size() >= 2)
			{
				Pulse pulse = item[0].get<Pulse>();
				Interval interval;
				interval.length = item[1].get<RelPulse>();
				lane[pulse] = interval;
			}
			else if (item.is_number_integer())
			{
				// Compact format: pulse only (chip note with length=0)
				Pulse pulse = item.get<Pulse>();
				lane[pulse] = Interval{ 0 };
			}
			else
			{
				chartData.warnings.push_back("Invalid note entry format");
			}
		}
	}

	void ParseLaserSection(const nlohmann::json& j, ByPulse<LaserSection>& lane, ChartData& chartData)
	{
		if (!j.is_array())
		{
			return;
		}

		for (const auto& item : j)
		{
			if (item.is_array() && item.size() >= 2)
			{
				Pulse pulse = item[0].get<Pulse>();
				LaserSection section;

				// Parse laser points
				const auto& points = item[1];
				if (points.is_array())
				{
					for (const auto& point : points)
					{
						if (point.is_array() && point.size() >= 2)
						{
							RelPulse ry = point[0].get<RelPulse>();
							GraphPoint graphPoint = ParseGraphPointFromArrayItem(point, 1, 2, chartData);
							section.v[ry] = graphPoint;
						}
					}
				}

				// Parse width (optional, defaults to 1)
				if (item.size() >= 3)
				{
					section.w = item[2].get<std::int32_t>();
				}
				else
				{
					section.w = kLaserXScale1x;
				}

				lane[pulse] = section;
			}
			else
			{
				chartData.warnings.push_back("Invalid laser section format");
			}
		}
	}

	NoteInfo ParseNoteInfo(const nlohmann::json& j, ChartData& chartData)
	{
		NoteInfo note;

		// Parse BT lanes
		if (j.contains("bt"))
		{
			const auto& btArray = j["bt"];
			if (btArray.is_array())
			{
				for (std::size_t i = 0; i < btArray.size() && i < note.bt.size(); ++i)
				{
					ParseLaneNotes(btArray[i], note.bt[i], chartData);
				}
			}
		}

		// Parse FX lanes
		if (j.contains("fx"))
		{
			const auto& fxArray = j["fx"];
			if (fxArray.is_array())
			{
				for (std::size_t i = 0; i < fxArray.size() && i < note.fx.size(); ++i)
				{
					ParseLaneNotes(fxArray[i], note.fx[i], chartData);
				}
			}
		}

		// Parse laser lanes
		if (j.contains("laser"))
		{
			const auto& laserArray = j["laser"];
			if (laserArray.is_array())
			{
				for (std::size_t i = 0; i < laserArray.size() && i < note.laser.size(); ++i)
				{
					ParseLaserSection(laserArray[i], note.laser[i], chartData);
				}
			}
		}

		return note;
	}

	template <typename ChartDataType>
	BGMPreviewInfo ParseBGMPreviewInfo(const nlohmann::json& j, ChartDataType& chartData)
#ifdef __cpp_concepts
		requires std::is_same_v<ChartDataType, kson::ChartData> || std::is_same_v<ChartDataType, kson::MetaChartData>
#endif

	{
		BGMPreviewInfo preview;
		preview.offset = GetWithDefault<std::int32_t>(j, "offset", 0);
		preview.duration = GetWithDefault<std::int32_t>(j, "duration", 15000);
		return preview;
	}

	LegacyBGMInfo ParseLegacyBGMInfo(const nlohmann::json& j, ChartData& chartData)
	{
		LegacyBGMInfo legacy;
		if (j.contains("fp_filenames") && j["fp_filenames"].is_array())
		{
			const auto& fpArray = j["fp_filenames"];
			if (fpArray.size() >= 1) legacy.filenameF = fpArray[0].get<std::string>();
			if (fpArray.size() >= 2) legacy.filenameP = fpArray[1].get<std::string>();
			if (fpArray.size() >= 3) legacy.filenameFP = fpArray[2].get<std::string>();
		}
		return legacy;
	}

	BGMInfo ParseBGMInfo(const nlohmann::json& j, ChartData& chartData)
	{
		BGMInfo bgm;
		bgm.filename = GetWithDefault<std::string>(j, "filename", "");
		bgm.vol = GetWithDefault<double>(j, "vol", 1.0);
		bgm.offset = GetWithDefault<std::int32_t>(j, "offset", 0);
		
		if (j.contains("preview"))
		{
			bgm.preview = ParseBGMPreviewInfo(j["preview"], chartData);
		}
		
		if (j.contains("legacy"))
		{
			bgm.legacy = ParseLegacyBGMInfo(j["legacy"], chartData);
		}
		
		return bgm;
	}

	MetaBGMInfo ParseMetaBGMInfo(const nlohmann::json& j, MetaChartData& chartData)
	{
		MetaBGMInfo bgm;
		bgm.filename = GetWithDefault<std::string>(j, "filename", "");
		bgm.vol = GetWithDefault<double>(j, "vol", 1.0);

		if (j.contains("preview"))
		{
			bgm.preview = ParseBGMPreviewInfo(j["preview"], chartData);
		}

		return bgm;
	}

	AudioEffectType ParseAudioEffectType(const std::string& typeStr)
	{
		static const std::unordered_map<std::string, AudioEffectType> typeMap = {
			{"retrigger", AudioEffectType::Retrigger},
			{"gate", AudioEffectType::Gate},
			{"flanger", AudioEffectType::Flanger},
			{"pitch_shift", AudioEffectType::PitchShift},
			{"bitcrusher", AudioEffectType::Bitcrusher},
			{"phaser", AudioEffectType::Phaser},
			{"wobble", AudioEffectType::Wobble},
			{"tapestop", AudioEffectType::Tapestop},
			{"echo", AudioEffectType::Echo},
			{"sidechain", AudioEffectType::Sidechain},
			{"switch_audio", AudioEffectType::SwitchAudio},
			{"high_pass_filter", AudioEffectType::HighPassFilter},
			{"low_pass_filter", AudioEffectType::LowPassFilter},
			{"peaking_filter", AudioEffectType::PeakingFilter}
		};
		
		auto it = typeMap.find(typeStr);
		if (it != typeMap.end())
		{
			return it->second;
		}
		return AudioEffectType::Unspecified;
	}

	AudioEffectDef ParseAudioEffectDef(const nlohmann::json& j, ChartData& chartData)
	{
		AudioEffectDef def;
		
		if (j.contains("type") && j["type"].is_string())
		{
			def.type = ParseAudioEffectType(j["type"].get<std::string>());
		}
		
		if (j.contains("v") && j["v"].is_object())
		{
			for (const auto& [key, value] : j["v"].items())
			{
				if (value.is_string())
				{
					def.v[key] = value.get<std::string>();
				}
			}
		}
		
		return def;
	}

	AudioEffectFXInfo ParseAudioEffectFXInfo(const nlohmann::json& j, ChartData& chartData)
	{
		AudioEffectFXInfo fx;
		
		// Parse def array
		if (j.contains("def") && j["def"].is_array())
		{
			for (const auto& item : j["def"])
			{
				if (item.is_array() && item.size() >= 2)
				{
					AudioEffectDefKVP kvp;
					kvp.name = item[0].get<std::string>();
					kvp.v = ParseAudioEffectDef(item[1], chartData);
					fx.def.push_back(kvp);
				}
			}
		}
		
		// Parse param_change
		if (j.contains("param_change") && j["param_change"].is_object())
		{
			for (const auto& [effectName, params] : j["param_change"].items())
			{
				if (params.is_object())
				{
					for (const auto& [paramName, values] : params.items())
					{
						if (values.is_array())
						{
							fx.paramChange[effectName][paramName] = ParseByPulse<std::string>(values, chartData);
						}
					}
				}
			}
		}
		
		// Parse long_event
		if (j.contains("long_event") && j["long_event"].is_object())
		{
			for (const auto& [effectName, lanes] : j["long_event"].items())
			{
				if (lanes.is_array())
				{
					FXLane<AudioEffectParams> fxLanes;
					for (std::size_t i = 0; i < lanes.size() && i < fxLanes.size(); ++i)
					{
						if (lanes[i].is_array())
						{
							for (const auto& event : lanes[i])
							{
								if (event.is_number_unsigned())
								{
									Pulse pulse = event.get<Pulse>();
									AudioEffectParams params;
									fxLanes[i][pulse] = params;
								}
								else if (event.is_array() && event.size() >= 2)
								{
									Pulse pulse = event[0].get<Pulse>();
									AudioEffectParams params;
									if (event[1].is_object())
									{
										for (const auto& [key, value] : event[1].items())
										{
											if (value.is_string())
											{
												params[key] = value.get<std::string>();
											}
										}
									}
									fxLanes[i][pulse] = params;
								}
							}
						}
					}
					fx.longEvent[effectName] = fxLanes;
				}
			}
		}
		
		return fx;
	}

	AudioEffectLaserInfo ParseAudioEffectLaserInfo(const nlohmann::json& j, ChartData& chartData)
	{
		AudioEffectLaserInfo laser;
		
		// Parse def array
		if (j.contains("def") && j["def"].is_array())
		{
			for (const auto& item : j["def"])
			{
				if (item.is_array() && item.size() >= 2)
				{
					AudioEffectDefKVP kvp;
					kvp.name = item[0].get<std::string>();
					kvp.v = ParseAudioEffectDef(item[1], chartData);
					laser.def.push_back(kvp);
				}
			}
		}
		
		// Parse param_change
		if (j.contains("param_change") && j["param_change"].is_object())
		{
			for (const auto& [effectName, params] : j["param_change"].items())
			{
				if (params.is_object())
				{
					for (const auto& [paramName, values] : params.items())
					{
						if (values.is_array())
						{
							laser.paramChange[effectName][paramName] = ParseByPulse<std::string>(values, chartData);
						}
					}
				}
			}
		}
		
		// Parse pulse_event
		if (j.contains("pulse_event") && j["pulse_event"].is_object())
		{
			for (const auto& [effectName, pulses] : j["pulse_event"].items())
			{
				if (pulses.is_array())
				{
					std::set<Pulse> pulseSet;
					for (const auto& pulse : pulses)
					{
						if (pulse.is_number_integer())
						{
							pulseSet.insert(pulse.get<Pulse>());
						}
					}
					laser.pulseEvent[effectName] = pulseSet;
				}
			}
		}
		
		// Parse peaking_filter_delay
		laser.peakingFilterDelay = GetWithDefault<std::int32_t>(j, "peaking_filter_delay", 0);

		// Parse legacy
		if (j.contains("legacy") && j["legacy"].is_object())
		{
			const auto& legacyObj = j["legacy"];
			if (legacyObj.contains("filter_gain") && legacyObj["filter_gain"].is_array())
			{
				laser.legacy.filterGain = ParseByPulse<double>(legacyObj["filter_gain"], chartData);
			}
		}

		return laser;
	}

	AudioEffectInfo ParseAudioEffectInfo(const nlohmann::json& j, ChartData& chartData)
	{
		AudioEffectInfo audioEffect;
		
		if (j.contains("fx"))
		{
			audioEffect.fx = ParseAudioEffectFXInfo(j["fx"], chartData);
		}
		
		if (j.contains("laser"))
		{
			audioEffect.laser = ParseAudioEffectLaserInfo(j["laser"], chartData);
		}
		
		return audioEffect;
	}

	KeySoundFXInfo ParseKeySoundFXInfo(const nlohmann::json& j, ChartData& chartData)
	{
		KeySoundFXInfo fx;
		
		if (j.contains("chip_event") && j["chip_event"].is_object())
		{
			for (const auto& [soundName, lanes] : j["chip_event"].items())
			{
				if (lanes.is_array())
				{
					FXLane<KeySoundInvokeFX> fxLanes;
					for (std::size_t i = 0; i < lanes.size() && i < fxLanes.size(); ++i)
					{
						if (lanes[i].is_array())
						{
							for (const auto& event : lanes[i])
							{
								if (event.is_number_unsigned())
								{
									Pulse pulse = event.get<Pulse>();
									KeySoundInvokeFX invoke;
									fxLanes[i][pulse] = invoke;
								}
								else if (event.is_array() && event.size() >= 2)
								{
									Pulse pulse = event[0].get<Pulse>();
									KeySoundInvokeFX invoke;
									if (event[1].is_object() && event[1].contains("vol"))
									{
										invoke.vol = event[1]["vol"].get<double>();
									}
									fxLanes[i][pulse] = invoke;
								}
							}
						}
					}
					fx.chipEvent[soundName] = fxLanes;
				}
			}
		}
		
		return fx;
	}

	KeySoundLaserInfo ParseKeySoundLaserInfo(const nlohmann::json& j, ChartData& chartData)
	{
		KeySoundLaserInfo laser;
		
		if (j.contains("vol"))
		{
			laser.vol = ParseByPulse<double>(j["vol"], chartData);
		}
		
		if (j.contains("slam_event") && j["slam_event"].is_object())
		{
			for (const auto& [eventName, pulses] : j["slam_event"].items())
			{
				if (pulses.is_array())
				{
					std::set<Pulse> pulseSet;
					for (const auto& pulse : pulses)
					{
						if (pulse.is_number_integer())
						{
							pulseSet.insert(pulse.get<Pulse>());
						}
					}
					laser.slamEvent[eventName] = pulseSet;
				}
			}
		}
		
		if (j.contains("legacy") && j["legacy"].is_object())
		{
			laser.legacy.volAuto = GetWithDefault<bool>(j["legacy"], "vol_auto", false);
		}
		
		return laser;
	}

	KeySoundInfo ParseKeySoundInfo(const nlohmann::json& j, ChartData& chartData)
	{
		KeySoundInfo keySound;
		
		if (j.contains("fx"))
		{
			keySound.fx = ParseKeySoundFXInfo(j["fx"], chartData);
		}
		
		if (j.contains("laser"))
		{
			keySound.laser = ParseKeySoundLaserInfo(j["laser"], chartData);
		}
		
		return keySound;
	}

	AudioInfo ParseAudioInfo(const nlohmann::json& j, ChartData& chartData)
	{
		AudioInfo audio;
		
		if (j.contains("bgm"))
		{
			audio.bgm = ParseBGMInfo(j["bgm"], chartData);
		}
		
		if (j.contains("key_sound"))
		{
			audio.keySound = ParseKeySoundInfo(j["key_sound"], chartData);
		}
		
		if (j.contains("audio_effect"))
		{
			audio.audioEffect = ParseAudioEffectInfo(j["audio_effect"], chartData);
		}
		
		return audio;
	}

	MetaAudioInfo ParseMetaAudioInfo(const nlohmann::json& j, MetaChartData& chartData)
	{
		MetaAudioInfo audio;

		if (j.contains("bgm"))
		{
			audio.bgm = ParseMetaBGMInfo(j["bgm"], chartData);
		}

		return audio;
	}

	CamGraphs ParseCamGraphs(const nlohmann::json& j, ChartData& chartData)
	{
		CamGraphs graphs;

		if (j.contains("zoom_bottom")) graphs.zoomBottom = ParseGraph(j["zoom_bottom"], chartData);
		if (j.contains("zoom_side")) graphs.zoomSide = ParseGraph(j["zoom_side"], chartData);
		if (j.contains("zoom_top")) graphs.zoomTop = ParseGraph(j["zoom_top"], chartData);
		if (j.contains("rotation_deg")) graphs.rotationDeg = ParseGraph(j["rotation_deg"], chartData);
		if (j.contains("center_split")) graphs.centerSplit = ParseGraph(j["center_split"], chartData);

		return graphs;
	}

	AutoTiltType ParseAutoTiltType(const std::string& str)
	{
		if (str == "bigger") return AutoTiltType::kBigger;
		if (str == "biggest") return AutoTiltType::kBiggest;
		if (str == "keep_normal") return AutoTiltType::kKeepNormal;
		if (str == "keep_bigger") return AutoTiltType::kKeepBigger;
		if (str == "keep_biggest") return AutoTiltType::kKeepBiggest;
		if (str == "zero") return AutoTiltType::kZero;
		return AutoTiltType::kNormal;
	}

	ByPulse<TiltValue> ParseTilt(const nlohmann::json& j, ChartData& chartData)
	{
		ByPulse<TiltValue> tilt;

		if (j.is_array())
		{
			for (const auto& item : j)
			{
				if (item.is_array() && item.size() >= 2)
				{
					Pulse pulse = item[0].get<Pulse>();

					if (item[1].is_string())
					{
						// Auto tilt type: [pulse, "string"]
						tilt[pulse] = ParseAutoTiltType(item[1].get<std::string>());
					}
					else if (item[1].is_number())
					{
						// Simple value: [pulse, double]
						tilt[pulse] = TiltGraphPoint{ TiltGraphValue{ item[1].get<double>() } };
					}
					else if (item[1].is_array() && item[1].size() == 2)
					{
						// Check if this is [v, vf], [v, [a, b]], or [[v, vf], [a, b]]
						if (item[1][0].is_array())
						{
							// [[v, vf], [a, b]]: TiltGraphValue with immediate change and curve
							TiltGraphValue gv{
								item[1][0][0].get<double>(),
								item[1][0][1].get<double>()
							};
							GraphCurveValue curve{
								item[1][1][0].get<double>(),
								item[1][1][1].get<double>()
							};
							tilt[pulse] = TiltGraphPoint{ gv, curve };
						}
						else if (item[1][1].is_array())
						{
							// [v, [a, b]]: Single value with curve
							TiltGraphValue gv{ item[1][0].get<double>() };
							GraphCurveValue curve{
								item[1][1][0].get<double>(),
								item[1][1][1].get<double>()
							};
							tilt[pulse] = TiltGraphPoint{ gv, curve };
						}
						else
						{
							// [v, vf]: TiltGraphValue with immediate change, no curve
							// vf can be either double or string (AutoTiltType)
							if (item[1][1].is_string())
							{
								// [double, string]: manual tilt to auto tilt
								tilt[pulse] = TiltGraphPoint{
									TiltGraphValue{
										item[1][0].get<double>(),
										ParseAutoTiltType(item[1][1].get<std::string>())
									}
								};
							}
							else
							{
								// [double, double]: manual tilt with immediate change
								tilt[pulse] = TiltGraphPoint{
									TiltGraphValue{
										item[1][0].get<double>(),
										item[1][1].get<double>()
									}
								};
							}
						}
					}
				}
			}
		}

		return tilt;
	}

	CameraInfo ParseCameraInfo(const nlohmann::json& j, ChartData& chartData)
	{
		CameraInfo camera;
		
		if (j.contains("tilt"))
		{
			camera.tilt = ParseTilt(j["tilt"], chartData);
		}
		
		if (j.contains("cam"))
		{
			const auto& camJ = j["cam"];
			if (camJ.contains("body"))
			{
				camera.cam.body = ParseCamGraphs(camJ["body"], chartData);
			}
			
			if (camJ.contains("pattern") && camJ["pattern"].is_object())
			{
				const auto& patternJ = camJ["pattern"];
				if (patternJ.contains("laser") && patternJ["laser"].is_object())
				{
					const auto& laserJ = patternJ["laser"];
					if (laserJ.contains("slam_event") && laserJ["slam_event"].is_object())
					{
						const auto& slamEventJ = laserJ["slam_event"];
						
						if (slamEventJ.contains("spin") && slamEventJ["spin"].is_array())
						{
							for (const auto& item : slamEventJ["spin"])
							{
								if (item.is_array() && item.size() >= 3)
								{
									Pulse y = item[0].get<Pulse>();
									camera.cam.pattern.laser.slamEvent.spin[y] = CamPatternInvokeSpin
									{
										.d = item[1].get<std::int32_t>(),
										.length = item[2].get<RelPulse>(),
									};
								}
							}
						}
						
						if (slamEventJ.contains("half_spin") && slamEventJ["half_spin"].is_array())
						{
							for (const auto& item : slamEventJ["half_spin"])
							{
								if (item.is_array() && item.size() >= 3)
								{
									Pulse y = item[0].get<Pulse>();
									camera.cam.pattern.laser.slamEvent.halfSpin[y] = CamPatternInvokeSpin
									{
										.d = item[1].get<std::int32_t>(),
										.length = item[2].get<RelPulse>(),
									};
								}
							}
						}
						
						if (slamEventJ.contains("swing") && slamEventJ["swing"].is_array())
						{
							for (const auto& item : slamEventJ["swing"])
							{
								if (item.is_array() && item.size() >= 3)
								{
									Pulse y = item[0].get<Pulse>();
									CamPatternInvokeSwing swing;
									swing.d = item[1].get<std::int32_t>();
									swing.length = item[2].get<RelPulse>();
									if (item.size() >= 4 && item[3].is_object())
									{
										if (item[3].contains("scale"))
										{
											swing.v.scale = item[3]["scale"].get<double>();
										}
										if (item[3].contains("repeat"))
										{
											swing.v.repeat = item[3]["repeat"].get<std::int32_t>();
										}
										if (item[3].contains("decay_order"))
										{
											swing.v.decayOrder = item[3]["decay_order"].get<std::int32_t>();
										}
									}
									camera.cam.pattern.laser.slamEvent.swing[y] = std::move(swing);
								}
							}
						}
					}
				}
			}
		}
		
		return camera;
	}

	LegacyBGInfo ParseLegacyBGInfo(const nlohmann::json& j, ChartData& chartData)
	{
		LegacyBGInfo legacy;
		
		if (j.contains("bg") && j["bg"].is_array())
		{
			const auto& bgArray = j["bg"];
			for (std::size_t i = 0; i < bgArray.size() && i < legacy.bg.size(); ++i)
			{
				if (bgArray[i].contains("filename"))
				{
					legacy.bg[i].filename = bgArray[i]["filename"].get<std::string>();
				}
			}
		}
		
		if (j.contains("layer") && j["layer"].is_object())
		{
			const auto& layerJ = j["layer"];
			legacy.layer.filename = GetWithDefault<std::string>(layerJ, "filename", "");
			legacy.layer.duration = GetWithDefault<std::int32_t>(layerJ, "duration", 0);
			
			if (layerJ.contains("rotation") && layerJ["rotation"].is_object())
			{
				const auto& rotationJ = layerJ["rotation"];
				legacy.layer.rotation.tilt = GetWithDefault<bool>(rotationJ, "tilt", true);
				legacy.layer.rotation.spin = GetWithDefault<bool>(rotationJ, "spin", true);
			}
		}
		
		if (j.contains("movie") && j["movie"].is_object())
		{
			const auto& movieJ = j["movie"];
			legacy.movie.filename = GetWithDefault<std::string>(movieJ, "filename", "");
			legacy.movie.offset = GetWithDefault<std::int32_t>(movieJ, "offset", 0);
		}
		
		return legacy;
	}

	BGInfo ParseBGInfo(const nlohmann::json& j, ChartData& chartData)
	{
		BGInfo bg;

		bg.filename = GetWithDefault<std::string>(j, "filename", "");

		if (j.contains("legacy"))
		{
			bg.legacy = ParseLegacyBGInfo(j["legacy"], chartData);
		}

		return bg;
	}

	EditorInfo ParseEditorInfo(const nlohmann::json& j, ChartData& chartData)
	{
		EditorInfo editor;
		
		editor.appName = GetWithDefault<std::string>(j, "app_name", "");
		editor.appVersion = GetWithDefault<std::string>(j, "app_version", "");
		
		if (j.contains("comment"))
		{
			editor.comment = ParseByPulse<std::string>(j["comment"], chartData);
		}
		
		return editor;
	}

	CompatInfo ParseCompatInfo(const nlohmann::json& j, ChartData& chartData)
	{
		CompatInfo compat;
		
		compat.kshVersion = GetWithDefault<std::string>(j, "ksh_version", "");
		
		if (j.contains("ksh_unknown") && j["ksh_unknown"].is_object())
		{
			const auto& unknownJ = j["ksh_unknown"];
			
			if (unknownJ.contains("meta") && unknownJ["meta"].is_object())
			{
				for (const auto& [key, value] : unknownJ["meta"].items())
				{
					if (value.is_string())
					{
						compat.kshUnknown.meta[key] = value.get<std::string>();
					}
				}
			}
			
			if (unknownJ.contains("option") && unknownJ["option"].is_object())
			{
				for (const auto& [key, values] : unknownJ["option"].items())
				{
					if (values.is_array())
					{
						for (const auto& item : values)
						{
							if (item.is_array() && item.size() >= 2)
							{
								Pulse pulse = item[0].get<Pulse>();
								compat.kshUnknown.option[key].emplace(pulse, item[1].get<std::string>());
							}
						}
					}
				}
			}
			
			if (unknownJ.contains("line") && unknownJ["line"].is_array())
			{
				for (const auto& item : unknownJ["line"])
				{
					if (item.is_array() && item.size() >= 2)
					{
						Pulse pulse = item[0].get<Pulse>();
						compat.kshUnknown.line.emplace(pulse, item[1].get<std::string>());
					}
				}
			}
		}
		
		return compat;
	}
}

kson::MetaChartData kson::LoadKSONMetaChartData(std::istream& stream)
{
	MetaChartData chartData;

	if (!stream.good())
	{
		chartData.error = ErrorType::GeneralIOError;
		return chartData;
	}

	try
	{
		nlohmann::json j;
		stream >> j;

		// format_versionフィールドの必須チェック
		if (!j.contains("format_version"))
		{
			chartData.error = ErrorType::KSONParseError;
			chartData.warnings.push_back("Missing required field: format_version");
			return chartData;
		}

		if (!j["format_version"].is_number_integer())
		{
			chartData.error = ErrorType::KSONParseError;
			chartData.warnings.push_back("Invalid format_version: must be an integer");
			return chartData;
		}

		// Parse each component
		if (j.contains("meta"))
		{
			chartData.meta = ParseMetaInfo<MetaChartData>(j["meta"], chartData);
		}

		if (j.contains("audio"))
		{
			chartData.audio = ParseMetaAudioInfo(j["audio"], chartData);
		}

		chartData.error = ErrorType::None;
	}
	catch (const nlohmann::json::parse_error& e)
	{
		chartData.error = ErrorType::KSONParseError;
		chartData.warnings.push_back("JSON parse error: " + std::string(e.what()));
	}
	catch (const nlohmann::json::type_error& e)
	{
		chartData.error = ErrorType::KSONParseError;
		chartData.warnings.push_back("JSON type error: " + std::string(e.what()));
	}
	catch (const std::exception& e)
	{
		chartData.error = ErrorType::UnknownError;
		chartData.warnings.push_back("Unexpected error: " + std::string(e.what()));
	}

	return chartData;
}

kson::MetaChartData kson::LoadKSONMetaChartData(const std::string& filePath)
{
	std::ifstream ifs(filePath);
	if (!ifs.good())
	{
		MetaChartData chartData;
		chartData.error = ErrorType::CouldNotOpenInputFileStream;
		return chartData;
	}
	return kson::LoadKSONMetaChartData(ifs);
}

kson::ChartData kson::LoadKSONChartData(std::istream& stream)
{
	ChartData chartData;
	
	if (!stream.good())
	{
		chartData.error = ErrorType::GeneralIOError;
		return chartData;
	}
	
	try
	{
		nlohmann::json j;
		stream >> j;

		// Check for required format_version field
		if (!j.contains("format_version"))
		{
			chartData.error = ErrorType::KSONParseError;
			chartData.warnings.push_back("Missing required field: format_version");
			return chartData;
		}

		if (!j["format_version"].is_number_integer())
		{
			chartData.error = ErrorType::KSONParseError;
			chartData.warnings.push_back("Invalid format_version: must be an integer");
			return chartData;
		}

		// Parse each component
		if (j.contains("meta"))
		{
			chartData.meta = ParseMetaInfo(j["meta"], chartData);
		}
		
		if (j.contains("beat"))
		{
			chartData.beat = ParseBeatInfo(j["beat"], chartData);
		}
		
		if (j.contains("gauge"))
		{
			chartData.gauge = ParseGaugeInfo(j["gauge"], chartData);
		}
		
		if (j.contains("note"))
		{
			chartData.note = ParseNoteInfo(j["note"], chartData);
		}
		
		if (j.contains("audio"))
		{
			chartData.audio = ParseAudioInfo(j["audio"], chartData);
		}
		
		if (j.contains("camera"))
		{
			chartData.camera = ParseCameraInfo(j["camera"], chartData);
		}
		
		if (j.contains("bg"))
		{
			chartData.bg = ParseBGInfo(j["bg"], chartData);
		}
		
		if (j.contains("editor"))
		{
			chartData.editor = ParseEditorInfo(j["editor"], chartData);
		}
		
		if (j.contains("compat"))
		{
			chartData.compat = ParseCompatInfo(j["compat"], chartData);
		}
		
		if (j.contains("impl"))
		{
			chartData.impl = j["impl"];
		}
		
		chartData.error = ErrorType::None;
	}
	catch (const nlohmann::json::parse_error& e)
	{
		chartData.error = ErrorType::KSONParseError;
		chartData.warnings.push_back("JSON parse error: " + std::string(e.what()));
	}
	catch (const nlohmann::json::type_error& e)
	{
		chartData.error = ErrorType::KSONParseError;
		chartData.warnings.push_back("JSON type error: " + std::string(e.what()));
	}
	catch (const std::exception& e)
	{
		chartData.error = ErrorType::UnknownError;
		chartData.warnings.push_back("Unexpected error: " + std::string(e.what()));
	}
	
	return chartData;
}

kson::ChartData kson::LoadKSONChartData(const std::string& filePath)
{
	std::ifstream ifs(filePath);
	if (!ifs.good())
	{
		ChartData chartData;
		chartData.error = ErrorType::CouldNotOpenInputFileStream;
		return chartData;
	}
	return kson::LoadKSONChartData(ifs);
}
#endif
