/**************************************************************************/
/*  animation.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "animation.h"
#include "animation.compat.inc"

#include "core/io/marshalls.h"
#include "core/math/geometry_3d.h"

bool Animation::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;

	if (prop_name.begins_with("tracks/")) {
		int track = prop_name.get_slicec('/', 1).to_int();
		String what = prop_name.get_slicec('/', 2);

		if (tracks.size() == track && what == "type") {
			String type = p_value;

			if (type == "value") {
				add_track(TYPE_VALUE);
			} else if (type == "method") {
				add_track(TYPE_METHOD);
			} else if (type == "bezier") {
				add_track(TYPE_BEZIER);
			} else if (type == "audio") {
				add_track(TYPE_AUDIO);
			} else if (type == "animation") {
				add_track(TYPE_ANIMATION);
			} else {
				return false;
			}

			return true;
		}

		ERR_FAIL_INDEX_V(track, tracks.size(), false);

		if (what == "path") {
			track_set_path(track, p_value);
		} else if (what == "use_blend") {
			if (track_get_type(track) == TYPE_AUDIO) {
				audio_track_set_use_blend(track, p_value);
			}
		} else if (what == "interp") {
			track_set_interpolation_type(track, InterpolationType(p_value.operator int()));
		} else if (what == "loop_wrap") {
			track_set_interpolation_loop_wrap(track, p_value);
		} else if (what == "imported") {
			track_set_imported(track, p_value);
		} else if (what == "enabled") {
			track_set_enabled(track, p_value);
		} else if (what == "keys" || what == "key_values") {
			if (track_get_type(track) == TYPE_VALUE) {
				ValueTrack *vt = static_cast<ValueTrack *>(tracks[track]);
				Dictionary d = p_value;
				ERR_FAIL_COND_V(!d.has("times"), false);
				ERR_FAIL_COND_V(!d.has("values"), false);
				if (d.has("cont")) {
					bool v = d["cont"];
					vt->update_mode = v ? UPDATE_CONTINUOUS : UPDATE_DISCRETE;
				}

				if (d.has("update")) {
					int um = d["update"];
					if (um < 0) {
						um = 0;
					} else if (um > 3) {
						um = 3;
					}
					vt->update_mode = UpdateMode(um);
				}
				capture_included = capture_included || (vt->update_mode == UPDATE_CAPTURE);

				Vector<real_t> times = d["times"];
				Array values = d["values"];

				ERR_FAIL_COND_V(times.size() != values.size(), false);

				if (times.size()) {
					int valcount = times.size();

					const real_t *rt = times.ptr();

					vt->values.resize(valcount);

					for (int i = 0; i < valcount; i++) {
						vt->values.write[i].time = rt[i];
						vt->values.write[i].value = values[i];
					}

					if (d.has("transitions")) {
						Vector<real_t> transitions = d["transitions"];
						ERR_FAIL_COND_V(transitions.size() != valcount, false);

						const real_t *rtr = transitions.ptr();

						for (int i = 0; i < valcount; i++) {
							vt->values.write[i].transition = rtr[i];
						}
					}
				}

				return true;

			} else if (track_get_type(track) == TYPE_METHOD) {
				while (track_get_key_count(track)) {
					track_remove_key(track, 0); //well shouldn't be set anyway
				}

				Dictionary d = p_value;
				ERR_FAIL_COND_V(!d.has("times"), false);
				ERR_FAIL_COND_V(!d.has("values"), false);

				Vector<real_t> times = d["times"];
				Array values = d["values"];

				ERR_FAIL_COND_V(times.size() != values.size(), false);

				if (times.size()) {
					int valcount = times.size();

					const real_t *rt = times.ptr();

					for (int i = 0; i < valcount; i++) {
						track_insert_key(track, rt[i], values[i]);
					}

					if (d.has("transitions")) {
						Vector<real_t> transitions = d["transitions"];
						ERR_FAIL_COND_V(transitions.size() != valcount, false);

						const real_t *rtr = transitions.ptr();

						for (int i = 0; i < valcount; i++) {
							track_set_key_transition(track, i, rtr[i]);
						}
					}
				}
			} else if (track_get_type(track) == TYPE_BEZIER) {
				BezierTrack *bt = static_cast<BezierTrack *>(tracks[track]);
				Dictionary d = p_value;
				ERR_FAIL_COND_V(!d.has("times"), false);
				ERR_FAIL_COND_V(!d.has("points"), false);
				Vector<real_t> times = d["times"];
				Vector<real_t> values = d["points"];
#ifdef TOOLS_ENABLED
				ERR_FAIL_COND_V(!d.has("handle_modes"), false);
				Vector<int> handle_modes = d["handle_modes"];
#endif // TOOLS_ENABLED

				ERR_FAIL_COND_V(times.size() * 5 != values.size(), false);

				if (times.size()) {
					int valcount = times.size();

					const real_t *rt = times.ptr();
					const real_t *rv = values.ptr();
#ifdef TOOLS_ENABLED
					const int *rh = handle_modes.ptr();
#endif // TOOLS_ENABLED

					bt->values.resize(valcount);

					for (int i = 0; i < valcount; i++) {
						bt->values.write[i].time = rt[i];
						bt->values.write[i].transition = 0; //unused in bezier
						bt->values.write[i].value.value = rv[i * 5 + 0];
						bt->values.write[i].value.in_handle.x = rv[i * 5 + 1];
						bt->values.write[i].value.in_handle.y = rv[i * 5 + 2];
						bt->values.write[i].value.out_handle.x = rv[i * 5 + 3];
						bt->values.write[i].value.out_handle.y = rv[i * 5 + 4];
#ifdef TOOLS_ENABLED
						bt->values.write[i].value.handle_mode = static_cast<HandleMode>(rh[i]);
#endif // TOOLS_ENABLED
					}
				}

				return true;
			} else if (track_get_type(track) == TYPE_AUDIO) {
				AudioTrack *ad = static_cast<AudioTrack *>(tracks[track]);
				Dictionary d = p_value;
				ERR_FAIL_COND_V(!d.has("times"), false);
				ERR_FAIL_COND_V(!d.has("clips"), false);

				Vector<real_t> times = d["times"];
				Array clips = d["clips"];

				ERR_FAIL_COND_V(clips.size() != times.size(), false);

				if (times.size()) {
					int valcount = times.size();

					const real_t *rt = times.ptr();

					ad->values.clear();

					for (int i = 0; i < valcount; i++) {
						Dictionary d2 = clips[i];
						if (!d2.has("start_offset")) {
							continue;
						}
						if (!d2.has("end_offset")) {
							continue;
						}
						if (!d2.has("stream")) {
							continue;
						}

						TKey<AudioKey> ak;
						ak.time = rt[i];
						ak.value.start_offset = d2["start_offset"];
						ak.value.end_offset = d2["end_offset"];
						ak.value.stream = d2["stream"];

						ad->values.push_back(ak);
					}
				}

				return true;
			} else if (track_get_type(track) == TYPE_ANIMATION) {
				AnimationTrack *an = static_cast<AnimationTrack *>(tracks[track]);
				Dictionary d = p_value;
				ERR_FAIL_COND_V(!d.has("times"), false);
				ERR_FAIL_COND_V(!d.has("clips"), false);

				Vector<real_t> times = d["times"];
				Vector<String> clips = d["clips"];

				ERR_FAIL_COND_V(clips.size() != times.size(), false);

				if (times.size()) {
					int valcount = times.size();

					const real_t *rt = times.ptr();
					const String *rc = clips.ptr();

					an->values.resize(valcount);

					for (int i = 0; i < valcount; i++) {
						TKey<StringName> ak;
						ak.time = rt[i];
						ak.value = rc[i];
						an->values.write[i] = ak;
					}
				}

				return true;
			} else {
				return false;
			}
		} else {
			return false;
		}
#ifndef DISABLE_DEPRECATED
	} else if (prop_name == "loop" && p_value.operator bool()) { // Compatibility with Godot 3.x.
		loop_mode = Animation::LoopMode::LOOP_LINEAR;
		return true;
#endif // DISABLE_DEPRECATED
	} else {
		return false;
	}

	return true;
}

bool Animation::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name;

	if (prop_name == "length") {
		r_ret = length;
	} else if (prop_name == "loop_mode") {
		r_ret = loop_mode;
	} else if (prop_name == "step") {
		r_ret = step;
	} else if (prop_name.begins_with("tracks/")) {
		int track = prop_name.get_slicec('/', 1).to_int();
		String what = prop_name.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(track, tracks.size(), false);
		if (what == "type") {
			switch (track_get_type(track)) {
				case TYPE_VALUE:
					r_ret = "value";
					break;
				case TYPE_METHOD:
					r_ret = "method";
					break;
				case TYPE_BEZIER:
					r_ret = "bezier";
					break;
				case TYPE_AUDIO:
					r_ret = "audio";
					break;
				case TYPE_ANIMATION:
					r_ret = "animation";
					break;
			}

			return true;

		} else if (what == "path") {
			r_ret = track_get_path(track);
		} else if (what == "use_blend") {
			if (track_get_type(track) == TYPE_AUDIO) {
				r_ret = audio_track_is_use_blend(track);
			}
		} else if (what == "interp") {
			r_ret = track_get_interpolation_type(track);
		} else if (what == "loop_wrap") {
			r_ret = track_get_interpolation_loop_wrap(track);
		} else if (what == "imported") {
			r_ret = track_is_imported(track);
		} else if (what == "enabled") {
			r_ret = track_is_enabled(track);
		} else if (what == "keys") {
			if (track_get_type(track) == TYPE_VALUE) {
				const ValueTrack *vt = static_cast<const ValueTrack *>(tracks[track]);

				Dictionary d;

				Vector<real_t> key_times;
				Vector<real_t> key_transitions;
				Array key_values;

				int kk = vt->values.size();

				key_times.resize(kk);
				key_transitions.resize(kk);
				key_values.resize(kk);

				real_t *wti = key_times.ptrw();
				real_t *wtr = key_transitions.ptrw();

				int idx = 0;

				const TKey<Variant> *vls = vt->values.ptr();

				for (int i = 0; i < kk; i++) {
					wti[idx] = vls[i].time;
					wtr[idx] = vls[i].transition;
					key_values[idx] = vls[i].value;
					idx++;
				}

				d["times"] = key_times;
				d["transitions"] = key_transitions;
				d["values"] = key_values;
				if (track_get_type(track) == TYPE_VALUE) {
					d["update"] = value_track_get_update_mode(track);
				}

				r_ret = d;

				return true;

			} else if (track_get_type(track) == TYPE_METHOD) {
				Dictionary d;

				Vector<real_t> key_times;
				Vector<real_t> key_transitions;
				Array key_values;

				int kk = track_get_key_count(track);

				key_times.resize(kk);
				key_transitions.resize(kk);
				key_values.resize(kk);

				real_t *wti = key_times.ptrw();
				real_t *wtr = key_transitions.ptrw();

				int idx = 0;
				for (int i = 0; i < track_get_key_count(track); i++) {
					wti[idx] = track_get_key_time(track, i);
					wtr[idx] = track_get_key_transition(track, i);
					key_values[idx] = track_get_key_value(track, i);
					idx++;
				}

				d["times"] = key_times;
				d["transitions"] = key_transitions;
				d["values"] = key_values;
				if (track_get_type(track) == TYPE_VALUE) {
					d["update"] = value_track_get_update_mode(track);
				}

				r_ret = d;

				return true;
			} else if (track_get_type(track) == TYPE_BEZIER) {
				const BezierTrack *bt = static_cast<const BezierTrack *>(tracks[track]);

				Dictionary d;

				Vector<real_t> key_times;
				Vector<real_t> key_points;

				int kk = bt->values.size();

				key_times.resize(kk);
				key_points.resize(kk * 5);

				real_t *wti = key_times.ptrw();
				real_t *wpo = key_points.ptrw();

#ifdef TOOLS_ENABLED
				Vector<int> handle_modes;
				handle_modes.resize(kk);
				int *whm = handle_modes.ptrw();
#endif // TOOLS_ENABLED

				int idx = 0;

				const TKey<BezierKey> *vls = bt->values.ptr();

				for (int i = 0; i < kk; i++) {
					wti[idx] = vls[i].time;
					wpo[idx * 5 + 0] = vls[i].value.value;
					wpo[idx * 5 + 1] = vls[i].value.in_handle.x;
					wpo[idx * 5 + 2] = vls[i].value.in_handle.y;
					wpo[idx * 5 + 3] = vls[i].value.out_handle.x;
					wpo[idx * 5 + 4] = vls[i].value.out_handle.y;
#ifdef TOOLS_ENABLED
					whm[idx] = static_cast<int>(vls[i].value.handle_mode);
#endif // TOOLS_ENABLED
					idx++;
				}

				d["times"] = key_times;
				d["points"] = key_points;
#ifdef TOOLS_ENABLED
				d["handle_modes"] = handle_modes;
#endif // TOOLS_ENABLED

				r_ret = d;

				return true;
			} else if (track_get_type(track) == TYPE_AUDIO) {
				const AudioTrack *ad = static_cast<const AudioTrack *>(tracks[track]);

				Dictionary d;

				Vector<real_t> key_times;
				Array clips;

				int kk = ad->values.size();

				key_times.resize(kk);

				real_t *wti = key_times.ptrw();

				int idx = 0;

				const TKey<AudioKey> *vls = ad->values.ptr();

				for (int i = 0; i < kk; i++) {
					wti[idx] = vls[i].time;
					Dictionary clip;
					clip["start_offset"] = vls[i].value.start_offset;
					clip["end_offset"] = vls[i].value.end_offset;
					clip["stream"] = vls[i].value.stream;
					clips.push_back(clip);
					idx++;
				}

				d["times"] = key_times;
				d["clips"] = clips;

				r_ret = d;

				return true;
			} else if (track_get_type(track) == TYPE_ANIMATION) {
				const AnimationTrack *an = static_cast<const AnimationTrack *>(tracks[track]);

				Dictionary d;

				Vector<real_t> key_times;
				Vector<String> clips;

				int kk = an->values.size();

				key_times.resize(kk);
				clips.resize(kk);

				real_t *wti = key_times.ptrw();
				String *wcl = clips.ptrw();

				const TKey<StringName> *vls = an->values.ptr();

				for (int i = 0; i < kk; i++) {
					wti[i] = vls[i].time;
					wcl[i] = vls[i].value;
				}

				d["times"] = key_times;
				d["clips"] = clips;

				r_ret = d;

				return true;
			}
		} else {
			return false;
		}
	} else {
		return false;
	}

	return true;
}

void Animation::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < tracks.size(); i++) {
		p_list->push_back(PropertyInfo(Variant::STRING, "tracks/" + itos(i) + "/type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "tracks/" + itos(i) + "/imported", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "tracks/" + itos(i) + "/enabled", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::NODE_PATH, "tracks/" + itos(i) + "/path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::INT, "tracks/" + itos(i) + "/interp", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::BOOL, "tracks/" + itos(i) + "/loop_wrap", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		p_list->push_back(PropertyInfo(Variant::ARRAY, "tracks/" + itos(i) + "/keys", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		if (track_get_type(i) == TYPE_AUDIO) {
			p_list->push_back(PropertyInfo(Variant::BOOL, "tracks/" + itos(i) + "/use_blend", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
		}
	}
}

void Animation::reset_state() {
	clear();
}

int Animation::add_track(TrackType p_type, int p_at_pos) {
	if (p_at_pos < 0 || p_at_pos >= tracks.size()) {
		p_at_pos = tracks.size();
	}

	switch (p_type) {
		case TYPE_VALUE: {
			tracks.insert(p_at_pos, memnew(ValueTrack));

		} break;
		case TYPE_METHOD: {
			tracks.insert(p_at_pos, memnew(MethodTrack));

		} break;
		case TYPE_BEZIER: {
			tracks.insert(p_at_pos, memnew(BezierTrack));

		} break;
		case TYPE_AUDIO: {
			tracks.insert(p_at_pos, memnew(AudioTrack));

		} break;
		case TYPE_ANIMATION: {
			tracks.insert(p_at_pos, memnew(AnimationTrack));

		} break;
		default: {
			ERR_PRINT("Unknown track type");
		}
	}
	emit_changed();
	return p_at_pos;
}

void Animation::remove_track(int p_track) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			_clear(vt->values);

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			_clear(mt->methods);

		} break;
		case TYPE_BEZIER: {
			BezierTrack *bz = static_cast<BezierTrack *>(t);
			_clear(bz->values);

		} break;
		case TYPE_AUDIO: {
			AudioTrack *ad = static_cast<AudioTrack *>(t);
			_clear(ad->values);

		} break;
		case TYPE_ANIMATION: {
			AnimationTrack *an = static_cast<AnimationTrack *>(t);
			_clear(an->values);

		} break;
	}

	memdelete(t);
	tracks.remove_at(p_track);
	emit_changed();
	_check_capture_included();
}

bool Animation::is_capture_included() const {
	return capture_included;
}

void Animation::_check_capture_included() {
	capture_included = false;
	for (int i = 0; i < tracks.size(); i++) {
		if (tracks[i]->type == TYPE_VALUE) {
			ValueTrack *vt = static_cast<ValueTrack *>(tracks[i]);
			if (vt->update_mode == UPDATE_CAPTURE) {
				capture_included = true;
				break;
			}
		}
	}
}

int Animation::get_track_count() const {
	return tracks.size();
}

Animation::TrackType Animation::track_get_type(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), TYPE_VALUE);
	return tracks[p_track]->type;
}

void Animation::track_set_path(int p_track, const NodePath &p_path) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	tracks[p_track]->path = p_path;
	_track_update_hash(p_track);
	emit_changed();
}

NodePath Animation::track_get_path(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), NodePath());
	return tracks[p_track]->path;
}

int Animation::find_track(const NodePath &p_path, const TrackType p_type) const {
	for (int i = 0; i < tracks.size(); i++) {
		if (tracks[i]->path == p_path && tracks[i]->type == p_type) {
			return i;
		}
	};
	return -1;
};

Animation::TrackType Animation::get_cache_type(TrackType p_type) {
	if (p_type == Animation::TYPE_BEZIER) {
		return Animation::TYPE_VALUE;
	}
	return p_type;
}

void Animation::_track_update_hash(int p_track) {
	NodePath track_path = tracks[p_track]->path;
	TrackType track_cache_type = get_cache_type(tracks[p_track]->type);
	tracks[p_track]->thash = StringName(String(track_path.get_concatenated_names()) + String(track_path.get_concatenated_subnames()) + itos(track_cache_type)).hash();
}

Animation::TypeHash Animation::track_get_type_hash(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), 0);
	return tracks[p_track]->thash;
}

void Animation::track_set_interpolation_type(int p_track, InterpolationType p_interp) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	tracks[p_track]->interpolation = p_interp;
	emit_changed();
}

Animation::InterpolationType Animation::track_get_interpolation_type(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), INTERPOLATION_NEAREST);
	return tracks[p_track]->interpolation;
}

void Animation::track_set_interpolation_loop_wrap(int p_track, bool p_enable) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	tracks[p_track]->loop_wrap = p_enable;
	emit_changed();
}

bool Animation::track_get_interpolation_loop_wrap(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), INTERPOLATION_NEAREST);
	return tracks[p_track]->loop_wrap;
}

template <typename T, typename V>
int Animation::_insert(double p_time, T &p_keys, const V &p_value) {
	int idx = p_keys.size();

	while (true) {
		// Condition for replacement.
		if (idx > 0 && Math::is_equal_approx((double)p_keys[idx - 1].time, p_time)) {
			float transition = p_keys[idx - 1].transition;
			p_keys.write[idx - 1] = p_value;
			p_keys.write[idx - 1].transition = transition;
			return idx - 1;

			// Condition for insert.
		} else if (idx == 0 || p_keys[idx - 1].time < p_time) {
			p_keys.insert(idx, p_value);
			return idx;
		}

		idx--;
	}

	return -1;
}

template <typename T>
void Animation::_clear(T &p_keys) {
	p_keys.clear();
}

////

void Animation::track_remove_key_at_time(int p_track, double p_time) {
	int idx = track_find_key(p_track, p_time, FIND_MODE_APPROX);
	ERR_FAIL_COND(idx < 0);
	track_remove_key(p_track, idx);
}

void Animation::track_remove_key(int p_track, int p_idx) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			ERR_FAIL_INDEX(p_idx, vt->values.size());
			vt->values.remove_at(p_idx);

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			ERR_FAIL_INDEX(p_idx, mt->methods.size());
			mt->methods.remove_at(p_idx);

		} break;
		case TYPE_BEZIER: {
			BezierTrack *bz = static_cast<BezierTrack *>(t);
			ERR_FAIL_INDEX(p_idx, bz->values.size());
			bz->values.remove_at(p_idx);

		} break;
		case TYPE_AUDIO: {
			AudioTrack *ad = static_cast<AudioTrack *>(t);
			ERR_FAIL_INDEX(p_idx, ad->values.size());
			ad->values.remove_at(p_idx);

		} break;
		case TYPE_ANIMATION: {
			AnimationTrack *an = static_cast<AnimationTrack *>(t);
			ERR_FAIL_INDEX(p_idx, an->values.size());
			an->values.remove_at(p_idx);

		} break;
	}

	emit_changed();
}

int Animation::track_find_key(int p_track, double p_time, FindMode p_find_mode, bool p_limit, bool p_backward) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), -1);
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			int k = _find(vt->values, p_time, p_backward, p_limit);
			if (k < 0 || k >= vt->values.size()) {
				return -1;
			}
			if ((p_find_mode == FIND_MODE_APPROX && !Math::is_equal_approx(vt->values[k].time, p_time)) || (p_find_mode == FIND_MODE_EXACT && vt->values[k].time != p_time)) {
				return -1;
			}
			return k;

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			int k = _find(mt->methods, p_time, p_backward, p_limit);
			if (k < 0 || k >= mt->methods.size()) {
				return -1;
			}
			if ((p_find_mode == FIND_MODE_APPROX && !Math::is_equal_approx(mt->methods[k].time, p_time)) || (p_find_mode == FIND_MODE_EXACT && mt->methods[k].time != p_time)) {
				return -1;
			}
			return k;

		} break;
		case TYPE_BEZIER: {
			BezierTrack *bt = static_cast<BezierTrack *>(t);
			int k = _find(bt->values, p_time, p_backward, p_limit);
			if (k < 0 || k >= bt->values.size()) {
				return -1;
			}
			if ((p_find_mode == FIND_MODE_APPROX && !Math::is_equal_approx(bt->values[k].time, p_time)) || (p_find_mode == FIND_MODE_EXACT && bt->values[k].time != p_time)) {
				return -1;
			}
			return k;

		} break;
		case TYPE_AUDIO: {
			AudioTrack *at = static_cast<AudioTrack *>(t);
			int k = _find(at->values, p_time, p_backward, p_limit);
			if (k < 0 || k >= at->values.size()) {
				return -1;
			}
			if ((p_find_mode == FIND_MODE_APPROX && !Math::is_equal_approx(at->values[k].time, p_time)) || (p_find_mode == FIND_MODE_EXACT && at->values[k].time != p_time)) {
				return -1;
			}
			return k;

		} break;
		case TYPE_ANIMATION: {
			AnimationTrack *at = static_cast<AnimationTrack *>(t);
			int k = _find(at->values, p_time, p_backward, p_limit);
			if (k < 0 || k >= at->values.size()) {
				return -1;
			}
			if ((p_find_mode == FIND_MODE_APPROX && !Math::is_equal_approx(at->values[k].time, p_time)) || (p_find_mode == FIND_MODE_EXACT && at->values[k].time != p_time)) {
				return -1;
			}
			return k;

		} break;
	}

	return -1;
}

int Animation::track_insert_key(int p_track, double p_time, const Variant &p_key, real_t p_transition) {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), -1);
	Track *t = tracks[p_track];

	int ret = -1;

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);

			TKey<Variant> k;
			k.time = p_time;
			k.transition = p_transition;
			k.value = p_key;
			ret = _insert(p_time, vt->values, k);

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);

			ERR_FAIL_COND_V(p_key.get_type() != Variant::DICTIONARY, -1);

			Dictionary d = p_key;
			ERR_FAIL_COND_V(!d.has("method") || (d["method"].get_type() != Variant::STRING_NAME && d["method"].get_type() != Variant::STRING), -1);
			ERR_FAIL_COND_V(!d.has("args") || !d["args"].is_array(), -1);

			MethodKey k;

			k.time = p_time;
			k.transition = p_transition;
			k.method = d["method"];
			k.params = d["args"];

			ret = _insert(p_time, mt->methods, k);

		} break;
		case TYPE_BEZIER: {
			BezierTrack *bt = static_cast<BezierTrack *>(t);

			Array arr = p_key;
			ERR_FAIL_COND_V(arr.size() != 5, -1);

			TKey<BezierKey> k;
			k.time = p_time;
			k.value.value = arr[0];
			k.value.in_handle.x = arr[1];
			k.value.in_handle.y = arr[2];
			k.value.out_handle.x = arr[3];
			k.value.out_handle.y = arr[4];
			ret = _insert(p_time, bt->values, k);

			Vector<int> key_neighborhood;
			key_neighborhood.push_back(ret);
			if (ret > 0) {
				key_neighborhood.push_back(ret - 1);
			}
			if (ret < track_get_key_count(p_track) - 1) {
				key_neighborhood.push_back(ret + 1);
			}
		} break;
		case TYPE_AUDIO: {
			AudioTrack *at = static_cast<AudioTrack *>(t);

			Dictionary k = p_key;
			ERR_FAIL_COND_V(!k.has("start_offset"), -1);
			ERR_FAIL_COND_V(!k.has("end_offset"), -1);
			ERR_FAIL_COND_V(!k.has("stream"), -1);

			TKey<AudioKey> ak;
			ak.time = p_time;
			ak.value.start_offset = k["start_offset"];
			ak.value.end_offset = k["end_offset"];
			ak.value.stream = k["stream"];
			ret = _insert(p_time, at->values, ak);

		} break;
		case TYPE_ANIMATION: {
			AnimationTrack *at = static_cast<AnimationTrack *>(t);

			TKey<StringName> ak;
			ak.time = p_time;
			ak.value = p_key;

			ret = _insert(p_time, at->values, ak);

		} break;
	}

	emit_changed();

	return ret;
}

int Animation::track_get_key_count(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), -1);
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			return vt->values.size();

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			return mt->methods.size();
		} break;
		case TYPE_BEZIER: {
			BezierTrack *bt = static_cast<BezierTrack *>(t);
			return bt->values.size();
		} break;
		case TYPE_AUDIO: {
			AudioTrack *at = static_cast<AudioTrack *>(t);
			return at->values.size();
		} break;
		case TYPE_ANIMATION: {
			AnimationTrack *at = static_cast<AnimationTrack *>(t);
			return at->values.size();
		} break;
	}

	ERR_FAIL_V(-1);
}

Variant Animation::track_get_key_value(int p_track, int p_key_idx) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), Variant());
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, vt->values.size(), Variant());
			return vt->values[p_key_idx].value;

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, mt->methods.size(), Variant());
			Dictionary d;
			d["method"] = mt->methods[p_key_idx].method;
			d["args"] = mt->methods[p_key_idx].params;
			return d;

		} break;
		case TYPE_BEZIER: {
			BezierTrack *bt = static_cast<BezierTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, bt->values.size(), Variant());

			Array arr;
			arr.resize(5);
			arr[0] = bt->values[p_key_idx].value.value;
			arr[1] = bt->values[p_key_idx].value.in_handle.x;
			arr[2] = bt->values[p_key_idx].value.in_handle.y;
			arr[3] = bt->values[p_key_idx].value.out_handle.x;
			arr[4] = bt->values[p_key_idx].value.out_handle.y;
			return arr;

		} break;
		case TYPE_AUDIO: {
			AudioTrack *at = static_cast<AudioTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, at->values.size(), Variant());

			Dictionary k;
			k["start_offset"] = at->values[p_key_idx].value.start_offset;
			k["end_offset"] = at->values[p_key_idx].value.end_offset;
			k["stream"] = at->values[p_key_idx].value.stream;
			return k;

		} break;
		case TYPE_ANIMATION: {
			AnimationTrack *at = static_cast<AnimationTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, at->values.size(), Variant());

			return at->values[p_key_idx].value;

		} break;
	}

	ERR_FAIL_V(Variant());
}

double Animation::track_get_key_time(int p_track, int p_key_idx) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), -1);
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, vt->values.size(), -1);
			return vt->values[p_key_idx].time;

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, mt->methods.size(), -1);
			return mt->methods[p_key_idx].time;

		} break;
		case TYPE_BEZIER: {
			BezierTrack *bt = static_cast<BezierTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, bt->values.size(), -1);
			return bt->values[p_key_idx].time;

		} break;
		case TYPE_AUDIO: {
			AudioTrack *at = static_cast<AudioTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, at->values.size(), -1);
			return at->values[p_key_idx].time;

		} break;
		case TYPE_ANIMATION: {
			AnimationTrack *at = static_cast<AnimationTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, at->values.size(), -1);
			return at->values[p_key_idx].time;

		} break;
	}

	ERR_FAIL_V(-1);
}

void Animation::track_set_key_time(int p_track, int p_key_idx, double p_time) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, vt->values.size());
			TKey<Variant> key = vt->values[p_key_idx];
			key.time = p_time;
			vt->values.remove_at(p_key_idx);
			_insert(p_time, vt->values, key);
			return;
		}
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, mt->methods.size());
			MethodKey key = mt->methods[p_key_idx];
			key.time = p_time;
			mt->methods.remove_at(p_key_idx);
			_insert(p_time, mt->methods, key);
			return;
		}
		case TYPE_BEZIER: {
			BezierTrack *bt = static_cast<BezierTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, bt->values.size());
			TKey<BezierKey> key = bt->values[p_key_idx];
			key.time = p_time;
			bt->values.remove_at(p_key_idx);
			_insert(p_time, bt->values, key);
			return;
		}
		case TYPE_AUDIO: {
			AudioTrack *at = static_cast<AudioTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, at->values.size());
			TKey<AudioKey> key = at->values[p_key_idx];
			key.time = p_time;
			at->values.remove_at(p_key_idx);
			_insert(p_time, at->values, key);
			return;
		}
		case TYPE_ANIMATION: {
			AnimationTrack *at = static_cast<AnimationTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, at->values.size());
			TKey<StringName> key = at->values[p_key_idx];
			key.time = p_time;
			at->values.remove_at(p_key_idx);
			_insert(p_time, at->values, key);
			return;
		}
	}

	ERR_FAIL();
}

real_t Animation::track_get_key_transition(int p_track, int p_key_idx) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), -1);
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, vt->values.size(), -1);
			return vt->values[p_key_idx].transition;

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			ERR_FAIL_INDEX_V(p_key_idx, mt->methods.size(), -1);
			return mt->methods[p_key_idx].transition;

		} break;
		case TYPE_BEZIER: {
			return 1; //bezier does not really use transitions
		} break;
		case TYPE_AUDIO: {
			return 1; //audio does not really use transitions
		} break;
		case TYPE_ANIMATION: {
			return 1; //animation does not really use transitions
		} break;
	}

	ERR_FAIL_V(0);
}

void Animation::track_set_key_value(int p_track, int p_key_idx, const Variant &p_value) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, vt->values.size());

			vt->values.write[p_key_idx].value = p_value;

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, mt->methods.size());

			Dictionary d = p_value;

			if (d.has("method")) {
				mt->methods.write[p_key_idx].method = d["method"];
			}
			if (d.has("args")) {
				mt->methods.write[p_key_idx].params = d["args"];
			}

		} break;
		case TYPE_BEZIER: {
			BezierTrack *bt = static_cast<BezierTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, bt->values.size());

			Array arr = p_value;
			ERR_FAIL_COND(arr.size() != 5);

			bt->values.write[p_key_idx].value.value = arr[0];
			bt->values.write[p_key_idx].value.in_handle.x = arr[1];
			bt->values.write[p_key_idx].value.in_handle.y = arr[2];
			bt->values.write[p_key_idx].value.out_handle.x = arr[3];
			bt->values.write[p_key_idx].value.out_handle.y = arr[4];

		} break;
		case TYPE_AUDIO: {
			AudioTrack *at = static_cast<AudioTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, at->values.size());

			Dictionary k = p_value;
			ERR_FAIL_COND(!k.has("start_offset"));
			ERR_FAIL_COND(!k.has("end_offset"));
			ERR_FAIL_COND(!k.has("stream"));

			at->values.write[p_key_idx].value.start_offset = k["start_offset"];
			at->values.write[p_key_idx].value.end_offset = k["end_offset"];
			at->values.write[p_key_idx].value.stream = k["stream"];

		} break;
		case TYPE_ANIMATION: {
			AnimationTrack *at = static_cast<AnimationTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, at->values.size());

			at->values.write[p_key_idx].value = p_value;

		} break;
	}

	emit_changed();
}

void Animation::track_set_key_transition(int p_track, int p_key_idx, real_t p_transition) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];

	switch (t->type) {
		case TYPE_VALUE: {
			ValueTrack *vt = static_cast<ValueTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, vt->values.size());
			vt->values.write[p_key_idx].transition = p_transition;

		} break;
		case TYPE_METHOD: {
			MethodTrack *mt = static_cast<MethodTrack *>(t);
			ERR_FAIL_INDEX(p_key_idx, mt->methods.size());
			mt->methods.write[p_key_idx].transition = p_transition;

		} break;
		case TYPE_BEZIER:
		case TYPE_AUDIO:
		case TYPE_ANIMATION: {
			// they don't use transition
		} break;
	}

	emit_changed();
}

template <typename K>
int Animation::_find(const Vector<K> &p_keys, double p_time, bool p_backward, bool p_limit) const {
	int len = p_keys.size();
	if (len == 0) {
		return -2;
	}

	int low = 0;
	int high = len - 1;
	int middle = 0;

#ifdef DEBUG_ENABLED
	if (low > high) {
		ERR_PRINT("low > high, this may be a bug.");
	}
#endif

	const K *keys = &p_keys[0];

	while (low <= high) {
		middle = (low + high) / 2;

		if (Math::is_equal_approx(p_time, (double)keys[middle].time)) { //match
			return middle;
		} else if (p_time < keys[middle].time) {
			high = middle - 1; //search low end of array
		} else {
			low = middle + 1; //search high end of array
		}
	}

	if (!p_backward) {
		if (keys[middle].time > p_time) {
			middle--;
		}
	} else {
		if (keys[middle].time < p_time) {
			middle++;
		}
	}

	if (p_limit) {
		double diff = length - keys[middle].time;
		if ((signbit(keys[middle].time) && !Math::is_zero_approx(keys[middle].time)) || (signbit(diff) && !Math::is_zero_approx(diff))) {
			ERR_PRINT_ONCE_ED("Found the key outside the animation range. Consider using the clean-up option in AnimationTrackEditor to fix it.");
			return -1;
		}
	}

	return middle;
}

// Linear interpolation for anytype.

Vector3 Animation::_interpolate(const Vector3 &p_a, const Vector3 &p_b, real_t p_c) const {
	return p_a.lerp(p_b, p_c);
}

Quaternion Animation::_interpolate(const Quaternion &p_a, const Quaternion &p_b, real_t p_c) const {
	return p_a.slerp(p_b, p_c);
}

Variant Animation::_interpolate(const Variant &p_a, const Variant &p_b, real_t p_c) const {
	return interpolate_variant(p_a, p_b, p_c);
}

real_t Animation::_interpolate(const real_t &p_a, const real_t &p_b, real_t p_c) const {
	return Math::lerp(p_a, p_b, p_c);
}

Variant Animation::_interpolate_angle(const Variant &p_a, const Variant &p_b, real_t p_c) const {
	Variant::Type type_a = p_a.get_type();
	Variant::Type type_b = p_b.get_type();
	uint32_t vformat = 1 << type_a;
	vformat |= 1 << type_b;
	if (vformat == ((1 << Variant::INT) | (1 << Variant::FLOAT)) || vformat == (1 << Variant::FLOAT)) {
		real_t a = p_a;
		real_t b = p_b;
		return Math::fposmod((float)Math::lerp_angle(a, b, p_c), (float)Math_TAU);
	}
	return _interpolate(p_a, p_b, p_c);
}

// Cubic interpolation for anytype.

Vector3 Animation::_cubic_interpolate_in_time(const Vector3 &p_pre_a, const Vector3 &p_a, const Vector3 &p_b, const Vector3 &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const {
	return p_a.cubic_interpolate_in_time(p_b, p_pre_a, p_post_b, p_c, p_b_t, p_pre_a_t, p_post_b_t);
}

Quaternion Animation::_cubic_interpolate_in_time(const Quaternion &p_pre_a, const Quaternion &p_a, const Quaternion &p_b, const Quaternion &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const {
	return p_a.spherical_cubic_interpolate_in_time(p_b, p_pre_a, p_post_b, p_c, p_b_t, p_pre_a_t, p_post_b_t);
}

Variant Animation::_cubic_interpolate_in_time(const Variant &p_pre_a, const Variant &p_a, const Variant &p_b, const Variant &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const {
	return cubic_interpolate_in_time_variant(p_pre_a, p_a, p_b, p_post_b, p_c, p_pre_a_t, p_b_t, p_post_b_t);
}

real_t Animation::_cubic_interpolate_in_time(const real_t &p_pre_a, const real_t &p_a, const real_t &p_b, const real_t &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const {
	return Math::cubic_interpolate_in_time(p_a, p_b, p_pre_a, p_post_b, p_c, p_b_t, p_pre_a_t, p_post_b_t);
}

Variant Animation::_cubic_interpolate_angle_in_time(const Variant &p_pre_a, const Variant &p_a, const Variant &p_b, const Variant &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const {
	Variant::Type type_a = p_a.get_type();
	Variant::Type type_b = p_b.get_type();
	Variant::Type type_pa = p_pre_a.get_type();
	Variant::Type type_pb = p_post_b.get_type();
	uint32_t vformat = 1 << type_a;
	vformat |= 1 << type_b;
	vformat |= 1 << type_pa;
	vformat |= 1 << type_pb;
	if (vformat == ((1 << Variant::INT) | (1 << Variant::FLOAT)) || vformat == (1 << Variant::FLOAT)) {
		real_t a = p_a;
		real_t b = p_b;
		real_t pa = p_pre_a;
		real_t pb = p_post_b;
		return Math::fposmod((float)Math::cubic_interpolate_angle_in_time(a, b, pa, pb, p_c, p_b_t, p_pre_a_t, p_post_b_t), (float)Math_TAU);
	}
	return _cubic_interpolate_in_time(p_pre_a, p_a, p_b, p_post_b, p_c, p_pre_a_t, p_b_t, p_post_b_t);
}

template <typename T>
T Animation::_interpolate(const Vector<TKey<T>> &p_keys, double p_time, InterpolationType p_interp, bool p_loop_wrap, bool *p_ok, bool p_backward) const {
	int len = _find(p_keys, length) + 1; // try to find last key (there may be more past the end)

	if (len <= 0) {
		// (-1 or -2 returned originally) (plus one above)
		// meaning no keys, or only key time is larger than length
		if (p_ok) {
			*p_ok = false;
		}
		return T();
	} else if (len == 1) { // one key found (0+1), return it

		if (p_ok) {
			*p_ok = true;
		}
		return p_keys[0].value;
	}

	int idx = _find(p_keys, p_time, p_backward);

	ERR_FAIL_COND_V(idx == -2, T());
	int maxi = len - 1;
	bool is_start_edge = p_backward ? idx >= len : idx == -1;
	bool is_end_edge = p_backward ? idx == 0 : idx >= maxi;

	real_t c = 0.0;
	// Prepare for all cases of interpolation.
	real_t delta = 0.0;
	real_t from = 0.0;

	int pre = -1;
	int next = -1;
	int post = -1;
	real_t pre_t = 0.0;
	real_t to_t = 0.0;
	real_t post_t = 0.0;

	bool use_cubic = p_interp == INTERPOLATION_CUBIC || p_interp == INTERPOLATION_CUBIC_ANGLE;

	if (!p_loop_wrap || loop_mode == LOOP_NONE) {
		if (is_start_edge) {
			idx = p_backward ? maxi : 0;
		}
		next = CLAMP(idx + (p_backward ? -1 : 1), 0, maxi);
		if (use_cubic) {
			pre = CLAMP(idx + (p_backward ? 1 : -1), 0, maxi);
			post = CLAMP(idx + (p_backward ? -2 : 2), 0, maxi);
		}
	} else if (loop_mode == LOOP_LINEAR) {
		if (is_start_edge) {
			idx = p_backward ? 0 : maxi;
		}
		next = Math::posmod(idx + (p_backward ? -1 : 1), len);
		if (use_cubic) {
			pre = Math::posmod(idx + (p_backward ? 1 : -1), len);
			post = Math::posmod(idx + (p_backward ? -2 : 2), len);
		}
		if (is_start_edge) {
			if (!p_backward) {
				real_t endtime = (length - p_keys[idx].time);
				if (endtime < 0) { // may be keys past the end
					endtime = 0;
				}
				delta = endtime + p_keys[next].time;
				from = endtime + p_time;
			} else {
				real_t endtime = p_keys[idx].time;
				if (endtime > length) { // may be keys past the end
					endtime = length;
				}
				delta = endtime + length - p_keys[next].time;
				from = endtime + length - p_time;
			}
		} else if (is_end_edge) {
			if (!p_backward) {
				delta = (length - p_keys[idx].time) + p_keys[next].time;
				from = p_time - p_keys[idx].time;
			} else {
				delta = p_keys[idx].time + (length - p_keys[next].time);
				from = (length - p_time) - (length - p_keys[idx].time);
			}
		}
	} else {
		if (is_start_edge) {
			idx = p_backward ? len : -1;
		}
		next = (int)Math::round(Math::pingpong((float)(idx + (p_backward ? -1 : 1)) + 0.5f, (float)len) - 0.5f);
		if (use_cubic) {
			pre = (int)Math::round(Math::pingpong((float)(idx + (p_backward ? 1 : -1)) + 0.5f, (float)len) - 0.5f);
			post = (int)Math::round(Math::pingpong((float)(idx + (p_backward ? -2 : 2)) + 0.5f, (float)len) - 0.5f);
		}
		idx = (int)Math::round(Math::pingpong((float)idx + 0.5f, (float)len) - 0.5f);
		if (is_start_edge) {
			if (!p_backward) {
				real_t endtime = p_keys[idx].time;
				if (endtime < 0) { // may be keys past the end
					endtime = 0;
				}
				delta = endtime + p_keys[next].time;
				from = endtime + p_time;
			} else {
				real_t endtime = length - p_keys[idx].time;
				if (endtime > length) { // may be keys past the end
					endtime = length;
				}
				delta = endtime + length - p_keys[next].time;
				from = endtime + length - p_time;
			}
		} else if (is_end_edge) {
			if (!p_backward) {
				delta = length * 2.0 - p_keys[idx].time - p_keys[next].time;
				from = p_time - p_keys[idx].time;
			} else {
				delta = p_keys[idx].time + p_keys[next].time;
				from = (length - p_time) - (length - p_keys[idx].time);
			}
		}
	}

	if (!is_start_edge && !is_end_edge) {
		if (!p_backward) {
			delta = p_keys[next].time - p_keys[idx].time;
			from = p_time - p_keys[idx].time;
		} else {
			delta = (length - p_keys[next].time) - (length - p_keys[idx].time);
			from = (length - p_time) - (length - p_keys[idx].time);
		}
	}

	if (Math::is_zero_approx(delta)) {
		c = 0;
	} else {
		c = from / delta;
	}

	if (p_ok) {
		*p_ok = true;
	}

	real_t tr = p_keys[idx].transition;
	if (tr == 0) {
		// Don't interpolate if not needed.
		return p_keys[idx].value;
	}

	if (tr != 1.0) {
		c = Math::ease(c, tr);
	}

	switch (p_interp) {
		case INTERPOLATION_NEAREST: {
			return p_keys[idx].value;
		} break;
		case INTERPOLATION_LINEAR: {
			return _interpolate(p_keys[idx].value, p_keys[next].value, c);
		} break;
		case INTERPOLATION_LINEAR_ANGLE: {
			return _interpolate_angle(p_keys[idx].value, p_keys[next].value, c);
		} break;
		case INTERPOLATION_CUBIC:
		case INTERPOLATION_CUBIC_ANGLE: {
			if (!p_loop_wrap || loop_mode == LOOP_NONE) {
				pre_t = p_keys[pre].time - p_keys[idx].time;
				to_t = p_keys[next].time - p_keys[idx].time;
				post_t = p_keys[post].time - p_keys[idx].time;
			} else if (loop_mode == LOOP_LINEAR) {
				pre_t = pre > idx ? -length + p_keys[pre].time - p_keys[idx].time : p_keys[pre].time - p_keys[idx].time;
				to_t = next < idx ? length + p_keys[next].time - p_keys[idx].time : p_keys[next].time - p_keys[idx].time;
				post_t = next < idx || post <= idx ? length + p_keys[post].time - p_keys[idx].time : p_keys[post].time - p_keys[idx].time;
			} else {
				pre_t = p_keys[pre].time - p_keys[idx].time;
				to_t = p_keys[next].time - p_keys[idx].time;
				post_t = p_keys[post].time - p_keys[idx].time;

				if ((pre > idx && idx == next && post < next) || (pre < idx && idx == next && post > next)) {
					pre_t = p_keys[idx].time - p_keys[pre].time;
				} else if (pre == idx) {
					pre_t = idx < next ? -p_keys[idx].time * 2.0 : (length - p_keys[idx].time) * 2.0;
				}

				if (idx == next) {
					to_t = pre < idx ? (length - p_keys[idx].time) * 2.0 : -p_keys[idx].time * 2.0;
					post_t = p_keys[next].time - p_keys[post].time + to_t;
				} else if (next == post) {
					post_t = idx < next ? (length - p_keys[next].time) * 2.0 + to_t : -p_keys[next].time * 2.0 + to_t;
				}
			}

			if (p_interp == INTERPOLATION_CUBIC_ANGLE) {
				return _cubic_interpolate_angle_in_time(
						p_keys[pre].value, p_keys[idx].value, p_keys[next].value, p_keys[post].value, c,
						pre_t, to_t, post_t);
			}
			return _cubic_interpolate_in_time(
					p_keys[pre].value, p_keys[idx].value, p_keys[next].value, p_keys[post].value, c,
					pre_t, to_t, post_t);
		} break;
		default:
			return p_keys[idx].value;
	}

	// do a barrel roll
}

Variant Animation::value_track_interpolate(int p_track, double p_time, bool p_backward) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), 0);
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_VALUE, Variant());
	ValueTrack *vt = static_cast<ValueTrack *>(t);

	bool ok = false;

	Variant res = _interpolate(vt->values, p_time, vt->update_mode == UPDATE_DISCRETE ? INTERPOLATION_NEAREST : vt->interpolation, vt->loop_wrap, &ok, p_backward);

	if (ok) {
		return res;
	}

	return Variant();
}

void Animation::value_track_set_update_mode(int p_track, UpdateMode p_mode) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_VALUE);
	ERR_FAIL_INDEX((int)p_mode, 3);

	ValueTrack *vt = static_cast<ValueTrack *>(t);
	vt->update_mode = p_mode;

	_check_capture_included();
	emit_changed();
}

Animation::UpdateMode Animation::value_track_get_update_mode(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), UPDATE_CONTINUOUS);
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_VALUE, UPDATE_CONTINUOUS);

	ValueTrack *vt = static_cast<ValueTrack *>(t);
	return vt->update_mode;
}

template <typename T>
void Animation::_track_get_key_indices_in_range(const Vector<T> &p_array, double from_time, double to_time, List<int> *p_indices, bool p_is_backward) const {
	int len = p_array.size();
	if (len == 0) {
		return;
	}

	int from = 0;
	int to = len - 1;

	if (!p_is_backward) {
		while (p_array[from].time < from_time || Math::is_equal_approx(p_array[from].time, from_time)) {
			from++;
			if (to < from) {
				return;
			}
		}
		while (p_array[to].time > to_time && !Math::is_equal_approx(p_array[to].time, to_time)) {
			to--;
			if (to < from) {
				return;
			}
		}
	} else {
		while (p_array[from].time < from_time && !Math::is_equal_approx(p_array[from].time, from_time)) {
			from++;
			if (to < from) {
				return;
			}
		}
		while (p_array[to].time > to_time || Math::is_equal_approx(p_array[to].time, to_time)) {
			to--;
			if (to < from) {
				return;
			}
		}
	}

	if (from == to) {
		p_indices->push_back(from);
		return;
	}

	if (!p_is_backward) {
		for (int i = from; i <= to; i++) {
			p_indices->push_back(i);
		}
	} else {
		for (int i = to; i >= from; i--) {
			p_indices->push_back(i);
		}
	}
}

void Animation::track_get_key_indices_in_range(int p_track, double p_time, double p_delta, List<int> *p_indices, Animation::LoopedFlag p_looped_flag) const {
	ERR_FAIL_INDEX(p_track, tracks.size());

	if (p_delta == 0) {
		return; // Prevent to get key continuously.
	}

	const Track *t = tracks[p_track];

	double from_time = p_time - p_delta;
	double to_time = p_time;

	bool is_backward = false;
	if (from_time > to_time) {
		is_backward = true;
		SWAP(from_time, to_time);
	}

	switch (loop_mode) {
		case LOOP_NONE: {
			if (from_time < 0) {
				from_time = 0;
			}
			if (from_time > length) {
				from_time = length;
			}

			if (to_time < 0) {
				to_time = 0;
			}
			if (to_time > length) {
				to_time = length;
			}
		} break;
		case LOOP_LINEAR: {
			if (from_time > length || from_time < 0) {
				from_time = Math::fposmod(from_time, length);
			}
			if (to_time > length || to_time < 0) {
				to_time = Math::fposmod(to_time, length);
			}

			if (from_time > to_time) {
				// Handle loop by splitting.
				double anim_end = length + CMP_EPSILON;
				double anim_start = -CMP_EPSILON;

				switch (t->type) {
					case TYPE_VALUE: {
						const ValueTrack *vt = static_cast<const ValueTrack *>(t);
						if (!is_backward) {
							_track_get_key_indices_in_range(vt->values, from_time, anim_end, p_indices, is_backward);
							_track_get_key_indices_in_range(vt->values, anim_start, to_time, p_indices, is_backward);
						} else {
							_track_get_key_indices_in_range(vt->values, anim_start, to_time, p_indices, is_backward);
							_track_get_key_indices_in_range(vt->values, from_time, anim_end, p_indices, is_backward);
						}
					} break;
					case TYPE_METHOD: {
						const MethodTrack *mt = static_cast<const MethodTrack *>(t);
						if (!is_backward) {
							_track_get_key_indices_in_range(mt->methods, from_time, anim_end, p_indices, is_backward);
							_track_get_key_indices_in_range(mt->methods, anim_start, to_time, p_indices, is_backward);
						} else {
							_track_get_key_indices_in_range(mt->methods, anim_start, to_time, p_indices, is_backward);
							_track_get_key_indices_in_range(mt->methods, from_time, anim_end, p_indices, is_backward);
						}
					} break;
					case TYPE_BEZIER: {
						const BezierTrack *bz = static_cast<const BezierTrack *>(t);
						if (!is_backward) {
							_track_get_key_indices_in_range(bz->values, from_time, anim_end, p_indices, is_backward);
							_track_get_key_indices_in_range(bz->values, anim_start, to_time, p_indices, is_backward);
						} else {
							_track_get_key_indices_in_range(bz->values, anim_start, to_time, p_indices, is_backward);
							_track_get_key_indices_in_range(bz->values, from_time, anim_end, p_indices, is_backward);
						}
					} break;
					case TYPE_AUDIO: {
						const AudioTrack *ad = static_cast<const AudioTrack *>(t);
						if (!is_backward) {
							_track_get_key_indices_in_range(ad->values, from_time, anim_end, p_indices, is_backward);
							_track_get_key_indices_in_range(ad->values, anim_start, to_time, p_indices, is_backward);
						} else {
							_track_get_key_indices_in_range(ad->values, anim_start, to_time, p_indices, is_backward);
							_track_get_key_indices_in_range(ad->values, from_time, anim_end, p_indices, is_backward);
						}
					} break;
					case TYPE_ANIMATION: {
						const AnimationTrack *an = static_cast<const AnimationTrack *>(t);
						if (!is_backward) {
							_track_get_key_indices_in_range(an->values, from_time, anim_end, p_indices, is_backward);
							_track_get_key_indices_in_range(an->values, anim_start, to_time, p_indices, is_backward);
						} else {
							_track_get_key_indices_in_range(an->values, anim_start, to_time, p_indices, is_backward);
							_track_get_key_indices_in_range(an->values, from_time, anim_end, p_indices, is_backward);
						}
					} break;
				}
				return;
			}

			// Not from_time > to_time but most recent of looping...
			if (p_looped_flag != Animation::LOOPED_FLAG_NONE) {
				if (!is_backward && Math::is_equal_approx(from_time, 0)) {
					int edge = track_find_key(p_track, 0, FIND_MODE_EXACT);
					if (edge >= 0) {
						p_indices->push_back(edge);
					}
				} else if (is_backward && Math::is_equal_approx(to_time, length)) {
					int edge = track_find_key(p_track, length, FIND_MODE_EXACT);
					if (edge >= 0) {
						p_indices->push_back(edge);
					}
				}
			}
		} break;
		case LOOP_PINGPONG: {
			if (from_time > length || from_time < 0) {
				from_time = Math::pingpong(from_time, length);
			}
			if (to_time > length || to_time < 0) {
				to_time = Math::pingpong(to_time, length);
			}

			if (p_looped_flag == Animation::LOOPED_FLAG_START) {
				// Handle loop by splitting.
				switch (t->type) {
					case TYPE_VALUE: {
						const ValueTrack *vt = static_cast<const ValueTrack *>(t);
						_track_get_key_indices_in_range(vt->values, 0, from_time, p_indices, true);
						_track_get_key_indices_in_range(vt->values, 0, to_time, p_indices, false);
					} break;
					case TYPE_METHOD: {
						const MethodTrack *mt = static_cast<const MethodTrack *>(t);
						_track_get_key_indices_in_range(mt->methods, 0, from_time, p_indices, true);
						_track_get_key_indices_in_range(mt->methods, 0, to_time, p_indices, false);
					} break;
					case TYPE_BEZIER: {
						const BezierTrack *bz = static_cast<const BezierTrack *>(t);
						_track_get_key_indices_in_range(bz->values, 0, from_time, p_indices, true);
						_track_get_key_indices_in_range(bz->values, 0, to_time, p_indices, false);
					} break;
					case TYPE_AUDIO: {
						const AudioTrack *ad = static_cast<const AudioTrack *>(t);
						_track_get_key_indices_in_range(ad->values, 0, from_time, p_indices, true);
						_track_get_key_indices_in_range(ad->values, 0, to_time, p_indices, false);
					} break;
					case TYPE_ANIMATION: {
						const AnimationTrack *an = static_cast<const AnimationTrack *>(t);
						_track_get_key_indices_in_range(an->values, 0, from_time, p_indices, true);
						_track_get_key_indices_in_range(an->values, 0, to_time, p_indices, false);
					} break;
				}
				return;
			}
			if (p_looped_flag == Animation::LOOPED_FLAG_END) {
				// Handle loop by splitting.
				switch (t->type) {
					case TYPE_VALUE: {
						const ValueTrack *vt = static_cast<const ValueTrack *>(t);
						_track_get_key_indices_in_range(vt->values, from_time, length, p_indices, false);
						_track_get_key_indices_in_range(vt->values, to_time, length, p_indices, true);
					} break;
					case TYPE_METHOD: {
						const MethodTrack *mt = static_cast<const MethodTrack *>(t);
						_track_get_key_indices_in_range(mt->methods, from_time, length, p_indices, false);
						_track_get_key_indices_in_range(mt->methods, to_time, length, p_indices, true);
					} break;
					case TYPE_BEZIER: {
						const BezierTrack *bz = static_cast<const BezierTrack *>(t);
						_track_get_key_indices_in_range(bz->values, from_time, length, p_indices, false);
						_track_get_key_indices_in_range(bz->values, to_time, length, p_indices, true);
					} break;
					case TYPE_AUDIO: {
						const AudioTrack *ad = static_cast<const AudioTrack *>(t);
						_track_get_key_indices_in_range(ad->values, from_time, length, p_indices, false);
						_track_get_key_indices_in_range(ad->values, to_time, length, p_indices, true);
					} break;
					case TYPE_ANIMATION: {
						const AnimationTrack *an = static_cast<const AnimationTrack *>(t);
						_track_get_key_indices_in_range(an->values, from_time, length, p_indices, false);
						_track_get_key_indices_in_range(an->values, to_time, length, p_indices, true);
					} break;
				}
				return;
			}

			// The edge will be pingponged in the next frame and processed there, so let's ignore it now...
			if (!is_backward && Math::is_equal_approx(to_time, length)) {
				to_time -= CMP_EPSILON;
			} else if (is_backward && Math::is_equal_approx(from_time, 0)) {
				from_time += CMP_EPSILON;
			}
		} break;
	}
	switch (t->type) {
		case TYPE_VALUE: {
			const ValueTrack *vt = static_cast<const ValueTrack *>(t);
			_track_get_key_indices_in_range(vt->values, from_time, to_time, p_indices, is_backward);
		} break;
		case TYPE_METHOD: {
			const MethodTrack *mt = static_cast<const MethodTrack *>(t);
			_track_get_key_indices_in_range(mt->methods, from_time, to_time, p_indices, is_backward);
		} break;
		case TYPE_BEZIER: {
			const BezierTrack *bz = static_cast<const BezierTrack *>(t);
			_track_get_key_indices_in_range(bz->values, from_time, to_time, p_indices, is_backward);
		} break;
		case TYPE_AUDIO: {
			const AudioTrack *ad = static_cast<const AudioTrack *>(t);
			_track_get_key_indices_in_range(ad->values, from_time, to_time, p_indices, is_backward);
		} break;
		case TYPE_ANIMATION: {
			const AnimationTrack *an = static_cast<const AnimationTrack *>(t);
			_track_get_key_indices_in_range(an->values, from_time, to_time, p_indices, is_backward);
		} break;
	}
}

Vector<Variant> Animation::method_track_get_params(int p_track, int p_key_idx) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), Vector<Variant>());
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_METHOD, Vector<Variant>());

	MethodTrack *pm = static_cast<MethodTrack *>(t);

	ERR_FAIL_INDEX_V(p_key_idx, pm->methods.size(), Vector<Variant>());

	const MethodKey &mk = pm->methods[p_key_idx];

	return mk.params;
}

StringName Animation::method_track_get_name(int p_track, int p_key_idx) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), StringName());
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_METHOD, StringName());

	MethodTrack *pm = static_cast<MethodTrack *>(t);

	ERR_FAIL_INDEX_V(p_key_idx, pm->methods.size(), StringName());

	return pm->methods[p_key_idx].method;
}

Array Animation::make_default_bezier_key(float p_value) {
	const double max_width = length / 2.0;
	Array new_point;
	new_point.resize(5);

	new_point[0] = p_value;
	new_point[1] = MAX(-0.25, -max_width);
	new_point[2] = 0;
	new_point[3] = MIN(0.25, max_width);
	new_point[4] = 0;

	return new_point;
}

int Animation::bezier_track_insert_key(int p_track, double p_time, real_t p_value, const Vector2 &p_in_handle, const Vector2 &p_out_handle) {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), -1);
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_BEZIER, -1);

	BezierTrack *bt = static_cast<BezierTrack *>(t);

	TKey<BezierKey> k;
	k.time = p_time;
	k.value.value = p_value;
	k.value.in_handle = p_in_handle;
	if (k.value.in_handle.x > 0) {
		k.value.in_handle.x = 0;
	}
	k.value.out_handle = p_out_handle;
	if (k.value.out_handle.x < 0) {
		k.value.out_handle.x = 0;
	}

	int key = _insert(p_time, bt->values, k);

	emit_changed();

	return key;
}

void Animation::bezier_track_set_key_value(int p_track, int p_index, real_t p_value) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_BEZIER);

	BezierTrack *bt = static_cast<BezierTrack *>(t);

	ERR_FAIL_INDEX(p_index, bt->values.size());

	bt->values.write[p_index].value.value = p_value;

	emit_changed();
}

void Animation::bezier_track_set_key_in_handle(int p_track, int p_index, const Vector2 &p_handle, real_t p_balanced_value_time_ratio) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_BEZIER);

	BezierTrack *bt = static_cast<BezierTrack *>(t);

	ERR_FAIL_INDEX(p_index, bt->values.size());

	Vector2 in_handle = p_handle;
	if (in_handle.x > 0) {
		in_handle.x = 0;
	}
	bt->values.write[p_index].value.in_handle = in_handle;

#ifdef TOOLS_ENABLED
	if (bt->values[p_index].value.handle_mode == HANDLE_MODE_LINEAR) {
		bt->values.write[p_index].value.in_handle = Vector2();
		bt->values.write[p_index].value.out_handle = Vector2();
	} else if (bt->values[p_index].value.handle_mode == HANDLE_MODE_BALANCED) {
		Transform2D xform;
		xform.set_scale(Vector2(1.0, 1.0 / p_balanced_value_time_ratio));

		Vector2 vec_out = xform.xform(bt->values[p_index].value.out_handle);
		Vector2 vec_in = xform.xform(in_handle);

		bt->values.write[p_index].value.out_handle = xform.affine_inverse().xform(-vec_in.normalized() * vec_out.length());
	} else if (bt->values[p_index].value.handle_mode == HANDLE_MODE_MIRRORED) {
		bt->values.write[p_index].value.out_handle = -in_handle;
	}
#endif // TOOLS_ENABLED

	emit_changed();
}

void Animation::bezier_track_set_key_out_handle(int p_track, int p_index, const Vector2 &p_handle, real_t p_balanced_value_time_ratio) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_BEZIER);

	BezierTrack *bt = static_cast<BezierTrack *>(t);

	ERR_FAIL_INDEX(p_index, bt->values.size());

	Vector2 out_handle = p_handle;
	if (out_handle.x < 0) {
		out_handle.x = 0;
	}
	bt->values.write[p_index].value.out_handle = out_handle;

#ifdef TOOLS_ENABLED
	if (bt->values[p_index].value.handle_mode == HANDLE_MODE_LINEAR) {
		bt->values.write[p_index].value.in_handle = Vector2();
		bt->values.write[p_index].value.out_handle = Vector2();
	} else if (bt->values[p_index].value.handle_mode == HANDLE_MODE_BALANCED) {
		Transform2D xform;
		xform.set_scale(Vector2(1.0, 1.0 / p_balanced_value_time_ratio));

		Vector2 vec_in = xform.xform(bt->values[p_index].value.in_handle);
		Vector2 vec_out = xform.xform(out_handle);

		bt->values.write[p_index].value.in_handle = xform.affine_inverse().xform(-vec_out.normalized() * vec_in.length());
	} else if (bt->values[p_index].value.handle_mode == HANDLE_MODE_MIRRORED) {
		bt->values.write[p_index].value.in_handle = -out_handle;
	}
#endif // TOOLS_ENABLED

	emit_changed();
}

real_t Animation::bezier_track_get_key_value(int p_track, int p_index) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), 0);
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_BEZIER, 0);

	BezierTrack *bt = static_cast<BezierTrack *>(t);

	ERR_FAIL_INDEX_V(p_index, bt->values.size(), 0);

	return bt->values[p_index].value.value;
}

Vector2 Animation::bezier_track_get_key_in_handle(int p_track, int p_index) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), Vector2());
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_BEZIER, Vector2());

	BezierTrack *bt = static_cast<BezierTrack *>(t);

	ERR_FAIL_INDEX_V(p_index, bt->values.size(), Vector2());

	return bt->values[p_index].value.in_handle;
}

Vector2 Animation::bezier_track_get_key_out_handle(int p_track, int p_index) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), Vector2());
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_BEZIER, Vector2());

	BezierTrack *bt = static_cast<BezierTrack *>(t);

	ERR_FAIL_INDEX_V(p_index, bt->values.size(), Vector2());

	return bt->values[p_index].value.out_handle;
}

#ifdef TOOLS_ENABLED
void Animation::bezier_track_set_key_handle_mode(int p_track, int p_index, HandleMode p_mode, HandleSetMode p_set_mode) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_BEZIER);

	BezierTrack *bt = static_cast<BezierTrack *>(t);

	ERR_FAIL_INDEX(p_index, bt->values.size());

	bt->values.write[p_index].value.handle_mode = p_mode;

	switch (p_mode) {
		case HANDLE_MODE_LINEAR: {
			bt->values.write[p_index].value.in_handle = Vector2(0, 0);
			bt->values.write[p_index].value.out_handle = Vector2(0, 0);
		} break;
		case HANDLE_MODE_BALANCED:
		case HANDLE_MODE_MIRRORED: {
			int prev_key = MAX(0, p_index - 1);
			int next_key = MIN(bt->values.size() - 1, p_index + 1);
			if (prev_key == next_key) {
				break; // Exists only one key.
			}
			real_t in_handle_x = 0;
			real_t in_handle_y = 0;
			real_t out_handle_x = 0;
			real_t out_handle_y = 0;
			if (p_mode == HANDLE_MODE_BALANCED) {
				// Note:
				// If p_set_mode == HANDLE_SET_MODE_NONE, I don't know if it should change the Tangent implicitly.
				// At the least, we need to avoid corrupting the handles when loading animation from the resource.
				// However, changes made by the Inspector do not go through the BezierEditor,
				// so if you change from Free to Balanced or Mirrored in Inspector, there is no guarantee that
				// it is Balanced or Mirrored until there is a handle operation.
				if (p_set_mode == HANDLE_SET_MODE_RESET) {
					real_t handle_length = 1.0 / 3.0;
					in_handle_x = (bt->values[prev_key].time - bt->values[p_index].time) * handle_length;
					in_handle_y = 0;
					out_handle_x = (bt->values[next_key].time - bt->values[p_index].time) * handle_length;
					out_handle_y = 0;
					bt->values.write[p_index].value.in_handle = Vector2(in_handle_x, in_handle_y);
					bt->values.write[p_index].value.out_handle = Vector2(out_handle_x, out_handle_y);
				} else if (p_set_mode == HANDLE_SET_MODE_AUTO) {
					real_t handle_length = 1.0 / 6.0;
					real_t tangent = (bt->values[next_key].value.value - bt->values[prev_key].value.value) / (bt->values[next_key].time - bt->values[prev_key].time);
					in_handle_x = (bt->values[prev_key].time - bt->values[p_index].time) * handle_length;
					in_handle_y = in_handle_x * tangent;
					out_handle_x = (bt->values[next_key].time - bt->values[p_index].time) * handle_length;
					out_handle_y = out_handle_x * tangent;
					bt->values.write[p_index].value.in_handle = Vector2(in_handle_x, in_handle_y);
					bt->values.write[p_index].value.out_handle = Vector2(out_handle_x, out_handle_y);
				}
			} else {
				real_t handle_length = 1.0 / 4.0;
				real_t prev_interval = Math::abs(bt->values[p_index].time - bt->values[prev_key].time);
				real_t next_interval = Math::abs(bt->values[p_index].time - bt->values[next_key].time);
				real_t min_time = 0;
				if (Math::is_zero_approx(prev_interval)) {
					min_time = next_interval;
				} else if (Math::is_zero_approx(next_interval)) {
					min_time = prev_interval;
				} else {
					min_time = MIN(prev_interval, next_interval);
				}
				if (p_set_mode == HANDLE_SET_MODE_RESET) {
					in_handle_x = -min_time * handle_length;
					in_handle_y = 0;
					out_handle_x = min_time * handle_length;
					out_handle_y = 0;
					bt->values.write[p_index].value.in_handle = Vector2(in_handle_x, in_handle_y);
					bt->values.write[p_index].value.out_handle = Vector2(out_handle_x, out_handle_y);
				} else if (p_set_mode == HANDLE_SET_MODE_AUTO) {
					real_t tangent = (bt->values[next_key].value.value - bt->values[prev_key].value.value) / min_time;
					in_handle_x = -min_time * handle_length;
					in_handle_y = in_handle_x * tangent;
					out_handle_x = min_time * handle_length;
					out_handle_y = out_handle_x * tangent;
					bt->values.write[p_index].value.in_handle = Vector2(in_handle_x, in_handle_y);
					bt->values.write[p_index].value.out_handle = Vector2(out_handle_x, out_handle_y);
				}
			}
		} break;
		default: {
		} break;
	}

	emit_changed();
}

Animation::HandleMode Animation::bezier_track_get_key_handle_mode(int p_track, int p_index) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), HANDLE_MODE_FREE);
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_BEZIER, HANDLE_MODE_FREE);

	BezierTrack *bt = static_cast<BezierTrack *>(t);

	ERR_FAIL_INDEX_V(p_index, bt->values.size(), HANDLE_MODE_FREE);

	return bt->values[p_index].value.handle_mode;
}
#endif // TOOLS_ENABLED

real_t Animation::bezier_track_interpolate(int p_track, double p_time) const {
	//this uses a different interpolation scheme
	ERR_FAIL_INDEX_V(p_track, tracks.size(), 0);
	Track *track = tracks[p_track];
	ERR_FAIL_COND_V(track->type != TYPE_BEZIER, 0);

	BezierTrack *bt = static_cast<BezierTrack *>(track);

	int len = _find(bt->values, length) + 1; // try to find last key (there may be more past the end)

	if (len <= 0) {
		// (-1 or -2 returned originally) (plus one above)
		return 0;
	} else if (len == 1) { // one key found (0+1), return it
		return bt->values[0].value.value;
	}

	int idx = _find(bt->values, p_time);

	ERR_FAIL_COND_V(idx == -2, 0);

	//there really is no looping interpolation on bezier

	if (idx < 0) {
		return bt->values[0].value.value;
	}

	if (idx >= bt->values.size() - 1) {
		return bt->values[bt->values.size() - 1].value.value;
	}

	double t = p_time - bt->values[idx].time;

	int iterations = 10;

	real_t duration = bt->values[idx + 1].time - bt->values[idx].time; // time duration between our two keyframes
	real_t low = 0.0; // 0% of the current animation segment
	real_t high = 1.0; // 100% of the current animation segment

	Vector2 start(0, bt->values[idx].value.value);
	Vector2 start_out = start + bt->values[idx].value.out_handle;
	Vector2 end(duration, bt->values[idx + 1].value.value);
	Vector2 end_in = end + bt->values[idx + 1].value.in_handle;

	//narrow high and low as much as possible
	for (int i = 0; i < iterations; i++) {
		real_t middle = (low + high) / 2;

		Vector2 interp = start.bezier_interpolate(start_out, end_in, end, middle);

		if (interp.x < t) {
			low = middle;
		} else {
			high = middle;
		}
	}

	//interpolate the result:
	Vector2 low_pos = start.bezier_interpolate(start_out, end_in, end, low);
	Vector2 high_pos = start.bezier_interpolate(start_out, end_in, end, high);
	real_t c = (t - low_pos.x) / (high_pos.x - low_pos.x);

	return low_pos.lerp(high_pos, c).y;
}

int Animation::audio_track_insert_key(int p_track, double p_time, const Ref<Resource> &p_stream, real_t p_start_offset, real_t p_end_offset) {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), -1);
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_AUDIO, -1);

	AudioTrack *at = static_cast<AudioTrack *>(t);

	TKey<AudioKey> k;
	k.time = p_time;
	k.value.stream = p_stream;
	k.value.start_offset = p_start_offset;
	if (k.value.start_offset < 0) {
		k.value.start_offset = 0;
	}
	k.value.end_offset = p_end_offset;
	if (k.value.end_offset < 0) {
		k.value.end_offset = 0;
	}

	int key = _insert(p_time, at->values, k);

	emit_changed();

	return key;
}

void Animation::audio_track_set_key_stream(int p_track, int p_key, const Ref<Resource> &p_stream) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_AUDIO);

	AudioTrack *at = static_cast<AudioTrack *>(t);

	ERR_FAIL_INDEX(p_key, at->values.size());

	at->values.write[p_key].value.stream = p_stream;

	emit_changed();
}

void Animation::audio_track_set_key_start_offset(int p_track, int p_key, real_t p_offset) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_AUDIO);

	AudioTrack *at = static_cast<AudioTrack *>(t);

	ERR_FAIL_INDEX(p_key, at->values.size());

	if (p_offset < 0) {
		p_offset = 0;
	}

	at->values.write[p_key].value.start_offset = p_offset;

	emit_changed();
}

void Animation::audio_track_set_key_end_offset(int p_track, int p_key, real_t p_offset) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_AUDIO);

	AudioTrack *at = static_cast<AudioTrack *>(t);

	ERR_FAIL_INDEX(p_key, at->values.size());

	if (p_offset < 0) {
		p_offset = 0;
	}

	at->values.write[p_key].value.end_offset = p_offset;

	emit_changed();
}

Ref<Resource> Animation::audio_track_get_key_stream(int p_track, int p_key) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), Ref<Resource>());
	const Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_AUDIO, Ref<Resource>());

	const AudioTrack *at = static_cast<const AudioTrack *>(t);

	ERR_FAIL_INDEX_V(p_key, at->values.size(), Ref<Resource>());

	return at->values[p_key].value.stream;
}

real_t Animation::audio_track_get_key_start_offset(int p_track, int p_key) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), 0);
	const Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_AUDIO, 0);

	const AudioTrack *at = static_cast<const AudioTrack *>(t);

	ERR_FAIL_INDEX_V(p_key, at->values.size(), 0);

	return at->values[p_key].value.start_offset;
}

real_t Animation::audio_track_get_key_end_offset(int p_track, int p_key) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), 0);
	const Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_AUDIO, 0);

	const AudioTrack *at = static_cast<const AudioTrack *>(t);

	ERR_FAIL_INDEX_V(p_key, at->values.size(), 0);

	return at->values[p_key].value.end_offset;
}

void Animation::audio_track_set_use_blend(int p_track, bool p_enable) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_AUDIO);

	AudioTrack *at = static_cast<AudioTrack *>(t);

	at->use_blend = p_enable;
	emit_changed();
}

bool Animation::audio_track_is_use_blend(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), false);
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_AUDIO, false);

	AudioTrack *at = static_cast<AudioTrack *>(t);

	return at->use_blend;
}

//

int Animation::animation_track_insert_key(int p_track, double p_time, const StringName &p_animation) {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), -1);
	Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_ANIMATION, -1);

	AnimationTrack *at = static_cast<AnimationTrack *>(t);

	TKey<StringName> k;
	k.time = p_time;
	k.value = p_animation;

	int key = _insert(p_time, at->values, k);

	emit_changed();

	return key;
}

void Animation::animation_track_set_key_animation(int p_track, int p_key, const StringName &p_animation) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	Track *t = tracks[p_track];
	ERR_FAIL_COND(t->type != TYPE_ANIMATION);

	AnimationTrack *at = static_cast<AnimationTrack *>(t);

	ERR_FAIL_INDEX(p_key, at->values.size());

	at->values.write[p_key].value = p_animation;

	emit_changed();
}

StringName Animation::animation_track_get_key_animation(int p_track, int p_key) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), StringName());
	const Track *t = tracks[p_track];
	ERR_FAIL_COND_V(t->type != TYPE_ANIMATION, StringName());

	const AnimationTrack *at = static_cast<const AnimationTrack *>(t);

	ERR_FAIL_INDEX_V(p_key, at->values.size(), StringName());

	return at->values[p_key].value;
}

void Animation::set_length(real_t p_length) {
	if (p_length < ANIM_MIN_LENGTH) {
		p_length = ANIM_MIN_LENGTH;
	}
	length = p_length;
	emit_changed();
}

real_t Animation::get_length() const {
	return length;
}

void Animation::set_loop_mode(Animation::LoopMode p_loop_mode) {
	loop_mode = p_loop_mode;
	emit_changed();
}

Animation::LoopMode Animation::get_loop_mode() const {
	return loop_mode;
}

void Animation::track_set_imported(int p_track, bool p_imported) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	tracks[p_track]->imported = p_imported;
}

bool Animation::track_is_imported(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), false);
	return tracks[p_track]->imported;
}

void Animation::track_set_enabled(int p_track, bool p_enabled) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	tracks[p_track]->enabled = p_enabled;
	emit_changed();
}

bool Animation::track_is_enabled(int p_track) const {
	ERR_FAIL_INDEX_V(p_track, tracks.size(), false);
	return tracks[p_track]->enabled;
}

void Animation::track_move_up(int p_track) {
	if (p_track >= 0 && p_track < (tracks.size() - 1)) {
		SWAP(tracks.write[p_track], tracks.write[p_track + 1]);
	}

	emit_changed();
}

void Animation::track_move_down(int p_track) {
	if (p_track > 0 && p_track < tracks.size()) {
		SWAP(tracks.write[p_track], tracks.write[p_track - 1]);
	}

	emit_changed();
}

void Animation::track_move_to(int p_track, int p_to_index) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	ERR_FAIL_INDEX(p_to_index, tracks.size() + 1);
	if (p_track == p_to_index || p_track == p_to_index - 1) {
		return;
	}

	Track *track = tracks.get(p_track);
	tracks.remove_at(p_track);
	// Take into account that the position of the tracks that come after the one removed will change.
	tracks.insert(p_to_index > p_track ? p_to_index - 1 : p_to_index, track);

	emit_changed();
}

void Animation::track_swap(int p_track, int p_with_track) {
	ERR_FAIL_INDEX(p_track, tracks.size());
	ERR_FAIL_INDEX(p_with_track, tracks.size());
	if (p_track == p_with_track) {
		return;
	}
	SWAP(tracks.write[p_track], tracks.write[p_with_track]);

	emit_changed();
}

void Animation::set_step(real_t p_step) {
	step = p_step;
	emit_changed();
}

real_t Animation::get_step() const {
	return step;
}

void Animation::copy_track(int p_track, Ref<Animation> p_to_animation) {
	ERR_FAIL_COND(p_to_animation.is_null());
	ERR_FAIL_INDEX(p_track, get_track_count());
	int dst_track = p_to_animation->get_track_count();
	p_to_animation->add_track(track_get_type(p_track));

	p_to_animation->track_set_path(dst_track, track_get_path(p_track));
	p_to_animation->track_set_imported(dst_track, track_is_imported(p_track));
	p_to_animation->track_set_enabled(dst_track, track_is_enabled(p_track));
	p_to_animation->track_set_interpolation_type(dst_track, track_get_interpolation_type(p_track));
	p_to_animation->track_set_interpolation_loop_wrap(dst_track, track_get_interpolation_loop_wrap(p_track));
	if (track_get_type(p_track) == TYPE_VALUE) {
		p_to_animation->value_track_set_update_mode(dst_track, value_track_get_update_mode(p_track));
	}

	for (int i = 0; i < track_get_key_count(p_track); i++) {
		p_to_animation->track_insert_key(dst_track, track_get_key_time(p_track, i), track_get_key_value(p_track, i), track_get_key_transition(p_track, i));
	}
}

void Animation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_track", "type", "at_position"), &Animation::add_track, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("remove_track", "track_idx"), &Animation::remove_track);
	ClassDB::bind_method(D_METHOD("get_track_count"), &Animation::get_track_count);
	ClassDB::bind_method(D_METHOD("track_get_type", "track_idx"), &Animation::track_get_type);
	ClassDB::bind_method(D_METHOD("track_get_path", "track_idx"), &Animation::track_get_path);
	ClassDB::bind_method(D_METHOD("track_set_path", "track_idx", "path"), &Animation::track_set_path);
	ClassDB::bind_method(D_METHOD("find_track", "path", "type"), &Animation::find_track);

	ClassDB::bind_method(D_METHOD("track_move_up", "track_idx"), &Animation::track_move_up);
	ClassDB::bind_method(D_METHOD("track_move_down", "track_idx"), &Animation::track_move_down);
	ClassDB::bind_method(D_METHOD("track_move_to", "track_idx", "to_idx"), &Animation::track_move_to);
	ClassDB::bind_method(D_METHOD("track_swap", "track_idx", "with_idx"), &Animation::track_swap);

	ClassDB::bind_method(D_METHOD("track_set_imported", "track_idx", "imported"), &Animation::track_set_imported);
	ClassDB::bind_method(D_METHOD("track_is_imported", "track_idx"), &Animation::track_is_imported);

	ClassDB::bind_method(D_METHOD("track_set_enabled", "track_idx", "enabled"), &Animation::track_set_enabled);
	ClassDB::bind_method(D_METHOD("track_is_enabled", "track_idx"), &Animation::track_is_enabled);

	ClassDB::bind_method(D_METHOD("track_insert_key", "track_idx", "time", "key", "transition"), &Animation::track_insert_key, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("track_remove_key", "track_idx", "key_idx"), &Animation::track_remove_key);
	ClassDB::bind_method(D_METHOD("track_remove_key_at_time", "track_idx", "time"), &Animation::track_remove_key_at_time);
	ClassDB::bind_method(D_METHOD("track_set_key_value", "track_idx", "key", "value"), &Animation::track_set_key_value);
	ClassDB::bind_method(D_METHOD("track_set_key_transition", "track_idx", "key_idx", "transition"), &Animation::track_set_key_transition);
	ClassDB::bind_method(D_METHOD("track_set_key_time", "track_idx", "key_idx", "time"), &Animation::track_set_key_time);
	ClassDB::bind_method(D_METHOD("track_get_key_transition", "track_idx", "key_idx"), &Animation::track_get_key_transition);

	ClassDB::bind_method(D_METHOD("track_get_key_count", "track_idx"), &Animation::track_get_key_count);
	ClassDB::bind_method(D_METHOD("track_get_key_value", "track_idx", "key_idx"), &Animation::track_get_key_value);
	ClassDB::bind_method(D_METHOD("track_get_key_time", "track_idx", "key_idx"), &Animation::track_get_key_time);
	ClassDB::bind_method(D_METHOD("track_find_key", "track_idx", "time", "find_mode", "limit", "backward"), &Animation::track_find_key, DEFVAL(FIND_MODE_NEAREST), DEFVAL(false), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("track_set_interpolation_type", "track_idx", "interpolation"), &Animation::track_set_interpolation_type);
	ClassDB::bind_method(D_METHOD("track_get_interpolation_type", "track_idx"), &Animation::track_get_interpolation_type);

	ClassDB::bind_method(D_METHOD("track_set_interpolation_loop_wrap", "track_idx", "interpolation"), &Animation::track_set_interpolation_loop_wrap);
	ClassDB::bind_method(D_METHOD("track_get_interpolation_loop_wrap", "track_idx"), &Animation::track_get_interpolation_loop_wrap);

	ClassDB::bind_method(D_METHOD("value_track_set_update_mode", "track_idx", "mode"), &Animation::value_track_set_update_mode);
	ClassDB::bind_method(D_METHOD("value_track_get_update_mode", "track_idx"), &Animation::value_track_get_update_mode);

	ClassDB::bind_method(D_METHOD("value_track_interpolate", "track_idx", "time_sec", "backward"), &Animation::value_track_interpolate, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("method_track_get_name", "track_idx", "key_idx"), &Animation::method_track_get_name);
	ClassDB::bind_method(D_METHOD("method_track_get_params", "track_idx", "key_idx"), &Animation::method_track_get_params);

	ClassDB::bind_method(D_METHOD("bezier_track_insert_key", "track_idx", "time", "value", "in_handle", "out_handle"), &Animation::bezier_track_insert_key, DEFVAL(Vector2()), DEFVAL(Vector2()));

	ClassDB::bind_method(D_METHOD("bezier_track_set_key_value", "track_idx", "key_idx", "value"), &Animation::bezier_track_set_key_value);
	ClassDB::bind_method(D_METHOD("bezier_track_set_key_in_handle", "track_idx", "key_idx", "in_handle", "balanced_value_time_ratio"), &Animation::bezier_track_set_key_in_handle, DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("bezier_track_set_key_out_handle", "track_idx", "key_idx", "out_handle", "balanced_value_time_ratio"), &Animation::bezier_track_set_key_out_handle, DEFVAL(1.0));

	ClassDB::bind_method(D_METHOD("bezier_track_get_key_value", "track_idx", "key_idx"), &Animation::bezier_track_get_key_value);
	ClassDB::bind_method(D_METHOD("bezier_track_get_key_in_handle", "track_idx", "key_idx"), &Animation::bezier_track_get_key_in_handle);
	ClassDB::bind_method(D_METHOD("bezier_track_get_key_out_handle", "track_idx", "key_idx"), &Animation::bezier_track_get_key_out_handle);

	ClassDB::bind_method(D_METHOD("bezier_track_interpolate", "track_idx", "time"), &Animation::bezier_track_interpolate);

	ClassDB::bind_method(D_METHOD("audio_track_insert_key", "track_idx", "time", "stream", "start_offset", "end_offset"), &Animation::audio_track_insert_key, DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("audio_track_set_key_stream", "track_idx", "key_idx", "stream"), &Animation::audio_track_set_key_stream);
	ClassDB::bind_method(D_METHOD("audio_track_set_key_start_offset", "track_idx", "key_idx", "offset"), &Animation::audio_track_set_key_start_offset);
	ClassDB::bind_method(D_METHOD("audio_track_set_key_end_offset", "track_idx", "key_idx", "offset"), &Animation::audio_track_set_key_end_offset);
	ClassDB::bind_method(D_METHOD("audio_track_get_key_stream", "track_idx", "key_idx"), &Animation::audio_track_get_key_stream);
	ClassDB::bind_method(D_METHOD("audio_track_get_key_start_offset", "track_idx", "key_idx"), &Animation::audio_track_get_key_start_offset);
	ClassDB::bind_method(D_METHOD("audio_track_get_key_end_offset", "track_idx", "key_idx"), &Animation::audio_track_get_key_end_offset);
	ClassDB::bind_method(D_METHOD("audio_track_set_use_blend", "track_idx", "enable"), &Animation::audio_track_set_use_blend);
	ClassDB::bind_method(D_METHOD("audio_track_is_use_blend", "track_idx"), &Animation::audio_track_is_use_blend);

	ClassDB::bind_method(D_METHOD("animation_track_insert_key", "track_idx", "time", "animation"), &Animation::animation_track_insert_key);
	ClassDB::bind_method(D_METHOD("animation_track_set_key_animation", "track_idx", "key_idx", "animation"), &Animation::animation_track_set_key_animation);
	ClassDB::bind_method(D_METHOD("animation_track_get_key_animation", "track_idx", "key_idx"), &Animation::animation_track_get_key_animation);

	ClassDB::bind_method(D_METHOD("set_length", "time_sec"), &Animation::set_length);
	ClassDB::bind_method(D_METHOD("get_length"), &Animation::get_length);

	ClassDB::bind_method(D_METHOD("set_loop_mode", "loop_mode"), &Animation::set_loop_mode);
	ClassDB::bind_method(D_METHOD("get_loop_mode"), &Animation::get_loop_mode);

	ClassDB::bind_method(D_METHOD("set_step", "size_sec"), &Animation::set_step);
	ClassDB::bind_method(D_METHOD("get_step"), &Animation::get_step);

	ClassDB::bind_method(D_METHOD("clear"), &Animation::clear);
	ClassDB::bind_method(D_METHOD("copy_track", "track_idx", "to_animation"), &Animation::copy_track);

	ClassDB::bind_method(D_METHOD("is_capture_included"), &Animation::is_capture_included);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "length", PROPERTY_HINT_RANGE, "0.001,99999,0.001,suffix:s"), "set_length", "get_length");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_mode", PROPERTY_HINT_ENUM, "None,Linear,Ping-Pong"), "set_loop_mode", "get_loop_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "step", PROPERTY_HINT_RANGE, "0,4096,0.001,suffix:s"), "set_step", "get_step");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "capture_included", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "", "is_capture_included");

	BIND_ENUM_CONSTANT(TYPE_VALUE);
	BIND_ENUM_CONSTANT(TYPE_METHOD);
	BIND_ENUM_CONSTANT(TYPE_BEZIER);
	BIND_ENUM_CONSTANT(TYPE_AUDIO);
	BIND_ENUM_CONSTANT(TYPE_ANIMATION);

	BIND_ENUM_CONSTANT(INTERPOLATION_NEAREST);
	BIND_ENUM_CONSTANT(INTERPOLATION_LINEAR);
	BIND_ENUM_CONSTANT(INTERPOLATION_CUBIC);
	BIND_ENUM_CONSTANT(INTERPOLATION_LINEAR_ANGLE);
	BIND_ENUM_CONSTANT(INTERPOLATION_CUBIC_ANGLE);

	BIND_ENUM_CONSTANT(UPDATE_CONTINUOUS);
	BIND_ENUM_CONSTANT(UPDATE_DISCRETE);
	BIND_ENUM_CONSTANT(UPDATE_CAPTURE);

	BIND_ENUM_CONSTANT(LOOP_NONE);
	BIND_ENUM_CONSTANT(LOOP_LINEAR);
	BIND_ENUM_CONSTANT(LOOP_PINGPONG);

	BIND_ENUM_CONSTANT(LOOPED_FLAG_NONE);
	BIND_ENUM_CONSTANT(LOOPED_FLAG_END);
	BIND_ENUM_CONSTANT(LOOPED_FLAG_START);

	BIND_ENUM_CONSTANT(FIND_MODE_NEAREST);
	BIND_ENUM_CONSTANT(FIND_MODE_APPROX);
	BIND_ENUM_CONSTANT(FIND_MODE_EXACT);
}

void Animation::clear() {
	for (int i = 0; i < tracks.size(); i++) {
		memdelete(tracks[i]);
	}
	tracks.clear();
	loop_mode = LOOP_NONE;
	length = 1;
	emit_changed();
}

bool Animation::_float_track_optimize_key(const TKey<float> t0, const TKey<float> t1, const TKey<float> t2, real_t p_allowed_velocity_err, real_t p_allowed_precision_error) {
	// Remove overlapping keys.
	if (Math::is_equal_approx(t0.time, t1.time) || Math::is_equal_approx(t1.time, t2.time)) {
		return true;
	}
	if (abs(t0.value - t1.value) < p_allowed_precision_error && abs(t1.value - t2.value) < p_allowed_precision_error) {
		return true;
	}
	// Calc velocities.
	double v0 = (t1.value - t0.value) / (t1.time - t0.time);
	double v1 = (t2.value - t1.value) / (t2.time - t1.time);
	// Avoid zero div but check equality.
	if (abs(v0 - v1) < p_allowed_precision_error) {
		return true;
	} else if (abs(v0) < p_allowed_precision_error || abs(v1) < p_allowed_precision_error) {
		return false;
	}
	if (!signbit(v0 * v1)) {
		v0 = abs(v0);
		v1 = abs(v1);
		double ratio = v0 < v1 ? v0 / v1 : v1 / v0;
		if (ratio >= 1.0 - p_allowed_velocity_err) {
			return true;
		}
	}
	return false;
}

bool Animation::_vector2_track_optimize_key(const TKey<Vector2> t0, const TKey<Vector2> t1, const TKey<Vector2> t2, real_t p_allowed_velocity_err, real_t p_allowed_angular_error, real_t p_allowed_precision_error) {
	// Remove overlapping keys.
	if (Math::is_equal_approx(t0.time, t1.time) || Math::is_equal_approx(t1.time, t2.time)) {
		return true;
	}
	if ((t0.value - t1.value).length() < p_allowed_precision_error && (t1.value - t2.value).length() < p_allowed_precision_error) {
		return true;
	}
	// Calc velocities.
	Vector2 vc0 = (t1.value - t0.value) / (t1.time - t0.time);
	Vector2 vc1 = (t2.value - t1.value) / (t2.time - t1.time);
	double v0 = vc0.length();
	double v1 = vc1.length();
	// Avoid zero div but check equality.
	if (abs(v0 - v1) < p_allowed_precision_error) {
		return true;
	} else if (abs(v0) < p_allowed_precision_error || abs(v1) < p_allowed_precision_error) {
		return false;
	}
	// Check axis.
	if (vc0.normalized().dot(vc1.normalized()) >= 1.0 - p_allowed_angular_error * 2.0) {
		v0 = abs(v0);
		v1 = abs(v1);
		double ratio = v0 < v1 ? v0 / v1 : v1 / v0;
		if (ratio >= 1.0 - p_allowed_velocity_err) {
			return true;
		}
	}
	return false;
}

bool Animation::_vector3_track_optimize_key(const TKey<Vector3> t0, const TKey<Vector3> t1, const TKey<Vector3> t2, real_t p_allowed_velocity_err, real_t p_allowed_angular_error, real_t p_allowed_precision_error) {
	// Remove overlapping keys.
	if (Math::is_equal_approx(t0.time, t1.time) || Math::is_equal_approx(t1.time, t2.time)) {
		return true;
	}
	if ((t0.value - t1.value).length() < p_allowed_precision_error && (t1.value - t2.value).length() < p_allowed_precision_error) {
		return true;
	}
	// Calc velocities.
	Vector3 vc0 = (t1.value - t0.value) / (t1.time - t0.time);
	Vector3 vc1 = (t2.value - t1.value) / (t2.time - t1.time);
	double v0 = vc0.length();
	double v1 = vc1.length();
	// Avoid zero div but check equality.
	if (abs(v0 - v1) < p_allowed_precision_error) {
		return true;
	} else if (abs(v0) < p_allowed_precision_error || abs(v1) < p_allowed_precision_error) {
		return false;
	}
	// Check axis.
	if (vc0.normalized().dot(vc1.normalized()) >= 1.0 - p_allowed_angular_error * 2.0) {
		v0 = abs(v0);
		v1 = abs(v1);
		double ratio = v0 < v1 ? v0 / v1 : v1 / v0;
		if (ratio >= 1.0 - p_allowed_velocity_err) {
			return true;
		}
	}
	return false;
}

bool Animation::_quaternion_track_optimize_key(const TKey<Quaternion> t0, const TKey<Quaternion> t1, const TKey<Quaternion> t2, real_t p_allowed_velocity_err, real_t p_allowed_angular_error, real_t p_allowed_precision_error) {
	// Remove overlapping keys.
	if (Math::is_equal_approx(t0.time, t1.time) || Math::is_equal_approx(t1.time, t2.time)) {
		return true;
	}
	if ((t0.value - t1.value).length() < p_allowed_precision_error && (t1.value - t2.value).length() < p_allowed_precision_error) {
		return true;
	}
	// Check axis.
	Quaternion q0 = t0.value * t1.value * t0.value.inverse();
	Quaternion q1 = t1.value * t2.value * t1.value.inverse();
	if (q0.get_axis().dot(q1.get_axis()) >= 1.0 - p_allowed_angular_error * 2.0) {
		double a0 = Math::acos(t0.value.dot(t1.value));
		double a1 = Math::acos(t1.value.dot(t2.value));
		if (a0 + a1 >= Math_PI / 2) {
			return false; // Rotation is more than 180 deg, keep key.
		}
		// Calc velocities.
		double v0 = a0 / (t1.time - t0.time);
		double v1 = a1 / (t2.time - t1.time);
		// Avoid zero div but check equality.
		if (abs(v0 - v1) < p_allowed_precision_error) {
			return true;
		} else if (abs(v0) < p_allowed_precision_error || abs(v1) < p_allowed_precision_error) {
			return false;
		}
		double ratio = v0 < v1 ? v0 / v1 : v1 / v0;
		if (ratio >= 1.0 - p_allowed_velocity_err) {
			return true;
		}
	}
	return false;
}

void Animation::_value_track_optimize(int p_idx, real_t p_allowed_velocity_err, real_t p_allowed_angular_err, real_t p_allowed_precision_error) {
	ERR_FAIL_INDEX(p_idx, tracks.size());
	ERR_FAIL_COND(tracks[p_idx]->type != TYPE_VALUE);
	ValueTrack *vt = static_cast<ValueTrack *>(tracks[p_idx]);
	if (vt->values.size() == 0) {
		return;
	}
	Variant::Type type = vt->values[0].value.get_type();

	// Special case for angle interpolation.
	bool is_using_angle = vt->interpolation == Animation::INTERPOLATION_LINEAR_ANGLE || vt->interpolation == Animation::INTERPOLATION_CUBIC_ANGLE;
	int i = 0;
	while (i < vt->values.size() - 2) {
		bool erase = false;
		switch (type) {
			case Variant::FLOAT: {
				TKey<float> t0;
				TKey<float> t1;
				TKey<float> t2;
				t0.time = vt->values[i].time;
				t1.time = vt->values[i + 1].time;
				t2.time = vt->values[i + 2].time;
				t0.value = vt->values[i].value;
				t1.value = vt->values[i + 1].value;
				t2.value = vt->values[i + 2].value;
				if (is_using_angle) {
					float diff1 = fmod(t1.value - t0.value, Math_TAU);
					t1.value = t0.value + fmod(2.0 * diff1, Math_TAU) - diff1;
					float diff2 = fmod(t2.value - t1.value, Math_TAU);
					t2.value = t1.value + fmod(2.0 * diff2, Math_TAU) - diff2;
					if (abs(abs(diff1) + abs(diff2)) >= Math_PI) {
						break; // Rotation is more than 180 deg, keep key.
					}
				}
				erase = _float_track_optimize_key(t0, t1, t2, p_allowed_velocity_err, p_allowed_precision_error);
			} break;
			case Variant::VECTOR2: {
				TKey<Vector2> t0;
				TKey<Vector2> t1;
				TKey<Vector2> t2;
				t0.time = vt->values[i].time;
				t1.time = vt->values[i + 1].time;
				t2.time = vt->values[i + 2].time;
				t0.value = vt->values[i].value;
				t1.value = vt->values[i + 1].value;
				t2.value = vt->values[i + 2].value;
				erase = _vector2_track_optimize_key(t0, t1, t2, p_allowed_velocity_err, p_allowed_angular_err, p_allowed_precision_error);
			} break;
			case Variant::VECTOR3: {
				TKey<Vector3> t0;
				TKey<Vector3> t1;
				TKey<Vector3> t2;
				t0.time = vt->values[i].time;
				t1.time = vt->values[i + 1].time;
				t2.time = vt->values[i + 2].time;
				t0.value = vt->values[i].value;
				t1.value = vt->values[i + 1].value;
				t2.value = vt->values[i + 2].value;
				erase = _vector3_track_optimize_key(t0, t1, t2, p_allowed_velocity_err, p_allowed_angular_err, p_allowed_precision_error);
			} break;
			case Variant::QUATERNION: {
				TKey<Quaternion> t0;
				TKey<Quaternion> t1;
				TKey<Quaternion> t2;
				t0.time = vt->values[i].time;
				t1.time = vt->values[i + 1].time;
				t2.time = vt->values[i + 2].time;
				t0.value = vt->values[i].value;
				t1.value = vt->values[i + 1].value;
				t2.value = vt->values[i + 2].value;
				erase = _quaternion_track_optimize_key(t0, t1, t2, p_allowed_velocity_err, p_allowed_angular_err, p_allowed_precision_error);
			} break;
			default: {
			} break;
		}

		if (erase) {
			vt->values.remove_at(i + 1);
		} else {
			i++;
		}
	}

	if (vt->values.size() == 2) {
		bool single_key = false;
		switch (type) {
			case Variant::FLOAT: {
				float val_0 = vt->values[0].value;
				float val_1 = vt->values[1].value;
				if (is_using_angle) {
					float diff1 = fmod(val_1 - val_0, Math_TAU);
					val_1 = val_0 + fmod(2.0 * diff1, Math_TAU) - diff1;
				}
				single_key = abs(val_0 - val_1) < p_allowed_precision_error;
			} break;
			case Variant::VECTOR2: {
				Vector2 val_0 = vt->values[0].value;
				Vector2 val_1 = vt->values[1].value;
				single_key = (val_0 - val_1).length() < p_allowed_precision_error;
			} break;
			case Variant::VECTOR3: {
				Vector3 val_0 = vt->values[0].value;
				Vector3 val_1 = vt->values[1].value;
				single_key = (val_0 - val_1).length() < p_allowed_precision_error;
			} break;
			case Variant::QUATERNION: {
				Quaternion val_0 = vt->values[0].value;
				Quaternion val_1 = vt->values[1].value;
				single_key = (val_0 - val_1).length() < p_allowed_precision_error;
			} break;
			default: {
			} break;
		}
		if (single_key) {
			vt->values.remove_at(1);
		}
	}
}

void Animation::optimize(real_t p_allowed_velocity_err, real_t p_allowed_angular_err, int p_precision) {
	real_t precision = Math::pow(0.1, p_precision);
	for (int i = 0; i < tracks.size(); i++) {
		if (tracks[i]->type == TYPE_VALUE) {
			_value_track_optimize(i, p_allowed_velocity_err, p_allowed_angular_err, precision);
		}
	}
}

#define print_animc(m_str)
//#define print_animc(m_str) print_line(m_str);

// Helper math functions for Variant.
bool Animation::is_variant_interpolatable(const Variant p_value) {
	Variant::Type type = p_value.get_type();
	return (type >= Variant::BOOL && type <= Variant::STRING_NAME) || type == Variant::ARRAY || type >= Variant::PACKED_INT32_ARRAY; // PackedByteArray is unsigned, so it would be better to ignore since blending uses float.
}

Variant Animation::cast_to_blendwise(const Variant p_value) {
	switch (p_value.get_type()) {
		case Variant::BOOL:
		case Variant::INT: {
			return p_value.operator double();
		} break;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			return string_to_array(p_value);
		} break;
		case Variant::RECT2I: {
			return p_value.operator Rect2();
		} break;
		case Variant::VECTOR2I: {
			return p_value.operator Vector2();
		} break;
		case Variant::VECTOR3I: {
			return p_value.operator Vector3();
		} break;
		case Variant::VECTOR4I: {
			return p_value.operator Vector4();
		} break;
		case Variant::PACKED_INT32_ARRAY: {
			return p_value.operator PackedFloat32Array();
		} break;
		case Variant::PACKED_INT64_ARRAY: {
			return p_value.operator PackedFloat64Array();
		} break;
		default: {
		} break;
	}
	return p_value;
}

Variant Animation::cast_from_blendwise(const Variant p_value, const Variant::Type p_type) {
	switch (p_type) {
		case Variant::BOOL: {
			return p_value.operator real_t() >= 0.5;
		} break;
		case Variant::INT: {
			return (int64_t)Math::round(p_value.operator double());
		} break;
		case Variant::STRING: {
			return array_to_string(p_value);
		} break;
		case Variant::STRING_NAME: {
			return StringName(array_to_string(p_value));
		} break;
		case Variant::RECT2I: {
			return Rect2i(p_value.operator Rect2().round());
		} break;
		case Variant::VECTOR2I: {
			return Vector2i(p_value.operator Vector2().round());
		} break;
		case Variant::VECTOR3I: {
			return Vector3i(p_value.operator Vector3().round());
		} break;
		case Variant::VECTOR4I: {
			return Vector4i(p_value.operator Vector4().round());
		} break;
		case Variant::PACKED_INT32_ARRAY: {
			PackedFloat32Array old_val = p_value.operator PackedFloat32Array();
			PackedInt32Array new_val;
			new_val.resize(old_val.size());
			int *new_val_w = new_val.ptrw();
			for (int i = 0; i < old_val.size(); i++) {
				new_val_w[i] = (int32_t)Math::round(old_val[i]);
			}
			return new_val;
		} break;
		case Variant::PACKED_INT64_ARRAY: {
			PackedFloat64Array old_val = p_value.operator PackedFloat64Array();
			PackedInt64Array new_val;
			for (int i = 0; i < old_val.size(); i++) {
				new_val.push_back((int64_t)Math::round(old_val[i]));
			}
			return new_val;
		} break;
		default: {
		} break;
	}
	return p_value;
}

Variant Animation::string_to_array(const Variant p_value) {
	if (!p_value.is_string()) {
		return p_value;
	};
	const String &str = p_value.operator String();
	PackedFloat32Array arr;
	for (int i = 0; i < str.length(); i++) {
		arr.push_back((float)str[i]);
	}
	return arr;
}

Variant Animation::array_to_string(const Variant p_value) {
	if (!p_value.is_array()) {
		return p_value;
	};
	const PackedFloat32Array &arr = p_value.operator PackedFloat32Array();
	String str;
	for (int i = 0; i < arr.size(); i++) {
		char32_t c = (char32_t)Math::round(arr[i]);
		if (c == 0 || (c & 0xfffff800) == 0xd800 || c > 0x10ffff) {
			c = ' ';
		}
		str += c;
	}
	return str;
}

Variant Animation::add_variant(const Variant &a, const Variant &b) {
	if (a.get_type() != b.get_type()) {
		if (a.is_num() && b.is_num()) {
			return add_variant(cast_to_blendwise(a), cast_to_blendwise(b));
		} else if (!a.is_array()) {
			return a;
		}
	}

	switch (a.get_type()) {
		case Variant::NIL: {
			return Variant();
		} break;
		case Variant::FLOAT: {
			return (a.operator double()) + (b.operator double());
		} break;
		case Variant::RECT2: {
			const Rect2 ra = a.operator Rect2();
			const Rect2 rb = b.operator Rect2();
			return Rect2(ra.position + rb.position, ra.size + rb.size);
		} break;
		case Variant::PLANE: {
			const Plane pa = a.operator Plane();
			const Plane pb = b.operator Plane();
			return Plane(pa.normal + pb.normal, pa.d + pb.d);
		} break;
		case Variant::AABB: {
			const ::AABB aa = a.operator ::AABB();
			const ::AABB ab = b.operator ::AABB();
			return ::AABB(aa.position + ab.position, aa.size + ab.size);
		} break;
		case Variant::BASIS: {
			return (a.operator Basis()) * (b.operator Basis());
		} break;
		case Variant::QUATERNION: {
			return (a.operator Quaternion()) * (b.operator Quaternion());
		} break;
		case Variant::TRANSFORM2D: {
			return (a.operator Transform2D()) * (b.operator Transform2D());
		} break;
		case Variant::TRANSFORM3D: {
			return (a.operator Transform3D()) * (b.operator Transform3D());
		} break;
		case Variant::INT:
		case Variant::RECT2I:
		case Variant::VECTOR2I:
		case Variant::VECTOR3I:
		case Variant::VECTOR4I:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY: {
			// Fallback the interpolatable value which needs casting.
			return cast_from_blendwise(add_variant(cast_to_blendwise(a), cast_to_blendwise(b)), a.get_type());
		} break;
		case Variant::BOOL:
		case Variant::STRING:
		case Variant::STRING_NAME: {
			// Specialized for Tween.
			return b;
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			// Skip.
		} break;
		default: {
			if (a.is_array()) {
				const Array arr_a = a.operator Array();
				const Array arr_b = b.operator Array();

				int min_size = arr_a.size();
				int max_size = arr_b.size();
				bool is_a_larger = inform_variant_array(min_size, max_size);

				Array result;
				result.set_typed(MAX(arr_a.get_typed_builtin(), arr_b.get_typed_builtin()), StringName(), Variant());
				result.resize(min_size);
				int i = 0;
				for (; i < min_size; i++) {
					result[i] = add_variant(arr_a[i], arr_b[i]);
				}
				if (min_size != max_size) {
					// Process with last element of the lesser array.
					// This is pretty funny and bizarre, but artists like to use it for polygon animation.
					Variant lesser_last;
					result.resize(max_size);
					if (is_a_larger) {
						if (i > 0) {
							lesser_last = arr_b[i - 1];
						} else {
							Variant vz = arr_a[i];
							vz.zero();
							lesser_last = vz;
						}
						for (; i < max_size; i++) {
							result[i] = add_variant(arr_a[i], lesser_last);
						}
					} else {
						if (i > 0) {
							lesser_last = arr_a[i - 1];
						} else {
							Variant vz = arr_b[i];
							vz.zero();
							lesser_last = vz;
						}
						for (; i < max_size; i++) {
							result[i] = add_variant(lesser_last, arr_b[i]);
						}
					}
				}
				return result;
			}
		} break;
	}
	return Variant::evaluate(Variant::OP_ADD, a, b);
}

Variant Animation::subtract_variant(const Variant &a, const Variant &b) {
	if (a.get_type() != b.get_type()) {
		if (a.is_num() && b.is_num()) {
			return subtract_variant(cast_to_blendwise(a), cast_to_blendwise(b));
		} else if (!a.is_array()) {
			return a;
		}
	}

	switch (a.get_type()) {
		case Variant::NIL: {
			return Variant();
		} break;
		case Variant::FLOAT: {
			return (a.operator double()) - (b.operator double());
		} break;
		case Variant::RECT2: {
			const Rect2 ra = a.operator Rect2();
			const Rect2 rb = b.operator Rect2();
			return Rect2(ra.position - rb.position, ra.size - rb.size);
		} break;
		case Variant::PLANE: {
			const Plane pa = a.operator Plane();
			const Plane pb = b.operator Plane();
			return Plane(pa.normal - pb.normal, pa.d - pb.d);
		} break;
		case Variant::AABB: {
			const ::AABB aa = a.operator ::AABB();
			const ::AABB ab = b.operator ::AABB();
			return ::AABB(aa.position - ab.position, aa.size - ab.size);
		} break;
		case Variant::BASIS: {
			return (b.operator Basis()).inverse() * (a.operator Basis());
		} break;
		case Variant::QUATERNION: {
			return (b.operator Quaternion()).inverse() * (a.operator Quaternion());
		} break;
		case Variant::TRANSFORM2D: {
			return (b.operator Transform2D()).affine_inverse() * (a.operator Transform2D());
		} break;
		case Variant::TRANSFORM3D: {
			return (b.operator Transform3D()).affine_inverse() * (a.operator Transform3D());
		} break;
		case Variant::INT:
		case Variant::RECT2I:
		case Variant::VECTOR2I:
		case Variant::VECTOR3I:
		case Variant::VECTOR4I:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY: {
			// Fallback the interpolatable value which needs casting.
			return cast_from_blendwise(subtract_variant(cast_to_blendwise(a), cast_to_blendwise(b)), a.get_type());
		} break;
		case Variant::BOOL:
		case Variant::STRING:
		case Variant::STRING_NAME: {
			// Specialized for Tween.
			return a;
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			// Skip.
		} break;
		default: {
			if (a.is_array()) {
				const Array arr_a = a.operator Array();
				const Array arr_b = b.operator Array();

				int min_size = arr_a.size();
				int max_size = arr_b.size();
				bool is_a_larger = inform_variant_array(min_size, max_size);

				Array result;
				result.set_typed(MAX(arr_a.get_typed_builtin(), arr_b.get_typed_builtin()), StringName(), Variant());
				result.resize(min_size);
				int i = 0;
				for (; i < min_size; i++) {
					result[i] = subtract_variant(arr_a[i], arr_b[i]);
				}
				if (min_size != max_size) {
					// Process with last element of the lesser array.
					// This is pretty funny and bizarre, but artists like to use it for polygon animation.
					Variant lesser_last;
					result.resize(max_size);
					if (is_a_larger) {
						if (i > 0) {
							lesser_last = arr_b[i - 1];
						} else {
							Variant vz = arr_a[i];
							vz.zero();
							lesser_last = vz;
						}
						for (; i < max_size; i++) {
							result[i] = subtract_variant(arr_a[i], lesser_last);
						}
					} else {
						if (i > 0) {
							lesser_last = arr_a[i - 1];
						} else {
							Variant vz = arr_b[i];
							vz.zero();
							lesser_last = vz;
						}
						for (; i < max_size; i++) {
							result[i] = subtract_variant(lesser_last, arr_b[i]);
						}
					}
				}
				return result;
			}
		} break;
	}
	return Variant::evaluate(Variant::OP_SUBTRACT, a, b);
}

Variant Animation::blend_variant(const Variant &a, const Variant &b, float c) {
	if (a.get_type() != b.get_type()) {
		if (a.is_num() && b.is_num()) {
			return blend_variant(cast_to_blendwise(a), cast_to_blendwise(b), c);
		} else if (!a.is_array()) {
			return a;
		}
	}

	switch (a.get_type()) {
		case Variant::NIL: {
			return Variant();
		} break;
		case Variant::FLOAT: {
			return (a.operator double()) + (b.operator double()) * c;
		} break;
		case Variant::VECTOR2: {
			return (a.operator Vector2()) + (b.operator Vector2()) * c;
		} break;
		case Variant::RECT2: {
			const Rect2 ra = a.operator Rect2();
			const Rect2 rb = b.operator Rect2();
			return Rect2(ra.position + rb.position * c, ra.size + rb.size * c);
		} break;
		case Variant::VECTOR3: {
			return (a.operator Vector3()) + (b.operator Vector3()) * c;
		} break;
		case Variant::VECTOR4: {
			return (a.operator Vector4()) + (b.operator Vector4()) * c;
		} break;
		case Variant::PLANE: {
			const Plane pa = a.operator Plane();
			const Plane pb = b.operator Plane();
			return Plane(pa.normal + pb.normal * c, pa.d + pb.d * c);
		} break;
		case Variant::COLOR: {
			return (a.operator Color()) + (b.operator Color()) * c;
		} break;
		case Variant::AABB: {
			const ::AABB aa = a.operator ::AABB();
			const ::AABB ab = b.operator ::AABB();
			return ::AABB(aa.position + ab.position * c, aa.size + ab.size * c);
		} break;
		case Variant::BASIS: {
			return (a.operator Basis()) + (b.operator Basis()) * c;
		} break;
		case Variant::QUATERNION: {
			return (a.operator Quaternion()) * Quaternion().slerp((b.operator Quaternion()), c);
		} break;
		case Variant::TRANSFORM2D: {
			return (a.operator Transform2D()) * Transform2D().interpolate_with((b.operator Transform2D()), c);
		} break;
		case Variant::TRANSFORM3D: {
			return (a.operator Transform3D()) * Transform3D().interpolate_with((b.operator Transform3D()), c);
		} break;
		case Variant::BOOL:
		case Variant::INT:
		case Variant::RECT2I:
		case Variant::VECTOR2I:
		case Variant::VECTOR3I:
		case Variant::VECTOR4I:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY: {
			// Fallback the interpolatable value which needs casting.
			return cast_from_blendwise(blend_variant(cast_to_blendwise(a), cast_to_blendwise(b), c), a.get_type());
		} break;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			Array arr_a = cast_to_blendwise(a);
			Array arr_b = cast_to_blendwise(b);
			int min_size = arr_a.size();
			int max_size = arr_b.size();
			bool is_a_larger = inform_variant_array(min_size, max_size);
			int mid_size = interpolate_variant(arr_a.size(), arr_b.size(), c);
			if (is_a_larger) {
				arr_a.resize(mid_size);
			} else {
				arr_b.resize(mid_size);
			}
			return cast_from_blendwise(blend_variant(arr_a, arr_b, c), a.get_type());
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			// Skip.
		} break;
		default: {
			if (a.is_array()) {
				const Array arr_a = a.operator Array();
				const Array arr_b = b.operator Array();

				int min_size = arr_a.size();
				int max_size = arr_b.size();
				bool is_a_larger = inform_variant_array(min_size, max_size);

				Array result;
				result.set_typed(MAX(arr_a.get_typed_builtin(), arr_b.get_typed_builtin()), StringName(), Variant());
				result.resize(min_size);
				int i = 0;
				for (; i < min_size; i++) {
					result[i] = blend_variant(arr_a[i], arr_b[i], c);
				}
				if (min_size != max_size) {
					// Process with last element of the lesser array.
					// This is pretty funny and bizarre, but artists like to use it for polygon animation.
					Variant lesser_last;
					if (is_a_larger && !Math::is_equal_approx(c, 1.0f)) {
						result.resize(max_size);
						if (i > 0) {
							lesser_last = arr_b[i - 1];
						} else {
							Variant vz = arr_a[i];
							vz.zero();
							lesser_last = vz;
						}
						for (; i < max_size; i++) {
							result[i] = blend_variant(arr_a[i], lesser_last, c);
						}
					} else if (!is_a_larger && !Math::is_zero_approx(c)) {
						result.resize(max_size);
						if (i > 0) {
							lesser_last = arr_a[i - 1];
						} else {
							Variant vz = arr_b[i];
							vz.zero();
							lesser_last = vz;
						}
						for (; i < max_size; i++) {
							result[i] = blend_variant(lesser_last, arr_b[i], c);
						}
					}
				}
				return result;
			}
		} break;
	}
	return c < 0.5 ? a : b;
}

Variant Animation::interpolate_variant(const Variant &a, const Variant &b, float c, bool p_snap_array_element) {
	if (a.get_type() != b.get_type()) {
		if (a.is_num() && b.is_num()) {
			return interpolate_variant(cast_to_blendwise(a), cast_to_blendwise(b), c);
		} else if (!a.is_array()) {
			return a;
		}
	}

	switch (a.get_type()) {
		case Variant::NIL: {
			return Variant();
		} break;
		case Variant::FLOAT: {
			return Math::lerp(a.operator double(), b.operator double(), (double)c);
		} break;
		case Variant::VECTOR2: {
			return (a.operator Vector2()).lerp(b.operator Vector2(), c);
		} break;
		case Variant::RECT2: {
			const Rect2 ra = a.operator Rect2();
			const Rect2 rb = b.operator Rect2();
			return Rect2(ra.position.lerp(rb.position, c), ra.size.lerp(rb.size, c));
		} break;
		case Variant::VECTOR3: {
			return (a.operator Vector3()).lerp(b.operator Vector3(), c);
		} break;
		case Variant::VECTOR4: {
			return (a.operator Vector4()).lerp(b.operator Vector4(), c);
		} break;
		case Variant::PLANE: {
			const Plane pa = a.operator Plane();
			const Plane pb = b.operator Plane();
			return Plane(pa.normal.lerp(pb.normal, c), Math::lerp((double)pa.d, (double)pb.d, (double)c));
		} break;
		case Variant::COLOR: {
			return (a.operator Color()).lerp(b.operator Color(), c);
		} break;
		case Variant::AABB: {
			const ::AABB aa = a.operator ::AABB();
			const ::AABB ab = b.operator ::AABB();
			return ::AABB(aa.position.lerp(ab.position, c), aa.size.lerp(ab.size, c));
		} break;
		case Variant::BASIS: {
			return (a.operator Basis()).lerp(b.operator Basis(), c);
		} break;
		case Variant::QUATERNION: {
			return (a.operator Quaternion()).slerp(b.operator Quaternion(), c);
		} break;
		case Variant::TRANSFORM2D: {
			return (a.operator Transform2D()).interpolate_with(b.operator Transform2D(), c);
		} break;
		case Variant::TRANSFORM3D: {
			return (a.operator Transform3D()).interpolate_with(b.operator Transform3D(), c);
		} break;
		case Variant::BOOL:
		case Variant::INT:
		case Variant::RECT2I:
		case Variant::VECTOR2I:
		case Variant::VECTOR3I:
		case Variant::VECTOR4I:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY: {
			// Fallback the interpolatable value which needs casting.
			return cast_from_blendwise(interpolate_variant(cast_to_blendwise(a), cast_to_blendwise(b), c), a.get_type());
		} break;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			Array arr_a = cast_to_blendwise(a);
			Array arr_b = cast_to_blendwise(b);
			int min_size = arr_a.size();
			int max_size = arr_b.size();
			bool is_a_larger = inform_variant_array(min_size, max_size);
			int mid_size = interpolate_variant(arr_a.size(), arr_b.size(), c);
			if (is_a_larger) {
				arr_a.resize(mid_size);
			} else {
				arr_b.resize(mid_size);
			}
			return cast_from_blendwise(interpolate_variant(arr_a, arr_b, c, true), a.get_type());
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			// Skip.
		} break;
		default: {
			if (a.is_array()) {
				const Array arr_a = a.operator Array();
				const Array arr_b = b.operator Array();

				int min_size = arr_a.size();
				int max_size = arr_b.size();
				bool is_a_larger = inform_variant_array(min_size, max_size);

				Array result;
				result.set_typed(MAX(arr_a.get_typed_builtin(), arr_b.get_typed_builtin()), StringName(), Variant());
				result.resize(min_size);
				int i = 0;
				for (; i < min_size; i++) {
					result[i] = interpolate_variant(arr_a[i], arr_b[i], c);
				}
				if (min_size != max_size) {
					// Process with last element of the lesser array.
					// This is pretty funny and bizarre, but artists like to use it for polygon animation.
					Variant lesser_last;
					if (is_a_larger && !Math::is_equal_approx(c, 1.0f)) {
						result.resize(max_size);
						if (p_snap_array_element) {
							c = 0;
						}
						if (i > 0) {
							lesser_last = arr_b[i - 1];
						} else {
							Variant vz = arr_a[i];
							vz.zero();
							lesser_last = vz;
						}
						for (; i < max_size; i++) {
							result[i] = interpolate_variant(arr_a[i], lesser_last, c);
						}
					} else if (!is_a_larger && !Math::is_zero_approx(c)) {
						result.resize(max_size);
						if (p_snap_array_element) {
							c = 1;
						}
						if (i > 0) {
							lesser_last = arr_a[i - 1];
						} else {
							Variant vz = arr_b[i];
							vz.zero();
							lesser_last = vz;
						}
						for (; i < max_size; i++) {
							result[i] = interpolate_variant(lesser_last, arr_b[i], c);
						}
					}
				}
				return result;
			}
		} break;
	}
	return c < 0.5 ? a : b;
}

Variant Animation::cubic_interpolate_in_time_variant(const Variant &pre_a, const Variant &a, const Variant &b, const Variant &post_b, float c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t, bool p_snap_array_element) {
	if (pre_a.get_type() != a.get_type() || pre_a.get_type() != b.get_type() || pre_a.get_type() != post_b.get_type()) {
		if (pre_a.is_num() && a.is_num() && b.is_num() && post_b.is_num()) {
			return cubic_interpolate_in_time_variant(cast_to_blendwise(pre_a), cast_to_blendwise(a), cast_to_blendwise(b), cast_to_blendwise(post_b), c, p_pre_a_t, p_b_t, p_post_b_t, p_snap_array_element);
		} else if (!a.is_array()) {
			return a;
		}
	}

	switch (a.get_type()) {
		case Variant::NIL: {
			return Variant();
		} break;
		case Variant::FLOAT: {
			return Math::cubic_interpolate_in_time(a.operator double(), b.operator double(), pre_a.operator double(), post_b.operator double(), (double)c, (double)p_b_t, (double)p_pre_a_t, (double)p_post_b_t);
		} break;
		case Variant::VECTOR2: {
			return (a.operator Vector2()).cubic_interpolate_in_time(b.operator Vector2(), pre_a.operator Vector2(), post_b.operator Vector2(), c, p_b_t, p_pre_a_t, p_post_b_t);
		} break;
		case Variant::RECT2: {
			const Rect2 rpa = pre_a.operator Rect2();
			const Rect2 ra = a.operator Rect2();
			const Rect2 rb = b.operator Rect2();
			const Rect2 rpb = post_b.operator Rect2();
			return Rect2(
					ra.position.cubic_interpolate_in_time(rb.position, rpa.position, rpb.position, c, p_b_t, p_pre_a_t, p_post_b_t),
					ra.size.cubic_interpolate_in_time(rb.size, rpa.size, rpb.size, c, p_b_t, p_pre_a_t, p_post_b_t));
		} break;
		case Variant::VECTOR3: {
			return (a.operator Vector3()).cubic_interpolate_in_time(b.operator Vector3(), pre_a.operator Vector3(), post_b.operator Vector3(), c, p_b_t, p_pre_a_t, p_post_b_t);
		} break;
		case Variant::VECTOR4: {
			return (a.operator Vector4()).cubic_interpolate_in_time(b.operator Vector4(), pre_a.operator Vector4(), post_b.operator Vector4(), c, p_b_t, p_pre_a_t, p_post_b_t);
		} break;
		case Variant::PLANE: {
			const Plane ppa = pre_a.operator Plane();
			const Plane pa = a.operator Plane();
			const Plane pb = b.operator Plane();
			const Plane ppb = post_b.operator Plane();
			return Plane(
					pa.normal.cubic_interpolate_in_time(pb.normal, ppa.normal, ppb.normal, c, p_b_t, p_pre_a_t, p_post_b_t),
					Math::cubic_interpolate_in_time((double)pa.d, (double)pb.d, (double)ppa.d, (double)ppb.d, (double)c, (double)p_b_t, (double)p_pre_a_t, (double)p_post_b_t));
		} break;
		case Variant::COLOR: {
			const Color cpa = pre_a.operator Color();
			const Color ca = a.operator Color();
			const Color cb = b.operator Color();
			const Color cpb = post_b.operator Color();
			return Color(
					Math::cubic_interpolate_in_time((double)ca.r, (double)cb.r, (double)cpa.r, (double)cpb.r, (double)c, (double)p_b_t, (double)p_pre_a_t, (double)p_post_b_t),
					Math::cubic_interpolate_in_time((double)ca.g, (double)cb.g, (double)cpa.g, (double)cpb.g, (double)c, (double)p_b_t, (double)p_pre_a_t, (double)p_post_b_t),
					Math::cubic_interpolate_in_time((double)ca.b, (double)cb.b, (double)cpa.b, (double)cpb.b, (double)c, (double)p_b_t, (double)p_pre_a_t, (double)p_post_b_t),
					Math::cubic_interpolate_in_time((double)ca.a, (double)cb.a, (double)cpa.a, (double)cpb.a, (double)c, (double)p_b_t, (double)p_pre_a_t, (double)p_post_b_t));
		} break;
		case Variant::AABB: {
			const ::AABB apa = pre_a.operator ::AABB();
			const ::AABB aa = a.operator ::AABB();
			const ::AABB ab = b.operator ::AABB();
			const ::AABB apb = post_b.operator ::AABB();
			return AABB(
					aa.position.cubic_interpolate_in_time(ab.position, apa.position, apb.position, c, p_b_t, p_pre_a_t, p_post_b_t),
					aa.size.cubic_interpolate_in_time(ab.size, apa.size, apb.size, c, p_b_t, p_pre_a_t, p_post_b_t));
		} break;
		case Variant::BASIS: {
			const Basis bpa = pre_a.operator Basis();
			const Basis ba = a.operator Basis();
			const Basis bb = b.operator Basis();
			const Basis bpb = post_b.operator Basis();
			return Basis(
					ba.rows[0].cubic_interpolate_in_time(bb.rows[0], bpa.rows[0], bpb.rows[0], c, p_b_t, p_pre_a_t, p_post_b_t),
					ba.rows[1].cubic_interpolate_in_time(bb.rows[1], bpa.rows[1], bpb.rows[1], c, p_b_t, p_pre_a_t, p_post_b_t),
					ba.rows[2].cubic_interpolate_in_time(bb.rows[2], bpa.rows[2], bpb.rows[2], c, p_b_t, p_pre_a_t, p_post_b_t));
		} break;
		case Variant::QUATERNION: {
			return (a.operator Quaternion()).spherical_cubic_interpolate_in_time(b.operator Quaternion(), pre_a.operator Quaternion(), post_b.operator Quaternion(), c, p_b_t, p_pre_a_t, p_post_b_t);
		} break;
		case Variant::TRANSFORM2D: {
			const Transform2D tpa = pre_a.operator Transform2D();
			const Transform2D ta = a.operator Transform2D();
			const Transform2D tb = b.operator Transform2D();
			const Transform2D tpb = post_b.operator Transform2D();
			// TODO: May cause unintended skew, we needs spherical_cubic_interpolate_in_time() for angle and Transform2D::cubic_interpolate_with().
			return Transform2D(
					ta[0].cubic_interpolate_in_time(tb[0], tpa[0], tpb[0], c, p_b_t, p_pre_a_t, p_post_b_t),
					ta[1].cubic_interpolate_in_time(tb[1], tpa[1], tpb[1], c, p_b_t, p_pre_a_t, p_post_b_t),
					ta[2].cubic_interpolate_in_time(tb[2], tpa[2], tpb[2], c, p_b_t, p_pre_a_t, p_post_b_t));
		} break;
		case Variant::TRANSFORM3D: {
			const Transform3D tpa = pre_a.operator Transform3D();
			const Transform3D ta = a.operator Transform3D();
			const Transform3D tb = b.operator Transform3D();
			const Transform3D tpb = post_b.operator Transform3D();
			// TODO: May cause unintended skew, we needs Transform3D::cubic_interpolate_with().
			return Transform3D(
					ta.basis.rows[0].cubic_interpolate_in_time(tb.basis.rows[0], tpa.basis.rows[0], tpb.basis.rows[0], c, p_b_t, p_pre_a_t, p_post_b_t),
					ta.basis.rows[1].cubic_interpolate_in_time(tb.basis.rows[1], tpa.basis.rows[1], tpb.basis.rows[1], c, p_b_t, p_pre_a_t, p_post_b_t),
					ta.basis.rows[2].cubic_interpolate_in_time(tb.basis.rows[2], tpa.basis.rows[2], tpb.basis.rows[2], c, p_b_t, p_pre_a_t, p_post_b_t),
					ta.origin.cubic_interpolate_in_time(tb.origin, tpa.origin, tpb.origin, c, p_b_t, p_pre_a_t, p_post_b_t));
		} break;
		case Variant::BOOL:
		case Variant::INT:
		case Variant::RECT2I:
		case Variant::VECTOR2I:
		case Variant::VECTOR3I:
		case Variant::VECTOR4I:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY: {
			// Fallback the interpolatable value which needs casting.
			return cast_from_blendwise(cubic_interpolate_in_time_variant(cast_to_blendwise(pre_a), cast_to_blendwise(a), cast_to_blendwise(b), cast_to_blendwise(post_b), c, p_pre_a_t, p_b_t, p_post_b_t, p_snap_array_element), a.get_type());
		} break;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			// TODO:
			// String interpolation works on both the character array size and the character code, to apply cubic interpolation neatly,
			// we need to figure out how to interpolate well in cases where there are fewer than 4 keys. So, for now, fallback to linear interpolation.
			return interpolate_variant(a, b, c);
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			// Skip.
		} break;
		default: {
			if (a.is_array()) {
				const Array arr_pa = pre_a.operator Array();
				const Array arr_a = a.operator Array();
				const Array arr_b = b.operator Array();
				const Array arr_pb = post_b.operator Array();

				int min_size = arr_a.size();
				int max_size = arr_b.size();
				bool is_a_larger = inform_variant_array(min_size, max_size);

				Array result;
				result.set_typed(MAX(arr_a.get_typed_builtin(), arr_b.get_typed_builtin()), StringName(), Variant());
				result.resize(min_size);

				if (min_size == 0 && max_size == 0) {
					return result;
				}

				Variant vz;
				if (is_a_larger) {
					vz = arr_a[0];
				} else {
					vz = arr_b[0];
				}
				vz.zero();
				Variant pre_last = arr_pa.size() ? arr_pa[arr_pa.size() - 1] : vz;
				Variant post_last = arr_pb.size() ? arr_pb[arr_pb.size() - 1] : vz;

				int i = 0;
				for (; i < min_size; i++) {
					result[i] = cubic_interpolate_in_time_variant(i >= arr_pa.size() ? pre_last : arr_pa[i], arr_a[i], arr_b[i], i >= arr_pb.size() ? post_last : arr_pb[i], c, p_pre_a_t, p_b_t, p_post_b_t);
				}
				if (min_size != max_size) {
					// Process with last element of the lesser array.
					// This is pretty funny and bizarre, but artists like to use it for polygon animation.
					Variant lesser_last = vz;
					if (is_a_larger && !Math::is_equal_approx(c, 1.0f)) {
						result.resize(max_size);
						if (p_snap_array_element) {
							c = 0;
						}
						if (i > 0) {
							lesser_last = arr_b[i - 1];
						}
						for (; i < max_size; i++) {
							result[i] = cubic_interpolate_in_time_variant(i >= arr_pa.size() ? pre_last : arr_pa[i], arr_a[i], lesser_last, i >= arr_pb.size() ? post_last : arr_pb[i], c, p_pre_a_t, p_b_t, p_post_b_t);
						}
					} else if (!is_a_larger && !Math::is_zero_approx(c)) {
						result.resize(max_size);
						if (p_snap_array_element) {
							c = 1;
						}
						if (i > 0) {
							lesser_last = arr_a[i - 1];
						}
						for (; i < max_size; i++) {
							result[i] = cubic_interpolate_in_time_variant(i >= arr_pa.size() ? pre_last : arr_pa[i], lesser_last, arr_b[i], i >= arr_pb.size() ? post_last : arr_pb[i], c, p_pre_a_t, p_b_t, p_post_b_t);
						}
					}
				}
				return result;
			}
		} break;
	}
	return c < 0.5 ? a : b;
}

bool Animation::inform_variant_array(int &r_min, int &r_max) {
	if (r_min <= r_max) {
		return false;
	}
	SWAP(r_min, r_max);
	return true;
}

Animation::Animation() {
}

Animation::~Animation() {
	for (int i = 0; i < tracks.size(); i++) {
		memdelete(tracks[i]);
	}
}
