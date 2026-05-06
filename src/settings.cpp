#include <string>

// Scene settings
std::string CURRENT_SCENE = "default.scene";
float AMBIENT_MULTIPLIER = 0.01f;
bool SKYBOX_ENABLED = true;
bool FREECAM_ENABLED = false;
bool GUI_ENABLED = true;

bool RENDER_DEBUG_TEXTURE = false;
int  DEBUG_TEXTURE_INDEX = 0;

// Global Illumination Settings
int GI_MODE = 2;
float GI_INTENSITY = 0.05f;
bool DEBUG_PROBES = false;

// Path Tracing
int PATH_TRACING_GI_BOUNCES = 3;  // Bounces per pixel
int PATH_TRACING_GI_SAMPLES = 4; // Samples per pixel
int PATH_TRACING_GI_FACE_SIZE = 16; // Resolution of cubemap

// Clouds
bool CLOUD_ENABLED = false;
bool DEBUG_CLOUDS = false;
int CLOUD_LIGHTING_UPDATE_INTERVAL = 1;
int CLOUD_RAYMARCH_STEPS = 32;
int CLOUD_RESOLUTION_SCALE = 1;
int CLOUD_RAYMARCH_LIGHTING_STEPS = 32;
float CLOUD_RAYMARCH_LIGHTING_RAY_DEPTH = 1.0f;
float CLOUD_ABSORPTION = 0.8f;

// SSR
bool SSR_ENABLED = true;
bool REFLECTION_PROBE_ENABLED = false;
int SSR_RAYMARCH_STEPS = 40;
float SSR_MAX_STEP_SIZE = 0.5;
float SSR_MIN_STEP_SIZE = 0.0025;

// Reflection Probes
bool DEBUG_REFLECTION_PROBES = false;
int MAX_REFLECTION_PROBES = 8;

// Shadow settings
int MAX_SHADOW_LIGHTS = 3;
int SHADOW_RESOLUTION = 2048;
int POINT_SHADOW_RESOLUTION = 1024;
float POINT_SHADOW_FAR_PLANE = 25.0f;
float SHADOW_BIAS = 0.0008;
float SHADOW_NORMAL_OFFSET = 0.015;
int PCF_KERNEL_SIZE = 3;
bool PCF_ENABLED = true;

// Fog settings
bool FOG_ENABLED = true;
float FOG_DENSITY = 0.3f;
int FOG_STEPS = 48;
float FOG_SCALE = 0.5f;
float FOG_SCROLL_SPEED = 0.03f;
int FOG_RESOLUTION_SCALE = 4;
int FOG_BLUR_KERNEL_SIZE = 5;

// SSAO settings
bool SSAO_ENABLED = true;
float SSAO_RADIUS = 0.5f;
float SSAO_BIAS = 0.025f;
int SSAO_KERNEL_SIZE = 16;

// FXAA Settings. Inspired by NVIDIA FXAA paper: https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf
bool FXAA_ENABLED = true;
float FXAA_EDGE_THRESHOLD = 0.125f;      // 1/4 low quality, 1/8 high quality
float FXAA_EDGE_THRESHOLD_MIN = 0.0625f; // 1/16

// Camera
float CAMERA_SPEED = 1.5f;

// Debug
bool DEBUG_COLLIDERS = false;

