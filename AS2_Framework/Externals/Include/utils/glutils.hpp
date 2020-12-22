/*
The MIT License (MIT)

Copyright (c) 2015-2017 Wayne Lin.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

//
// last modified: 2017/04/24
// 

#ifndef GLUTILS_H_
#define GLUTILS_H_

#include "../GLEW/glew.h"
#include <iostream>

void glPrintContextInfo(bool printExtension = false);
void glPrintError(void);
void glPrintFramebufferStatus(GLenum target);
void glPrintShaderLog(GLuint shader);
void glPrintProgramLog(GLuint program);

#ifdef GLUTILS_IMPLEMENTATION

void glPrintContextInfo(bool printExtension)
{
    std::cout << "GL_VENDOR = " << reinterpret_cast<const char*>(glGetString(GL_VENDOR)) << std::endl;
    std::cout << "GL_RENDERER = " << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << std::endl;
    std::cout << "GL_VERSION = " << reinterpret_cast<const char*>(glGetString(GL_VERSION)) << std::endl;
    std::cout << "GL_SHADING_LANGUAGE_VERSION = " << reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)) << std::endl;
    if(printExtension)
    {
        GLint numExt;
        glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
        std::cout << "GL_EXTENSIONS =" << std::endl;
        for(GLint i = 0; i < numExt; i++)
        {
            std::cout << "\t" << reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)) << std::endl;
        }
    }
}

void glPrintError(void)
{
    GLenum code = glGetError();
    switch(code)
    {
        case GL_NO_ERROR:
            std::cout << "GL_NO_ERROR" << std::endl;
            break;
        case GL_INVALID_ENUM:
            std::cout << "GL_INVALID_ENUM" << std::endl;
            break;
        case GL_INVALID_VALUE:
            std::cout << "GL_INVALID_VALUE" << std::endl;
            break;
        case GL_INVALID_OPERATION:
            std::cout << "GL_INVALID_OPERATION" << std::endl;
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            std::cout << "GL_INVALID_FRAMEBUFFER_OPERATION" << std::endl;
            break;
        case GL_OUT_OF_MEMORY:
            std::cout << "GL_OUT_OF_MEMORY" << std::endl;
            break;
        case GL_STACK_OVERFLOW:
            std::cout << "GL_STACK_OVERFLOW" << std::endl;
            break;
        case GL_STACK_UNDERFLOW:
            std::cout << "GL_STACK_UNDERFLOW" << std::endl;
            break;
        case GL_CONTEXT_LOST:
            std::cout << "GL_CONTEXT_LOST" << std::endl;
            break;
        default:
            std::cout << "UNKNOWN ERROR ENUM: " << std::hex << code << std::endl;
    }
}

void glPrintFramebufferStatus(GLenum target)
{
    GLenum status = glCheckFramebufferStatus(target);
    switch(status)
    {
        case GL_FRAMEBUFFER_COMPLETE:
            std::cout << "GL_FRAMEBUFFER_COMPLETE" << std::endl;
            break;
        case GL_FRAMEBUFFER_UNDEFINED:
            std::cout << "GL_FRAMEBUFFER_UNDEFINED" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cout << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cout << "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            std::cout << "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            std::cout << "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER" << std::endl;
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            std::cout << "GL_FRAMEBUFFER_UNSUPPORTED" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            std::cout << "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            std::cout << "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS" << std::endl;
            break;
        default:
            std::cout << "UNKNOWN ENUM: " << std::hex << status << std::endl;
    }
}

void glPrintShaderLog(GLuint shader)
{
    GLint maxLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
    if(maxLength > 0)
    {
        GLchar* infoLog = new GLchar[maxLength];
        glGetShaderInfoLog(shader, maxLength, NULL, infoLog);

        std::cout << infoLog << std::endl;
        delete[] infoLog;
    }
}

void glPrintProgramLog(GLuint program)
{
	GLint maxLength;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
	if (maxLength > 0)
	{
		GLchar *infoLog = new GLchar[maxLength];
		glGetProgramInfoLog(program, maxLength, NULL, infoLog);
        std::cout << infoLog << std::endl;
		delete[] infoLog;
	}
}

#endif // GLUTILS_IMPLEMENTATION

#endif // GLUTILS_H_
