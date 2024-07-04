/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2021 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#ifndef __IceTGL3_h
#define __IceTGL3_h

#include <IceT.h>

#ifdef __gl_h
// Check that sizes of GL types are compatible with IceT types.
#if sizeof(GLfloat) != sizeof(IceTFloat)
#error "Unexpected size of GLfloat."
#endif
#if sizeof(GLdouble) != sizeof(IceTDouble)
#error "Unexpected size of GLdouble."
#endif
#if sizeof(GLenum) != sizeof(IceTEnum)
#error "Unexpected size of GLenum."
#endif
#if sizeof(GLint) != sizeof(IceTInt)
#error "Unexpected size of GLint."
#endif
#if sizeof(GLuint) != sizeof(IceTUInt)
#error "Unexpected size of GLuint."
#endif
#endif //__gl_h

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

ICET_GL3_EXPORT void icetGL3Initialize(void);

ICET_GL3_EXPORT IceTBoolean icetGL3IsInitialized(void);

ICET_GL3_EXPORT IceTImage icetGL3DrawFrame(const IceTDouble *projection_matrix,
                                           const IceTDouble *modelview_matrix);

typedef void (*IceTGL3DrawCallbackTextureType)(
        const IceTDouble *projection_matrix,
        const IceTDouble *modelview_matrix,
        const IceTInt *readback_viewport,
        IceTUInt framebuffer_id);

ICET_GL3_EXPORT void icetGL3DrawCallbackTexture(
        IceTGL3DrawCallbackTextureType callback);

#define ICET_GL3_STATE_START ICET_RENDER_LAYER_STATE_START
#define ICET_GL3_STATE_END   ICET_RENDER_LAYER_STATE_END

#define ICET_GL3_INITIALIZED               (ICET_GL3_STATE_START | (IceTEnum)0x0001)

#define ICET_GL3_DRAW_FUNCTION             (ICET_GL3_STATE_START | (IceTEnum)0x0020)
#define ICET_GL3_COLOR_TEXTURE             (ICET_GL3_STATE_START | (IceTEnum)0x0021)
#define ICET_GL3_DEPTH_TEXTURE             (ICET_GL3_STATE_START | (IceTEnum)0x0022)
#define ICET_GL3_FRAMEBUFFER               (ICET_GL3_STATE_START | (IceTEnum)0x0023)
#define ICET_GL3_DEPTH_R32F_TEXTURE        (ICET_GL3_STATE_START | (IceTEnum)0x0024)
#define ICET_GL3_DEPTH_FRAMEBUFFER         (ICET_GL3_STATE_START | (IceTEnum)0x0025)
#define ICET_GL3_RENDERIMAGE_PROGRAM       (ICET_GL3_STATE_START | (IceTEnum)0x0026)
#define ICET_GL3_RENDERIMAGE_IMAGE_UNIFORM (ICET_GL3_STATE_START | (IceTEnum)0x0027)
#define ICET_GL3_PLANE_VERTEXARRAY         (ICET_GL3_STATE_START | (IceTEnum)0x0028)
#define ICET_GL3_COLOR_RESOURCE            (ICET_GL3_STATE_START | (IceTEnum)0x0029)
#define ICET_GL3_COLOR_DESCRIPTION         (ICET_GL3_STATE_START | (IceTEnum)0x002A)
#define ICET_GL3_DEPTH_RESOURCE            (ICET_GL3_STATE_START | (IceTEnum)0x002B)
#define ICET_GL3_DEPTH_DESCRIPTION         (ICET_GL3_STATE_START | (IceTEnum)0x002C)
#define ICET_GL3_SPARSE_GPU_BUFFER         (ICET_GL3_STATE_START | (IceTEnum)0x002D)
#define ICET_GL3_SPARSE_OUTPUT             (ICET_GL3_STATE_START | (IceTEnum)0x002E)
#define ICET_GL3_SPARSE_OUTPUT_SIZE        (ICET_GL3_STATE_START | (IceTEnum)0x002F)

#define ICET_GL3_STATE_ENABLE_START ICET_RENDER_LAYER_ENABLE_START
#define ICET_GL3_STATE_ENABLE_END   ICET_RENDER_LAYER_ENABLE_END

#define ICET_GL3_BUFFER_START    ICET_RENDER_LAYER_BUFFER_START
#define ICET_GL3_BUFFER_END      ICET_RENDER_LAYER_BUFFER_END


#ifdef __cplusplus
}
#endif

#endif /* __IceTGL3_h */
