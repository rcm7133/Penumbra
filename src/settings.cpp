#include <string>

// Scene settings
std::string CURRENT_SCENE = "default.scene";
float AMBIENT_MULTIPLIER = 0.01f;
bool SKYBOX_ENABLED = true;
bool FREECAM_ENABLED = false;

// Global Illumination Settings
int GI_MODE = 1;
float GI_INTENSITY = 0.05f;
bool DEBUG_PROBES = false;

// Path Tracing
int PATH_TRACING_GI_BOUNCES = 3;  // Bounces per pixel
int PATH_TRACING_GI_SAMPLES = 4; // Samples per pixel
int PATH_TRACING_GI_FACE_SIZE = 16; // Resolution of cubemap

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

