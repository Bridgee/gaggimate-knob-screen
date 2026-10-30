"""Candidate-only bench installation permission; never enables normal OTA."""
def qualification_enabled(config):
    enabled = config.get('screen_ota_qualification', False)
    if not isinstance(enabled, bool):
        raise RuntimeError('screen_ota_qualification must be boolean')
    if enabled and (config.get('release_channel') != 'candidate' or
                    '-' not in config.get('port_version', '').split('+', 1)[0]):
        raise RuntimeError('Screen OTA qualification is restricted to explicit candidate builds')
    return enabled
