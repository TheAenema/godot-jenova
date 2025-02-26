/**************************************************************************/
/*  animation.h                                                           */
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

#ifndef ANIMATION_H
#define ANIMATION_H

#include "core/io/resource.h"
#include "core/templates/local_vector.h"

#define ANIM_MIN_LENGTH 0.001

class Animation : public Resource {
	GDCLASS(Animation, Resource);
	RES_BASE_EXTENSION("anim");

public:
	typedef uint32_t TypeHash;

	static inline String PARAMETERS_BASE_PATH = "parameters/";

	enum TrackType {
		TYPE_VALUE, // Set a value in a property, can be interpolated.
		TYPE_METHOD, // Call any method on a specific node.
		TYPE_BEZIER, // Bezier curve.
		TYPE_AUDIO,
		TYPE_ANIMATION,
	};

	enum InterpolationType {
		INTERPOLATION_NEAREST,
		INTERPOLATION_LINEAR,
		INTERPOLATION_CUBIC,
		INTERPOLATION_LINEAR_ANGLE,
		INTERPOLATION_CUBIC_ANGLE,
	};

	enum UpdateMode {
		UPDATE_CONTINUOUS,
		UPDATE_DISCRETE,
		UPDATE_CAPTURE,
	};

	enum LoopMode {
		LOOP_NONE,
		LOOP_LINEAR,
		LOOP_PINGPONG,
	};

	// LoopedFlag is used in Animataion to "process the keys at both ends correct".
	enum LoopedFlag {
		LOOPED_FLAG_NONE,
		LOOPED_FLAG_END,
		LOOPED_FLAG_START,
	};

	enum FindMode {
		FIND_MODE_NEAREST,
		FIND_MODE_APPROX,
		FIND_MODE_EXACT,
	};

#ifdef TOOLS_ENABLED
	enum HandleMode {
		HANDLE_MODE_FREE,
		HANDLE_MODE_LINEAR,
		HANDLE_MODE_BALANCED,
		HANDLE_MODE_MIRRORED,
	};
	enum HandleSetMode {
		HANDLE_SET_MODE_NONE,
		HANDLE_SET_MODE_RESET,
		HANDLE_SET_MODE_AUTO,
	};
#endif // TOOLS_ENABLED

private:
	struct Track {
		TrackType type = TrackType::TYPE_ANIMATION;
		InterpolationType interpolation = INTERPOLATION_LINEAR;
		bool loop_wrap = true;
		NodePath path; // Path to something.
		TypeHash thash = 0; // Hash by Path + SubPath + TrackType.
		bool imported = false;
		bool enabled = true;
		Track() {}
		virtual ~Track() {}
	};

	struct Key {
		real_t transition = 1.0;
		double time = 0.0; // Time in secs.
	};

	// Transform key holds either Vector3 or Quaternion.
	template <typename T>
	struct TKey : public Key {
		T value;
	};

	/* PROPERTY VALUE TRACK */

	struct ValueTrack : public Track {
		UpdateMode update_mode = UPDATE_CONTINUOUS;
		bool update_on_seek = false;
		Vector<TKey<Variant>> values;

		ValueTrack() {
			type = TYPE_VALUE;
		}
	};

	/* METHOD TRACK */

	struct MethodKey : public Key {
		StringName method;
		Vector<Variant> params;
	};

	struct MethodTrack : public Track {
		Vector<MethodKey> methods;
		MethodTrack() { type = TYPE_METHOD; }
	};

	/* BEZIER TRACK */

	struct BezierKey {
		Vector2 in_handle; // Relative (x always <0)
		Vector2 out_handle; // Relative (x always >0)
		real_t value = 0.0;
#ifdef TOOLS_ENABLED
		HandleMode handle_mode = HANDLE_MODE_FREE;
#endif // TOOLS_ENABLED
	};

	struct BezierTrack : public Track {
		Vector<TKey<BezierKey>> values;

		BezierTrack() {
			type = TYPE_BEZIER;
		}
	};

	/* AUDIO TRACK */

	struct AudioKey {
		Ref<Resource> stream;
		real_t start_offset = 0.0; // Offset from start.
		real_t end_offset = 0.0; // Offset from end, if 0 then full length or infinite.
		AudioKey() {
		}
	};

	struct AudioTrack : public Track {
		Vector<TKey<AudioKey>> values;
		bool use_blend = true;

		AudioTrack() {
			type = TYPE_AUDIO;
		}
	};

	/* ANIMATION TRACK */

	struct AnimationTrack : public Track {
		Vector<TKey<StringName>> values;

		AnimationTrack() {
			type = TYPE_ANIMATION;
		}
	};

	Vector<Track *> tracks;

	template <typename T>
	void _clear(T &p_keys);

	template <typename T, typename V>
	int _insert(double p_time, T &p_keys, const V &p_value);

	template <typename K>

	inline int _find(const Vector<K> &p_keys, double p_time, bool p_backward = false, bool p_limit = false) const;

	_FORCE_INLINE_ Vector3 _interpolate(const Vector3 &p_a, const Vector3 &p_b, real_t p_c) const;
	_FORCE_INLINE_ Quaternion _interpolate(const Quaternion &p_a, const Quaternion &p_b, real_t p_c) const;
	_FORCE_INLINE_ Variant _interpolate(const Variant &p_a, const Variant &p_b, real_t p_c) const;
	_FORCE_INLINE_ real_t _interpolate(const real_t &p_a, const real_t &p_b, real_t p_c) const;
	_FORCE_INLINE_ Variant _interpolate_angle(const Variant &p_a, const Variant &p_b, real_t p_c) const;

	_FORCE_INLINE_ Vector3 _cubic_interpolate_in_time(const Vector3 &p_pre_a, const Vector3 &p_a, const Vector3 &p_b, const Vector3 &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const;
	_FORCE_INLINE_ Quaternion _cubic_interpolate_in_time(const Quaternion &p_pre_a, const Quaternion &p_a, const Quaternion &p_b, const Quaternion &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const;
	_FORCE_INLINE_ Variant _cubic_interpolate_in_time(const Variant &p_pre_a, const Variant &p_a, const Variant &p_b, const Variant &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const;
	_FORCE_INLINE_ real_t _cubic_interpolate_in_time(const real_t &p_pre_a, const real_t &p_a, const real_t &p_b, const real_t &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const;
	_FORCE_INLINE_ Variant _cubic_interpolate_angle_in_time(const Variant &p_pre_a, const Variant &p_a, const Variant &p_b, const Variant &p_post_b, real_t p_c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t) const;

	template <typename T>
	_FORCE_INLINE_ T _interpolate(const Vector<TKey<T>> &p_keys, double p_time, InterpolationType p_interp, bool p_loop_wrap, bool *p_ok, bool p_backward = false) const;

	template <typename T>
	_FORCE_INLINE_ void _track_get_key_indices_in_range(const Vector<T> &p_array, double from_time, double to_time, List<int> *p_indices, bool p_is_backward) const;

	double length = 1.0;
	real_t step = 1.0 / 30;
	LoopMode loop_mode = LOOP_NONE;
	bool capture_included = false;
	void _check_capture_included();

	void _track_update_hash(int p_track);

	// bind helpers
private:
	bool _float_track_optimize_key(const TKey<float> t0, const TKey<float> t1, const TKey<float> t2, real_t p_allowed_velocity_err, real_t p_allowed_precision_error);
	bool _vector2_track_optimize_key(const TKey<Vector2> t0, const TKey<Vector2> t1, const TKey<Vector2> t2, real_t p_alowed_velocity_err, real_t p_allowed_angular_error, real_t p_allowed_precision_error);
	bool _vector3_track_optimize_key(const TKey<Vector3> t0, const TKey<Vector3> t1, const TKey<Vector3> t2, real_t p_alowed_velocity_err, real_t p_allowed_angular_error, real_t p_allowed_precision_error);
	bool _quaternion_track_optimize_key(const TKey<Quaternion> t0, const TKey<Quaternion> t1, const TKey<Quaternion> t2, real_t p_allowed_velocity_err, real_t p_allowed_angular_error, real_t p_allowed_precision_error);
	void _value_track_optimize(int p_idx, real_t p_allowed_velocity_err, real_t p_allowed_angular_err, real_t p_allowed_precision_error);

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	virtual void reset_state() override;

	static void _bind_methods();

	static bool inform_variant_array(int &r_min, int &r_max); // Returns true if max and min are swapped.

#ifndef DISABLE_DEPRECATED
	Vector3 _position_track_interpolate_bind_compat_86629(int p_track, double p_time) const;
	Quaternion _rotation_track_interpolate_bind_compat_86629(int p_track, double p_time) const;
	Vector3 _scale_track_interpolate_bind_compat_86629(int p_track, double p_time) const;
	float _blend_shape_track_interpolate_bind_compat_86629(int p_track, double p_time) const;
	Variant _value_track_interpolate_bind_compat_86629(int p_track, double p_time) const;
	int _track_find_key_bind_compat_92861(int p_track, double p_time, FindMode p_find_mode = FIND_MODE_NEAREST) const;
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

public:
	int add_track(TrackType p_type, int p_at_pos = -1);
	void remove_track(int p_track);

	bool is_capture_included() const;

	int get_track_count() const;
	TrackType track_get_type(int p_track) const;

	void track_set_path(int p_track, const NodePath &p_path);
	NodePath track_get_path(int p_track) const;
	int find_track(const NodePath &p_path, const TrackType p_type) const;

	TypeHash track_get_type_hash(int p_track) const;

	void track_move_up(int p_track);
	void track_move_down(int p_track);
	void track_move_to(int p_track, int p_to_index);
	void track_swap(int p_track, int p_with_track);

	void track_set_imported(int p_track, bool p_imported);
	bool track_is_imported(int p_track) const;

	void track_set_enabled(int p_track, bool p_enabled);
	bool track_is_enabled(int p_track) const;

	int track_insert_key(int p_track, double p_time, const Variant &p_key, real_t p_transition = 1);
	void track_set_key_transition(int p_track, int p_key_idx, real_t p_transition);
	void track_set_key_value(int p_track, int p_key_idx, const Variant &p_value);
	void track_set_key_time(int p_track, int p_key_idx, double p_time);
	int track_find_key(int p_track, double p_time, FindMode p_find_mode = FIND_MODE_NEAREST, bool p_limit = false, bool p_backward = false) const;
	void track_remove_key(int p_track, int p_idx);
	void track_remove_key_at_time(int p_track, double p_time);
	int track_get_key_count(int p_track) const;
	Variant track_get_key_value(int p_track, int p_key_idx) const;
	double track_get_key_time(int p_track, int p_key_idx) const;
	real_t track_get_key_transition(int p_track, int p_key_idx) const;

	void track_set_interpolation_type(int p_track, InterpolationType p_interp);
	InterpolationType track_get_interpolation_type(int p_track) const;

	Array make_default_bezier_key(float p_value);
	int bezier_track_insert_key(int p_track, double p_time, real_t p_value, const Vector2 &p_in_handle, const Vector2 &p_out_handle);
	void bezier_track_set_key_value(int p_track, int p_index, real_t p_value);
	void bezier_track_set_key_in_handle(int p_track, int p_index, const Vector2 &p_handle, real_t p_balanced_value_time_ratio = 1.0);
	void bezier_track_set_key_out_handle(int p_track, int p_index, const Vector2 &p_handle, real_t p_balanced_value_time_ratio = 1.0);
	real_t bezier_track_get_key_value(int p_track, int p_index) const;
	Vector2 bezier_track_get_key_in_handle(int p_track, int p_index) const;
	Vector2 bezier_track_get_key_out_handle(int p_track, int p_index) const;
#ifdef TOOLS_ENABLED
	void bezier_track_set_key_handle_mode(int p_track, int p_index, HandleMode p_mode, HandleSetMode p_set_mode = HANDLE_SET_MODE_NONE);
	HandleMode bezier_track_get_key_handle_mode(int p_track, int p_index) const;
#endif // TOOLS_ENABLED

	real_t bezier_track_interpolate(int p_track, double p_time) const;

	int audio_track_insert_key(int p_track, double p_time, const Ref<Resource> &p_stream, real_t p_start_offset = 0, real_t p_end_offset = 0);
	void audio_track_set_key_stream(int p_track, int p_key, const Ref<Resource> &p_stream);
	void audio_track_set_key_start_offset(int p_track, int p_key, real_t p_offset);
	void audio_track_set_key_end_offset(int p_track, int p_key, real_t p_offset);
	Ref<Resource> audio_track_get_key_stream(int p_track, int p_key) const;
	real_t audio_track_get_key_start_offset(int p_track, int p_key) const;
	real_t audio_track_get_key_end_offset(int p_track, int p_key) const;
	void audio_track_set_use_blend(int p_track, bool p_enable);
	bool audio_track_is_use_blend(int p_track) const;

	int animation_track_insert_key(int p_track, double p_time, const StringName &p_animation);
	void animation_track_set_key_animation(int p_track, int p_key, const StringName &p_animation);
	StringName animation_track_get_key_animation(int p_track, int p_key) const;

	void track_set_interpolation_loop_wrap(int p_track, bool p_enable);
	bool track_get_interpolation_loop_wrap(int p_track) const;

	Variant value_track_interpolate(int p_track, double p_time, bool p_backward = false) const;
	void value_track_set_update_mode(int p_track, UpdateMode p_mode);
	UpdateMode value_track_get_update_mode(int p_track) const;

	Vector<Variant> method_track_get_params(int p_track, int p_key_idx) const;
	StringName method_track_get_name(int p_track, int p_key_idx) const;

	void copy_track(int p_track, Ref<Animation> p_to_animation);

	void track_get_key_indices_in_range(int p_track, double p_time, double p_delta, List<int> *p_indices, Animation::LoopedFlag p_looped_flag = Animation::LOOPED_FLAG_NONE) const;

	void set_length(real_t p_length);
	real_t get_length() const;

	void set_loop_mode(LoopMode p_loop_mode);
	LoopMode get_loop_mode() const;

	void set_step(real_t p_step);
	real_t get_step() const;

	void clear();

	void optimize(real_t p_allowed_velocity_err = 0.01, real_t p_allowed_angular_err = 0.01, int p_precision = 3);

	// Helper functions for Variant.
	static bool is_variant_interpolatable(const Variant p_value);

	static Variant cast_to_blendwise(const Variant p_value);
	static Variant cast_from_blendwise(const Variant p_value, const Variant::Type p_type);

	static Variant string_to_array(const Variant p_value);
	static Variant array_to_string(const Variant p_value);

	static Variant add_variant(const Variant &a, const Variant &b);
	static Variant subtract_variant(const Variant &a, const Variant &b);
	static Variant blend_variant(const Variant &a, const Variant &b, float c);
	static Variant interpolate_variant(const Variant &a, const Variant &b, float c, bool p_snap_array_element = false);
	static Variant cubic_interpolate_in_time_variant(const Variant &pre_a, const Variant &a, const Variant &b, const Variant &post_b, float c, real_t p_pre_a_t, real_t p_b_t, real_t p_post_b_t, bool p_snap_array_element = false);

	static bool is_less_or_equal_approx(double a, double b) {
		return a < b || Math::is_equal_approx(a, b);
	}

	static bool is_less_approx(double a, double b) {
		return a < b && !Math::is_equal_approx(a, b);
	}

	static bool is_greater_or_equal_approx(double a, double b) {
		return a > b || Math::is_equal_approx(a, b);
	}

	static bool is_greater_approx(double a, double b) {
		return a > b && !Math::is_equal_approx(a, b);
	}

	static TrackType get_cache_type(TrackType p_type);

	Animation();
	~Animation();
};

VARIANT_ENUM_CAST(Animation::TrackType);
VARIANT_ENUM_CAST(Animation::InterpolationType);
VARIANT_ENUM_CAST(Animation::UpdateMode);
VARIANT_ENUM_CAST(Animation::LoopMode);
VARIANT_ENUM_CAST(Animation::LoopedFlag);
VARIANT_ENUM_CAST(Animation::FindMode);
#ifdef TOOLS_ENABLED
VARIANT_ENUM_CAST(Animation::HandleMode);
VARIANT_ENUM_CAST(Animation::HandleSetMode);
#endif // TOOLS_ENABLED

#endif // ANIMATION_H
