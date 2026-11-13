// Generic fragment program: replaces the OpenGL 1.x fixed function
// texture environment and alpha test.
// Variant defines (prepended by tr_glsl.c):
//   USE_TEXTURE0, USE_TEXTURE1
//   TEXENV1_MODULATE, TEXENV1_ADD, TEXENV1_REPLACE, TEXENV1_DECAL
//   ATEST_GT_0, ATEST_LT_80, ATEST_GE_80

#if defined(USE_TEXTURE0)
uniform sampler2D u_Texture0;
varying HIGHP vec2 var_TexCoord0;
#endif

#if defined(USE_TEXTURE1)
uniform sampler2D u_Texture1;
varying HIGHP vec2 var_TexCoord1;
#endif

varying lowp vec4 var_Color;

void main()
{
	lowp vec4 color = var_Color;

#if defined(USE_TEXTURE0)
	// GL_MODULATE with the primary color
	color *= texture2D(u_Texture0, var_TexCoord0);
#endif

#if defined(USE_TEXTURE1)
	lowp vec4 tex1 = texture2D(u_Texture1, var_TexCoord1);
  #if defined(TEXENV1_MODULATE)
	color *= tex1;
  #elif defined(TEXENV1_ADD)
	color = vec4(color.rgb + tex1.rgb, color.a * tex1.a);
  #elif defined(TEXENV1_REPLACE)
	color = tex1;
  #elif defined(TEXENV1_DECAL)
	color.rgb = mix(color.rgb, tex1.rgb, tex1.a);
  #endif
#endif

#if defined(ATEST_GT_0)
	if (color.a <= 0.0)
		discard;
#elif defined(ATEST_LT_80)
	if (color.a >= 0.5)
		discard;
#elif defined(ATEST_GE_80)
	if (color.a < 0.5)
		discard;
#endif

	gl_FragColor = color;
}
