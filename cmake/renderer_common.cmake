include_guard(GLOBAL)

set(RENDERER_COMMON_SOURCES
    ${SOURCE_DIR}/renderercommon/tr_font.c
    ${SOURCE_DIR}/renderercommon/tr_image_bmp.c
    ${SOURCE_DIR}/renderercommon/tr_image_jpg.c
    ${SOURCE_DIR}/renderercommon/tr_image_pcx.c
    ${SOURCE_DIR}/renderercommon/tr_image_png.c
    ${SOURCE_DIR}/renderercommon/tr_image_pvr.c
    ${SOURCE_DIR}/renderercommon/tr_image_tga.c
    ${SOURCE_DIR}/renderercommon/tr_noise.c
    ${SOURCE_DIR}/renderercommon/puff.c
)

set(SDL_RENDERER_SOURCES
    ${SOURCE_DIR}/sdl/sdl_gamma.c
    ${SOURCE_DIR}/sdl/sdl_glimp.c
)

set(DYNAMIC_RENDERER_SOURCES
    ${SOURCE_DIR}/renderercommon/tr_subs.c
    ${SOURCE_DIR}/qcommon/q_shared.c
    ${SOURCE_DIR}/qcommon/q_math.c
)

if(USE_FREETYPE)
    list(APPEND RENDERER_DEFINITIONS BUILD_FREETYPE)
endif()

set(NUM_STATIC_RENDERERS 0)
foreach(RENDERER_OPTION BUILD_RENDERER_GL1 BUILD_RENDERER_GL2 BUILD_RENDERER_GLES2)
    if(${RENDERER_OPTION})
        math(EXPR NUM_STATIC_RENDERERS "${NUM_STATIC_RENDERERS} + 1")
    endif()
endforeach()

if(USE_RENDERER_DLOPEN)
    list(APPEND RENDERER_DEFINITIONS USE_RENDERER_DLOPEN)
elseif(NUM_STATIC_RENDERERS GREATER 1)
    message(FATAL_ERROR "Multiple static renderers enabled; choose one")
elseif(NUM_STATIC_RENDERERS EQUAL 0)
    message(FATAL_ERROR "Zero static renderers enabled; choose one")
endif()

list(APPEND RENDERER_LIBRARIES ${COMMON_LIBRARIES})
