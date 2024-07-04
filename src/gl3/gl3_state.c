/* -*- c -*- *******************************************************/
/*
 * Copyright (C) 2021 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
 * the U.S. Government retains certain rights in this software.
 *
 * This source code is released under the New BSD License.
 */

#include <glad/glad.h>

#include <IceTGL3.h>

#include <IceTDevGL3Image.h>

#include <IceTDevDiagnostics.h>
#include <IceTDevState.h>

#ifdef ICET_USE_PARICOMPRESS
#include <paricompress.h>
#endif

#include <string.h>

static void gl3_destroy(void);
#ifdef ICET_USE_PARICOMPRESS
static void icetGL3CreateRenderImageProgram(GLuint *program, GLint *image_uniform);
static GLuint icetGL3CompileShader(const char *source, GLenum type);
static void icetGL3LinkShaderProgram(GLuint program);
static void icetGL3CreatePlaneVertexArray(GLuint *plane_vertex_array);
#endif

static const char gl_identifier[] = "OGL3";

void icetGL3Initialize(void)
{
    IceTGetRenderedBufferImage getImageCallback = icetGL3GetRenderedBufferImage;
    IceTGetCompressedRenderedBufferImage getCompressedImageCallback =
        icetGL3GetCompressedRenderedBufferImage;

    if (icetGL3IsInitialized()) {
        icetRaiseWarning(ICET_INVALID_OPERATION,
                         "icetGLInitialize called multiple times.");
    } else {
        if (icetStateGetType(ICET_RENDER_LAYER_ID) != ICET_NULL) {
            icetRaiseError(ICET_INVALID_OPERATION,
                           "Attempted to initialize two different rendering "
                           "layers.");
        }
    }

    gladLoadGL();

    icetStateSetPointer(ICET_RENDER_LAYER_ID, gl_identifier);
    icetStateSetBoolean(ICET_GL3_INITIALIZED, ICET_TRUE);

    icetStateSetBoolean(ICET_RENDER_LAYER_HOLDS_BUFFER, ICET_TRUE);
    icetStateSetPointer(ICET_GET_RENDERED_BUFFER_IMAGE, getImageCallback);
    icetStateSetPointer(ICET_GET_COMPRESSED_RENDERED_BUFFER_IMAGE,
                        getCompressedImageCallback);

    icetStateSetPointer(ICET_GL3_DRAW_FUNCTION, NULL);
    icetStateSetInteger(ICET_GL3_COLOR_TEXTURE, 0);
    icetStateSetInteger(ICET_GL3_DEPTH_TEXTURE, 0);
    icetStateSetInteger(ICET_GL3_DEPTH_R32F_TEXTURE, 0);

    {
        GLuint framebuffer_id[2];
        glGenFramebuffers(2, framebuffer_id);
        icetStateSetInteger(ICET_GL3_FRAMEBUFFER, framebuffer_id[0]);
        icetStateSetInteger(ICET_GL3_DEPTH_FRAMEBUFFER, framebuffer_id[1]);
    }
#ifdef ICET_USE_PARICOMPRESS
    {
        GLuint program;
        GLint image_uniform;
        icetGL3CreateRenderImageProgram(&program, &image_uniform);
        icetStateSetInteger(ICET_GL3_RENDERIMAGE_PROGRAM, program);
        icetStateSetInteger(ICET_GL3_RENDERIMAGE_IMAGE_UNIFORM, image_uniform);
    }

    {
        GLuint plane_vertex_array;
        icetGL3CreatePlaneVertexArray(&plane_vertex_array);
        icetStateSetInteger(ICET_GL3_PLANE_VERTEXARRAY, plane_vertex_array);
    }
    pariSetGpuDevice(PARI_DEVICE_OPENGL);
#else
    icetStateSetInteger(ICET_GL3_RENDERIMAGE_PROGRAM, 0);
    icetStateSetInteger(ICET_GL3_RENDERIMAGE_IMAGE_UNIFORM, 0);
    icetStateSetInteger(ICET_GL3_PLANE_VERTEXARRAY, 0);
#endif
    icetStateSetPointer(ICET_GL3_COLOR_RESOURCE, NULL);
    icetStateSetPointer(ICET_GL3_COLOR_DESCRIPTION, NULL);
    icetStateSetPointer(ICET_GL3_DEPTH_RESOURCE, NULL);
    icetStateSetPointer(ICET_GL3_DEPTH_DESCRIPTION, NULL);
    icetStateSetPointer(ICET_GL3_SPARSE_GPU_BUFFER, NULL);
    icetStateSetPointer(ICET_GL3_SPARSE_OUTPUT, NULL);
    icetStateSetInteger(ICET_GL3_SPARSE_OUTPUT_SIZE, 0);

    icetStateSetPointer(ICET_RENDER_LAYER_DESTRUCTOR, gl3_destroy);
}

IceTBoolean icetGL3IsInitialized(void)
{
    if (icetStateGetType(ICET_RENDER_LAYER_ID) != ICET_NULL) {
        IceTVoid* render_layer_id;
        icetGetPointerv(ICET_RENDER_LAYER_ID, &render_layer_id);
        if (strcmp(gl_identifier, (const char*)render_layer_id) != 0) {
            return ICET_FALSE;
        }
    } else {
        return ICET_FALSE;
    }

    if (icetStateGetType(ICET_GL3_INITIALIZED) != ICET_NULL) {
        IceTBoolean initialized;
        icetGetBooleanv(ICET_GL3_INITIALIZED, &initialized);
        if (initialized) {
            return ICET_TRUE;
        }
    }
    return ICET_FALSE;
}

void gl3_destroy(void)
{
    icetRaiseDebug("In OpenGL layer destructor.");

    {
        IceTInt icet_texture;
        GLuint gl_texture;
        icetGetIntegerv(ICET_GL3_COLOR_TEXTURE, &icet_texture);
        gl_texture = icet_texture;
        if (gl_texture != 0)
        {
            glDeleteTextures(1, &gl_texture);
        }
    }
    {
        IceTInt icet_texture;
        GLuint gl_texture;
        icetGetIntegerv(ICET_GL3_DEPTH_TEXTURE, &icet_texture);
        gl_texture = icet_texture;
        if (gl_texture != 0)
        {
            glDeleteTextures(1, &gl_texture);
        }
    }
    {
        IceTInt icet_texture;
        GLuint gl_texture;
        icetGetIntegerv(ICET_GL3_DEPTH_R32F_TEXTURE, &icet_texture);
        gl_texture = icet_texture;
        if (gl_texture != 0)
        {
            glDeleteTextures(1, &gl_texture);
        }
    }

    {
        IceTInt icet_framebuffer;
        GLuint gl_framebuffer;
        icetGetIntegerv(ICET_GL3_FRAMEBUFFER, &icet_framebuffer);
        gl_framebuffer = icet_framebuffer;
        if (gl_framebuffer != 0)
        {
            glDeleteTextures(1, &gl_framebuffer);
        }
    }
    {
        IceTInt icet_framebuffer;
        GLuint gl_framebuffer;
        icetGetIntegerv(ICET_GL3_DEPTH_FRAMEBUFFER, &icet_framebuffer);
        gl_framebuffer = icet_framebuffer;
        if (gl_framebuffer != 0)
        {
            glDeleteTextures(1, &gl_framebuffer);
        }
    }
    
    {
        IceTInt icet_vertex_array;
        GLuint gl_vertex_array;
        icetGetIntegerv(ICET_GL3_PLANE_VERTEXARRAY, &icet_vertex_array);
        gl_vertex_array = icet_vertex_array;
        if (gl_vertex_array != 0)
        {
            glDeleteVertexArrays(1, &gl_vertex_array);
        }
    }

    /* If icetGL3GetCompressedRenderBufferImage in gl3_image.c uses a special
     * allocator, then a matching deallocator should be used here. */
    {
        IceTVoid *sparse_buffer;
        icetGetPointerv(ICET_GL3_SPARSE_OUTPUT, &sparse_buffer);
        if (sparse_buffer != NULL) {
#ifdef ICET_USE_PARICOMPRESS
            pariFreeCpuBuffer(sparse_buffer);
#else
            free(sparse_buffer);
#endif
        }
    }
}

#ifdef ICET_USE_PARICOMPRESS
void icetGL3CreateRenderImageProgram(GLuint *program, GLint *image_uniform)
{
    /* Vertex shader source */
    const char *vert_source =
#ifndef __APPLE__
        "#version 130\n"
#else
        "#version 330\n"
#endif
        "\n"
        "in vec3 vertex_position;\n"
        "in vec2 vertex_texcoord;\n"
        "\n"
        "out vec2 frag_texcoord;\n"
        "\n"
        "void main() {\n"
        "    gl_Position = vec4(vertex_position, 1.0);\n"
        "    frag_texcoord = vertex_texcoord;\n"
        "}\n";
    const char *frag_source =
#ifndef __APPLE__
        "#version 130\n"
#else
        "#version 330\n"
#endif
        "\n"
        "uniform sampler2D image;\n"
        "\n"
        "in vec2 frag_texcoord;\n"
        "\n"
        "out vec4 FragColor;\n"
        "\n"
        "void main() {\n"
        "    FragColor = texture(image, frag_texcoord);\n"
        "}\n";

    /* Compile vetex shader */
    GLuint vertex_shader = icetGL3CompileShader(vert_source, GL_VERTEX_SHADER);
    /* Compile fragment shader */
    GLuint fragment_shader = icetGL3CompileShader(frag_source, GL_FRAGMENT_SHADER);

    /* Create GPU program from the compiled vertex and fragment shaders */
    *program = glCreateProgram();
    glAttachShader(*program, vertex_shader);
    glAttachShader(*program, fragment_shader);

    /* Specify input and output attributes for the GPU program */
    glBindAttribLocation(*program, 0, "vertex_position");
    glBindAttribLocation(*program, 1, "vertex_texcoord");
    glBindFragDataLocation(*program, 0, "FragColor");

    /* Link compiled GPU program */
    icetGL3LinkShaderProgram(*program);

    /* Get uniform location for texture */
    *image_uniform = glGetUniformLocation(*program, "image");
}

GLuint icetGL3CompileShader(const char *source, GLenum type)
{
    /* Create a shader object */
    GLint status;
    GLuint shader = glCreateShader(type);

    /* Send the source to the shader object */
    const GLint len = strlen(source);
    glShaderSource(shader, 1, &source, &len);

    /* Compile the shader program */
    glCompileShader(shader);

    /* Check to see if it compiled successfully */
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == 0)
    {
        GLint log_length;
        char *info;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        info = (char*)malloc(log_length + 1);
        glGetShaderInfoLog(shader, log_length, NULL, info);
        if (type == GL_VERTEX_SHADER)
        {
            icetRaiseError(
                ICET_INVALID_OPERATION, "Error: failed to compile vertex shader:\n%s", info);
        }
        else
        {
            icetRaiseError(
                ICET_INVALID_OPERATION, "Error: failed to compile fragment shader:\n%s", info);
        }
        free(info);
        
        return 0;
    }

    return shader;
}

void icetGL3LinkShaderProgram(GLuint program)
{
    /* Link GPU program */
    GLint status;
    glLinkProgram(program);

    /* Check to see if it linked successfully */
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == 0)
    {
        GLint log_length;
        char *info;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        info = (char*)malloc(log_length + 1);
        glGetProgramInfoLog(program, log_length, NULL, info);
        icetRaiseError(ICET_INVALID_OPERATION, "Error: failed to link shader program:\n%s", info);
        free(info);
    }
}

void icetGL3CreatePlaneVertexArray(GLuint *plane_vertex_array)
{
    GLuint vertex_position_attrib = 0;
    GLuint vertex_texcoord_attrib = 1;
    GLuint vertex_position_buffer;
    GLuint vertex_texcoord_buffer;
    GLuint vertex_index_buffer;

    /* Create vertex array object */
    glGenVertexArrays(1, plane_vertex_array);
    glBindVertexArray(*plane_vertex_array);

    /* Vertex positions */
    glGenBuffers(1, &vertex_position_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_position_buffer);
    {
        GLfloat vertices[12] = {
            -1.0, -1.0,  0.0,
             1.0, -1.0,  0.0,
             1.0,  1.0,  0.0,
            -1.0,  1.0,  0.0
        };
        glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
    }
    glEnableVertexAttribArray(vertex_position_attrib);
    glVertexAttribPointer(vertex_position_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);

    /* Vertex texture coordinates */
    glGenBuffers(1, &vertex_texcoord_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_texcoord_buffer);
    {
        GLfloat texcoords[8] = {
            0.0,  0.0,
            1.0,  0.0,
            1.0,  1.0,
            0.0,  1.0
        };
        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), texcoords, GL_STATIC_DRAW);
    }
    glEnableVertexAttribArray(vertex_texcoord_attrib);
    glVertexAttribPointer(vertex_texcoord_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

    /* Faces of the triangles */
    glGenBuffers(1, &vertex_index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertex_index_buffer);
    {
        GLushort indices[6] = {
             0,  1,  2,    0,  2,  3
        };
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLushort), indices, GL_STATIC_DRAW);
    }

    glBindVertexArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
#endif
