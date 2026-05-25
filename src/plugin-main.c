/*
 * obs-glass — Glass Source Plugin fuer OBS Studio.
 * Copyright (C) 2026 K_STYER1, GPLv2 oder spaeter.
 */

#include <obs-module.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Eigenstaendige Glass-Source mit Liquid-Glass-Effekt (Blur, "
	       "Verzerrung, Leuchten, Schatten). Frei im Layer-Stack platzierbar.";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Glass Source";
}

MODULE_EXPORT const char *obs_module_author(void)
{
	return "K_STYER1";
}

extern struct obs_source_info glass_source_info;

bool obs_module_load(void)
{
	obs_register_source(&glass_source_info);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)",
		PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
