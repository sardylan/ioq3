/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_glsl.c -- GLSL ES 1.00 programs replacing the OpenGL 1.x fixed function pipeline

#include "tr_local.h"

extern const char *fallbackShader_generic_vp;
extern const char *fallbackShader_generic_fp;

static shaderProgram_t	glslPrograms[GLSL_NUM_PROGRAMS];

static const char *glslAttribNames[ATTR_INDEX_COUNT] = {
	"attr_Position",
	"attr_TexCoord0",
	"attr_TexCoord1",
	"attr_Color"
};

// Vertex programs always run at full precision; fragment programs use
// highp only for texture coordinates (scrolling texcoords grow large)
static const char *glslVertexHeader =
	"#version 100\n"
	"#define HIGHP highp\n"
	"precision highp float;\n";

static const char *glslFragmentHeader =
	"#version 100\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
	"#define HIGHP highp\n"
	"#else\n"
	"#define HIGHP mediump\n"
	"#endif\n"
	"precision mediump float;\n";


/*
================
GLSL_KeyForTexEnv

Map a multitexture environment (GL_MODULATE, GL_ADD, ...) to program key bits
================
*/
int GLSL_KeyForTexEnv( int env )
{
	switch ( env )
	{
	case GL_MODULATE:
		return GLSLKEY_TEXENV1_MODULATE;
	case GL_ADD:
		return GLSLKEY_TEXENV1_ADD;
	case GL_REPLACE:
		return GLSLKEY_TEXENV1_REPLACE;
	case GL_DECAL:
		return GLSLKEY_TEXENV1_DECAL;
	default:
		ri.Error( ERR_DROP, "GLSL_KeyForTexEnv: invalid env '%d' passed", env );
		return 0;
	}
}


/*
================
GLSL_KeyForStateBits

Map GLS_ATEST_* state bits to program key bits
================
*/
int GLSL_KeyForStateBits( unsigned long stateBits )
{
	switch ( stateBits & GLS_ATEST_BITS )
	{
	case GLS_ATEST_GT_0:
		return GLSLKEY_ATEST_GT_0;
	case GLS_ATEST_LT_80:
		return GLSLKEY_ATEST_LT_80;
	case GLS_ATEST_GE_80:
		return GLSLKEY_ATEST_GE_80;
	default:
		return 0;
	}
}


/*
================
GLSL_BuildDefines
================
*/
static void GLSL_BuildDefines( int key, char *defines, int size )
{
	defines[0] = '\0';

	if ( key & GLSLKEY_TEXTURE0 )
		Q_strcat( defines, size, "#define USE_TEXTURE0\n" );

	if ( key & GLSLKEY_TEXTURE1 )
	{
		Q_strcat( defines, size, "#define USE_TEXTURE1\n" );

		switch ( key & GLSLKEY_TEXENV1_BITS )
		{
		case GLSLKEY_TEXENV1_MODULATE:
			Q_strcat( defines, size, "#define TEXENV1_MODULATE\n" );
			break;
		case GLSLKEY_TEXENV1_ADD:
			Q_strcat( defines, size, "#define TEXENV1_ADD\n" );
			break;
		case GLSLKEY_TEXENV1_REPLACE:
			Q_strcat( defines, size, "#define TEXENV1_REPLACE\n" );
			break;
		case GLSLKEY_TEXENV1_DECAL:
			Q_strcat( defines, size, "#define TEXENV1_DECAL\n" );
			break;
		}
	}

	switch ( key & GLSLKEY_ATEST_BITS )
	{
	case GLSLKEY_ATEST_GT_0:
		Q_strcat( defines, size, "#define ATEST_GT_0\n" );
		break;
	case GLSLKEY_ATEST_LT_80:
		Q_strcat( defines, size, "#define ATEST_LT_80\n" );
		break;
	case GLSLKEY_ATEST_GE_80:
		Q_strcat( defines, size, "#define ATEST_GE_80\n" );
		break;
	}
}


/*
================
GLSL_PrintLog
================
*/
static void GLSL_PrintLog( GLuint object, qboolean isProgram )
{
	char	log[4096];
	GLsizei	length = 0;

	if ( isProgram )
		qglGetProgramInfoLog( object, sizeof( log ), &length, log );
	else
		qglGetShaderInfoLog( object, sizeof( log ), &length, log );

	if ( length > 0 )
		ri.Printf( PRINT_ALL, "%s\n", log );
}


/*
================
GLSL_CompileShader
================
*/
static GLuint GLSL_CompileShader( GLenum type, const char *header, const char *defines, const char *source, int key )
{
	const char	*sources[3];
	GLuint		shader;
	GLint		compiled;

	sources[0] = header;
	sources[1] = defines;
	sources[2] = source;

	shader = qglCreateShader( type );
	qglShaderSource( shader, 3, sources, NULL );
	qglCompileShader( shader );

	qglGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
	if ( !compiled )
	{
		ri.Printf( PRINT_ALL, "GLSL: failed to compile %s shader for program 0x%02x:\n",
			type == GL_VERTEX_SHADER ? "vertex" : "fragment", key );
		GLSL_PrintLog( shader, qfalse );
		qglDeleteShader( shader );
		return 0;
	}

	return shader;
}


/*
================
GLSL_CompileProgram
================
*/
static qboolean GLSL_CompileProgram( shaderProgram_t *sp, int key )
{
	char	defines[512];
	GLuint	vp, fp;
	GLint	linked;
	int		i;

	GLSL_BuildDefines( key, defines, sizeof( defines ) );

	vp = GLSL_CompileShader( GL_VERTEX_SHADER, glslVertexHeader, defines, fallbackShader_generic_vp, key );
	fp = GLSL_CompileShader( GL_FRAGMENT_SHADER, glslFragmentHeader, defines, fallbackShader_generic_fp, key );

	if ( !vp || !fp )
	{
		if ( vp )
			qglDeleteShader( vp );
		if ( fp )
			qglDeleteShader( fp );
		return qfalse;
	}

	sp->program = qglCreateProgram();
	qglAttachShader( sp->program, vp );
	qglAttachShader( sp->program, fp );

	// fixed attribute locations so vertex arrays don't depend on the program
	for ( i = 0; i < ATTR_INDEX_COUNT; i++ )
		qglBindAttribLocation( sp->program, i, glslAttribNames[i] );

	qglLinkProgram( sp->program );

	// shaders are no longer needed once linked
	qglDetachShader( sp->program, vp );
	qglDetachShader( sp->program, fp );
	qglDeleteShader( vp );
	qglDeleteShader( fp );

	qglGetProgramiv( sp->program, GL_LINK_STATUS, &linked );
	if ( !linked )
	{
		ri.Printf( PRINT_ALL, "GLSL: failed to link program 0x%02x:\n", key );
		GLSL_PrintLog( sp->program, qtrue );
		qglDeleteProgram( sp->program );
		sp->program = 0;
		return qfalse;
	}

	sp->u_ModelViewProjection = qglGetUniformLocation( sp->program, "u_ModelViewProjection" );
	sp->matrixGeneration = -1;

	// samplers never change, set them once
	qglUseProgram( sp->program );
	if ( key & GLSLKEY_TEXTURE0 )
		qglUniform1i( qglGetUniformLocation( sp->program, "u_Texture0" ), 0 );
	if ( key & GLSLKEY_TEXTURE1 )
		qglUniform1i( qglGetUniformLocation( sp->program, "u_Texture1" ), 1 );
	qglUseProgram( glState.currentProgram ? glState.currentProgram->program : 0 );

	return qtrue;
}


/*
================
GLSL_NormalizeKey

Drop key bits that don't affect the generated program
================
*/
static int GLSL_NormalizeKey( int key )
{
	key &= GLSL_NUM_PROGRAMS - 1;

	// a second texture requires the first one
	if ( key & GLSLKEY_TEXTURE1 )
		key |= GLSLKEY_TEXTURE0;
	else
		key &= ~GLSLKEY_TEXENV1_BITS;

	return key;
}


/*
================
GLSL_GetProgram
================
*/
static shaderProgram_t *GLSL_GetProgram( int key )
{
	shaderProgram_t *sp;

	key = GLSL_NormalizeKey( key );
	sp = &glslPrograms[key];

	if ( !sp->program && !sp->failed )
	{
		if ( !GLSL_CompileProgram( sp, key ) )
		{
			sp->failed = qtrue;
			ri.Error( ERR_FATAL, "GLSL: unable to build generic program 0x%02x", key );
		}
	}

	return sp;
}


/*
================
GLSL_BindProgram

Make the program for the given key current and make sure its
model-view-projection matrix is up to date.
================
*/
shaderProgram_t *GLSL_BindProgram( int key )
{
	shaderProgram_t *sp = GLSL_GetProgram( key );

	if ( glState.currentProgram != sp )
	{
		qglUseProgram( sp->program );
		glState.currentProgram = sp;
	}

	if ( sp->matrixGeneration != glState.matrixGeneration )
	{
		qglUniformMatrix4fv( sp->u_ModelViewProjection, 1, GL_FALSE, glState.modelViewProjection );
		sp->matrixGeneration = glState.matrixGeneration;
	}

	return sp;
}


/*
================
GLSL_VertexAttribsState

Enable exactly the vertex attribute arrays in attribBits (ATTR_* flags)
================
*/
void GLSL_VertexAttribsState( unsigned int attribBits )
{
	unsigned int diff = attribBits ^ glState.vertexAttribsEnabled;
	int i;

	if ( !diff )
		return;

	for ( i = 0; i < ATTR_INDEX_COUNT; i++ )
	{
		if ( !( diff & ( 1 << i ) ) )
			continue;

		if ( attribBits & ( 1 << i ) )
			qglEnableVertexAttribArray( i );
		else
			qglDisableVertexAttribArray( i );
	}

	glState.vertexAttribsEnabled = attribBits;
}


/*
================
GLSL_SetConstantColor

Color used while the color attribute array is disabled
================
*/
void GLSL_SetConstantColor( float r, float g, float b, float a )
{
	qglVertexAttrib4f( ATTR_INDEX_COLOR, r, g, b, a );
}


/*
================
GLSL_Init
================
*/
void GLSL_Init( void )
{
	static const int precompile[] = {
		0,
		GLSLKEY_TEXTURE0,
		GLSLKEY_TEXTURE0 | GLSLKEY_ATEST_GT_0,
		GLSLKEY_TEXTURE0 | GLSLKEY_ATEST_LT_80,
		GLSLKEY_TEXTURE0 | GLSLKEY_ATEST_GE_80,
		GLSLKEY_TEXTURE0 | GLSLKEY_TEXTURE1 | GLSLKEY_TEXENV1_MODULATE,
		GLSLKEY_TEXTURE0 | GLSLKEY_TEXTURE1 | GLSLKEY_TEXENV1_ADD,
	};
	int i, startTime;

	ri.Printf( PRINT_ALL, "------- GLSL_Init -------\n" );

	startTime = ri.Milliseconds();

	Com_Memset( glslPrograms, 0, sizeof( glslPrograms ) );
	glState.currentProgram = NULL;
	glState.vertexAttribsEnabled = 0;

	// the most common variants are built up front to avoid hitches,
	// the others are built the first time they are used
	for ( i = 0; i < ARRAY_LEN( precompile ); i++ )
		GLSL_GetProgram( precompile[i] );

	GLSL_SetConstantColor( 1.0f, 1.0f, 1.0f, 1.0f );

	ri.Printf( PRINT_ALL, "GLSL programs built in %5.2f seconds\n", ( ri.Milliseconds() - startTime ) / 1000.0 );
}


/*
================
GLSL_Shutdown
================
*/
void GLSL_Shutdown( void )
{
	int i;

	if ( !qglUseProgram )
		return;

	GLSL_VertexAttribsState( 0 );
	qglUseProgram( 0 );
	glState.currentProgram = NULL;

	for ( i = 0; i < GLSL_NUM_PROGRAMS; i++ )
	{
		if ( glslPrograms[i].program )
			qglDeleteProgram( glslPrograms[i].program );
	}

	Com_Memset( glslPrograms, 0, sizeof( glslPrograms ) );
}
