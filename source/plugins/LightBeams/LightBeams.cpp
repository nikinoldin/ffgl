#include "LightBeams.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
using namespace ffglex;

static CFFGLPluginInfo PluginInfo(
	PluginFactory< LightBeams >,                  // Create method
	"LBM1",                                       // Plugin unique ID
	"Light Beams",                                // Plugin name
	2,                                            // API major version number
	1,                                            // API minor version number
	1,                                            // Plugin major version number
	0,                                            // Plugin minor version number
	FF_SOURCE,                                    // Plugin type
	"Grid of hanging beam lights in haze, with BPM synced patterns and triggers",
	"Light Beams FFGL source" );

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------
enum ParamIndex : unsigned int
{
	// Animation
	P_PATTERN,
	P_RATE,
	P_FREE_RATE,
	P_BASE_LEVEL,
	P_ATTACK,
	P_DECAY,
	P_DROP_TIME,
	P_DENSITY,
	P_CHASE_WIDTH,

	// Triggers
	P_FLASH_ALL,
	P_FLASH_RANDOM,
	P_STEP,
	P_NEXT_ROW,
	P_NEXT_COL,
	P_RESYNC,

	// Look
	P_INTENSITY,
	P_HUE,
	P_SATURATION,
	P_BEAM_WIDTH,
	P_SPREAD,
	P_GLOW,
	P_HAZE,
	P_HAZE_SPEED,
	P_ATMOSPHERE,
	P_FLOOR_GLOW,
	P_REFLECTION,
	P_FIXTURE_GLOW,
	P_DEPTH_FADE,
	P_BACKGROUND,
	P_TRANSPARENT,

	// Layout
	P_ROWS,
	P_COLS,
	P_SPACING_X,
	P_SPACING_Z,
	P_STAGGER,
	P_JITTER,
	P_ANGLE_JITTER,
	P_BEAM_TILT,
	P_BEAM_TURN,
	P_THROW,
	P_MIRROR,
	P_SEED,
	P_HEIGHT,
	P_BEAM_LENGTH,

	// Camera
	P_CAM_DIST,
	P_CAM_HEIGHT,
	P_CAM_TILT,
	P_CAM_FOV,
	P_CAM_PAN,

	P_COUNT
};

enum Pattern
{
	PAT_STATIC,
	PAT_CHASE,
	PAT_ROW_CHASE,
	PAT_SNAKE,
	PAT_RANDOM,
	PAT_RANDOM_SINGLE,
	PAT_ALTERNATE,
	PAT_STROBE,
	PAT_FILL,
	PAT_CENTER_OUT,
	PAT_WAVE,
	PAT_MANUAL,
	PAT_COUNT
};
static const char* patternNames[ PAT_COUNT ] = {
	"Static (all on)", "Chase", "Row Chase", "Snake", "Random", "Random Single",
	"Alternate", "Strobe", "Fill", "Center Out", "Wave", "Manual (triggers only)"
};

static const int MIRROR_COUNT                  = 4;
static const char* mirrorNames[ MIRROR_COUNT ] = { "Off", "Every Other Column", "Every Other Row", "Checker" };

static const int RATE_COUNT                = 8;
static const char* rateNames[ RATE_COUNT ] = { "Free (Hz)", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16" };
static const float stepsPerBar[ RATE_COUNT ] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };

struct ParamDef
{
	const char* name;
	FFUInt32 type;
	float def, min, max;
};

// Ranges are in real units (meters, degrees, seconds). Option params use min/max as element count.
static const ParamDef paramDefs[ P_COUNT ] = {
	// Animation
	{ "Pattern", FF_TYPE_OPTION, PAT_STATIC, 0, PAT_COUNT },
	{ "Rate", FF_TYPE_OPTION, 5, 0, RATE_COUNT },
	{ "Free Rate Hz", FF_TYPE_STANDARD, 2.0f, 0.05f, 20.0f },
	{ "Base Level", FF_TYPE_STANDARD, 0.0f, 0.0f, 1.0f },
	{ "Attack", FF_TYPE_STANDARD, 0.0f, 0.0f, 2.0f },
	{ "Decay", FF_TYPE_STANDARD, 0.35f, 0.01f, 4.0f },
	{ "Drop Time", FF_TYPE_STANDARD, 0.0f, 0.0f, 2.0f },
	{ "Density", FF_TYPE_STANDARD, 0.3f, 0.0f, 1.0f },
	{ "Chase Width", FF_TYPE_INTEGER, 1, 1, LightBeams::MAX_COLS },
	// Triggers
	{ "Flash All", FF_TYPE_EVENT, 0, 0, 1 },
	{ "Flash Random", FF_TYPE_EVENT, 0, 0, 1 },
	{ "Step", FF_TYPE_EVENT, 0, 0, 1 },
	{ "Next Row", FF_TYPE_EVENT, 0, 0, 1 },
	{ "Next Column", FF_TYPE_EVENT, 0, 0, 1 },
	{ "Resync", FF_TYPE_EVENT, 0, 0, 1 },
	// Look
	{ "Intensity", FF_TYPE_STANDARD, 1.0f, 0.0f, 3.0f },
	{ "Hue", FF_TYPE_STANDARD, 0.5f, 0.0f, 1.0f },
	{ "Saturation", FF_TYPE_STANDARD, 0.22f, 0.0f, 1.0f },
	{ "Beam Width", FF_TYPE_STANDARD, 0.12f, 0.01f, 1.0f },
	{ "Beam Spread", FF_TYPE_STANDARD, 0.25f, 0.0f, 1.0f },
	{ "Glow", FF_TYPE_STANDARD, 0.6f, 0.0f, 1.0f },
	{ "Haze", FF_TYPE_STANDARD, 0.5f, 0.0f, 1.0f },
	{ "Haze Speed", FF_TYPE_STANDARD, 0.3f, 0.0f, 1.0f },
	{ "Atmosphere", FF_TYPE_STANDARD, 0.6f, 0.0f, 1.0f },
	{ "Floor Glow", FF_TYPE_STANDARD, 0.6f, 0.0f, 1.0f },
	{ "Reflection", FF_TYPE_STANDARD, 0.4f, 0.0f, 1.0f },
	{ "Fixture Glow", FF_TYPE_STANDARD, 0.5f, 0.0f, 1.0f },
	{ "Depth Fade", FF_TYPE_STANDARD, 0.35f, 0.0f, 1.0f },
	{ "Background", FF_TYPE_STANDARD, 0.4f, 0.0f, 1.0f },
	{ "Transparent BG", FF_TYPE_BOOLEAN, 0, 0, 1 },
	// Layout
	{ "Rows", FF_TYPE_INTEGER, 4, 1, LightBeams::MAX_ROWS },
	{ "Columns", FF_TYPE_INTEGER, 8, 1, LightBeams::MAX_COLS },
	{ "Spacing X", FF_TYPE_STANDARD, 3.0f, 0.3f, 10.0f },
	{ "Spacing Depth", FF_TYPE_STANDARD, 5.0f, 0.3f, 15.0f },
	{ "Row Stagger", FF_TYPE_STANDARD, 0.5f, 0.0f, 1.0f },
	{ "Position Jitter", FF_TYPE_STANDARD, 0.25f, 0.0f, 1.0f },
	{ "Angle Jitter", FF_TYPE_STANDARD, 0.06f, 0.0f, 1.0f },
	{ "Beam Tilt", FF_TYPE_STANDARD, 0.0f, 0.0f, 180.0f },
	{ "Beam Turn", FF_TYPE_STANDARD, 0.0f, -180.0f, 180.0f },
	{ "Throw", FF_TYPE_STANDARD, 40.0f, 1.0f, 100.0f },
	{ "Mirror", FF_TYPE_OPTION, 0, 0, MIRROR_COUNT },
	{ "Layout Seed", FF_TYPE_INTEGER, 1, 0, 99 },
	{ "Fixture Height", FF_TYPE_STANDARD, 8.0f, 0.0f, 25.0f },
	{ "Beam Length", FF_TYPE_STANDARD, 1.0f, 0.0f, 1.0f },
	// Camera
	{ "Cam Distance", FF_TYPE_STANDARD, 11.0f, 0.5f, 40.0f },
	{ "Cam Height", FF_TYPE_STANDARD, 1.7f, 0.0f, 20.0f },
	{ "Cam Tilt", FF_TYPE_STANDARD, 4.0f, -45.0f, 60.0f },
	{ "Cam FOV", FF_TYPE_STANDARD, 60.0f, 15.0f, 120.0f },
	{ "Cam Pan X", FF_TYPE_STANDARD, 0.0f, -20.0f, 20.0f },
};

static const int DATA_ROWS = 4;// texels per light, stacked vertically

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------
static const char vertexShaderCode[] = R"(#version 410 core
layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;
out vec2 uv;
void main()
{
	gl_Position = vPosition;
	uv = vUV;
}
)";

// Per light data texture (one column per light):
//  row 0: top.xy, end.xy        (screen space, y in -1..1, x scaled by aspect)
//  row 1: widthTop, widthEnd, intensity, floorAmount
//  row 2: reflectionStart.xy, reflectionEnd.xy (mirror image of the beam in the floor)
//  row 3: spotRadius.xy, reflectionAmount, reflectionFadePower
static const char fragmentShaderCode[] = R"(#version 410 core
in vec2 uv;
out vec4 fragColor;

uniform sampler2D lightData;
uniform int lightCount;
uniform vec2 resolution;
uniform float time;
uniform float hazeTime;
uniform vec3 tint;
uniform float glow;
uniform float haze;
uniform float ambient;
uniform float floorGlow;
uniform float reflection;
uniform float fixtureGlow;
uniform float background;
uniform float transparentBg;
uniform float exposure;
uniform float horizonY;

float hash12( vec2 p )
{
	vec3 p3 = fract( vec3( p.xyx ) * 0.1031 );
	p3 += dot( p3, p3.yzx + 33.33 );
	return fract( ( p3.x + p3.y ) * p3.z );
}
float vnoise( vec2 p )
{
	vec2 i = floor( p );
	vec2 f = fract( p );
	vec2 u = f * f * ( 3.0 - 2.0 * f );
	return mix( mix( hash12( i ), hash12( i + vec2( 1, 0 ) ), u.x ),
	            mix( hash12( i + vec2( 0, 1 ) ), hash12( i + vec2( 1, 1 ) ), u.x ), u.y );
}
float fbm( vec2 p )
{
	float v = 0.0;
	float a = 0.5;
	for( int i = 0; i < 4; i++ )
	{
		v += a * vnoise( p );
		p = p * 2.03 + vec2( 17.1, 9.2 );
		a *= 0.5;
	}
	return v / 0.9375;
}
float segment( vec2 p, vec2 a, vec2 b, out float t )
{
	vec2 pa = p - a;
	vec2 ba = b - a;
	t = clamp( dot( pa, ba ) / max( dot( ba, ba ), 1e-8 ), 0.0, 1.0 );
	return length( pa - ba * t );
}

void main()
{
	vec2 p   = ( uv * 2.0 - 1.0 ) * vec2( resolution.x / resolution.y, 1.0 );
	float px = 2.0 / resolution.y;

	// Smoke field shared by all beams: drifts sideways and slowly rises.
	float smokeA   = fbm( vec2( p.x * 1.6 + hazeTime * 0.12, p.y * 2.4 - hazeTime * 0.30 ) );
	float smokeB   = fbm( vec2( p.x * 4.5 - hazeTime * 0.25, p.y * 6.0 - hazeTime * 0.55 ) + 7.0 );
	float smoke    = 0.65 * smokeA + 0.35 * smokeB;
	float smokeMod = mix( 1.0, 0.2 + 1.6 * smoke, haze );

	float coreSum = 0.0;
	float glowSum = 0.0;
	for( int i = 0; i < lightCount; i++ )
	{
		vec4 A = texelFetch( lightData, ivec2( i, 0 ), 0 );
		vec4 B = texelFetch( lightData, ivec2( i, 1 ), 0 );
		vec4 C = texelFetch( lightData, ivec2( i, 2 ), 0 );
		vec4 D = texelFetch( lightData, ivec2( i, 3 ), 0 );
		float I = B.z;

		float wMax  = max( max( B.x, B.y ), px );
		float reach = wMax * 25.0 + max( D.x, 0.0 ) * 2.5;
		reach       = max( reach, wMax * 3.0 * 6.0 );// room for the wider reflection
		float lo    = min( min( A.x, A.z ), min( C.x, C.z ) ) - reach;
		float hi    = max( max( A.x, A.z ), max( C.x, C.z ) ) + reach;
		if( p.x < lo || p.x > hi )
			continue;

		// Beam body
		float t;
		float d     = segment( p, A.xy, A.zw, t );
		float w     = max( mix( B.x, B.y, t ), px * 0.75 );
		float x2    = ( d * d ) / ( w * w );
		float along = mix( 1.0, 0.72, t );
		float core  = exp( -x2 * 1.5 ) * along * mix( 1.0, smokeMod, 0.55 );
		float halo  = ( exp( -x2 * 0.06 ) * 0.22 + 0.05 / ( 1.0 + x2 * 0.02 ) ) * glow;
		halo *= smoothstep( reach, reach * 0.35, d ) * along * smokeMod;

		// Lens of the fixture at the top
		float dt  = length( p - A.xy );
		float wt  = max( B.x, px );
		float fix = ( exp( -( dt * dt ) / ( wt * wt * 1.4 ) ) * 2.5 + exp( -dt / ( wt * 5.0 ) ) * 0.25 ) * fixtureGlow;

		float spot = 0.0;
		float refl = 0.0;
		if( B.w > 0.001 )
		{
			// Hot spot where the beam hits the floor, an ellipse squashed by perspective
			vec2 q   = ( p - A.zw ) / max( D.xy, vec2( px ) );
			float sd = dot( q, q );
			spot     = ( exp( -sd * 3.0 ) * 1.4 + exp( -sqrt( sd ) * 1.6 ) * 0.2 ) * floorGlow * B.w;
		}
		if( D.z > 0.001 )
		{
			// Mirror image of the beam in the glossy floor, with a little ripple
			vec2 pr = p;
			pr.x += ( vnoise( vec2( p.y * 18.0, time * 0.8 + float( i ) * 3.7 ) ) - 0.5 ) * B.y * 0.9;
			float tr;
			float dr  = segment( pr, C.xy, C.zw, tr );
			// D.w: fade power (3 for vertical beams, 0 for flat), negative when the fixture end is the lower one
			float steep = clamp( abs( D.w ) / 3.0, 0.0, 1.0 );
			float wLow  = D.w < 0.0 ? B.x : B.y;
			float wHigh = D.w < 0.0 ? B.y : B.x;
			float wr    = max( mix( wLow * 1.3, mix( wHigh * 1.3, wLow * 3.0, steep ), tr ), px );
			float xr2   = ( dr * dr ) / ( wr * wr );
			float fade  = abs( D.w ) > 0.01 ? pow( 1.0 - tr, abs( D.w ) ) : 1.0;
			fade *= mix( smoothstep( 0.0, 0.12, tr ) * smoothstep( 1.0, 0.88, tr ), 1.0, steep );// soft ends on flat beams
			refl = ( exp( -xr2 * 1.0 ) * 0.55 + exp( -xr2 * 0.05 ) * 0.08 * glow ) * fade * reflection * D.z;
			refl *= smoothstep( reach, reach * 0.35, dr );// fade out before the culling edge
		}

		coreSum += I * ( core + fix );
		glowSum += I * ( halo + spot + refl );
	}

	// Ambient haze lit by the beams, densest around eye level
	float band = exp( -pow( ( p.y - horizonY - 0.05 ) / 0.55, 2.0 ) );
	float fog  = ambient * ( 0.08 + 0.92 * band ) * mix( 1.0, 0.25 + 1.5 * smokeA, haze );

	vec3 coreCol = mix( tint, vec3( 1.0 ), 0.7 );
	vec3 col     = coreCol * coreSum + tint * ( glowSum + fog );
	col          = vec3( 1.0 ) - exp( -col * exposure );

	// Dark room
	vec3 bg = ( vec3( 0.006, 0.008, 0.010 ) + tint * 0.025 * ( 0.5 + band ) ) * background * 2.0;
	if( transparentBg < 0.5 )
		col += bg;

	// Dither against banding in the dark gradients
	col += ( hash12( gl_FragCoord.xy + fract( time ) * 91.0 ) - 0.5 ) / 255.0;
	col = max( col, vec3( 0.0 ) );

	if( transparentBg > 0.5 )
	{
		float a   = clamp( max( col.r, max( col.g, col.b ) ), 0.0, 1.0 );
		fragColor = vec4( a > 0.0 ? col / a : vec3( 0.0 ), a );
	}
	else
	{
		fragColor = vec4( col, 1.0 );
	}
}
)";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
struct Vec3
{
	float x, y, z;
};
struct Proj
{
	float x, y, depth;
};

struct Camera
{
	float px, py, pz;
	float cosT, sinT;
	float focal;
	float aspect;

	float Depth( const Vec3& p ) const
	{
		float dy = p.y - py;
		float dz = p.z - pz;
		return dy * sinT + dz * cosT;
	}
	Proj Project( const Vec3& p ) const
	{
		float dx = p.x - px;
		float dy = p.y - py;
		float dz = p.z - pz;
		float y  = dy * cosT - dz * sinT;
		float z  = dy * sinT + dz * cosT;
		z        = std::max( z, 1e-4f );
		return { dx * focal / z, y * focal / z, z };
	}
};

Vec3 Lerp( const Vec3& a, const Vec3& b, float t )
{
	return { a.x + ( b.x - a.x ) * t, a.y + ( b.y - a.y ) * t, a.z + ( b.z - a.z ) * t };
}

// Moves a and b so the segment stays in front of the near plane. Returns false if fully behind.
bool ClipNear( const Camera& cam, Vec3& a, Vec3& b, float nearZ )
{
	float da = cam.Depth( a );
	float db = cam.Depth( b );
	if( da < nearZ && db < nearZ )
		return false;
	if( da < nearZ )
		a = Lerp( a, b, ( nearZ - da ) / ( db - da ) );
	else if( db < nearZ )
		b = Lerp( a, b, ( nearZ - da ) / ( db - da ) );
	return true;
}

float Smoothstep( float e0, float e1, float x )
{
	float t = std::min( std::max( ( x - e0 ) / ( e1 - e0 ), 0.0f ), 1.0f );
	return t * t * ( 3.0f - 2.0f * t );
}
}// namespace

// ---------------------------------------------------------------------------
// Plugin
// ---------------------------------------------------------------------------
LightBeams::LightBeams() :
	values( P_COUNT, 0.0f ),
	pendingEvents( P_COUNT, false ),
	lights( MAX_LIGHTS ),
	rng( 1234 ),
	lightData( MAX_LIGHTS * DATA_ROWS * 4, 0.0f )
{
	SetMinInputs( 0 );
	SetMaxInputs( 0 );

	for( unsigned int i = 0; i < P_COUNT; ++i )
	{
		const ParamDef& d = paramDefs[ i ];
		values[ i ]       = d.def;
		const char* group = i < P_FLASH_ALL ? "Animation" : i < P_INTENSITY ? "Triggers" : i < P_ROWS ? "Look" : i < P_CAM_DIST ? "Layout" : "Camera";
		switch( d.type )
		{
		case FF_TYPE_OPTION:
		{
			const char** names = i == P_PATTERN ? patternNames : i == P_RATE ? rateNames : mirrorNames;
			SetOptionParamInfo( i, d.name, (unsigned int)d.max, d.def );
			for( unsigned int e = 0; e < (unsigned int)d.max; ++e )
				SetParamElementInfo( i, e, names[ e ], (float)e );
			break;
		}
		case FF_TYPE_BOOLEAN:
		case FF_TYPE_EVENT:
			SetParamInfo( i, d.name, d.type, d.def > 0.5f );
			break;
		default:
			SetParamInfo( i, d.name, d.type, d.def );
			SetParamRange( i, d.min, d.max );
			// SetParamInfo clamps standard defaults to 0..1, but our ranges are in real units
			if( ParamInfo* info = FindParamInfo( i ) )
				info->defaultFloatVal = d.def;
			break;
		}
		SetParamGroup( i, group );
	}
	// Option element values are indices, these are the defaults Resolume shows
	values[ P_PATTERN ] = PAT_STATIC;
	values[ P_RATE ]    = 5;
}

float LightBeams::P( int index ) const
{
	return values[ index ];
}
int LightBeams::PI( int index ) const
{
	return (int)std::lround( values[ index ] );
}

FFResult LightBeams::InitGL( const FFGLViewportStruct* vp )
{
	if( !shader.Compile( vertexShaderCode, fragmentShaderCode ) )
	{
		DeInitGL();
		return FF_FAIL;
	}
	if( !quad.Initialise() )
	{
		DeInitGL();
		return FF_FAIL;
	}

	glGenTextures( 1, &dataTexture );
	{
		Scoped2DTextureBinding binding( dataTexture );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, MAX_LIGHTS, DATA_ROWS, 0, GL_RGBA, GL_FLOAT, lightData.data() );
	}

	firstFrame = true;
	return CFFGLPlugin::InitGL( vp );
}

FFResult LightBeams::DeInitGL()
{
	shader.FreeGLResources();
	quad.Release();
	if( dataTexture != 0 )
	{
		glDeleteTextures( 1, &dataTexture );
		dataTexture = 0;
	}
	return FF_SUCCESS;
}

FFResult LightBeams::SetFloatParameter( unsigned int index, float value )
{
	if( index >= P_COUNT )
		return FF_FAIL;
	// Latch trigger presses here, so a short MIDI note that releases before the next frame still fires
	if( paramDefs[ index ].type == FF_TYPE_EVENT && value > 0.5f && values[ index ] <= 0.5f )
		pendingEvents[ index ] = true;
	values[ index ] = value;
	return FF_SUCCESS;
}

float LightBeams::GetFloatParameter( unsigned int index )
{
	if( index >= P_COUNT )
		return 0.0f;
	return values[ index ];
}

char* LightBeams::GetParameterDisplay( unsigned int index )
{
	if( index >= P_COUNT )
		return CFFGLPlugin::GetParameterDisplay( index );
	const ParamDef& d = paramDefs[ index ];
	switch( d.type )
	{
	case FF_TYPE_OPTION:
	{
		int i = std::min( std::max( PI( index ), 0 ), (int)d.max - 1 );
		snprintf( displayBuffer, sizeof( displayBuffer ), "%s", index == P_PATTERN ? patternNames[ i ] : index == P_RATE ? rateNames[ i ] : mirrorNames[ i ] );
		break;
	}
	case FF_TYPE_INTEGER:
		snprintf( displayBuffer, sizeof( displayBuffer ), "%d", PI( index ) );
		break;
	case FF_TYPE_BOOLEAN:
	case FF_TYPE_EVENT:
		snprintf( displayBuffer, sizeof( displayBuffer ), "%s", values[ index ] > 0.5f ? "On" : "Off" );
		break;
	default:
		snprintf( displayBuffer, sizeof( displayBuffer ), "%.2f", values[ index ] );
		break;
	}
	return displayBuffer;
}

// ---------------------------------------------------------------------------
// Triggering
// ---------------------------------------------------------------------------
void LightBeams::Hit( int index )
{
	if( index < 0 || index >= rows * cols )
		return;
	lights[ index ].age = 0.0f;
}

void LightBeams::RunPatternStep()
{
	const int n = rows * cols;
	if( n == 0 )
		return;
	const long long k = stepCount;
	std::uniform_real_distribution< float > uni( 0.0f, 1.0f );
	const int width = std::max( 1, std::min( PI( P_CHASE_WIDTH ), cols ) );

	switch( PI( P_PATTERN ) )
	{
	case PAT_CHASE:
	{
		int start = (int)( k % cols );
		for( int w = 0; w < width; ++w )
			for( int r = 0; r < rows; ++r )
				Hit( r * cols + ( start + w ) % cols );
		break;
	}
	case PAT_ROW_CHASE:
	{
		int row = (int)( k % rows );
		for( int c = 0; c < cols; ++c )
			Hit( row * cols + c );
		break;
	}
	case PAT_SNAKE:
	{
		for( int w = 0; w < width; ++w )
		{
			int idx = (int)( ( k + w ) % n );
			int r   = idx / cols;
			int c   = idx % cols;
			if( r % 2 == 1 )
				c = cols - 1 - c;
			Hit( r * cols + c );
		}
		break;
	}
	case PAT_RANDOM:
	{
		bool any = false;
		for( int i = 0; i < n; ++i )
		{
			if( uni( rng ) < P( P_DENSITY ) )
			{
				Hit( i );
				any = true;
			}
		}
		if( !any )
			Hit( std::uniform_int_distribution< int >( 0, n - 1 )( rng ) );
		break;
	}
	case PAT_RANDOM_SINGLE:
	{
		for( int w = 0; w < width; ++w )
			Hit( std::uniform_int_distribution< int >( 0, n - 1 )( rng ) );
		break;
	}
	case PAT_ALTERNATE:
	{
		for( int r = 0; r < rows; ++r )
			for( int c = 0; c < cols; ++c )
				if( ( r + c + k ) % 2 == 0 )
					Hit( r * cols + c );
		break;
	}
	case PAT_STROBE:
	{
		for( int i = 0; i < n; ++i )
			Hit( i );
		break;
	}
	case PAT_FILL:
	{
		// Fill left to right, column by column, then start over
		int count = (int)( k % n ) + 1;
		for( int o = 0; o < count; ++o )
			Hit( ( o % rows ) * cols + o / rows );
		break;
	}
	case PAT_CENTER_OUT:
	{
		int half      = ( cols + 1 ) / 2;
		int ring      = (int)( k % half );
		float center  = ( cols - 1 ) * 0.5f;
		for( int c = 0; c < cols; ++c )
		{
			int dist = (int)std::floor( std::fabs( c - center ) );
			if( dist == ring )
				for( int r = 0; r < rows; ++r )
					Hit( r * cols + c );
		}
		break;
	}
	default:
		break;// Static, Wave and Manual don't hit on steps
	}
}

void LightBeams::HandleTriggers()
{
	auto rising = [ & ]( int index ) {
		bool fired             = pendingEvents[ index ];
		pendingEvents[ index ] = false;
		return fired;
	};

	const int n = rows * cols;
	if( rising( P_FLASH_ALL ) )
		for( int i = 0; i < n; ++i )
			Hit( i );
	if( rising( P_FLASH_RANDOM ) && n > 0 )
	{
		std::uniform_real_distribution< float > uni( 0.0f, 1.0f );
		bool any = false;
		for( int i = 0; i < n; ++i )
			if( uni( rng ) < P( P_DENSITY ) )
			{
				Hit( i );
				any = true;
			}
		if( !any )
			Hit( std::uniform_int_distribution< int >( 0, n - 1 )( rng ) );
	}
	if( rising( P_STEP ) )
	{
		--stepOffset;// advance the pattern by one; automatic steps continue from here
		++stepCount;
		RunPatternStep();
	}
	if( rising( P_NEXT_ROW ) && rows > 0 )
	{
		int r = manualRow++ % rows;
		for( int c = 0; c < cols; ++c )
			Hit( r * cols + c );
	}
	if( rising( P_NEXT_COL ) && cols > 0 )
	{
		int c = manualCol++ % cols;
		for( int r = 0; r < rows; ++r )
			Hit( r * cols + c );
	}
	if( rising( P_RESYNC ) )
	{
		// Restart the pattern from its first step right now
		freeSteps  = 0.0;
		stepOffset = lastStep;
		stepCount  = 0;
		manualRow = manualCol = 0;
		int rate = std::min( std::max( PI( P_RATE ), 0 ), RATE_COUNT - 1 );
		if( rate == 0 || bpm <= 0.0f )
			stepOffset = lastStep = 0;
		RunPatternStep();
	}
}

// Advances the step clock. Synced rates follow the host's bar phase, "Free" uses Free Rate Hz.
void LightBeams::UpdateClock( float dt )
{
	int rate      = std::min( std::max( PI( P_RATE ), 0 ), RATE_COUNT - 1 );
	bool useHost  = rate != 0 && bpm > 0.0f;
	double pos;

	if( barPhase < lastBarPhase - 0.5f )
		barCount += 1.0;
	lastBarPhase = barPhase;

	if( useHost )
		pos = ( barCount + barPhase ) * stepsPerBar[ rate ];
	else
	{
		freeSteps += dt * P( P_FREE_RATE );
		pos = freeSteps;
	}

	long long step = (long long)std::floor( pos );
	if( step != lastStep )
	{
		lastStep  = step;
		stepCount = step - stepOffset;// stays locked to the bar, so chases start on the downbeat
		RunPatternStep();
	}
}

void LightBeams::UpdateLights( float dt )
{
	const int n        = rows * cols;
	const float attack = P( P_ATTACK );
	const float decay  = std::max( P( P_DECAY ), 0.01f );

	for( int i = 0; i < n; ++i )
	{
		Light& l = lights[ i ];
		l.age    = std::min( l.age + dt, 60.0f );
		if( l.age < attack )
			l.env = l.age / attack;
		else
			l.env = std::exp( -( l.age - attack ) * 3.0f / decay );// ~5% left after "Decay" seconds
	}
}

void LightBeams::RebuildJitter()
{
	std::mt19937 gen( (unsigned int)PI( P_SEED ) * 7919u + 17u );
	std::uniform_real_distribution< float > uni( -1.0f, 1.0f );
	for( Light& l : lights )
	{
		l.jitterX = uni( gen );
		l.jitterZ = uni( gen );
		l.angle   = uni( gen );
		l.phase   = uni( gen ) * 0.5f + 0.5f;
	}
	jitterSeed = (unsigned int)PI( P_SEED );
}

// ---------------------------------------------------------------------------
// Geometry: place the fixtures in 3D, project the beams to the screen
// ---------------------------------------------------------------------------
void LightBeams::BuildLightData()
{
	const float aspect = currentViewport.height > 0 ? (float)currentViewport.width / (float)currentViewport.height : 16.0f / 9.0f;
	const float deg    = 3.14159265f / 180.0f;
	const float tilt   = P( P_CAM_TILT ) * deg;

	Camera cam;
	cam.px     = P( P_CAM_PAN );
	cam.py     = P( P_CAM_HEIGHT );
	cam.pz     = 0.0f;
	cam.cosT   = std::cos( tilt );
	cam.sinT   = std::sin( tilt );
	cam.focal  = 1.0f / std::tan( P( P_CAM_FOV ) * 0.5f * deg );
	cam.aspect = aspect;

	const float height    = P( P_HEIGHT );
	const float sx        = P( P_SPACING_X );
	const float sz        = P( P_SPACING_Z );
	const float jitter    = P( P_JITTER );
	const float stagger   = P( P_STAGGER );
	const float width     = P( P_BEAM_WIDTH );
	const float spread    = P( P_SPREAD );
	const float beamLen   = P( P_BEAM_LENGTH );
	const float depthFade = P( P_DEPTH_FADE );
	const float dropTime  = P( P_DROP_TIME );
	const float base      = P( P_BASE_LEVEL );
	const int pattern     = PI( P_PATTERN );
	const float nearZ     = 0.05f;
	const float beamTilt  = P( P_BEAM_TILT ) * deg;
	const float beamTurn  = P( P_BEAM_TURN ) * deg;
	const float throwDist = P( P_THROW );
	const int mirrorMode  = PI( P_MIRROR );

	int rate        = std::min( std::max( PI( P_RATE ), 0 ), RATE_COUNT - 1 );
	double wavePos  = ( rate != 0 && bpm > 0.0f ) ? ( barCount + barPhase ) * stepsPerBar[ rate ] : freeSteps;
	float wavePhase = (float)std::fmod( wavePos / 4.0, 1.0 );

	const int stride = MAX_LIGHTS * 4;
	float* row0      = lightData.data();
	float* row1      = row0 + stride;
	float* row2      = row1 + stride;
	float* row3      = row2 + stride;

	activeCount = 0;
	totalLight  = 0.0f;

	// Draw back rows first; the order doesn't change the additive result, but it keeps things stable.
	for( int r = rows - 1; r >= 0; --r )
	{
		for( int c = 0; c < cols; ++c )
		{
			const int i    = r * cols + c;
			const Light& l = lights[ i ];

			// Level from the pattern, then hits on top
			float patternLevel = base;
			if( pattern == PAT_STATIC )
				patternLevel = 1.0f;
			else if( pattern == PAT_WAVE )
			{
				float xn     = cols > 1 ? (float)c / (float)( cols - 1 ) : 0.0f;
				float w      = 0.5f + 0.5f * std::cos( 6.2831853f * ( wavePhase - xn * 0.75f ) );
				patternLevel = std::max( base, w * w * w );
			}
			float level  = std::max( patternLevel, l.env );
			float length = beamLen;
			if( l.env > patternLevel && dropTime > 0.0f )
				length *= std::min( l.age / dropTime, 1.0f );

			if( level < 0.002f || length < 0.001f )
				continue;

			// World position of the fixture
			float x = ( c - ( cols - 1 ) * 0.5f ) * sx;
			if( r % 2 == 1 )
				x += stagger * sx * 0.5f;
			x += l.jitterX * jitter * sx * 0.4f;
			float z = P( P_CAM_DIST ) + r * sz + l.jitterZ * jitter * sz * 0.4f;

			// Beam direction: tilt 0 points down, 90 sideways, 180 up. Turn rotates it around the vertical.
			bool flip = false;
			switch( mirrorMode )
			{
			case 1: flip = c % 2 == 1; break;
			case 2: flip = r % 2 == 1; break;
			case 3: flip = ( r + c ) % 2 == 1; break;
			default: break;
			}
			float lean  = l.angle * P( P_ANGLE_JITTER ) * 0.3f;// up to ~17 degrees
			float pitch = beamTilt + lean;
			float yaw   = beamTurn + ( flip ? 3.14159265f : 0.0f );
			Vec3 dir    = { std::sin( pitch ) * std::cos( yaw ), -std::cos( pitch ), std::sin( pitch ) * std::sin( yaw ) };

			Vec3 top       = { x, height, z };
			float reach    = throwDist;
			bool hitsFloor = false;
			if( dir.y < -1e-4f && height / -dir.y <= throwDist )
			{
				reach     = height / -dir.y;
				hitsFloor = true;
			}
			float travel = reach * length;
			Vec3 end     = { top.x + dir.x * travel, top.y + dir.y * travel, top.z + dir.z * travel };
			Vec3 floorPt = { top.x + dir.x * reach, 0.0f, top.z + dir.z * reach };

			Vec3 a = top, b = end;
			if( !ClipNear( cam, a, b, nearZ ) )
				continue;
			Proj pa = cam.Project( a );
			Proj pb = cam.Project( b );

			float worldEndWidth = width * ( 1.0f + spread * 0.5f * travel );
			float wTop          = width * 0.5f * cam.focal / pa.depth;
			float wEnd          = worldEndWidth * 0.5f * cam.focal / pb.depth;

			// Floor contact: only when the beam reaches the floor and that point is in view
			float floorAmt = hitsFloor ? Smoothstep( 0.92f, 1.0f, length ) : 0.0f;
			float spotRx = 0.0f, spotRy = 0.0f;
			if( floorAmt > 0.0f && cam.Depth( floorPt ) > nearZ )
			{
				float radius = 0.25f + worldEndWidth * 2.0f;
				Proj pr      = cam.Project( { floorPt.x + radius, 0.0f, floorPt.z } );
				Proj pf      = cam.Project( { floorPt.x, 0.0f, floorPt.z + radius } );
				Proj pn      = cam.Project( { floorPt.x, 0.0f, std::max( floorPt.z - radius, cam.pz + nearZ ) } );
				spotRx       = std::fabs( pr.x - pb.x );
				spotRy       = std::max( std::fabs( pn.y - pf.y ) * 0.5f, spotRx * 0.04f );
			}
			else
				floorAmt = 0.0f;

			// Reflection: the beam mirrored in the floor, starting from the end nearest the floor.
			// Steep beams fade out along their length, flat ones stay even.
			Vec3 low = end, high = top;
			if( top.y < end.y )
				std::swap( low, high );
			float reflAmt  = 0.0f, reflPow = 0.0f;
			Proj r0 = pb, r1 = pb;
			if( low.y >= -1e-3f )
			{
				Vec3 m0 = { low.x, -low.y, low.z };
				Vec3 m1 = Lerp( m0, { high.x, -high.y, high.z }, std::fabs( dir.y ) > 0.5f ? 0.6f : 1.0f );
				if( ClipNear( cam, m0, m1, nearZ ) )
				{
					r0      = cam.Project( m0 );
					r1      = cam.Project( m1 );
					reflAmt = std::exp( -std::max( low.y, 0.0f ) * 0.25f );
					reflPow = 3.0f * std::fabs( dir.y );
					if( top.y < end.y )
						reflPow = -std::max( reflPow, 1e-3f );// tells the shader the fixture end is the low one
				}
			}

			float fade = std::exp( -depthFade * 0.08f * std::max( z - P( P_CAM_DIST ), 0.0f ) );
			float I    = level * fade;

			const int o = activeCount * 4;
			row0[ o + 0 ] = pa.x;
			row0[ o + 1 ] = pa.y;
			row0[ o + 2 ] = pb.x;
			row0[ o + 3 ] = pb.y;
			row1[ o + 0 ] = wTop;
			row1[ o + 1 ] = wEnd;
			row1[ o + 2 ] = I;
			row1[ o + 3 ] = floorAmt;
			row2[ o + 0 ] = r0.x;
			row2[ o + 1 ] = r0.y;
			row2[ o + 2 ] = r1.x;
			row2[ o + 3 ] = r1.y;
			row3[ o + 0 ] = spotRx;
			row3[ o + 1 ] = spotRy;
			row3[ o + 2 ] = reflAmt;
			row3[ o + 3 ] = reflPow;
			++activeCount;
			totalLight += I;
		}
	}
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
FFResult LightBeams::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	(void)pGL;
	auto now = std::chrono::steady_clock::now();
	float dt = firstFrame ? 0.0f : std::chrono::duration< float >( now - lastFrame ).count();
	dt       = std::min( std::max( dt, 0.0f ), 0.25f );
#ifdef LIGHTBEAMS_FIXED_DT
	dt = LIGHTBEAMS_FIXED_DT;// offline test renders only
#endif
	lastFrame  = now;
	firstFrame = false;
	timeSec += dt;
	hazeTime += dt * ( 0.1f + P( P_HAZE_SPEED ) * 1.5f );

	rows = std::min( std::max( PI( P_ROWS ), 1 ), (int)MAX_ROWS );
	cols = std::min( std::max( PI( P_COLS ), 1 ), (int)MAX_COLS );
	if( (unsigned int)PI( P_SEED ) != jitterSeed )
		RebuildJitter();

	HandleTriggers();
	UpdateClock( dt );
	UpdateLights( dt );
	BuildLightData();

	float hue = P( P_HUE );
	float tr, tg, tb;
	HSVtoRGB( hue >= 1.0f ? 0.0f : hue, P( P_SATURATION ), 1.0f, tr, tg, tb );

	const float deg = 3.14159265f / 180.0f;
	float focal     = 1.0f / std::tan( P( P_CAM_FOV ) * 0.5f * deg );
	float horizonY  = -std::tan( P( P_CAM_TILT ) * deg ) * focal;

	{
		Scoped2DTextureBinding binding( dataTexture );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, MAX_LIGHTS, DATA_ROWS, GL_RGBA, GL_FLOAT, lightData.data() );
	}

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	ScopedSamplerActivation samplerActivation( 0 );
	Scoped2DTextureBinding textureBinding( dataTexture );

	shader.Set( "lightData", 0 );
	shader.Set( "lightCount", activeCount );
	shader.Set( "resolution", (float)currentViewport.width, (float)currentViewport.height );
	shader.Set( "time", timeSec );
	shader.Set( "hazeTime", hazeTime );
	shader.Set( "tint", tr, tg, tb );
	shader.Set( "glow", P( P_GLOW ) );
	shader.Set( "haze", P( P_HAZE ) );
	shader.Set( "ambient", P( P_ATMOSPHERE ) * 0.05f * std::sqrt( totalLight ) );
	shader.Set( "floorGlow", P( P_FLOOR_GLOW ) );
	shader.Set( "reflection", P( P_REFLECTION ) );
	shader.Set( "fixtureGlow", P( P_FIXTURE_GLOW ) );
	shader.Set( "background", P( P_BACKGROUND ) );
	shader.Set( "transparentBg", P( P_TRANSPARENT ) );
	shader.Set( "exposure", P( P_INTENSITY ) * 1.3f );
	shader.Set( "horizonY", horizonY );

	quad.Draw();
	return FF_SUCCESS;
}
