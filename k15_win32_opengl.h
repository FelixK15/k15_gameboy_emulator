#ifndef K15_WIN32_OPENGL_HEADER
#define K15_WIN32_OPENGL_HEADER

#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_FLAGS_ARB                   0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB       	0x1
#define WGL_CONTEXT_DEBUG_BIT_ARB               0x1
#define GL_MAP_WRITE_BIT                  		0x2
#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013
#define WGL_TYPE_RGBA_ARB                       0x202B
#define WGL_COLOR_BITS_ARB                      0x2014

#define GL_MAJOR_VERSION                        0x821B
#define GL_MINOR_VERSION                        0x821C
#define GL_VERSION                              0x1F02

#define GL_FALSE                                0
#define GL_TRUE                                 1
#define GL_ONE                                  1

#define GL_SCISSOR_TEST                         0x0C11
#define GL_STENCIL_TEST                         0x0B90
#define GL_TEXTURE_BINDING_2D                   0x8069
#define GL_LINEAR                               0x2601
#define GL_CULL_FACE                            0x0B44
#define GL_BLEND                                0x0BE2
#define GL_UNSIGNED_SHORT                       0x1403
#define GL_UNSIGNED_INT                         0x1405
#define GL_BLEND_EQUATION_RGB                   0x8009
#define GL_BLEND_EQUATION_ALPHA                 0x883D
#define GL_BLEND_DST_RGB                        0x80C8
#define GL_BLEND_SRC_RGB                        0x80C9
#define GL_BLEND_DST_ALPHA                      0x80CA
#define GL_BLEND_SRC_ALPHA                      0x80CB
#define GL_VIEWPORT                             0x0BA2
#define GL_SCISSOR_BOX                          0x0C10
#define GL_ARRAY_BUFFER_BINDING                 0x8894
#define GL_TEXTURE_MAG_FILTER            		0x2800
#define GL_TEXTURE_MIN_FILTER            		0x2801
#define GL_TEXTURE_BASE_LEVEL             		0x813C
#define GL_TEXTURE_MAX_LEVEL              		0x813D
#define GL_NEAREST                        		0x2600
#define GL_TEXTURE_2D                     		0x0DE1
#define GL_RGB                            		0x1907
#define GL_RGB8                           		0x8051
#define GL_R8                           		0x8229
#define GL_RGBA                                 0x1908
#define GL_RED                            		0x1903
#define GL_UNSIGNED_BYTE                  		0x1401
#define GL_FLOAT                          		0x1406
#define GL_COLOR_BUFFER_BIT               		0x4000
#define GL_ARRAY_BUFFER                   		0x8892
#define GL_TRIANGLES                      		0x0004
#define GL_FUNC_ADD                             0x8006
#define GL_FRAGMENT_SHADER                		0x8B30
#define GL_VERTEX_SHADER                  		0x8B31
#define GL_ELEMENT_ARRAY_BUFFER                 0x8893
#define GL_SRC_ALPHA                            0x0302
#define GL_ONE_MINUS_SRC_ALPHA                  0x0303
#define GL_COMPILE_STATUS                 		0x8B81
#define GL_LINK_STATUS                    		0x8B82
#define GL_INFO_LOG_LENGTH                		0x8B84
#define GL_STATIC_DRAW                    		0x88E4
#define GL_DYNAMIC_DRAW                   		0x88E8
#define GL_STREAM_DRAW                          0x88E0
#define GL_DEPTH_TEST                           0x0B71
#define GL_ACTIVE_TEXTURE                       0x84E0
#define GL_TEXTURE0                             0x84C0
#define GL_CURRENT_PROGRAM                      0x8B8D
#define GL_VERTEX_ARRAY_BINDING                 0x85B5
#define GL_DEPTH_TEST                           0x0B71
#define GL_CLAMP_TO_EDGE                        0x812F
#define GL_TEXTURE_WRAP_S                       0x2802
#define GL_TEXTURE_WRAP_T                       0x2803
#define GL_WRITE_ONLY                     		0x88B9

typedef unsigned int 	GLenum;
typedef unsigned char 	GLboolean;
typedef unsigned int 	GLbitfield;
typedef signed char 	GLbyte;
typedef short 			GLshort;
typedef int 			GLint;
typedef int 			GLsizei;
typedef unsigned char 	GLubyte;
typedef unsigned short 	GLushort;
typedef unsigned int 	GLuint;
typedef float			GLfloat;
typedef float			GLclampf;
typedef double 			GLdouble;
typedef double 			GLclampd;
typedef void 			GLvoid;
typedef char			GLchar;
typedef ptrdiff_t 		GLsizeiptr;

typedef GLvoid* (WINAPI *PFNWGLGETPROCADDRESSPROC)(const char*);
typedef HGLRC	(WINAPI *PFNWGLCREATECONTEXTPROC)(HDC);
typedef BOOL	(WINAPI *PFNWGLDELETECONTEXTPROC)(HGLRC);
typedef BOOL	(WINAPI *PFNWGLMAKECURRENTPROC)(HDC, HGLRC);
typedef HGLRC	(WINAPI *PFNWGLGETCURRENTCONTEXTPROC)(void);
typedef HDC		(WINAPI *PFNWGLGETCURRENTDCPROC)(void);
typedef BOOL 	(WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
typedef int 	(WINAPI *PFNWGLGETSWAPINTERVALEXTPROC)(void);
typedef BOOL 	(WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
typedef HGLRC   (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);

typedef const GLubyte*  (APIENTRY *PFNGLGETSTRINGPROC) (GLenum name);

typedef GLvoid          (APIENTRY *PFNGLBINDTEXTUREPROC)(GLenum, GLuint);
typedef GLvoid          (APIENTRY *PFNGLDRAWELEMENTSPROC)(GLenum, GLsizei, GLenum, const GLvoid*);
typedef GLvoid          (APIENTRY *PFNGLTEXPARAMETERIPROC)(GLenum, GLenum, GLint);
typedef GLvoid          (APIENTRY *PFNGLVIEWPORTPROC)(GLint, GLint, GLsizei, GLsizei);
typedef GLvoid          (APIENTRY *PFNGLGENTEXTURESPROC)(GLsizei, GLuint*);
typedef GLvoid          (APIENTRY *PFNGLCLEARPROC)(GLbitfield);
typedef GLenum          (APIENTRY *PFNGLGETERRORPROC)(GLvoid);
typedef GLvoid          (APIENTRY *PFNGLGETINTEGERVPROC)(GLenum pname, GLint* pData);
typedef GLvoid          (APIENTRY *PFNGLENABLEPROC) (GLenum cap);
typedef GLvoid          (APIENTRY *PFNGLDISABLEPROC) (GLenum cap);
typedef GLboolean       (APIENTRY *PFNGLISENABLEDPROC) (GLenum cap);
typedef GLvoid          (APIENTRY *PFNGLBLENDEQUATIONPROC) (GLenum mode);
typedef GLvoid          (APIENTRY *PFNGLBLENDFUNCSEPARATEIPROC) (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef GLvoid          (APIENTRY *PFNGLTEXSTORAGE2DPROC)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
typedef GLvoid          (APIENTRY *PFNGLTEXSUBIMAGE2DPROC)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
typedef GLvoid          (APIENTRY *PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef GLvoid          (APIENTRY *PFNGLUNIFORMMATRIX4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef GLvoid          (APIENTRY *PFNGLACTIVETEXTUREPROC) (GLenum texture);
typedef GLvoid          (APIENTRY *PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef GLvoid          (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef GLvoid          (APIENTRY *PFNGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
typedef GLvoid          (APIENTRY *PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef GLvoid          (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLvoid          (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef GLuint          (APIENTRY *PFNGLCREATESHADERPROC) (GLenum type);
typedef GLvoid          (APIENTRY *PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef GLvoid          (APIENTRY *PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint          (APIENTRY *PFNGLCREATEPROGRAMPROC) (void);
typedef GLvoid          (APIENTRY *PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef GLvoid          (APIENTRY *PFNGLBINDATTRIBLOCATIONPROC) (GLuint program, GLuint index, const GLchar *name);
typedef GLvoid          (APIENTRY *PFNGLLINKPROGRAMPROC) (GLuint program);
typedef GLvoid          (APIENTRY *PFNGLUSEPROGRAMPROC) (GLuint program);
typedef GLvoid          (APIENTRY *PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef GLvoid          (APIENTRY *PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLvoid          (APIENTRY *PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef GLvoid          (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLint           (APIENTRY *PFNGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar *name);
typedef GLvoid          (APIENTRY *PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef GLvoid          (APIENTRY *PFNGLBINDVERTEXARRAYPROC) (GLuint array);
typedef GLvoid          (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint *arrays);
typedef GLvoid          (APIENTRY *PFNGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef GLvoid          (APIENTRY *PFNGLSCISSORPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef GLvoid          (APIENTRY *PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint *textures);
typedef GLint           (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef GLvoid          (APIENTRY *PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint *buffers);
typedef GLvoid          (APIENTRY *PFNGLDETACHSHADERPROC) (GLuint program, GLuint shader);
typedef GLvoid          (APIENTRY *PFNGLDELETESHADERPROC) (GLuint shader);
typedef GLvoid          (APIENTRY *PFNGLDELETEPROGRAMPROC) (GLuint program);
typedef GLvoid          (APIENTRY *PFNGLBLENDEQUATIONSEPARATEPROC) (GLenum modeRGB, GLenum modeAlpha);
typedef GLvoid          (APIENTRY *PFNGLBLENDFUNCSEPARATEPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef GLenum          (APIENTRY *PFNGLGETERRORPROC) (void);
typedef GLvoid 			(APIENTRY *PFNGLBUFFERSTORAGEPROC) (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
typedef GLvoid*			(APIENTRY *PFNGLMAPBUFFERPROC) (GLenum target, GLenum access);
typedef GLboolean 		(APIENTRY *PFNGLUNMAPBUFFERPROC) (GLenum target);

PFNWGLSWAPINTERVALEXTPROC 			w32glSwapIntervalEXT 			= nullptr;
PFNWGLGETSWAPINTERVALEXTPROC		w32glGetSwapIntervalEXT			= nullptr;
PFNWGLCHOOSEPIXELFORMATARBPROC		w32glChoosePixelFormatARB		= nullptr;
PFNWGLCREATECONTEXTATTRIBSARBPROC	w32glCreateContextAttribsARB 	= nullptr;
PFNWGLCREATECONTEXTPROC             w32glCreateContext              = nullptr;
PFNWGLDELETECONTEXTPROC             w32glDeleteContext              = nullptr;
PFNWGLGETCURRENTCONTEXTPROC         w32glGetCurrentContext          = nullptr;
PFNWGLGETPROCADDRESSPROC            w32glGetProcAddress             = nullptr;
PFNWGLMAKECURRENTPROC               w32glMakeCurrent                = nullptr;

PFNGLGENTEXTURESPROC				glGenTextures					= nullptr;
PFNGLBINDTEXTUREPROC				glBindTexture					= nullptr;
PFNGLTEXPARAMETERIPROC				glTexParameteri					= nullptr;
PFNGLVIEWPORTPROC					glViewport						= nullptr;
PFNGLCLEARPROC 						glClear							= nullptr;
PFNGLGETERRORPROC					glGetError						= nullptr;
PFNGLTEXSTORAGE2DPROC				glTexStorage2D					= nullptr;
PFNGLTEXSUBIMAGE2DPROC				glTexSubImage2D					= nullptr;
PFNGLGENBUFFERSPROC					glGenBuffers					= nullptr;
PFNGLBINDBUFFERPROC					glBindBuffer					= nullptr;
PFNGLBUFFERSTORAGEPROC				glBufferStorage					= nullptr;
PFNGLMAPBUFFERPROC					glMapBuffer						= nullptr;
PFNGLUNMAPBUFFERPROC				glUnmapBuffer					= nullptr;
PFNGLDRAWARRAYSPROC					glDrawArrays					= nullptr;
PFNGLVERTEXATTRIBPOINTERPROC		glVertexAttribPointer			= nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC	glEnableVertexAttribArray		= nullptr;
PFNGLCREATESHADERPROC				glCreateShader					= nullptr;
PFNGLSHADERSOURCEPROC				glShaderSource					= nullptr;
PFNGLCOMPILESHADERPROC				glCompileShader					= nullptr;
PFNGLCREATEPROGRAMPROC				glCreateProgram					= nullptr;
PFNGLATTACHSHADERPROC				glAttachShader					= nullptr;
PFNGLBINDATTRIBLOCATIONPROC			glBindAttribLocation			= nullptr;
PFNGLLINKPROGRAMPROC				glLinkProgram					= nullptr;
PFNGLUSEPROGRAMPROC					glUseProgram					= nullptr;
PFNGLGETSHADERIVPROC				glGetShaderiv					= nullptr;
PFNGLGETSHADERINFOLOGPROC			glGetShaderInfoLog				= nullptr;
PFNGLGETPROGRAMIVPROC				glGetProgramiv					= nullptr;
PFNGLGETPROGRAMINFOLOGPROC			glGetProgramInfoLog				= nullptr;
PFNGLGETATTRIBLOCATIONPROC			glGetAttribLocation				= nullptr;
PFNGLGENVERTEXARRAYSPROC			glGenVertexArrays				= nullptr;
PFNGLBINDVERTEXARRAYPROC			glBindVertexArray				= nullptr;

//imgui opengl subset
PFNGLGETINTEGERVPROC  	           	 glGetIntegerv	            	= nullptr;
PFNGLGETSTRINGPROC                   glGetString                	= nullptr;
PFNGLENABLEPROC                      glEnable                   	= nullptr;
PFNGLDISABLEPROC                     glDisable                  	= nullptr;
PFNGLBLENDEQUATIONPROC               glBlendEquation            	= nullptr;
PFNGLUNIFORM1IPROC                   glUniform1i                	= nullptr;
PFNGLUNIFORMMATRIX4FVPROC            glUniformMatrix4fv         	= nullptr;
PFNGLACTIVETEXTUREPROC               glActiveTexture            	= nullptr;
PFNGLISENABLEDPROC                   glIsEnabled                	= nullptr;
PFNGLSCISSORPROC                     glScissor                  	= nullptr;
PFNGLDRAWELEMENTSPROC                glDrawElements             	= nullptr;
PFNGLDELETEVERTEXARRAYSPROC          glDeleteVertexArrays       	= nullptr;
PFNGLTEXIMAGE2DPROC                  glTexImage2D               	= nullptr;
PFNGLDELETETEXTURESPROC              glDeleteTextures           	= nullptr;
PFNGLGETUNIFORMLOCATIONPROC          glGetUniformLocation       	= nullptr;
PFNGLDELETEBUFFERSPROC               glDeleteBuffers            	= nullptr;
PFNGLDETACHSHADERPROC                glDetachShader             	= nullptr;
PFNGLDELETESHADERPROC                glDeleteShader             	= nullptr;
PFNGLDELETEPROGRAMPROC               glDeleteProgram            	= nullptr;
PFNGLBLENDEQUATIONSEPARATEPROC       glBlendEquationSeparate    	= nullptr;
PFNGLBLENDFUNCSEPARATEPROC           glBlendFuncSeparate        	= nullptr;

void loadWin32OpenGLFunctionPointers( HMODULE pOpenGL32Module )
{
	//FK: wgl functions
	w32glGetProcAddress 	= (PFNWGLGETPROCADDRESSPROC)GetProcAddress(pOpenGL32Module, "wglGetProcAddress");
	w32glCreateContext      = (PFNWGLCREATECONTEXTPROC)GetProcAddress(pOpenGL32Module, "wglCreateContext");
	w32glDeleteContext   	= (PFNWGLDELETECONTEXTPROC)GetProcAddress(pOpenGL32Module, "wglDeleteContext");
	w32glGetCurrentContext  = (PFNWGLGETCURRENTCONTEXTPROC)GetProcAddress(pOpenGL32Module, "wglGetCurrentContext");
	w32glMakeCurrent        = (PFNWGLMAKECURRENTPROC)GetProcAddress(pOpenGL32Module, "wglMakeCurrent");

	//FK: the following gl functions are part of the opengl32.dll and can not be loaded by wglGetProcAddress
	glGenTextures 			= (PFNGLGENTEXTURESPROC)GetProcAddress(pOpenGL32Module, "glGenTextures");
	glBindTexture 			= (PFNGLBINDTEXTUREPROC)GetProcAddress(pOpenGL32Module, "glBindTexture");
	glTexParameteri			= (PFNGLTEXPARAMETERIPROC)GetProcAddress(pOpenGL32Module, "glTexParameteri");
	glViewport				= (PFNGLVIEWPORTPROC)GetProcAddress(pOpenGL32Module, "glViewport");
	glClear					= (PFNGLCLEARPROC)GetProcAddress(pOpenGL32Module, "glClear");
	glGetError				= (PFNGLGETERRORPROC)GetProcAddress(pOpenGL32Module, "glGetError");
	glGetIntegerv			= (PFNGLGETINTEGERVPROC)GetProcAddress(pOpenGL32Module, "glGetIntegerv");
	glGetString             = (PFNGLGETSTRINGPROC)GetProcAddress(pOpenGL32Module, "glGetString");
	glEnable                = (PFNGLENABLEPROC)GetProcAddress(pOpenGL32Module, "glEnable");
	glDisable               = (PFNGLDISABLEPROC)GetProcAddress(pOpenGL32Module, "glDisable");
	glIsEnabled             = (PFNGLISENABLEDPROC)GetProcAddress(pOpenGL32Module, "glIsEnabled");
	glScissor               = (PFNGLSCISSORPROC)GetProcAddress(pOpenGL32Module, "glScissor");
	glTexImage2D            = (PFNGLTEXIMAGE2DPROC)GetProcAddress(pOpenGL32Module, "glTexImage2D");

	RuntimeAssert(glGenTextures != nullptr);
	RuntimeAssert(glBindTexture != nullptr);
	RuntimeAssert(glTexParameteri != nullptr);
	RuntimeAssert(glViewport != nullptr);
	RuntimeAssert(glClear != nullptr);
	RuntimeAssert(glGetError != nullptr);
	RuntimeAssert(glGetIntegerv != nullptr);
	RuntimeAssert(glGetString != nullptr);
	RuntimeAssert(glEnable != nullptr);
	RuntimeAssert(glDisable != nullptr);
	RuntimeAssert(glIsEnabled != nullptr);
	RuntimeAssert(glScissor != nullptr);
	RuntimeAssert(glTexImage2D != nullptr);

	RuntimeAssert(w32glGetProcAddress != nullptr );
	RuntimeAssert(w32glCreateContext != nullptr );
	RuntimeAssert(w32glDeleteContext != nullptr );
	RuntimeAssert(w32glGetCurrentContext != nullptr );
	RuntimeAssert(w32glMakeCurrent != nullptr );
}

void loadWGLOpenGLFunctionPointers()
{
	w32glChoosePixelFormatARB 		= (PFNWGLCHOOSEPIXELFORMATARBPROC)w32glGetProcAddress("wglChoosePixelFormatARB");
	w32glCreateContextAttribsARB 	= (PFNWGLCREATECONTEXTATTRIBSARBPROC)w32glGetProcAddress("wglCreateContextAttribsARB");
	w32glSwapIntervalEXT			= (PFNWGLSWAPINTERVALEXTPROC)w32glGetProcAddress("wglSwapIntervalEXT");
	w32glGetSwapIntervalEXT			= (PFNWGLGETSWAPINTERVALEXTPROC)w32glGetProcAddress("wglGetSwapIntervalEXT");

	RuntimeAssert(w32glChoosePixelFormatARB != nullptr);
	RuntimeAssert(w32glCreateContextAttribsARB != nullptr);
	RuntimeAssert(w32glSwapIntervalEXT != nullptr);
	RuntimeAssert(w32glGetSwapIntervalEXT != nullptr);
}

void loadOpenGL4FunctionPointers()
{
	glTexStorage2D 				= (PFNGLTEXSTORAGE2DPROC)w32glGetProcAddress("glTexStorage2D");
	glTexSubImage2D 			= (PFNGLTEXSUBIMAGE2DPROC)w32glGetProcAddress("glTexSubImage2D");
	glGenBuffers				= (PFNGLGENBUFFERSPROC)w32glGetProcAddress("glGenBuffers");
	glBindBuffer				= (PFNGLBINDBUFFERPROC)w32glGetProcAddress("glBindBuffer");
	glBufferStorage				= (PFNGLBUFFERSTORAGEPROC)w32glGetProcAddress("glBufferStorage");
	glMapBuffer					= (PFNGLMAPBUFFERPROC)w32glGetProcAddress("glMapBuffer");
	glUnmapBuffer				= (PFNGLUNMAPBUFFERPROC)w32glGetProcAddress("glUnmapBuffer");
	glDrawArrays				= (PFNGLDRAWARRAYSPROC)w32glGetProcAddress("glDrawArrays");
	glVertexAttribPointer		= (PFNGLVERTEXATTRIBPOINTERPROC)w32glGetProcAddress("glVertexAttribPointer");
	glEnableVertexAttribArray	= (PFNGLENABLEVERTEXATTRIBARRAYPROC)w32glGetProcAddress("glEnableVertexAttribArray");
	glCreateShader				= (PFNGLCREATESHADERPROC)w32glGetProcAddress("glCreateShader");
	glShaderSource				= (PFNGLSHADERSOURCEPROC)w32glGetProcAddress("glShaderSource");
	glCompileShader				= (PFNGLCOMPILESHADERPROC)w32glGetProcAddress("glCompileShader");
	glCreateProgram				= (PFNGLCREATEPROGRAMPROC)w32glGetProcAddress("glCreateProgram");
	glAttachShader				= (PFNGLATTACHSHADERPROC)w32glGetProcAddress("glAttachShader");
	glBindAttribLocation		= (PFNGLBINDATTRIBLOCATIONPROC)w32glGetProcAddress("glBindAttribLocation");
	glLinkProgram				= (PFNGLLINKPROGRAMPROC)w32glGetProcAddress("glLinkProgram");
	glUseProgram				= (PFNGLUSEPROGRAMPROC)w32glGetProcAddress("glUseProgram");
	glGetShaderiv				= (PFNGLGETSHADERIVPROC)w32glGetProcAddress("glGetShaderiv");
	glGetShaderInfoLog 			= (PFNGLGETSHADERINFOLOGPROC)w32glGetProcAddress("glGetShaderInfoLog");
	glGetProgramiv				= (PFNGLGETPROGRAMIVPROC)w32glGetProcAddress("glGetProgramiv");
	glGetProgramInfoLog 		= (PFNGLGETPROGRAMINFOLOGPROC)w32glGetProcAddress("glGetProgramInfoLog");
	glGetAttribLocation			= (PFNGLGETATTRIBLOCATIONPROC)w32glGetProcAddress("glGetAttribLocation");
	glGenVertexArrays			= (PFNGLGENVERTEXARRAYSPROC)w32glGetProcAddress("glGenVertexArrays");
	glBindVertexArray			= (PFNGLBINDVERTEXARRAYPROC)w32glGetProcAddress("glBindVertexArray");
	glBlendEquation             = (PFNGLBLENDEQUATIONPROC)w32glGetProcAddress("glBlendEquation");               
	glUniform1i                 = (PFNGLUNIFORM1IPROC)w32glGetProcAddress("glUniform1i");                   
	glUniformMatrix4fv          = (PFNGLUNIFORMMATRIX4FVPROC)w32glGetProcAddress("glUniformMatrix4fv");            
	glActiveTexture             = (PFNGLACTIVETEXTUREPROC)w32glGetProcAddress("glActiveTexture");           
	glDrawElements              = (PFNGLDRAWELEMENTSPROC)w32glGetProcAddress("glDrawElements");                
	glDeleteVertexArrays        = (PFNGLDELETEVERTEXARRAYSPROC)w32glGetProcAddress("glDeleteVertexArrays");          
	glDeleteTextures            = (PFNGLDELETETEXTURESPROC)w32glGetProcAddress("glDeleteTextures");              
	glGetUniformLocation        = (PFNGLGETUNIFORMLOCATIONPROC)w32glGetProcAddress("glGetUniformLocation");          
	glDeleteBuffers             = (PFNGLDELETEBUFFERSPROC)w32glGetProcAddress("glDeleteBuffers");               
	glDetachShader              = (PFNGLDETACHSHADERPROC)w32glGetProcAddress("glDetachShader");                
	glDeleteShader              = (PFNGLDELETESHADERPROC)w32glGetProcAddress("glDeleteShader");                
	glDeleteProgram             = (PFNGLDELETEPROGRAMPROC)w32glGetProcAddress("glDeleteProgram");               
	glBlendEquationSeparate     = (PFNGLBLENDEQUATIONSEPARATEPROC)w32glGetProcAddress("glBlendEquationSeparate");       
	glBlendFuncSeparate         = (PFNGLBLENDFUNCSEPARATEPROC)w32glGetProcAddress("glBlendFuncSeparate");           

	RuntimeAssert(glTexStorage2D != nullptr);
	RuntimeAssert(glTexSubImage2D != nullptr);
	RuntimeAssert(glGenBuffers != nullptr);
	RuntimeAssert(glBindBuffer	!= nullptr);
	RuntimeAssert(glBufferStorage != nullptr);
	RuntimeAssert(glMapBuffer != nullptr);
	RuntimeAssert(glUnmapBuffer != nullptr);
	RuntimeAssert(glDrawArrays != nullptr);
	RuntimeAssert(glVertexAttribPointer != nullptr);
	RuntimeAssert(glEnableVertexAttribArray != nullptr);
	RuntimeAssert(glCreateShader != nullptr);
	RuntimeAssert(glShaderSource != nullptr);
	RuntimeAssert(glCompileShader != nullptr);
	RuntimeAssert(glCreateProgram != nullptr);
	RuntimeAssert(glAttachShader != nullptr);
	RuntimeAssert(glBindAttribLocation != nullptr);
	RuntimeAssert(glLinkProgram != nullptr);
	RuntimeAssert(glUseProgram != nullptr);
	RuntimeAssert(glGetShaderiv != nullptr);
	RuntimeAssert(glGetShaderInfoLog != nullptr);
	RuntimeAssert(glGetProgramiv != nullptr);
	RuntimeAssert(glGetProgramInfoLog != nullptr);
	RuntimeAssert(glGetAttribLocation != nullptr);
	RuntimeAssert(glGenVertexArrays != nullptr);
	RuntimeAssert(glBindVertexArray != nullptr);
	RuntimeAssert(glBlendEquation != nullptr);
	RuntimeAssert(glUniform1i != nullptr);
	RuntimeAssert(glUniformMatrix4fv != nullptr);
	RuntimeAssert(glActiveTexture != nullptr);
	RuntimeAssert(glDrawElements != nullptr);
	RuntimeAssert(glDeleteVertexArrays != nullptr);
	RuntimeAssert(glDeleteTextures != nullptr);
	RuntimeAssert(glGetUniformLocation != nullptr);
	RuntimeAssert(glDeleteBuffers != nullptr);
	RuntimeAssert(glDetachShader != nullptr);
	RuntimeAssert(glDeleteShader != nullptr);
	RuntimeAssert(glDeleteProgram != nullptr);
	RuntimeAssert(glBlendEquationSeparate != nullptr);
	RuntimeAssert(glBlendFuncSeparate != nullptr);
}

HGLRC createOpenGLDummyContext( HWND hwnd, HDC hdc )
{
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
		PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
		32,                        //Colordepth of the framebuffer.
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		0,                        //Number of bits for the depthbuffer
		0,                        //Number of bits for the stencilbuffer
		0,                        //Number of Aux buffers in the framebuffer.
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	int pixelFormat = w32ChoosePixelFormat(hdc, &pfd); 
	w32SetPixelFormat(hdc, pixelFormat, &pfd);

	const HGLRC pOpenGLContext = w32glCreateContext(hdc);
	w32glMakeCurrent(hdc, pOpenGLContext);

	return pOpenGLContext;
}

HGLRC createOpenGL4Context( HWND hwnd, HDC hdc )
{
	const HGLRC pOldOpenGLContext = w32glGetCurrentContext();

	const int pixelFormatAttributeList[] = {
		WGL_DRAW_TO_WINDOW_ARB, 1,
		WGL_SUPPORT_OPENGL_ARB, 1,
		WGL_DOUBLE_BUFFER_ARB, 1,
		WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB, 32,
		0
	};

	int pixelFormat;
	UINT numFormats;
	w32glChoosePixelFormatARB( hdc, pixelFormatAttributeList, nullptr, 1, &pixelFormat, &numFormats );
	w32SetPixelFormat( hdc, pixelFormat, nullptr );

	const int contextAttributeList[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
		WGL_CONTEXT_MINOR_VERSION_ARB, 1,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0,
	};

	const HGLRC pOpenGLContext = w32glCreateContextAttribsARB( hdc, nullptr, contextAttributeList );
	w32glMakeCurrent( hdc, pOpenGLContext );
	w32glDeleteContext( pOldOpenGLContext );

	return pOpenGLContext;
}

void compileOpenGLShader(GLuint shader, const char* pShaderSource, uint32_t shaderSourceLength )
{
	const char* shaderSources[] = {
		pShaderSource
	};

	glShaderSource( shader, 1, shaderSources, (const GLint*)&shaderSourceLength );
	glCompileShader( shader );

	GLint compileStatus = 0;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &compileStatus );

	if( compileStatus == 1 )
	{
		return;
	}

	GLint logLength = 0;
	glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &logLength );

	char infoLogBuffer[1024];
	glGetShaderInfoLog( shader, sizeof(infoLogBuffer), nullptr, infoLogBuffer );
	printf( "%s", infoLogBuffer );
}

#endif 