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
// tr_matrix.c -- CPU side projection / model-view matrices replacing the
// OpenGL 1.x matrix stack. All matrices are column major, as in OpenGL.

#include "tr_local.h"

void Mat4Identity( float *m )
{
	Com_Memset( m, 0, sizeof( float ) * 16 );
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void Mat4Copy( const float *in, float *out )
{
	Com_Memcpy( out, in, sizeof( float ) * 16 );
}

/*
** Mat4Multiply
**
** out = a * b (b is applied first). out may not alias a or b.
*/
void Mat4Multiply( const float *a, const float *b, float *out )
{
	int c, r;

	for ( c = 0; c < 4; c++ )
	{
		for ( r = 0; r < 4; r++ )
		{
			out[c * 4 + r] =
				a[0 * 4 + r] * b[c * 4 + 0] +
				a[1 * 4 + r] * b[c * 4 + 1] +
				a[2 * 4 + r] * b[c * 4 + 2] +
				a[3 * 4 + r] * b[c * 4 + 3];
		}
	}
}

/*
** Mat4Ortho
**
** Same as glOrtho() applied to an identity matrix
*/
void Mat4Ortho( float left, float right, float bottom, float top, float zNear, float zFar, float *out )
{
	Com_Memset( out, 0, sizeof( float ) * 16 );

	out[0]  = 2.0f / ( right - left );
	out[5]  = 2.0f / ( top - bottom );
	out[10] = -2.0f / ( zFar - zNear );
	out[12] = -( right + left ) / ( right - left );
	out[13] = -( top + bottom ) / ( top - bottom );
	out[14] = -( zFar + zNear ) / ( zFar - zNear );
	out[15] = 1.0f;
}

/*
** Mat4Translation
**
** out = in * translate( x, y, z ), same as glTranslatef()
*/
void Mat4Translation( const float *in, float x, float y, float z, float *out )
{
	int r;

	if ( in != out )
		Mat4Copy( in, out );

	for ( r = 0; r < 4; r++ )
		out[12 + r] = in[0 + r] * x + in[4 + r] * y + in[8 + r] * z + in[12 + r];
}


/*
** GL_UpdateModelViewProjection
*/
static void GL_UpdateModelViewProjection( void )
{
	Mat4Multiply( glState.projection, glState.modelView, glState.modelViewProjection );

	// programs compare against this to know when to re-upload the uniform
	glState.matrixGeneration++;
}

/*
** GL_SetProjectionMatrix
*/
void GL_SetProjectionMatrix( const float *m )
{
	Mat4Copy( m, glState.projection );
	GL_UpdateModelViewProjection();
}

/*
** GL_SetModelViewMatrix
*/
void GL_SetModelViewMatrix( const float *m )
{
	Mat4Copy( m, glState.modelView );
	GL_UpdateModelViewProjection();
}
