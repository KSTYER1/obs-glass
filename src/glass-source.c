/*
 * obs-glass — eigenstaendige Glass-Source fuer OBS Studio.
 * Implementiert den LucidGlass-Effekt als frei platzierbares Szenen-Element.
 * Copyright (C) 2026 K_STYER, GPLv2 oder spaeter.
 */

#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <plugin-support.h>
#include <stdlib.h>
#include <string.h>

/* Hilfsmakro: ABGR (obs_data_get_int Farbe) -> vec4 RGBA normalisiert */
static inline void packed_to_vec4(long long packed, struct vec4 *out)
{
	uint8_t a = (uint8_t)((packed >> 24) & 0xFF);
	uint8_t b = (uint8_t)((packed >> 16) & 0xFF);
	uint8_t g = (uint8_t)((packed >> 8) & 0xFF);
	uint8_t r = (uint8_t)((packed >> 0) & 0xFF);
	out->x = r / 255.0f;
	out->y = g / 255.0f;
	out->z = b / 255.0f;
	out->w = a / 255.0f;
}

/* =========================================================================
 * Kontext-Struktur
 * ========================================================================= */

struct glass_source {
	obs_source_t *source;
	obs_weak_source_t *target_weak;       /* Hintergrund-Source (kein Zyklus) */

	/* GPU-Ressourcen */
	gs_effect_t    *effect;
	gs_texrender_t *texrender;   /* Render-Target fuer Hintergrund-Source */

	/* Gecachte Shader-Parameter (einmal in create, nie per Frame) */
	gs_eparam_t *p_image;
	gs_eparam_t *p_uv_size;
	gs_eparam_t *p_pos_x;
	gs_eparam_t *p_pos_y;
	gs_eparam_t *p_transparent_bg;
	gs_eparam_t *p_shape_width;
	gs_eparam_t *p_shape_height;
	gs_eparam_t *p_corner_radius;
	gs_eparam_t *p_feathering;
	gs_eparam_t *p_blur_strength;
	gs_eparam_t *p_blur_intensity;
	gs_eparam_t *p_frost_strength;
	gs_eparam_t *p_tint_color;
	gs_eparam_t *p_tint_strength;
	gs_eparam_t *p_distortion_thickness;
	gs_eparam_t *p_max_distortion;
	gs_eparam_t *p_distortion_falloff;
	gs_eparam_t *p_magnification;
	gs_eparam_t *p_enable_ca;
	gs_eparam_t *p_ca_strength;
	gs_eparam_t *p_ca_thickness;
	gs_eparam_t *p_ca_falloff;
	gs_eparam_t *p_enable_glow;
	gs_eparam_t *p_glow_color;
	gs_eparam_t *p_glow_spread;
	gs_eparam_t *p_glow_falloff;
	gs_eparam_t *p_enable_dir_glow;
	gs_eparam_t *p_glow_angle;
	gs_eparam_t *p_glow_spread_deg;
	gs_eparam_t *p_glow_softness;
	gs_eparam_t *p_enable_inner_glow;
	gs_eparam_t *p_inner_glow_color;
	gs_eparam_t *p_inner_glow_spread;
	gs_eparam_t *p_inner_glow_falloff;
	gs_eparam_t *p_enable_dir_inner_glow;
	gs_eparam_t *p_inner_glow_angle;
	gs_eparam_t *p_inner_glow_spread_deg;
	gs_eparam_t *p_inner_glow_softness;
	gs_eparam_t *p_enable_shadow;
	gs_eparam_t *p_shadow_color;
	gs_eparam_t *p_shadow_offset_x;
	gs_eparam_t *p_shadow_offset_y;
	gs_eparam_t *p_shadow_blur;
	gs_eparam_t *p_shadow_opacity;

	/* Settings-Werte (aus update gelesen) */
	float pos_x, pos_y;
	bool transparent_bg;
	float shape_width, shape_height;
	float corner_radius, feathering;
	int blur_strength;
	float blur_intensity, frost_strength;
	long long tint_color_packed;
	float tint_strength;
	float distortion_thickness, max_distortion, distortion_falloff;
	float magnification;
	bool enable_ca;
	float ca_strength, ca_thickness, ca_falloff;
	bool enable_glow;
	long long glow_color_packed;
	float glow_spread, glow_falloff;
	bool enable_dir_glow;
	float glow_angle, glow_spread_deg, glow_softness;
	bool enable_inner_glow;
	long long inner_glow_color_packed;
	float inner_glow_spread, inner_glow_falloff;
	bool enable_dir_inner_glow;
	float inner_glow_angle, inner_glow_spread_deg, inner_glow_softness;
	bool enable_shadow;
	long long shadow_color_packed;
	float shadow_offset_x, shadow_offset_y, shadow_blur, shadow_opacity;

	/* Dynamische Ausgabe-Dimensionen (von der Hintergrund-Source) */
	uint32_t width;
	uint32_t height;

	/* Schutz vor rekursivem Render (Glass in eigener Szene) */
	bool rendering;
};

/* =========================================================================
 * Shader-Parameter cachen (einmalig in create)
 * ========================================================================= */

static void cache_params(struct glass_source *ctx)
{
	gs_effect_t *e = ctx->effect;
#define GP(name) gs_effect_get_param_by_name(e, name)
	ctx->p_image                = GP("image");
	ctx->p_uv_size              = GP("uv_size");
	ctx->p_pos_x                = GP("LensPositionX");
	ctx->p_pos_y                = GP("LensPositionY");
	ctx->p_transparent_bg       = GP("TransparentBackground");
	ctx->p_shape_width          = GP("shape_width");
	ctx->p_shape_height         = GP("shape_height");
	ctx->p_corner_radius        = GP("corner_radius");
	ctx->p_feathering           = GP("FeatheringPx");
	ctx->p_blur_strength        = GP("BlurStrength");
	ctx->p_blur_intensity       = GP("BlurIntensity");
	ctx->p_frost_strength       = GP("FrostStrength");
	ctx->p_tint_color           = GP("TintColor");
	ctx->p_tint_strength        = GP("TintStrength");
	ctx->p_distortion_thickness = GP("DistortionEdgeThicknessPx");
	ctx->p_max_distortion       = GP("MaxDistortionAmount");
	ctx->p_distortion_falloff   = GP("DistortionFalloffPower");
	ctx->p_magnification        = GP("MagnificationAmount");
	ctx->p_enable_ca            = GP("EnableChromaticAberration");
	ctx->p_ca_strength          = GP("ChromaticStrength");
	ctx->p_ca_thickness         = GP("ChromaticThickness");
	ctx->p_ca_falloff           = GP("ChromaticFalloff");
	ctx->p_enable_glow          = GP("EnableGlow");
	ctx->p_glow_color           = GP("GlowColor");
	ctx->p_glow_spread          = GP("GlowBaseSpreadPx");
	ctx->p_glow_falloff         = GP("GlowFalloffPower");
	ctx->p_enable_dir_glow      = GP("EnableDirectionalGlow");
	ctx->p_glow_angle           = GP("GlowDirectionAngle");
	ctx->p_glow_spread_deg      = GP("GlowDirectionalSpread");
	ctx->p_glow_softness        = GP("GlowDirectionalSoftness");
	ctx->p_enable_inner_glow    = GP("EnableInnerGlow");
	ctx->p_inner_glow_color     = GP("InnerGlowColor");
	ctx->p_inner_glow_spread    = GP("InnerGlowSpreadPx");
	ctx->p_inner_glow_falloff   = GP("InnerGlowFalloffPower");
	ctx->p_enable_dir_inner_glow= GP("EnableDirectionalInnerGlow");
	ctx->p_inner_glow_angle     = GP("InnerGlowDirectionAngle");
	ctx->p_inner_glow_spread_deg= GP("InnerGlowDirectionalSpread");
	ctx->p_inner_glow_softness  = GP("InnerGlowDirectionalSoftness");
	ctx->p_enable_shadow        = GP("EnableDropShadow");
	ctx->p_shadow_color         = GP("ShadowColor");
	ctx->p_shadow_offset_x      = GP("ShadowOffsetX");
	ctx->p_shadow_offset_y      = GP("ShadowOffsetY");
	ctx->p_shadow_blur          = GP("ShadowBlur");
	ctx->p_shadow_opacity       = GP("ShadowOpacity");
#undef GP
}

/* =========================================================================
 * create / destroy / update
 * ========================================================================= */

static const char *glass_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("GlassSourceName");
}

static void *glass_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct glass_source *ctx = bzalloc(sizeof(struct glass_source));
	ctx->source = source;
	ctx->width  = 1920;
	ctx->height = 1080;

	char *path = obs_module_file("effects/glass.effect");
	obs_enter_graphics();
	ctx->effect    = gs_effect_create_from_file(path, NULL);
	ctx->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	obs_leave_graphics();
	bfree(path);

	if (!ctx->effect || !ctx->texrender) {
		obs_log(LOG_ERROR, "glass-source: effect oder texrender laden fehlgeschlagen");
		obs_enter_graphics();
		gs_effect_destroy(ctx->effect);
		gs_texrender_destroy(ctx->texrender);
		obs_leave_graphics();
		bfree(ctx);
		return NULL;
	}

	obs_enter_graphics();
	cache_params(ctx);
	obs_leave_graphics();

	/* update() liest Settings initial ein */
	/* Wird von OBS nach create() automatisch aufgerufen */
	UNUSED_PARAMETER(settings);
	return ctx;
}

static void glass_source_destroy(void *data)
{
	struct glass_source *ctx = data;

	obs_enter_graphics();
	gs_effect_destroy(ctx->effect);
	gs_texrender_destroy(ctx->texrender);
	obs_leave_graphics();

	obs_weak_source_release(ctx->target_weak);
	bfree(ctx);
}

static void glass_source_update(void *data, obs_data_t *settings)
{
	struct glass_source *ctx = data;

	/* Hintergrund-Source aktualisieren */
	const char *target_name = obs_data_get_string(settings, "target_source");
	obs_weak_source_release(ctx->target_weak);
	ctx->target_weak = NULL;

	if (target_name && *target_name) {
		obs_source_t *target = obs_get_source_by_name(target_name);
		if (target) {
			/* Nur direkte Selbst-Referenz blocken — verschachtelte
			 * Zyklen (Szene enthaelt diese Quelle) sind durch das
			 * ctx->rendering-Flag in glass_source_render abgesichert. */
			if (target != ctx->source) {
				ctx->target_weak = obs_source_get_weak_source(target);
			} else {
				obs_log(LOG_WARNING,
					"glass-source: direkte Selbst-Referenz "
					"als Ziel ignoriert");
			}
			obs_source_release(target);
		}
	}

	/* Alle Glass-Parameter einlesen */
	ctx->pos_x                = (float)obs_data_get_double(settings, "pos_x");
	ctx->pos_y                = (float)obs_data_get_double(settings, "pos_y");
	ctx->transparent_bg       = obs_data_get_bool(settings, "transparent_bg");
	ctx->shape_width          = (float)obs_data_get_double(settings, "shape_width");
	ctx->shape_height         = (float)obs_data_get_double(settings, "shape_height");
	ctx->corner_radius        = (float)obs_data_get_double(settings, "corner_radius");
	ctx->feathering           = (float)obs_data_get_double(settings, "feathering");
	ctx->blur_strength        = (int)obs_data_get_int(settings, "blur_strength");
	ctx->blur_intensity       = (float)obs_data_get_double(settings, "blur_intensity");
	ctx->frost_strength       = (float)obs_data_get_double(settings, "frost_strength");
	ctx->tint_color_packed    = obs_data_get_int(settings, "tint_color");
	ctx->tint_strength        = (float)obs_data_get_double(settings, "tint_strength");
	ctx->distortion_thickness = (float)obs_data_get_double(settings, "distortion_thickness");
	ctx->max_distortion       = (float)obs_data_get_double(settings, "max_distortion");
	ctx->distortion_falloff   = (float)obs_data_get_double(settings, "distortion_falloff");
	ctx->magnification        = (float)obs_data_get_double(settings, "magnification");
	ctx->enable_ca            = obs_data_get_bool(settings, "enable_ca");
	ctx->ca_strength          = (float)obs_data_get_double(settings, "ca_strength");
	ctx->ca_thickness         = (float)obs_data_get_double(settings, "ca_thickness");
	ctx->ca_falloff           = (float)obs_data_get_double(settings, "ca_falloff");
	ctx->enable_glow          = obs_data_get_bool(settings, "enable_glow");
	ctx->glow_color_packed    = obs_data_get_int(settings, "glow_color");
	ctx->glow_spread          = (float)obs_data_get_double(settings, "glow_spread");
	ctx->glow_falloff         = (float)obs_data_get_double(settings, "glow_falloff");
	ctx->enable_dir_glow      = obs_data_get_bool(settings, "enable_dir_glow");
	ctx->glow_angle           = (float)obs_data_get_double(settings, "glow_angle");
	ctx->glow_spread_deg      = (float)obs_data_get_double(settings, "glow_spread_deg");
	ctx->glow_softness        = (float)obs_data_get_double(settings, "glow_softness");
	ctx->enable_inner_glow    = obs_data_get_bool(settings, "enable_inner_glow");
	ctx->inner_glow_color_packed = obs_data_get_int(settings, "inner_glow_color");
	ctx->inner_glow_spread    = (float)obs_data_get_double(settings, "inner_glow_spread");
	ctx->inner_glow_falloff   = (float)obs_data_get_double(settings, "inner_glow_falloff");
	ctx->enable_dir_inner_glow= obs_data_get_bool(settings, "enable_dir_inner_glow");
	ctx->inner_glow_angle     = (float)obs_data_get_double(settings, "inner_glow_angle");
	ctx->inner_glow_spread_deg= (float)obs_data_get_double(settings, "inner_glow_spread_deg");
	ctx->inner_glow_softness  = (float)obs_data_get_double(settings, "inner_glow_softness");
	ctx->enable_shadow        = obs_data_get_bool(settings, "enable_shadow");
	ctx->shadow_color_packed  = obs_data_get_int(settings, "shadow_color");
	ctx->shadow_offset_x      = (float)obs_data_get_double(settings, "shadow_offset_x");
	ctx->shadow_offset_y      = (float)obs_data_get_double(settings, "shadow_offset_y");
	ctx->shadow_blur          = (float)obs_data_get_double(settings, "shadow_blur");
	ctx->shadow_opacity       = (float)obs_data_get_double(settings, "shadow_opacity");
}

/* =========================================================================
 * Defaults
 * ========================================================================= */

static void glass_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "target_source", "");
	obs_data_set_default_double(settings, "pos_x",                960.0);
	obs_data_set_default_double(settings, "pos_y",                540.0);
	obs_data_set_default_bool(  settings, "transparent_bg",       true);
	obs_data_set_default_double(settings, "shape_width",          400.0);
	obs_data_set_default_double(settings, "shape_height",         400.0);
	obs_data_set_default_double(settings, "corner_radius",        100.0);
	obs_data_set_default_double(settings, "feathering",             2.0);
	obs_data_set_default_int(   settings, "blur_strength",            1);
	obs_data_set_default_double(settings, "blur_intensity",         1.0);
	obs_data_set_default_double(settings, "frost_strength",         0.2);
	obs_data_set_default_int(   settings, "tint_color",    0xFFFFFFFFLL); /* opaque white */
	obs_data_set_default_double(settings, "tint_strength",          0.05);
	obs_data_set_default_double(settings, "distortion_thickness",  40.0);
	obs_data_set_default_double(settings, "max_distortion",        -0.5);
	obs_data_set_default_double(settings, "distortion_falloff",     2.5);
	obs_data_set_default_double(settings, "magnification",          1.0);
	obs_data_set_default_bool(  settings, "enable_ca",             true);
	obs_data_set_default_double(settings, "ca_strength",            4.0);
	obs_data_set_default_double(settings, "ca_thickness",          40.0);
	obs_data_set_default_double(settings, "ca_falloff",             2.0);
	obs_data_set_default_bool(  settings, "enable_glow",          false);
	obs_data_set_default_int(   settings, "glow_color",    0xFFFFFFFFLL);
	obs_data_set_default_double(settings, "glow_spread",           20.0);
	obs_data_set_default_double(settings, "glow_falloff",           1.0);
	obs_data_set_default_bool(  settings, "enable_dir_glow",      false);
	obs_data_set_default_double(settings, "glow_angle",            45.0);
	obs_data_set_default_double(settings, "glow_spread_deg",       90.0);
	obs_data_set_default_double(settings, "glow_softness",          0.5);
	obs_data_set_default_bool(  settings, "enable_inner_glow",     true);
	obs_data_set_default_int(   settings, "inner_glow_color", 0xFFFFFFFFLL);
	obs_data_set_default_double(settings, "inner_glow_spread",      1.0);
	obs_data_set_default_double(settings, "inner_glow_falloff",     1.0);
	obs_data_set_default_bool(  settings, "enable_dir_inner_glow",false);
	obs_data_set_default_double(settings, "inner_glow_angle",      45.0);
	obs_data_set_default_double(settings, "inner_glow_spread_deg", 90.0);
	obs_data_set_default_double(settings, "inner_glow_softness",    0.5);
	obs_data_set_default_bool(  settings, "enable_shadow",        false);
	obs_data_set_default_int(   settings, "shadow_color",  0xFF000000LL);
	obs_data_set_default_double(settings, "shadow_offset_x",        5.0);
	obs_data_set_default_double(settings, "shadow_offset_y",        5.0);
	obs_data_set_default_double(settings, "shadow_blur",           10.0);
	obs_data_set_default_double(settings, "shadow_opacity",         0.5);
}

/* =========================================================================
 * Properties
 * ========================================================================= */

/* Hilfstruktur zum Sammeln und Sortieren von Source-Namen */
struct name_list {
	char  **items;
	size_t  count;
	size_t  cap;
};

static bool collect_source_name(void *param, obs_source_t *source)
{
	uint32_t flags = obs_source_get_output_flags(source);
	if (!(flags & OBS_SOURCE_VIDEO))
		return true;
	const char *name = obs_source_get_name(source);
	if (!name || !*name)
		return true;
	struct name_list *list = param;
	if (list->count >= list->cap) {
		list->cap   = list->cap ? list->cap * 2 : 64;
		list->items = brealloc(list->items, list->cap * sizeof(char *));
	}
	list->items[list->count++] = bstrdup(name);
	return true;
}

static bool collect_scene_name(void *param, obs_source_t *scene)
{
	const char *name = obs_source_get_name(scene);
	if (!name || !*name)
		return true;
	struct name_list *list = param;
	if (list->count >= list->cap) {
		list->cap   = list->cap ? list->cap * 2 : 64;
		list->items = brealloc(list->items, list->cap * sizeof(char *));
	}
	list->items[list->count++] = bstrdup(name);
	return true;
}

static int name_compare(const void *a, const void *b)
{
	return strcmp(*(const char **)a, *(const char **)b);
}

/* Gross-/Kleinschreibung-unempfindliche Teilstring-Suche */
static bool icontains(const char *hay, const char *needle)
{
	if (!needle || !*needle)
		return true;
	size_t hn = strlen(hay);
	size_t nn = strlen(needle);
	if (nn > hn)
		return false;
	for (size_t i = 0; i <= hn - nn; i++) {
		if (_strnicmp(hay + i, needle, nn) == 0)
			return true;
	}
	return false;
}

/* Source-Liste aufbauen (gefiltert nach filter, "" = alle) */
static void populate_sources_filtered(obs_property_t *list, const char *filter)
{
	obs_property_list_clear(list);
	obs_property_list_add_string(list, obs_module_text("None"), "");

	struct name_list names = {NULL, 0, 0};
	obs_enum_sources(collect_source_name, &names);
	obs_enum_scenes(collect_scene_name, &names);

	if (names.count > 0) {
		qsort(names.items, names.count, sizeof(char *), name_compare);
		for (size_t i = 0; i < names.count; i++) {
			if (icontains(names.items[i], filter))
				obs_property_list_add_string(list, names.items[i], names.items[i]);
			bfree(names.items[i]);
		}
		bfree(names.items);
	}
}

/* Callback: Suchfeld geaendert -> Liste neu aufbauen */
static bool on_source_search_changed(obs_properties_t *props,
				      obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	obs_property_t *list = obs_properties_get(props, "target_source");
	const char *filter   = obs_data_get_string(settings, "source_search");
	populate_sources_filtered(list, filter);
	return true;
}

/* Sichtbarkeits-Callbacks fuer bedingte Gruppen */
static bool on_enable_ca_changed(obs_properties_t *props,
				  obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	bool en = obs_data_get_bool(settings, "enable_ca");
	obs_property_set_visible(obs_properties_get(props, "ca_strength"), en);
	obs_property_set_visible(obs_properties_get(props, "ca_thickness"), en);
	obs_property_set_visible(obs_properties_get(props, "ca_falloff"), en);
	return true;
}

static bool on_enable_glow_changed(obs_properties_t *props,
				    obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	bool en = obs_data_get_bool(settings, "enable_glow");
	obs_property_set_visible(obs_properties_get(props, "glow_color"), en);
	obs_property_set_visible(obs_properties_get(props, "glow_spread"), en);
	obs_property_set_visible(obs_properties_get(props, "glow_falloff"), en);
	obs_property_set_visible(obs_properties_get(props, "enable_dir_glow"), en);
	obs_property_set_visible(obs_properties_get(props, "glow_angle"), en);
	obs_property_set_visible(obs_properties_get(props, "glow_spread_deg"), en);
	obs_property_set_visible(obs_properties_get(props, "glow_softness"), en);
	return true;
}

static bool on_enable_inner_glow_changed(obs_properties_t *props,
					  obs_property_t *p,
					  obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	bool en = obs_data_get_bool(settings, "enable_inner_glow");
	obs_property_set_visible(obs_properties_get(props, "inner_glow_color"), en);
	obs_property_set_visible(obs_properties_get(props, "inner_glow_spread"), en);
	obs_property_set_visible(obs_properties_get(props, "inner_glow_falloff"), en);
	obs_property_set_visible(obs_properties_get(props, "enable_dir_inner_glow"), en);
	obs_property_set_visible(obs_properties_get(props, "inner_glow_angle"), en);
	obs_property_set_visible(obs_properties_get(props, "inner_glow_spread_deg"), en);
	obs_property_set_visible(obs_properties_get(props, "inner_glow_softness"), en);
	return true;
}

static bool on_enable_shadow_changed(obs_properties_t *props,
				      obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	bool en = obs_data_get_bool(settings, "enable_shadow");
	obs_property_set_visible(obs_properties_get(props, "shadow_color"), en);
	obs_property_set_visible(obs_properties_get(props, "shadow_offset_x"), en);
	obs_property_set_visible(obs_properties_get(props, "shadow_offset_y"), en);
	obs_property_set_visible(obs_properties_get(props, "shadow_blur"), en);
	obs_property_set_visible(obs_properties_get(props, "shadow_opacity"), en);
	return true;
}

static obs_properties_t *glass_source_properties(void *data)
{
	struct glass_source *ctx = data;
	obs_properties_t *props = obs_properties_create();

#define TEXT(key) obs_module_text(key)
#define SLIDER_F(key, label, min, max, step) \
	obs_properties_add_float_slider(props, key, TEXT(label), min, max, step)
#define SLIDER_I(key, label, min, max, step) \
	obs_properties_add_int_slider(props, key, TEXT(label), min, max, step)
#define CHECK(key, label) \
	obs_properties_add_bool(props, key, TEXT(label))
#define COLOR(key, label) \
	obs_properties_add_color_alpha(props, key, TEXT(label))

	/* --- Suchfeld + Hintergrund-Source (alphabetisch sortiert, inkl. Szenen) --- */
	obs_property_t *search = obs_properties_add_text(
		props, "source_search", TEXT("SourceSearch"), OBS_TEXT_DEFAULT);
	obs_property_set_modified_callback(search, on_source_search_changed);

	obs_property_t *src_list = obs_properties_add_list(
		props, "target_source", TEXT("TargetSource"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	/* Initiale Befuellung mit gespeichertem Filter */
	{
		obs_data_t *s = ctx ? obs_source_get_settings(ctx->source) : NULL;
		const char *f = s ? obs_data_get_string(s, "source_search") : "";
		populate_sources_filtered(src_list, f);
		if (s) obs_data_release(s);
	}

	/* --- Globale Einstellungen --- */
	SLIDER_F("pos_x",         "PositionX",          -10000.0, 10000.0, 1.0);
	SLIDER_F("pos_y",         "PositionY",          -10000.0, 10000.0, 1.0);
	CHECK(   "transparent_bg","TransparentBg");

	/* --- Form-Definition --- */
	SLIDER_F("shape_width",   "ShapeWidth",          0.0, 4096.0, 1.0);
	SLIDER_F("shape_height",  "ShapeHeight",         0.0, 4096.0, 1.0);
	SLIDER_F("corner_radius", "CornerRadius",        0.0, 2048.0, 1.0);
	SLIDER_F("feathering",    "Feathering",          0.0,   50.0, 0.01);

	/* --- Glaseffekte --- */
	SLIDER_I("blur_strength", "BlurStrength",        0,     2,    1);
	SLIDER_F("blur_intensity","BlurIntensity",        0.1,   5.0, 0.1);
	SLIDER_F("frost_strength","FrostStrength",        0.0,  20.0, 0.01);
	obs_properties_add_color(props, "tint_color", TEXT("TintColor"));
	SLIDER_F("tint_strength", "TintStrength",         0.0,   1.0, 0.01);

	/* --- Linsenverzerrung --- */
	SLIDER_F("distortion_thickness","DistortionThickness", 0.0, 500.0, 1.0);
	SLIDER_F("max_distortion",      "MaxDistortion",      -2.0,   2.0, 0.01);
	SLIDER_F("distortion_falloff",  "DistortionFalloff",   0.1,  10.0, 0.01);
	SLIDER_F("magnification",       "Magnification",       0.1,   5.0, 0.01);

	/* --- Chromatische Aberration --- */
	obs_property_t *ca_chk = CHECK("enable_ca", "EnableCA");
	obs_property_set_modified_callback(ca_chk, on_enable_ca_changed);
	SLIDER_F("ca_strength",  "CAStrength",  0.0, 30.0, 0.1);
	SLIDER_F("ca_thickness", "CAThickness", 0.0, 500.0, 1.0);
	SLIDER_F("ca_falloff",   "CAFalloff",   0.1, 10.0, 0.1);

	/* --- Aeusseres Leuchten --- */
	obs_property_t *glow_chk = CHECK("enable_glow", "EnableGlow");
	obs_property_set_modified_callback(glow_chk, on_enable_glow_changed);
	COLOR(   "glow_color",     "GlowColor");
	SLIDER_F("glow_spread",    "GlowSpread",    0.0, 500.0, 1.0);
	SLIDER_F("glow_falloff",   "GlowFalloff",   0.0,  10.0, 0.01);
	CHECK(   "enable_dir_glow","EnableDirGlow");
	SLIDER_F("glow_angle",     "GlowAngle",       0.0, 360.0, 1.0);
	SLIDER_F("glow_spread_deg","GlowSpreadDeg",  10.0, 180.0, 1.0);
	SLIDER_F("glow_softness",  "GlowSoftness",    0.0,   1.0, 0.01);

	/* --- Inneres Leuchten --- */
	obs_property_t *ig_chk = CHECK("enable_inner_glow", "EnableInnerGlow");
	obs_property_set_modified_callback(ig_chk, on_enable_inner_glow_changed);
	COLOR(   "inner_glow_color",      "InnerGlowColor");
	SLIDER_F("inner_glow_spread",     "InnerGlowSpread",    0.0, 300.0, 1.0);
	SLIDER_F("inner_glow_falloff",    "InnerGlowFalloff",   0.0,  10.0, 0.01);
	CHECK(   "enable_dir_inner_glow", "EnableDirInnerGlow");
	SLIDER_F("inner_glow_angle",      "InnerGlowAngle",       0.0, 360.0, 1.0);
	SLIDER_F("inner_glow_spread_deg", "InnerGlowSpreadDeg",  10.0, 180.0, 1.0);
	SLIDER_F("inner_glow_softness",   "InnerGlowSoftness",    0.0,   1.0, 0.01);

	/* --- Schlagschatten --- */
	obs_property_t *sh_chk = CHECK("enable_shadow", "EnableShadow");
	obs_property_set_modified_callback(sh_chk, on_enable_shadow_changed);
	COLOR(   "shadow_color",    "ShadowColor");
	SLIDER_F("shadow_offset_x", "ShadowOffsetX", -500.0, 500.0, 1.0);
	SLIDER_F("shadow_offset_y", "ShadowOffsetY", -500.0, 500.0, 1.0);
	SLIDER_F("shadow_blur",     "ShadowBlur",       0.0, 200.0, 1.0);
	SLIDER_F("shadow_opacity",  "ShadowOpacity",    0.0,   1.0, 0.01);

#undef TEXT
#undef SLIDER_F
#undef SLIDER_I
#undef CHECK
#undef COLOR

	return props;
}

/* =========================================================================
 * Dimensionen
 * ========================================================================= */

static uint32_t glass_source_get_width(void *data)
{
	return ((struct glass_source *)data)->width;
}

static uint32_t glass_source_get_height(void *data)
{
	return ((struct glass_source *)data)->height;
}

/* =========================================================================
 * video_render
 * ========================================================================= */

static void glass_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct glass_source *ctx = data;

	if (!ctx->effect || !ctx->texrender)
		return;

	/* Rekursiven Render verhindern (Glass-Source als eigene Hintergrund-Szene) */
	if (ctx->rendering)
		return;
	ctx->rendering = true;

	/* Einheitlicher Pfad fuer alle Modi (Source / Scene / Aktuelle / Vorherige):
	 * Hintergrund-Source wird in privates Render-Target gerendert.
	 * Pattern aus obs-source-clone: Blend OUTSIDE texrender, ONE/ZERO (Replace)
	 * damit die Szene unveraendert eingefangen wird.
	 * Rekursionsschutz: ctx->rendering-Flag fängt re-entry ab, wenn die
	 * Ziel-Szene diese Glass-Source enthält (siehe oben). */
	obs_source_t *bg = ctx->target_weak
			   ? obs_weak_source_get_source(ctx->target_weak)
			   : NULL;
	if (!bg) {
		ctx->rendering = false;
		return;
	}

	uint32_t bg_w = obs_source_get_width(bg);
	uint32_t bg_h = obs_source_get_height(bg);

	if (bg_w == 0 || bg_h == 0) {
		obs_source_release(bg);
		ctx->rendering = false;
		return;
	}

	gs_texrender_reset(ctx->texrender);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	if (gs_texrender_begin(ctx->texrender, bg_w, bg_h)) {
		struct vec4 clear_color = {0};
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float)bg_w, 0.0f, (float)bg_h, -100.0f, 100.0f);
		obs_source_video_render(bg);
		gs_texrender_end(ctx->texrender);
	}
	gs_blend_state_pop();

	obs_source_release(bg);

	gs_texture_t *bg_tex = gs_texrender_get_texture(ctx->texrender);
	if (!bg_tex) {
		ctx->rendering = false;
		return;
	}

	/* Ausgabe-Dimensionen dynamisch an Hintergrund anpassen */
	ctx->width  = bg_w;
	ctx->height = bg_h;

	/* Uniforms setzen (gecachte eparam_t* — kein Lookup per Namen) */
	struct vec2 uv_size = {(float)bg_w, (float)bg_h};
	gs_effect_set_texture(ctx->p_image,     bg_tex);
	gs_effect_set_vec2(   ctx->p_uv_size,   &uv_size);
	gs_effect_set_float(  ctx->p_pos_x,     ctx->pos_x);
	gs_effect_set_float(  ctx->p_pos_y,     ctx->pos_y);
	gs_effect_set_bool(   ctx->p_transparent_bg, ctx->transparent_bg);
	gs_effect_set_float(  ctx->p_shape_width,    ctx->shape_width);
	gs_effect_set_float(  ctx->p_shape_height,   ctx->shape_height);
	gs_effect_set_float(  ctx->p_corner_radius,  ctx->corner_radius);
	gs_effect_set_float(  ctx->p_feathering,     ctx->feathering);
	gs_effect_set_int(    ctx->p_blur_strength,  ctx->blur_strength);
	gs_effect_set_float(  ctx->p_blur_intensity, ctx->blur_intensity);
	gs_effect_set_float(  ctx->p_frost_strength, ctx->frost_strength);

	struct vec4 tint_v4, glow_v4, iglow_v4, shadow_v4;
	packed_to_vec4(ctx->tint_color_packed,       &tint_v4);
	packed_to_vec4(ctx->glow_color_packed,        &glow_v4);
	packed_to_vec4(ctx->inner_glow_color_packed,  &iglow_v4);
	packed_to_vec4(ctx->shadow_color_packed,      &shadow_v4);

	gs_effect_set_vec4(ctx->p_tint_color,   &tint_v4);
	gs_effect_set_float(ctx->p_tint_strength, ctx->tint_strength);

	gs_effect_set_float(ctx->p_distortion_thickness, ctx->distortion_thickness);
	gs_effect_set_float(ctx->p_max_distortion,       ctx->max_distortion);
	gs_effect_set_float(ctx->p_distortion_falloff,   ctx->distortion_falloff);
	gs_effect_set_float(ctx->p_magnification,        ctx->magnification);

	gs_effect_set_bool( ctx->p_enable_ca,      ctx->enable_ca);
	gs_effect_set_float(ctx->p_ca_strength,    ctx->ca_strength);
	gs_effect_set_float(ctx->p_ca_thickness,   ctx->ca_thickness);
	gs_effect_set_float(ctx->p_ca_falloff,     ctx->ca_falloff);

	gs_effect_set_bool( ctx->p_enable_glow,    ctx->enable_glow);
	gs_effect_set_vec4( ctx->p_glow_color,     &glow_v4);
	gs_effect_set_float(ctx->p_glow_spread,    ctx->glow_spread);
	gs_effect_set_float(ctx->p_glow_falloff,   ctx->glow_falloff);
	gs_effect_set_bool( ctx->p_enable_dir_glow,ctx->enable_dir_glow);
	gs_effect_set_float(ctx->p_glow_angle,     ctx->glow_angle);
	gs_effect_set_float(ctx->p_glow_spread_deg,ctx->glow_spread_deg);
	gs_effect_set_float(ctx->p_glow_softness,  ctx->glow_softness);

	gs_effect_set_bool( ctx->p_enable_inner_glow,     ctx->enable_inner_glow);
	gs_effect_set_vec4( ctx->p_inner_glow_color,      &iglow_v4);
	gs_effect_set_float(ctx->p_inner_glow_spread,     ctx->inner_glow_spread);
	gs_effect_set_float(ctx->p_inner_glow_falloff,    ctx->inner_glow_falloff);
	gs_effect_set_bool( ctx->p_enable_dir_inner_glow, ctx->enable_dir_inner_glow);
	gs_effect_set_float(ctx->p_inner_glow_angle,      ctx->inner_glow_angle);
	gs_effect_set_float(ctx->p_inner_glow_spread_deg, ctx->inner_glow_spread_deg);
	gs_effect_set_float(ctx->p_inner_glow_softness,   ctx->inner_glow_softness);

	gs_effect_set_bool( ctx->p_enable_shadow,   ctx->enable_shadow);
	gs_effect_set_vec4( ctx->p_shadow_color,    &shadow_v4);
	gs_effect_set_float(ctx->p_shadow_offset_x, ctx->shadow_offset_x);
	gs_effect_set_float(ctx->p_shadow_offset_y, ctx->shadow_offset_y);
	gs_effect_set_float(ctx->p_shadow_blur,     ctx->shadow_blur);
	gs_effect_set_float(ctx->p_shadow_opacity,  ctx->shadow_opacity);

	/* Alpha-Blending fuer korrekte Transparenz */
	gs_blend_state_push();
	gs_reset_blend_state();

	while (gs_effect_loop(ctx->effect, "Draw"))
		gs_draw_sprite(NULL, 0, ctx->width, ctx->height);

	gs_blend_state_pop();
	ctx->rendering = false;
}

/* =========================================================================
 * obs_source_info Registrierung
 * ========================================================================= */

struct obs_source_info glass_source_info = {
	.id             = "glass_source",
	.type           = OBS_SOURCE_TYPE_INPUT,
	.output_flags   = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name       = glass_source_get_name,
	.create         = glass_source_create,
	.destroy        = glass_source_destroy,
	.update         = glass_source_update,
	.get_defaults   = glass_source_defaults,
	.get_properties = glass_source_properties,
	.get_width      = glass_source_get_width,
	.get_height     = glass_source_get_height,
	.video_render   = glass_source_render,
	.icon_type      = OBS_ICON_TYPE_CUSTOM,
};
