#define MAX_FOG_VOLUMES 8

struct FogVolume {
    vec3 boundsMin;
    vec3 boundsMax;
    float density;
    float scale;
    float scrollSpeed;
};

uniform int fogVolumeCount;
uniform FogVolume fogVolumes[MAX_FOG_VOLUMES];