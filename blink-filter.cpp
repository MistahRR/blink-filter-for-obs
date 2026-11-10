/*
 * OBS Blink Filter - fait disparaître / réapparaître une source en boucle,
 * avec animation d'entrée et de sortie.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/platform.h>

#include <cmath>
#include <cstdint>
#include <algorithm>

#define PLUGIN_NAME "obs-blink-filter"
#define PLUGIN_VERSION "1.0.0"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define T_(x) obs_module_text(x)

enum anim_type {
	ANIM_NONE = 0,
	ANIM_FADE,
	ANIM_ZOOM,
	ANIM_ZOOM_FADE,
	ANIM_SLIDE_LEFT,
	ANIM_SLIDE_RIGHT,
	ANIM_SLIDE_UP,
	ANIM_SLIDE_DOWN,
	ANIM_WIPE_LEFT,
	ANIM_WIPE_RIGHT,
	ANIM_WIPE_UP,
	ANIM_WIPE_DOWN,
	ANIM_GLITCH,
	ANIM_PIXELATE,
	ANIM_FLICKER,
	ANIM_COUNT
};

static const char *anim_keys[ANIM_COUNT] = {
	"Anim.None",      "Anim.Fade",      "Anim.Zoom",      "Anim.ZoomFade",  "Anim.SlideLeft",
	"Anim.SlideRight", "Anim.SlideUp",  "Anim.SlideDown", "Anim.WipeLeft",  "Anim.WipeRight",
	"Anim.WipeUp",    "Anim.WipeDown",  "Anim.Glitch",    "Anim.Pixelate",  "Anim.Flicker",
};

enum easing_type { EASE_LINEAR = 0, EASE_SMOOTH, EASE_OUT, EASE_BACK };

struct blink_filter {
	obs_source_t *ctx = nullptr;
	gs_texrender_t *tr = nullptr;
	gs_effect_t *effect = nullptr;

	/* réglages */
	double visible_time = 3.0;
	double hidden_time = 2.0;
	double offset = 0.0;
	bool start_hidden = false;
	int anim_in = ANIM_FADE, anim_out = ANIM_FADE;
	double in_time = 0.4, out_time = 0.4;
	int easing = EASE_SMOOTH;
	double glitch_strength = 1.0;

	/* état */
	double elapsed = 0.0;
	uint32_t rng = 0x12345678;
	float seed = 0.0f;
	float flicker_rand = 0.0f;
};

/* ------------------------------------------------------------------ */

static float ease(int type, float p)
{
	p = std::clamp(p, 0.0f, 1.0f);
	switch (type) {
	case EASE_SMOOTH:
		return p * p * (3.0f - 2.0f * p);
	case EASE_OUT:
		return 1.0f - powf(1.0f - p, 3.0f);
	case EASE_BACK: {
		const float c1 = 1.70158f, c3 = c1 + 1.0f;
		float q = p - 1.0f;
		return 1.0f + c3 * q * q * q + c1 * q * q;
	}
	default:
		return p;
	}
}

static uint32_t xorshift(uint32_t &s)
{
	s ^= s << 13;
	s ^= s >> 17;
	s ^= s << 5;
	return s;
}

/* Renvoie l'état courant : visible ou non, et si une animation est en cours,
 * son type et sa progression (0 = caché, 1 = complètement visible). */
static bool current_state(const blink_filter *f, int &anim, float &progress)
{
	double vis = std::max(0.05, f->visible_time);
	double hid = std::max(0.0, f->hidden_time);
	double cycle = vis + hid;
	double t = f->elapsed + f->offset + (f->start_hidden ? vis : 0.0);
	t = fmod(t, cycle);
	if (t < 0)
		t += cycle;

	anim = ANIM_NONE;
	progress = 1.0f;
	if (hid <= 0.0)
		return true; /* jamais caché */
	if (t >= vis)
		return false;

	double tin = f->anim_in == ANIM_NONE ? 0.0 : f->in_time;
	double tout = f->anim_out == ANIM_NONE ? 0.0 : f->out_time;
	if (tin + tout > vis) {
		double k = vis / (tin + tout);
		tin *= k;
		tout *= k;
	}

	if (tin > 0.0 && t < tin) {
		anim = f->anim_in;
		progress = (float)(t / tin);
	} else if (tout > 0.0 && t > vis - tout) {
		anim = f->anim_out;
		progress = (float)((vis - t) / tout);
	}
	return true;
}

/* ------------------------------------------------------------------ */

static const char *blink_get_name(void *)
{
	return T_("FilterName");
}

static void blink_update(void *data, obs_data_t *s)
{
	auto *f = (blink_filter *)data;
	f->visible_time = obs_data_get_double(s, "visible_time");
	f->hidden_time = obs_data_get_double(s, "hidden_time");
	f->offset = obs_data_get_double(s, "offset");
	f->start_hidden = obs_data_get_bool(s, "start_hidden");
	f->anim_in = (int)obs_data_get_int(s, "anim_in");
	f->anim_out = (int)obs_data_get_int(s, "anim_out");
	f->in_time = obs_data_get_int(s, "in_ms") / 1000.0;
	f->out_time = obs_data_get_int(s, "out_ms") / 1000.0;
	f->easing = (int)obs_data_get_int(s, "easing");
	f->glitch_strength = obs_data_get_double(s, "glitch_strength") / 100.0;
	if (f->anim_in < 0 || f->anim_in >= ANIM_COUNT)
		f->anim_in = ANIM_NONE;
	if (f->anim_out < 0 || f->anim_out >= ANIM_COUNT)
		f->anim_out = ANIM_NONE;
}

static void blink_defaults(obs_data_t *s)
{
	obs_data_set_default_double(s, "visible_time", 3.0);
	obs_data_set_default_double(s, "hidden_time", 2.0);
	obs_data_set_default_double(s, "offset", 0.0);
	obs_data_set_default_bool(s, "start_hidden", false);
	obs_data_set_default_int(s, "anim_in", ANIM_FADE);
	obs_data_set_default_int(s, "anim_out", ANIM_FADE);
	obs_data_set_default_int(s, "in_ms", 400);
	obs_data_set_default_int(s, "out_ms", 400);
	obs_data_set_default_int(s, "easing", EASE_SMOOTH);
	obs_data_set_default_double(s, "glitch_strength", 100.0);
}

static bool restart_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	auto *f = (blink_filter *)data;
	f->elapsed = 0.0;
	return false;
}

static void add_anim_list(obs_properties_t *props, const char *name, const char *label)
{
	obs_property_t *p = obs_properties_add_list(props, name, label, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	for (int i = 0; i < ANIM_COUNT; i++)
		obs_property_list_add_int(p, T_(anim_keys[i]), i);
}

static obs_properties_t *blink_properties(void *)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	obs_properties_t *timing = obs_properties_create();
	p = obs_properties_add_float(timing, "visible_time", T_("VisibleTime"), 0.1, 86400.0, 0.1);
	obs_property_float_set_suffix(p, " s");
	p = obs_properties_add_float(timing, "hidden_time", T_("HiddenTime"), 0.0, 86400.0, 0.1);
	obs_property_float_set_suffix(p, " s");
	obs_property_set_long_description(p, T_("HiddenTime.Desc"));
	p = obs_properties_add_float(timing, "offset", T_("Offset"), -3600.0, 3600.0, 0.1);
	obs_property_float_set_suffix(p, " s");
	obs_property_set_long_description(p, T_("Offset.Desc"));
	obs_properties_add_bool(timing, "start_hidden", T_("StartHidden"));
	obs_properties_add_button(timing, "restart", T_("Restart"), restart_clicked);
	obs_properties_add_group(props, "grp_timing", T_("Group.Timing"), OBS_GROUP_NORMAL, timing);

	obs_properties_t *anim = obs_properties_create();
	add_anim_list(anim, "anim_out", T_("AnimOut"));
	p = obs_properties_add_int_slider(anim, "out_ms", T_("AnimOutTime"), 0, 5000, 10);
	obs_property_int_set_suffix(p, " ms");
	add_anim_list(anim, "anim_in", T_("AnimIn"));
	p = obs_properties_add_int_slider(anim, "in_ms", T_("AnimInTime"), 0, 5000, 10);
	obs_property_int_set_suffix(p, " ms");
	p = obs_properties_add_list(anim, "easing", T_("Easing"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T_("Easing.Linear"), EASE_LINEAR);
	obs_property_list_add_int(p, T_("Easing.Smooth"), EASE_SMOOTH);
	obs_property_list_add_int(p, T_("Easing.Out"), EASE_OUT);
	obs_property_list_add_int(p, T_("Easing.Back"), EASE_BACK);
	p = obs_properties_add_float_slider(anim, "glitch_strength", T_("GlitchStrength"), 0.0, 300.0, 1.0);
	obs_property_float_set_suffix(p, " %");
	obs_properties_add_group(props, "grp_anim", T_("Group.Anim"), OBS_GROUP_NORMAL, anim);

	return props;
}

static void *blink_create(obs_data_t *settings, obs_source_t *source)
{
	auto *f = new blink_filter;
	f->ctx = source;
	f->rng ^= (uint32_t)(uintptr_t)f;
	if (!f->rng)
		f->rng = 1;

	char *path = obs_module_file("effects/blink.effect");
	obs_enter_graphics();
	f->tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	if (path) {
		char *err = nullptr;
		f->effect = gs_effect_create_from_file(path, &err);
		if (!f->effect)
			blog(LOG_ERROR, "[" PLUGIN_NAME "] cannot load %s: %s", path, err ? err : "?");
		bfree(err);
	}
	obs_leave_graphics();
	bfree(path);

	blink_update(f, settings);
	return f;
}

static void blink_destroy(void *data)
{
	auto *f = (blink_filter *)data;
	obs_enter_graphics();
	gs_texrender_destroy(f->tr);
	gs_effect_destroy(f->effect);
	obs_leave_graphics();
	delete f;
}

static void blink_tick(void *data, float seconds)
{
	auto *f = (blink_filter *)data;
	f->elapsed += seconds;
	f->seed = (float)(xorshift(f->rng) % 10000) / 100.0f;
	f->flicker_rand = (float)(xorshift(f->rng) % 10000) / 10000.0f;
}

/* ------------------------------------------------------------------ */

static void blink_render(void *data, gs_effect_t *)
{
	auto *f = (blink_filter *)data;

	int anim;
	float p;
	if (!current_state(f, anim, p))
		return; /* caché : on ne dessine rien (aucun coût) */

	/* visible sans animation : rendu direct, aucun coût supplémentaire */
	if (anim == ANIM_NONE || !f->effect) {
		obs_source_skip_video_filter(f->ctx);
		return;
	}

	obs_source_t *target = obs_filter_get_target(f->ctx);
	uint32_t w = target ? obs_source_get_base_width(target) : 0;
	uint32_t h = target ? obs_source_get_base_height(target) : 0;
	if (!w || !h) {
		obs_source_skip_video_filter(f->ctx);
		return;
	}

	/* 1) rendu de la source dans une texture (alpha prémultiplié) */
	gs_texrender_reset(f->tr);
	gs_blend_state_push();
	gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	if (gs_texrender_begin(f->tr, w, h)) {
		struct vec4 clear;
		vec4_zero(&clear);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, (float)w, 0.0f, (float)h, -100.0f, 100.0f);
		if (obs_source_process_filter_begin(f->ctx, GS_RGBA, OBS_NO_DIRECT_RENDERING))
			obs_source_process_filter_end(f->ctx, obs_get_base_effect(OBS_EFFECT_DEFAULT), w, h);
		gs_texrender_end(f->tr);
	}
	gs_blend_state_pop();

	gs_texture_t *tex = gs_texrender_get_texture(f->tr);
	if (!tex)
		return;

	/* 2) paramètres de l'animation */
	const float e = ease(f->easing, p);
	const float lin = std::clamp(p, 0.0f, 1.0f);
	float opacity = 1.0f, scale = 1.0f, dx = 0.0f, dy = 0.0f;
	float wipe_p = 2.0f, wdx = 0.0f, wdy = 0.0f, woff = 0.0f;
	float glitch = 0.0f, pixel = 1.0f;
	const float fw = (float)w, fh = (float)h;

	switch (anim) {
	case ANIM_FADE:
		opacity = std::clamp(e, 0.0f, 1.0f);
		break;
	case ANIM_ZOOM:
		scale = std::max(0.0f, e);
		break;
	case ANIM_ZOOM_FADE:
		scale = 0.6f + 0.4f * e;
		opacity = std::clamp(e, 0.0f, 1.0f);
		break;
	case ANIM_SLIDE_LEFT:
		dx = -(1.0f - e) * fw;
		break;
	case ANIM_SLIDE_RIGHT:
		dx = (1.0f - e) * fw;
		break;
	case ANIM_SLIDE_UP:
		dy = -(1.0f - e) * fh;
		break;
	case ANIM_SLIDE_DOWN:
		dy = (1.0f - e) * fh;
		break;
	case ANIM_WIPE_LEFT: /* révélé depuis la gauche */
		wdx = 1.0f;
		wipe_p = e;
		break;
	case ANIM_WIPE_RIGHT:
		wdx = -1.0f;
		woff = 1.0f;
		wipe_p = e;
		break;
	case ANIM_WIPE_UP:
		wdy = -1.0f;
		woff = 1.0f;
		wipe_p = e;
		break;
	case ANIM_WIPE_DOWN:
		wdy = 1.0f;
		wipe_p = e;
		break;
	case ANIM_GLITCH:
		glitch = (1.0f - lin) * (float)f->glitch_strength;
		/* clignotements aléatoires pendant la partie la plus forte */
		if (lin < 0.6f && f->flicker_rand > lin + 0.25f)
			opacity = 0.0f;
		else
			opacity = std::clamp(lin * 1.5f, 0.0f, 1.0f);
		break;
	case ANIM_PIXELATE:
		pixel = 1.0f + (1.0f - e) * (std::min(fw, fh) / 12.0f);
		opacity = std::clamp(e * 2.0f, 0.0f, 1.0f);
		break;
	case ANIM_FLICKER:
		opacity = f->flicker_rand < lin ? 1.0f : (f->flicker_rand < lin + 0.15f ? 0.35f : 0.0f);
		break;
	default:
		break;
	}

	/* 3) dessin */
	gs_effect_t *eff = f->effect;
	gs_effect_set_texture(gs_effect_get_param_by_name(eff, "image"), tex);
	gs_effect_set_float(gs_effect_get_param_by_name(eff, "opacity"), opacity);
	struct vec2 v;
	vec2_set(&v, wdx, wdy);
	gs_effect_set_vec2(gs_effect_get_param_by_name(eff, "wipe_dir"), &v);
	gs_effect_set_float(gs_effect_get_param_by_name(eff, "wipe_off"), woff);
	gs_effect_set_float(gs_effect_get_param_by_name(eff, "wipe_p"), wipe_p);
	gs_effect_set_float(gs_effect_get_param_by_name(eff, "glitch"), glitch);
	gs_effect_set_float(gs_effect_get_param_by_name(eff, "seed"), f->seed);
	vec2_set(&v, 1.0f / fw, 1.0f / fh);
	gs_effect_set_vec2(gs_effect_get_param_by_name(eff, "texel"), &v);
	gs_effect_set_float(gs_effect_get_param_by_name(eff, "pixel"), pixel);

	if (scale <= 0.001f || opacity <= 0.0f)
		return;

	gs_blend_state_push();
	gs_enable_blending(true);
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_matrix_push();
	gs_matrix_translate3f(fw * 0.5f + dx, fh * 0.5f + dy, 0.0f);
	gs_matrix_scale3f(scale, scale, 1.0f);
	gs_matrix_translate3f(-fw * 0.5f, -fh * 0.5f, 0.0f);
	while (gs_effect_loop(eff, "Draw"))
		gs_draw_sprite(tex, 0, w, h);
	gs_matrix_pop();
	gs_blend_state_pop();
}

/* ------------------------------------------------------------------ */

static struct obs_source_info blink_info = {};

bool obs_module_load(void)
{
	blink_info.id = "blink_animated_filter";
	blink_info.type = OBS_SOURCE_TYPE_FILTER;
	blink_info.output_flags = OBS_SOURCE_VIDEO;
	blink_info.get_name = blink_get_name;
	blink_info.create = blink_create;
	blink_info.destroy = blink_destroy;
	blink_info.update = blink_update;
	blink_info.get_defaults = blink_defaults;
	blink_info.get_properties = blink_properties;
	blink_info.video_tick = blink_tick;
	blink_info.video_render = blink_render;
	obs_register_source(&blink_info);
	blog(LOG_INFO, "[" PLUGIN_NAME "] loaded (version %s)", PLUGIN_VERSION);
	return true;
}
