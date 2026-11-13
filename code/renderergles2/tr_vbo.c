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
// tr_vbo.c -- vertex submission for the OpenGL ES 2.0 backend
//
// Two modes, selected by r_vbo:
//
// 0: client side vertex arrays. Mesa (vc4 / v3d on the Raspberry Pi and
//    every other Gallium driver) copies them into its own unsynchronized
//    upload ring, which is the cheapest way to stream dynamic geometry.
// 1: streaming buffer objects. Required by WebGL. Every vertex attribute
//    and the indexes have their own buffer object, and each upload
//    replaces its whole storage with glBufferData() ("orphaning"): the
//    driver hands out fresh memory instead of waiting for the GPU to
//    finish reading the previous contents. Appending to a shared buffer
//    with glBufferSubData() makes Mesa v3d flush and wait on every draw.
//
// Data that doesn't change while a batch is drawn (positions, indexes,
// static texture coordinates) is only uploaded once per batch.

#include "tr_local.h"

typedef struct {
	const void	*data;
	int			numVertexes;
	int			size;
	GLenum		type;
	int			stride;
	int			batch;
} attribCache_t;

static qboolean			useVBO;
static GLuint			attribBuffers[ATTR_INDEX_COUNT];
static GLuint			indexBuffer;
static int				batchCount;
static attribCache_t	attribCache[ATTR_INDEX_COUNT];
static const void		*indexCacheData;
static int				indexCacheNum, indexCacheBatch;


/*
================
R_StreamUpload

Replace the whole storage of a buffer with new data
================
*/
static void R_StreamUpload( GLenum target, GLuint buffer, const void *data, int size )
{
	qglBindBuffer( target, buffer );
	qglBufferData( target, size, data, GL_STREAM_DRAW );
}


/*
================
R_InitVertexStream
================
*/
void R_InitVertexStream( void )
{
#ifdef __EMSCRIPTEN__
	// WebGL has no client side vertex arrays
	useVBO = qtrue;
#else
	useVBO = r_vbo->integer ? qtrue : qfalse;
#endif

	Com_Memset( attribCache, 0, sizeof( attribCache ) );
	indexCacheData = NULL;
	batchCount = 1;

	if ( !useVBO )
	{
		ri.Printf( PRINT_ALL, "...using client side vertex arrays\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "...using streaming vertex buffer objects\n" );

	qglGenBuffers( ATTR_INDEX_COUNT, attribBuffers );
	qglGenBuffers( 1, &indexBuffer );
	qglBindBuffer( GL_ELEMENT_ARRAY_BUFFER, indexBuffer );
}

/*
================
R_ShutdownVertexStream
================
*/
void R_ShutdownVertexStream( void )
{
	if ( !useVBO || !qglDeleteBuffers )
		return;

	qglBindBuffer( GL_ARRAY_BUFFER, 0 );
	qglBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	qglDeleteBuffers( ATTR_INDEX_COUNT, attribBuffers );
	qglDeleteBuffers( 1, &indexBuffer );
	Com_Memset( attribBuffers, 0, sizeof( attribBuffers ) );
	indexBuffer = 0;
	useVBO = qfalse;
}

/*
================
R_BeginVertexBatch

Called when tess starts collecting a new batch; data cached as
"constant for the batch" is invalidated.
================
*/
void R_BeginVertexBatch( void )
{
	batchCount++;
}

/*
================
R_VertexAttribArray

Set the source of a vertex attribute. Enabling the array is done
separately with GLSL_VertexAttribsState().

batchConstant: the data won't change until the next R_BeginVertexBatch(),
so it doesn't need to be uploaded again when used by several stages.
================
*/
void R_VertexAttribArray( attribIndex_t attrib, int size, GLenum type, int stride, const void *data, int numVertexes, qboolean batchConstant )
{
	attribCache_t	*cache = &attribCache[attrib];
	const void		*pointer = data;
	GLboolean		normalized = ( type == GL_UNSIGNED_BYTE ) ? GL_TRUE : GL_FALSE;

	if ( batchConstant && cache->batch == batchCount && cache->data == data &&
		cache->numVertexes >= numVertexes && cache->size == size && cache->type == type && cache->stride == stride )
	{
		return;
	}

	if ( useVBO )
	{
		int elementSize = size * ( type == GL_UNSIGNED_BYTE ? 1 : 4 );
		int bytes = ( numVertexes - 1 ) * ( stride ? stride : elementSize ) + elementSize;

		if ( numVertexes <= 0 )
			return;

		R_StreamUpload( GL_ARRAY_BUFFER, attribBuffers[attrib], data, bytes );
		pointer = NULL;
	}

	qglVertexAttribPointer( attrib, size, type, normalized, stride, pointer );

	if ( batchConstant )
	{
		cache->data = data;
		cache->numVertexes = numVertexes;
		cache->size = size;
		cache->type = type;
		cache->stride = stride;
		cache->batch = batchCount;
	}
	else
	{
		cache->batch = 0;
	}
}

/*
================
R_BindProgramForDraw

Binds the generic program for the given texture key, with the alpha test
taken from the current GL_State() bits.
================
*/
static void R_BindProgramForDraw( int texKey )
{
	GLSL_BindProgram( texKey | GLSL_KeyForStateBits( glState.glStateBits ) );
}

/*
================
R_DrawElementsMode

Draws indexed primitives with the vertex arrays set by R_VertexAttribArray()
and the generic program selected by texKey (GLSLKEY_TEXTURE* / TEXENV1 bits).
tess.indexes are uploaded only once per batch.
================
*/
void R_DrawElementsMode( GLenum mode, int numIndexes, const glIndex_t *indexes, int texKey )
{
	const void *pointer = indexes;

	if ( numIndexes <= 0 )
		return;

	R_BindProgramForDraw( texKey );

	if ( useVBO )
	{
		// the index buffer keeps tess.indexes until something else is uploaded
		if ( !( indexes == tess.indexes && indexCacheData == indexes &&
			indexCacheBatch == batchCount && indexCacheNum == numIndexes ) )
		{
			R_StreamUpload( GL_ELEMENT_ARRAY_BUFFER, indexBuffer, indexes, numIndexes * sizeof( glIndex_t ) );

			indexCacheData = indexes;
			indexCacheNum = numIndexes;
			indexCacheBatch = batchCount;
		}

		pointer = NULL;
	}

	qglDrawElements( mode, numIndexes, GL_INDEX_TYPE, pointer );
}

/*
================
R_DrawElements
================
*/
void R_DrawElements( int numIndexes, const glIndex_t *indexes, int texKey )
{
	R_DrawElementsMode( GL_TRIANGLES, numIndexes, indexes, texKey );
}

/*
================
R_DrawArrays
================
*/
void R_DrawArrays( GLenum mode, int numVertexes, int texKey )
{
	if ( numVertexes <= 0 )
		return;

	R_BindProgramForDraw( texKey );
	qglDrawArrays( mode, 0, numVertexes );
}

/*
================
RB_DrawImmediate

Replacement for glBegin()/glEnd(): draws numVertexes vertexes with the
constant color set by GLSL_SetConstantColor(). st may be NULL for
untextured geometry. Breaks the current batch's attribute cache.
================
*/
void RB_DrawImmediate( GLenum mode, int numVertexes, const vec3_t *xyz, const vec2_t *st )
{
	R_BeginVertexBatch();

	R_VertexAttribArray( ATTR_INDEX_POSITION, 3, GL_FLOAT, 0, xyz, numVertexes, qfalse );

	if ( st )
	{
		R_VertexAttribArray( ATTR_INDEX_TEXCOORD0, 2, GL_FLOAT, 0, st, numVertexes, qfalse );
		GLSL_VertexAttribsState( ATTR_POSITION | ATTR_TEXCOORD0 );
	}
	else
	{
		GLSL_VertexAttribsState( ATTR_POSITION );
	}

	R_DrawArrays( mode, numVertexes, st ? GLSLKEY_TEXTURE0 : 0 );
}

/*
================
RB_DrawQuad2D

Textured screen space quad (x, y, w, h in the current 2D projection)
================
*/
void RB_DrawQuad2D( float x, float y, float w, float h, float s1, float t1, float s2, float t2 )
{
	vec3_t	xyz[4];
	vec2_t	st[4];

	VectorSet( xyz[0], x,     y,     0 );	st[0][0] = s1;	st[0][1] = t1;
	VectorSet( xyz[1], x + w, y,     0 );	st[1][0] = s2;	st[1][1] = t1;
	VectorSet( xyz[2], x + w, y + h, 0 );	st[2][0] = s2;	st[2][1] = t2;
	VectorSet( xyz[3], x,     y + h, 0 );	st[3][0] = s1;	st[3][1] = t2;

	RB_DrawImmediate( GL_TRIANGLE_FAN, 4, (const vec3_t *)xyz, (const vec2_t *)st );
}
