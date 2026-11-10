#pragma once
#include <FFGLSDK.h>
#include <chrono>
#include <random>
#include <vector>

// Light Beams: a grid of hanging fixtures shining straight down through haze,
// drawn as 2D beams placed in a simple perspective view. Lights can be
// triggered by built-in patterns (BPM synced or free running) or by trigger params.
class LightBeams : public CFFGLPlugin
{
public:
	LightBeams();

	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;
	FFResult DeInitGL() override;

	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;
	char* GetParameterDisplay( unsigned int index ) override;

	static const int MAX_ROWS   = 10;
	static const int MAX_COLS   = 24;
	static const int MAX_LIGHTS = MAX_ROWS * MAX_COLS;

private:
	struct Light
	{
		float env     = 0.0f; // envelope from hits, 0..1
		float age     = 1e6f; // seconds since last hit
		float jitterX = 0.0f; // random offsets, -1..1
		float jitterZ = 0.0f;
		float angle   = 0.0f;
		float phase   = 0.0f; // random 0..1, used for flicker
	};

	float P( int index ) const;
	int PI( int index ) const;

	void UpdateClock( float dt );
	void RunPatternStep();
	void HandleTriggers();
	void UpdateLights( float dt );
	void Hit( int index );
	void RebuildJitter();
	void BuildLightData();

	std::vector< float > values;
	std::vector< bool > pendingEvents;

	std::vector< Light > lights;
	int rows = 1, cols = 1;
	unsigned int jitterSeed = 0xffffffffu;
	std::mt19937 rng;

	// timing
	std::chrono::steady_clock::time_point lastFrame;
	bool firstFrame    = true;
	float timeSec      = 0.0f;
	float hazeTime     = 0.0f;
	float lastBarPhase = 0.0f;
	double barCount    = 0.0;
	double freeSteps   = 0.0;
	long long lastStep = -1;
	long long stepCount = 0;
	long long stepOffset = 0;
	int manualRow = 0, manualCol = 0;

	// GPU
	ffglex::FFGLShader shader;
	ffglex::FFGLScreenQuad quad;
	GLuint dataTexture = 0;
	std::vector< float > lightData;
	int activeCount = 0;
	float totalLight = 0.0f;
	char displayBuffer[ 32 ];
};
