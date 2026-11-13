// Generic vertex program: replaces the OpenGL 1.x fixed function transform.
// Variant defines (prepended by tr_glsl.c): USE_TEXTURE0, USE_TEXTURE1

attribute vec4 attr_Position;
attribute vec4 attr_Color;

#if defined(USE_TEXTURE0)
attribute vec2 attr_TexCoord0;
varying HIGHP vec2 var_TexCoord0;
#endif

#if defined(USE_TEXTURE1)
attribute vec2 attr_TexCoord1;
varying HIGHP vec2 var_TexCoord1;
#endif

uniform mat4 u_ModelViewProjection;

varying lowp vec4 var_Color;

void main()
{
	gl_Position = u_ModelViewProjection * vec4(attr_Position.xyz, 1.0);
	var_Color = attr_Color;

#if defined(USE_TEXTURE0)
	var_TexCoord0 = attr_TexCoord0;
#endif

#if defined(USE_TEXTURE1)
	var_TexCoord1 = attr_TexCoord1;
#endif
}
