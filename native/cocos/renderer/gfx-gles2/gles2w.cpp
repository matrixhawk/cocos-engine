/****************************************************************************
Copyright (c) 2020 Xiamen Yaji Software Co., Ltd.

http://www.cocos2d-x.org

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
****************************************************************************/
#include "gles2w.h"

#if defined(_WIN32) && !defined(ANDROID)
    #define WIN32_LEAN_AND_MEAN 1
    #include <windows.h>

static HMODULE libegl = NULL;
static HMODULE libgles = NULL;

static bool open_libgl(void) {
    libegl = LoadLibraryA("libEGL.dll");
    libgles = LoadLibraryA("libGLESv2.dll");
    return (libegl && libgles);
}

static void *get_egl_proc(const char *proc) {
    void *res = nullptr;
    if (eglGetProcAddress) res = eglGetProcAddress(proc);
    if (!res) res = (void *)GetProcAddress(libegl, proc);
    return res;
}

static void *get_gles_proc(const char *proc) {
    void *res = eglGetProcAddress(proc);
    if (!res) res = (void *)GetProcAddress(libgles, proc);
    return res;
}
#elif defined(__APPLE__) || defined(__APPLE_CC__)
    #import <CoreFoundation/CoreFoundation.h>
    #import <UIKit/UIDevice.h>
    #import <string>
    #import <iostream>
    #import <stdio.h>

// Routine to run a system command and retrieve the output.
// From http://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c
std::string ES2_EXEC(const char *cmd) {
    FILE *pipe = popen(cmd, "r");
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
}

static CFBundleRef g_es2Bundle;
static CFURLRef g_es2BundleURL;

static bool open_libgl(void) {
    CFStringRef frameworkPath = CFSTR("/System/Library/Frameworks/OpenGLES.framework");
    NSString *sysVersion = [UIDevice currentDevice].systemVersion;
    NSArray *sysVersionComponents = [sysVersion componentsSeparatedByString:@"."];
    //BOOL isSimulator = ([[UIDevice currentDevice].model rangeOfString:@"Simulator"].location != NSNotFound);

    #if TARGET_IPHONE_SIMULATOR
    bool isSimulator = true;
    #else
    bool isSimulator = false;
    #endif
    if (isSimulator) {
        // Ask where Xcode is installed
        std::string xcodePath = "/Applications/Xcode.app/Contents/Developer\n";

        // The result contains an end line character. Remove it.
        size_t pos = xcodePath.find("\n");
        xcodePath.erase(pos);

        char tempPath[PATH_MAX];
        sprintf(tempPath,
                "%s/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator%s.%s.sdk/System/Library/Frameworks/OpenGLES.framework",
                xcodePath.c_str(),
                [[sysVersionComponents objectAtIndex:0] cStringUsingEncoding:NSUTF8StringEncoding],
                [[sysVersionComponents objectAtIndex:1] cStringUsingEncoding:NSUTF8StringEncoding]);
        frameworkPath = CFStringCreateWithCString(kCFAllocatorDefault, tempPath, kCFStringEncodingUTF8);
    }

    g_es2BundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                   frameworkPath,
                                                   kCFURLPOSIXPathStyle, true);

    CFRelease(frameworkPath);

    g_es2Bundle = CFBundleCreate(kCFAllocatorDefault, g_es2BundleURL);

    return (g_es2Bundle != NULL);
}

static void *get_gles_proc(const char *proc) {
    void *res;

    CFStringRef procname = CFStringCreateWithCString(kCFAllocatorDefault, proc,
                                                     kCFStringEncodingASCII);
    res = CFBundleGetFunctionPointerForName(g_es2Bundle, procname);
    CFRelease(procname);
    return res;
}
#elif defined(__EMSCRIPTEN__)
static void open_libgl() {}
static void *get_gles_proc(const char *proc) {
    return (void *)eglGetProcAddress(proc);
}
#else
    #include <dlfcn.h>

static void *libegl = nullptr;
static void *libgles = nullptr;

static bool open_libgl(void) {
    libegl = dlopen("libEGL.so", RTLD_LAZY | RTLD_GLOBAL);
    libgles = dlopen("libGLESv2.so", RTLD_LAZY | RTLD_GLOBAL);
    return (libegl && libgles);
}

static void *get_egl_proc(const char *proc) {
    void *res = nullptr;
    if (eglGetProcAddress) res = eglGetProcAddress(proc);
    if (!res) res = dlsym(libegl, proc);
    return res;
}

static void *get_gles_proc(const char *proc) {
    void *res = eglGetProcAddress(proc);
    if (!res) res = dlsym(libgles, proc);
    return res;
}
#endif

static void load_procs(void);

bool gles2wInit() {
    if (!open_libgl()) {
        return 0;
    }

    load_procs();

    return 1;
}

PFNEGLGETPROCADDRESSPROC eglGetProcAddress;
PFNEGLCHOOSECONFIGPROC eglChooseConfig;
PFNEGLCREATECONTEXTPROC eglCreateContext;
PFNEGLCREATEWINDOWSURFACEPROC eglCreateWindowSurface;
PFNEGLDESTROYCONTEXTPROC eglDestroyContext;
PFNEGLDESTROYSURFACEPROC eglDestroySurface;
PFNEGLGETDISPLAYPROC eglGetDisplay;
PFNEGLGETERRORPROC eglGetError;
PFNEGLINITIALIZEPROC eglInitialize;
PFNEGLMAKECURRENTPROC eglMakeCurrent;
PFNEGLQUERYSTRINGPROC eglQueryString;
PFNEGLSWAPBUFFERSPROC eglSwapBuffers;
PFNEGLSWAPINTERVALPROC eglSwapInterval;
PFNEGLBINDAPIPROC eglBindAPI;

PFNGLACTIVETEXTUREPROC glActiveTexture;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
PFNGLBINDTEXTUREPROC glBindTexture;
PFNGLBLENDCOLORPROC glBlendColor;
PFNGLBLENDEQUATIONPROC glBlendEquation;
PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
PFNGLBLENDFUNCPROC glBlendFunc;
PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLBUFFERSUBDATAPROC glBufferSubData;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
PFNGLCLEARPROC glClear;
PFNGLCLEARCOLORPROC glClearColor;
PFNGLCLEARDEPTHFPROC glClearDepthf;
PFNGLCLEARSTENCILPROC glClearStencil;
PFNGLCOLORMASKPROC glColorMask;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;
PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLCULLFACEPROC glCullFace;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLDELETETEXTURESPROC glDeleteTextures;
PFNGLDEPTHFUNCPROC glDepthFunc;
PFNGLDEPTHMASKPROC glDepthMask;
PFNGLDEPTHRANGEFPROC glDepthRangef;
PFNGLDETACHSHADERPROC glDetachShader;
PFNGLDISABLEPROC glDisable;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
PFNGLDRAWARRAYSPROC glDrawArrays;
PFNGLDRAWELEMENTSPROC glDrawElements;
PFNGLENABLEPROC glEnable;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLFINISHPROC glFinish;
PFNGLFLUSHPROC glFlush;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
PFNGLFRONTFACEPROC glFrontFace;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
PFNGLGENTEXTURESPROC glGenTextures;
PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib;
PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform;
PFNGLGETATTACHEDSHADERSPROC glGetAttachedShaders;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
PFNGLGETBOOLEANVPROC glGetBooleanv;
PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
PFNGLGETERRORPROC glGetError;
PFNGLGETFLOATVPROC glGetFloatv;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv;
PFNGLGETINTEGERVPROC glGetIntegerv;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLGETSHADERPRECISIONFORMATPROC glGetShaderPrecisionFormat;
PFNGLGETSHADERSOURCEPROC glGetShaderSource;
PFNGLGETSTRINGPROC glGetString;
PFNGLGETTEXPARAMETERFVPROC glGetTexParameterfv;
PFNGLGETTEXPARAMETERIVPROC glGetTexParameteriv;
PFNGLGETUNIFORMFVPROC glGetUniformfv;
PFNGLGETUNIFORMIVPROC glGetUniformiv;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLGETVERTEXATTRIBFVPROC glGetVertexAttribfv;
PFNGLGETVERTEXATTRIBIVPROC glGetVertexAttribiv;
PFNGLGETVERTEXATTRIBPOINTERVPROC glGetVertexAttribPointerv;
PFNGLHINTPROC glHint;
PFNGLISBUFFERPROC glIsBuffer;
PFNGLISENABLEDPROC glIsEnabled;
PFNGLISFRAMEBUFFERPROC glIsFramebuffer;
PFNGLISPROGRAMPROC glIsProgram;
PFNGLISRENDERBUFFERPROC glIsRenderbuffer;
PFNGLISSHADERPROC glIsShader;
PFNGLISTEXTUREPROC glIsTexture;
PFNGLLINEWIDTHPROC glLineWidth;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLPIXELSTOREIPROC glPixelStorei;
PFNGLPOLYGONOFFSETPROC glPolygonOffset;
PFNGLREADPIXELSPROC glReadPixels;
PFNGLRELEASESHADERCOMPILERPROC glReleaseShaderCompiler;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
PFNGLSAMPLECOVERAGEPROC glSampleCoverage;
PFNGLSCISSORPROC glScissor;
PFNGLSHADERBINARYPROC glShaderBinary;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLSTENCILFUNCPROC glStencilFunc;
PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate;
PFNGLSTENCILMASKPROC glStencilMask;
PFNGLSTENCILMASKSEPARATEPROC glStencilMaskSeparate;
PFNGLSTENCILOPPROC glStencilOp;
PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate;
PFNGLTEXIMAGE2DPROC glTexImage2D;
PFNGLTEXPARAMETERFPROC glTexParameterf;
PFNGLTEXPARAMETERFVPROC glTexParameterfv;
PFNGLTEXPARAMETERIPROC glTexParameteri;
PFNGLTEXPARAMETERIVPROC glTexParameteriv;
PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM1FVPROC glUniform1fv;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLUNIFORM1IVPROC glUniform1iv;
PFNGLUNIFORM2FPROC glUniform2f;
PFNGLUNIFORM2FVPROC glUniform2fv;
PFNGLUNIFORM2IPROC glUniform2i;
PFNGLUNIFORM2IVPROC glUniform2iv;
PFNGLUNIFORM3FPROC glUniform3f;
PFNGLUNIFORM3FVPROC glUniform3fv;
PFNGLUNIFORM3IPROC glUniform3i;
PFNGLUNIFORM3IVPROC glUniform3iv;
PFNGLUNIFORM4FPROC glUniform4f;
PFNGLUNIFORM4FVPROC glUniform4fv;
PFNGLUNIFORM4IPROC glUniform4i;
PFNGLUNIFORM4IVPROC glUniform4iv;
PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;
PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLVALIDATEPROGRAMPROC glValidateProgram;
PFNGLVERTEXATTRIB1FPROC glVertexAttrib1f;
PFNGLVERTEXATTRIB1FVPROC glVertexAttrib1fv;
PFNGLVERTEXATTRIB2FPROC glVertexAttrib2f;
PFNGLVERTEXATTRIB2FVPROC glVertexAttrib2fv;
PFNGLVERTEXATTRIB3FPROC glVertexAttrib3f;
PFNGLVERTEXATTRIB3FVPROC glVertexAttrib3fv;
PFNGLVERTEXATTRIB4FPROC glVertexAttrib4f;
PFNGLVERTEXATTRIB4FVPROC glVertexAttrib4fv;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLVIEWPORTPROC glViewport;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES;
PFNGLGETPROGRAMBINARYOESPROC glGetProgramBinaryOES;
PFNGLPROGRAMBINARYOESPROC glProgramBinaryOES;
PFNGLMAPBUFFEROESPROC glMapBufferOES;
PFNGLUNMAPBUFFEROESPROC glUnmapBufferOES;
PFNGLGETBUFFERPOINTERVOESPROC glGetBufferPointervOES;
PFNGLTEXIMAGE3DOESPROC glTexImage3DOES;
PFNGLTEXSUBIMAGE3DOESPROC glTexSubImage3DOES;
PFNGLCOPYTEXSUBIMAGE3DOESPROC glCopyTexSubImage3DOES;
PFNGLCOMPRESSEDTEXIMAGE3DOESPROC glCompressedTexImage3DOES;
PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC glCompressedTexSubImage3DOES;
PFNGLFRAMEBUFFERTEXTURE3DOESPROC glFramebufferTexture3DOES;
PFNGLBINDVERTEXARRAYOESPROC glBindVertexArrayOES;
PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArraysOES;
PFNGLGENVERTEXARRAYSOESPROC glGenVertexArraysOES;
PFNGLISVERTEXARRAYOESPROC glIsVertexArrayOES;
PFNGLDEBUGMESSAGECONTROLKHRPROC glDebugMessageControlKHR;
PFNGLDEBUGMESSAGEINSERTKHRPROC glDebugMessageInsertKHR;
PFNGLDEBUGMESSAGECALLBACKKHRPROC glDebugMessageCallbackKHR;
PFNGLGETDEBUGMESSAGELOGKHRPROC glGetDebugMessageLogKHR;
PFNGLPUSHDEBUGGROUPKHRPROC glPushDebugGroupKHR;
PFNGLPOPDEBUGGROUPKHRPROC glPopDebugGroupKHR;
PFNGLOBJECTLABELKHRPROC glObjectLabelKHR;
PFNGLGETOBJECTLABELKHRPROC glGetObjectLabelKHR;
PFNGLOBJECTPTRLABELKHRPROC glObjectPtrLabelKHR;
PFNGLGETOBJECTPTRLABELKHRPROC glGetObjectPtrLabelKHR;
PFNGLGETPOINTERVKHRPROC glGetPointervKHR;
PFNGLGETPERFMONITORGROUPSAMDPROC glGetPerfMonitorGroupsAMD;
PFNGLGETPERFMONITORCOUNTERSAMDPROC glGetPerfMonitorCountersAMD;
PFNGLGETPERFMONITORGROUPSTRINGAMDPROC glGetPerfMonitorGroupStringAMD;
PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC glGetPerfMonitorCounterStringAMD;
PFNGLGETPERFMONITORCOUNTERINFOAMDPROC glGetPerfMonitorCounterInfoAMD;
PFNGLGENPERFMONITORSAMDPROC glGenPerfMonitorsAMD;
PFNGLDELETEPERFMONITORSAMDPROC glDeletePerfMonitorsAMD;
PFNGLSELECTPERFMONITORCOUNTERSAMDPROC glSelectPerfMonitorCountersAMD;
PFNGLBEGINPERFMONITORAMDPROC glBeginPerfMonitorAMD;
PFNGLENDPERFMONITORAMDPROC glEndPerfMonitorAMD;
PFNGLGETPERFMONITORCOUNTERDATAAMDPROC glGetPerfMonitorCounterDataAMD;
PFNGLBLITFRAMEBUFFERANGLEPROC glBlitFramebufferANGLE;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC glRenderbufferStorageMultisampleANGLE;
PFNGLDRAWARRAYSINSTANCEDANGLEPROC glDrawArraysInstancedANGLE;
PFNGLDRAWELEMENTSINSTANCEDANGLEPROC glDrawElementsInstancedANGLE;
PFNGLVERTEXATTRIBDIVISORANGLEPROC glVertexAttribDivisorANGLE;
PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC glGetTranslatedShaderSourceANGLE;
PFNGLDRAWARRAYSINSTANCEDEXTPROC glDrawArraysInstancedEXT;
PFNGLDRAWELEMENTSINSTANCEDEXTPROC glDrawElementsInstancedEXT;
PFNGLVERTEXATTRIBDIVISOREXTPROC glVertexAttribDivisorEXT;
PFNGLCOPYTEXTURELEVELSAPPLEPROC glCopyTextureLevelsAPPLE;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC glRenderbufferStorageMultisampleAPPLE;
PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC glResolveMultisampleFramebufferAPPLE;
PFNGLFENCESYNCAPPLEPROC glFenceSyncAPPLE;
PFNGLISSYNCAPPLEPROC glIsSyncAPPLE;
PFNGLDELETESYNCAPPLEPROC glDeleteSyncAPPLE;
PFNGLCLIENTWAITSYNCAPPLEPROC glClientWaitSyncAPPLE;
PFNGLWAITSYNCAPPLEPROC glWaitSyncAPPLE;
PFNGLGETINTEGER64VAPPLEPROC glGetInteger64vAPPLE;
PFNGLGETSYNCIVAPPLEPROC glGetSyncivAPPLE;
PFNGLLABELOBJECTEXTPROC glLabelObjectEXT;
PFNGLGETOBJECTLABELEXTPROC glGetObjectLabelEXT;
PFNGLINSERTEVENTMARKEREXTPROC glInsertEventMarkerEXT;
PFNGLPUSHGROUPMARKEREXTPROC glPushGroupMarkerEXT;
PFNGLPOPGROUPMARKEREXTPROC glPopGroupMarkerEXT;
PFNGLDISCARDFRAMEBUFFEREXTPROC glDiscardFramebufferEXT;
PFNGLGENQUERIESEXTPROC glGenQueriesEXT;
PFNGLDELETEQUERIESEXTPROC glDeleteQueriesEXT;
PFNGLISQUERYEXTPROC glIsQueryEXT;
PFNGLBEGINQUERYEXTPROC glBeginQueryEXT;
PFNGLENDQUERYEXTPROC glEndQueryEXT;
PFNGLQUERYCOUNTEREXTPROC glQueryCounterEXT;
PFNGLGETQUERYIVEXTPROC glGetQueryivEXT;
PFNGLGETQUERYOBJECTIVEXTPROC glGetQueryObjectivEXT;
PFNGLGETQUERYOBJECTUIVEXTPROC glGetQueryObjectuivEXT;
PFNGLGETQUERYOBJECTI64VEXTPROC glGetQueryObjecti64vEXT;
PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT;
PFNGLMAPBUFFERRANGEEXTPROC glMapBufferRangeEXT;
PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC glFlushMappedBufferRangeEXT;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT;
PFNGLREADBUFFERINDEXEDEXTPROC glReadBufferIndexedEXT;
PFNGLDRAWBUFFERSINDEXEDEXTPROC glDrawBuffersIndexedEXT;
PFNGLGETINTEGERI_VEXTPROC glGetIntegeri_vEXT;
PFNGLMULTIDRAWARRAYSEXTPROC glMultiDrawArraysEXT;
PFNGLMULTIDRAWELEMENTSEXTPROC glMultiDrawElementsEXT;
PFNGLGETGRAPHICSRESETSTATUSEXTPROC glGetGraphicsResetStatusEXT;
PFNGLREADNPIXELSEXTPROC glReadnPixelsEXT;
PFNGLGETNUNIFORMFVEXTPROC glGetnUniformfvEXT;
PFNGLGETNUNIFORMIVEXTPROC glGetnUniformivEXT;
PFNGLUSEPROGRAMSTAGESEXTPROC glUseProgramStagesEXT;
PFNGLACTIVESHADERPROGRAMEXTPROC glActiveShaderProgramEXT;
PFNGLCREATESHADERPROGRAMVEXTPROC glCreateShaderProgramvEXT;
PFNGLBINDPROGRAMPIPELINEEXTPROC glBindProgramPipelineEXT;
PFNGLDELETEPROGRAMPIPELINESEXTPROC glDeleteProgramPipelinesEXT;
PFNGLGENPROGRAMPIPELINESEXTPROC glGenProgramPipelinesEXT;
PFNGLISPROGRAMPIPELINEEXTPROC glIsProgramPipelineEXT;
PFNGLPROGRAMPARAMETERIEXTPROC glProgramParameteriEXT;
PFNGLGETPROGRAMPIPELINEIVEXTPROC glGetProgramPipelineivEXT;
PFNGLPROGRAMUNIFORM1IEXTPROC glProgramUniform1iEXT;
PFNGLPROGRAMUNIFORM2IEXTPROC glProgramUniform2iEXT;
PFNGLPROGRAMUNIFORM3IEXTPROC glProgramUniform3iEXT;
PFNGLPROGRAMUNIFORM4IEXTPROC glProgramUniform4iEXT;
PFNGLPROGRAMUNIFORM1FEXTPROC glProgramUniform1fEXT;
PFNGLPROGRAMUNIFORM2FEXTPROC glProgramUniform2fEXT;
PFNGLPROGRAMUNIFORM3FEXTPROC glProgramUniform3fEXT;
PFNGLPROGRAMUNIFORM4FEXTPROC glProgramUniform4fEXT;
PFNGLPROGRAMUNIFORM1IVEXTPROC glProgramUniform1ivEXT;
PFNGLPROGRAMUNIFORM2IVEXTPROC glProgramUniform2ivEXT;
PFNGLPROGRAMUNIFORM3IVEXTPROC glProgramUniform3ivEXT;
PFNGLPROGRAMUNIFORM4IVEXTPROC glProgramUniform4ivEXT;
PFNGLPROGRAMUNIFORM1FVEXTPROC glProgramUniform1fvEXT;
PFNGLPROGRAMUNIFORM2FVEXTPROC glProgramUniform2fvEXT;
PFNGLPROGRAMUNIFORM3FVEXTPROC glProgramUniform3fvEXT;
PFNGLPROGRAMUNIFORM4FVEXTPROC glProgramUniform4fvEXT;
PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC glProgramUniformMatrix2fvEXT;
PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC glProgramUniformMatrix3fvEXT;
PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC glProgramUniformMatrix4fvEXT;
PFNGLVALIDATEPROGRAMPIPELINEEXTPROC glValidateProgramPipelineEXT;
PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC glGetProgramPipelineInfoLogEXT;
PFNGLTEXSTORAGE1DEXTPROC glTexStorage1DEXT;
PFNGLTEXSTORAGE2DEXTPROC glTexStorage2DEXT;
PFNGLTEXSTORAGE3DEXTPROC glTexStorage3DEXT;
PFNGLTEXTURESTORAGE1DEXTPROC glTextureStorage1DEXT;
PFNGLTEXTURESTORAGE2DEXTPROC glTextureStorage2DEXT;
PFNGLTEXTURESTORAGE3DEXTPROC glTextureStorage3DEXT;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC glRenderbufferStorageMultisampleIMG;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC glFramebufferTexture2DMultisampleIMG;
PFNGLCOVERAGEMASKNVPROC glCoverageMaskNV;
PFNGLCOVERAGEOPERATIONNVPROC glCoverageOperationNV;
PFNGLDRAWBUFFERSNVPROC glDrawBuffersNV;
PFNGLDRAWARRAYSINSTANCEDNVPROC glDrawArraysInstancedNV;
PFNGLDRAWELEMENTSINSTANCEDNVPROC glDrawElementsInstancedNV;
PFNGLDELETEFENCESNVPROC glDeleteFencesNV;
PFNGLGENFENCESNVPROC glGenFencesNV;
PFNGLISFENCENVPROC glIsFenceNV;
PFNGLTESTFENCENVPROC glTestFenceNV;
PFNGLGETFENCEIVNVPROC glGetFenceivNV;
PFNGLFINISHFENCENVPROC glFinishFenceNV;
PFNGLSETFENCENVPROC glSetFenceNV;
PFNGLBLITFRAMEBUFFERNVPROC glBlitFramebufferNV;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC glRenderbufferStorageMultisampleNV;
PFNGLVERTEXATTRIBDIVISORNVPROC glVertexAttribDivisorNV;
PFNGLREADBUFFERNVPROC glReadBufferNV;
PFNGLALPHAFUNCQCOMPROC glAlphaFuncQCOM;
PFNGLGETDRIVERCONTROLSQCOMPROC glGetDriverControlsQCOM;
PFNGLGETDRIVERCONTROLSTRINGQCOMPROC glGetDriverControlStringQCOM;
PFNGLENABLEDRIVERCONTROLQCOMPROC glEnableDriverControlQCOM;
PFNGLDISABLEDRIVERCONTROLQCOMPROC glDisableDriverControlQCOM;
PFNGLEXTGETTEXTURESQCOMPROC glExtGetTexturesQCOM;
PFNGLEXTGETBUFFERSQCOMPROC glExtGetBuffersQCOM;
PFNGLEXTGETRENDERBUFFERSQCOMPROC glExtGetRenderbuffersQCOM;
PFNGLEXTGETFRAMEBUFFERSQCOMPROC glExtGetFramebuffersQCOM;
PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC glExtGetTexLevelParameterivQCOM;
PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC glExtTexObjectStateOverrideiQCOM;
PFNGLEXTGETTEXSUBIMAGEQCOMPROC glExtGetTexSubImageQCOM;
PFNGLEXTGETBUFFERPOINTERVQCOMPROC glExtGetBufferPointervQCOM;
PFNGLEXTGETSHADERSQCOMPROC glExtGetShadersQCOM;
PFNGLEXTGETPROGRAMSQCOMPROC glExtGetProgramsQCOM;
PFNGLEXTISPROGRAMBINARYQCOMPROC glExtIsProgramBinaryQCOM;
PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC glExtGetProgramBinarySourceQCOM;
PFNGLSTARTTILINGQCOMPROC glStartTilingQCOM;
PFNGLENDTILINGQCOMPROC glEndTilingQCOM;

static void load_procs(void) {
    eglGetProcAddress = (PFNEGLGETPROCADDRESSPROC)get_egl_proc("eglGetProcAddress");
    eglChooseConfig = (PFNEGLCHOOSECONFIGPROC)get_egl_proc("eglChooseConfig");
    eglCreateContext = (PFNEGLCREATECONTEXTPROC)get_egl_proc("eglCreateContext");
    eglCreateWindowSurface = (PFNEGLCREATEWINDOWSURFACEPROC)get_egl_proc("eglCreateWindowSurface");
    eglDestroyContext = (PFNEGLDESTROYCONTEXTPROC)get_egl_proc("eglDestroyContext");
    eglDestroySurface = (PFNEGLDESTROYSURFACEPROC)get_egl_proc("eglDestroySurface");
    eglGetDisplay = (PFNEGLGETDISPLAYPROC)get_egl_proc("eglGetDisplay");
    eglGetError = (PFNEGLGETERRORPROC)get_egl_proc("eglGetError");
    eglInitialize = (PFNEGLINITIALIZEPROC)get_egl_proc("eglInitialize");
    eglMakeCurrent = (PFNEGLMAKECURRENTPROC)get_egl_proc("eglMakeCurrent");
    eglQueryString = (PFNEGLQUERYSTRINGPROC)get_egl_proc("eglQueryString");
    eglSwapBuffers = (PFNEGLSWAPBUFFERSPROC)get_egl_proc("eglSwapBuffers");
    eglSwapInterval = (PFNEGLSWAPINTERVALPROC)get_egl_proc("eglSwapInterval");
    eglBindAPI = (PFNEGLBINDAPIPROC)get_egl_proc("eglBindAPI");

    glActiveTexture = (PFNGLACTIVETEXTUREPROC)get_gles_proc("glActiveTexture");
    glAttachShader = (PFNGLATTACHSHADERPROC)get_gles_proc("glAttachShader");
    glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)get_gles_proc("glBindAttribLocation");
    glBindBuffer = (PFNGLBINDBUFFERPROC)get_gles_proc("glBindBuffer");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)get_gles_proc("glBindFramebuffer");
    glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)get_gles_proc("glBindRenderbuffer");
    glBindTexture = (PFNGLBINDTEXTUREPROC)get_gles_proc("glBindTexture");
    glBlendColor = (PFNGLBLENDCOLORPROC)get_gles_proc("glBlendColor");
    glBlendEquation = (PFNGLBLENDEQUATIONPROC)get_gles_proc("glBlendEquation");
    glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC)get_gles_proc("glBlendEquationSeparate");
    glBlendFunc = (PFNGLBLENDFUNCPROC)get_gles_proc("glBlendFunc");
    glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)get_gles_proc("glBlendFuncSeparate");
    glBufferData = (PFNGLBUFFERDATAPROC)get_gles_proc("glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)get_gles_proc("glBufferSubData");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)get_gles_proc("glCheckFramebufferStatus");
    glClear = (PFNGLCLEARPROC)get_gles_proc("glClear");
    glClearColor = (PFNGLCLEARCOLORPROC)get_gles_proc("glClearColor");
    glClearDepthf = (PFNGLCLEARDEPTHFPROC)get_gles_proc("glClearDepthf");
    glClearStencil = (PFNGLCLEARSTENCILPROC)get_gles_proc("glClearStencil");
    glColorMask = (PFNGLCOLORMASKPROC)get_gles_proc("glColorMask");
    glCompileShader = (PFNGLCOMPILESHADERPROC)get_gles_proc("glCompileShader");
    glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)get_gles_proc("glCompressedTexImage2D");
    glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)get_gles_proc("glCompressedTexSubImage2D");
    glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC)get_gles_proc("glCopyTexImage2D");
    glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC)get_gles_proc("glCopyTexSubImage2D");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_gles_proc("glCreateProgram");
    glCreateShader = (PFNGLCREATESHADERPROC)get_gles_proc("glCreateShader");
    glCullFace = (PFNGLCULLFACEPROC)get_gles_proc("glCullFace");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)get_gles_proc("glDeleteBuffers");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)get_gles_proc("glDeleteFramebuffers");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_gles_proc("glDeleteProgram");
    glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)get_gles_proc("glDeleteRenderbuffers");
    glDeleteShader = (PFNGLDELETESHADERPROC)get_gles_proc("glDeleteShader");
    glDeleteTextures = (PFNGLDELETETEXTURESPROC)get_gles_proc("glDeleteTextures");
    glDepthFunc = (PFNGLDEPTHFUNCPROC)get_gles_proc("glDepthFunc");
    glDepthMask = (PFNGLDEPTHMASKPROC)get_gles_proc("glDepthMask");
    glDepthRangef = (PFNGLDEPTHRANGEFPROC)get_gles_proc("glDepthRangef");
    glDetachShader = (PFNGLDETACHSHADERPROC)get_gles_proc("glDetachShader");
    glDisable = (PFNGLDISABLEPROC)get_gles_proc("glDisable");
    glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)get_gles_proc("glDisableVertexAttribArray");
    glDrawArrays = (PFNGLDRAWARRAYSPROC)get_gles_proc("glDrawArrays");
    glDrawElements = (PFNGLDRAWELEMENTSPROC)get_gles_proc("glDrawElements");
    glEnable = (PFNGLENABLEPROC)get_gles_proc("glEnable");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_gles_proc("glEnableVertexAttribArray");
    glFinish = (PFNGLFINISHPROC)get_gles_proc("glFinish");
    glFlush = (PFNGLFLUSHPROC)get_gles_proc("glFlush");
    glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)get_gles_proc("glFramebufferRenderbuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)get_gles_proc("glFramebufferTexture2D");
    glFrontFace = (PFNGLFRONTFACEPROC)get_gles_proc("glFrontFace");
    glGenBuffers = (PFNGLGENBUFFERSPROC)get_gles_proc("glGenBuffers");
    glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)get_gles_proc("glGenerateMipmap");
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)get_gles_proc("glGenFramebuffers");
    glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)get_gles_proc("glGenRenderbuffers");
    glGenTextures = (PFNGLGENTEXTURESPROC)get_gles_proc("glGenTextures");
    glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC)get_gles_proc("glGetActiveAttrib");
    glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC)get_gles_proc("glGetActiveUniform");
    glGetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC)get_gles_proc("glGetAttachedShaders");
    glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)get_gles_proc("glGetAttribLocation");
    glGetBooleanv = (PFNGLGETBOOLEANVPROC)get_gles_proc("glGetBooleanv");
    glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC)get_gles_proc("glGetBufferParameteriv");
    glGetError = (PFNGLGETERRORPROC)get_gles_proc("glGetError");
    glGetFloatv = (PFNGLGETFLOATVPROC)get_gles_proc("glGetFloatv");
    glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)get_gles_proc("glGetFramebufferAttachmentParameteriv");
    glGetIntegerv = (PFNGLGETINTEGERVPROC)get_gles_proc("glGetIntegerv");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_gles_proc("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_gles_proc("glGetProgramInfoLog");
    glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)get_gles_proc("glGetRenderbufferParameteriv");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)get_gles_proc("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_gles_proc("glGetShaderInfoLog");
    glGetShaderPrecisionFormat = (PFNGLGETSHADERPRECISIONFORMATPROC)get_gles_proc("glGetShaderPrecisionFormat");
    glGetShaderSource = (PFNGLGETSHADERSOURCEPROC)get_gles_proc("glGetShaderSource");
    glGetString = (PFNGLGETSTRINGPROC)get_gles_proc("glGetString");
    glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC)get_gles_proc("glGetTexParameterfv");
    glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC)get_gles_proc("glGetTexParameteriv");
    glGetUniformfv = (PFNGLGETUNIFORMFVPROC)get_gles_proc("glGetUniformfv");
    glGetUniformiv = (PFNGLGETUNIFORMIVPROC)get_gles_proc("glGetUniformiv");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)get_gles_proc("glGetUniformLocation");
    glGetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC)get_gles_proc("glGetVertexAttribfv");
    glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)get_gles_proc("glGetVertexAttribiv");
    glGetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC)get_gles_proc("glGetVertexAttribPointerv");
    glHint = (PFNGLHINTPROC)get_gles_proc("glHint");
    glIsBuffer = (PFNGLISBUFFERPROC)get_gles_proc("glIsBuffer");
    glIsEnabled = (PFNGLISENABLEDPROC)get_gles_proc("glIsEnabled");
    glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC)get_gles_proc("glIsFramebuffer");
    glIsProgram = (PFNGLISPROGRAMPROC)get_gles_proc("glIsProgram");
    glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC)get_gles_proc("glIsRenderbuffer");
    glIsShader = (PFNGLISSHADERPROC)get_gles_proc("glIsShader");
    glIsTexture = (PFNGLISTEXTUREPROC)get_gles_proc("glIsTexture");
    glLineWidth = (PFNGLLINEWIDTHPROC)get_gles_proc("glLineWidth");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)get_gles_proc("glLinkProgram");
    glPixelStorei = (PFNGLPIXELSTOREIPROC)get_gles_proc("glPixelStorei");
    glPolygonOffset = (PFNGLPOLYGONOFFSETPROC)get_gles_proc("glPolygonOffset");
    glReadPixels = (PFNGLREADPIXELSPROC)get_gles_proc("glReadPixels");
    glReleaseShaderCompiler = (PFNGLRELEASESHADERCOMPILERPROC)get_gles_proc("glReleaseShaderCompiler");
    glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)get_gles_proc("glRenderbufferStorage");
    glSampleCoverage = (PFNGLSAMPLECOVERAGEPROC)get_gles_proc("glSampleCoverage");
    glScissor = (PFNGLSCISSORPROC)get_gles_proc("glScissor");
    glShaderBinary = (PFNGLSHADERBINARYPROC)get_gles_proc("glShaderBinary");
    glShaderSource = (PFNGLSHADERSOURCEPROC)get_gles_proc("glShaderSource");
    glStencilFunc = (PFNGLSTENCILFUNCPROC)get_gles_proc("glStencilFunc");
    glStencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC)get_gles_proc("glStencilFuncSeparate");
    glStencilMask = (PFNGLSTENCILMASKPROC)get_gles_proc("glStencilMask");
    glStencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC)get_gles_proc("glStencilMaskSeparate");
    glStencilOp = (PFNGLSTENCILOPPROC)get_gles_proc("glStencilOp");
    glStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC)get_gles_proc("glStencilOpSeparate");
    glTexImage2D = (PFNGLTEXIMAGE2DPROC)get_gles_proc("glTexImage2D");
    glTexParameterf = (PFNGLTEXPARAMETERFPROC)get_gles_proc("glTexParameterf");
    glTexParameterfv = (PFNGLTEXPARAMETERFVPROC)get_gles_proc("glTexParameterfv");
    glTexParameteri = (PFNGLTEXPARAMETERIPROC)get_gles_proc("glTexParameteri");
    glTexParameteriv = (PFNGLTEXPARAMETERIVPROC)get_gles_proc("glTexParameteriv");
    glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)get_gles_proc("glTexSubImage2D");
    glUniform1f = (PFNGLUNIFORM1FPROC)get_gles_proc("glUniform1f");
    glUniform1fv = (PFNGLUNIFORM1FVPROC)get_gles_proc("glUniform1fv");
    glUniform1i = (PFNGLUNIFORM1IPROC)get_gles_proc("glUniform1i");
    glUniform1iv = (PFNGLUNIFORM1IVPROC)get_gles_proc("glUniform1iv");
    glUniform2f = (PFNGLUNIFORM2FPROC)get_gles_proc("glUniform2f");
    glUniform2fv = (PFNGLUNIFORM2FVPROC)get_gles_proc("glUniform2fv");
    glUniform2i = (PFNGLUNIFORM2IPROC)get_gles_proc("glUniform2i");
    glUniform2iv = (PFNGLUNIFORM2IVPROC)get_gles_proc("glUniform2iv");
    glUniform3f = (PFNGLUNIFORM3FPROC)get_gles_proc("glUniform3f");
    glUniform3fv = (PFNGLUNIFORM3FVPROC)get_gles_proc("glUniform3fv");
    glUniform3i = (PFNGLUNIFORM3IPROC)get_gles_proc("glUniform3i");
    glUniform3iv = (PFNGLUNIFORM3IVPROC)get_gles_proc("glUniform3iv");
    glUniform4f = (PFNGLUNIFORM4FPROC)get_gles_proc("glUniform4f");
    glUniform4fv = (PFNGLUNIFORM4FVPROC)get_gles_proc("glUniform4fv");
    glUniform4i = (PFNGLUNIFORM4IPROC)get_gles_proc("glUniform4i");
    glUniform4iv = (PFNGLUNIFORM4IVPROC)get_gles_proc("glUniform4iv");
    glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC)get_gles_proc("glUniformMatrix2fv");
    glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)get_gles_proc("glUniformMatrix3fv");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)get_gles_proc("glUniformMatrix4fv");
    glUseProgram = (PFNGLUSEPROGRAMPROC)get_gles_proc("glUseProgram");
    glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)get_gles_proc("glValidateProgram");
    glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)get_gles_proc("glVertexAttrib1f");
    glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC)get_gles_proc("glVertexAttrib1fv");
    glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC)get_gles_proc("glVertexAttrib2f");
    glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC)get_gles_proc("glVertexAttrib2fv");
    glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC)get_gles_proc("glVertexAttrib3f");
    glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC)get_gles_proc("glVertexAttrib3fv");
    glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC)get_gles_proc("glVertexAttrib4f");
    glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)get_gles_proc("glVertexAttrib4fv");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)get_gles_proc("glVertexAttribPointer");
    glViewport = (PFNGLVIEWPORTPROC)get_gles_proc("glViewport");
    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)get_gles_proc("glEGLImageTargetTexture2DOES");
    glEGLImageTargetRenderbufferStorageOES = (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC)get_gles_proc("glEGLImageTargetRenderbufferStorageOES");
    glGetProgramBinaryOES = (PFNGLGETPROGRAMBINARYOESPROC)get_gles_proc("glGetProgramBinaryOES");
    glProgramBinaryOES = (PFNGLPROGRAMBINARYOESPROC)get_gles_proc("glProgramBinaryOES");
    glMapBufferOES = (PFNGLMAPBUFFEROESPROC)get_gles_proc("glMapBufferOES");
    glUnmapBufferOES = (PFNGLUNMAPBUFFEROESPROC)get_gles_proc("glUnmapBufferOES");
    glGetBufferPointervOES = (PFNGLGETBUFFERPOINTERVOESPROC)get_gles_proc("glGetBufferPointervOES");
    glTexImage3DOES = (PFNGLTEXIMAGE3DOESPROC)get_gles_proc("glTexImage3DOES");
    glTexSubImage3DOES = (PFNGLTEXSUBIMAGE3DOESPROC)get_gles_proc("glTexSubImage3DOES");
    glCopyTexSubImage3DOES = (PFNGLCOPYTEXSUBIMAGE3DOESPROC)get_gles_proc("glCopyTexSubImage3DOES");
    glCompressedTexImage3DOES = (PFNGLCOMPRESSEDTEXIMAGE3DOESPROC)get_gles_proc("glCompressedTexImage3DOES");
    glCompressedTexSubImage3DOES = (PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC)get_gles_proc("glCompressedTexSubImage3DOES");
    glFramebufferTexture3DOES = (PFNGLFRAMEBUFFERTEXTURE3DOESPROC)get_gles_proc("glFramebufferTexture3DOES");
    glBindVertexArrayOES = (PFNGLBINDVERTEXARRAYOESPROC)get_gles_proc("glBindVertexArrayOES");
    glDeleteVertexArraysOES = (PFNGLDELETEVERTEXARRAYSOESPROC)get_gles_proc("glDeleteVertexArraysOES");
    glGenVertexArraysOES = (PFNGLGENVERTEXARRAYSOESPROC)get_gles_proc("glGenVertexArraysOES");
    glIsVertexArrayOES = (PFNGLISVERTEXARRAYOESPROC)get_gles_proc("glIsVertexArrayOES");
    glDebugMessageControlKHR = (PFNGLDEBUGMESSAGECONTROLKHRPROC)get_gles_proc("glDebugMessageControlKHR");
    glDebugMessageInsertKHR = (PFNGLDEBUGMESSAGEINSERTKHRPROC)get_gles_proc("glDebugMessageInsertKHR");
    glDebugMessageCallbackKHR = (PFNGLDEBUGMESSAGECALLBACKKHRPROC)get_gles_proc("glDebugMessageCallbackKHR");
    glGetDebugMessageLogKHR = (PFNGLGETDEBUGMESSAGELOGKHRPROC)get_gles_proc("glGetDebugMessageLogKHR");
    glPushDebugGroupKHR = (PFNGLPUSHDEBUGGROUPKHRPROC)get_gles_proc("glPushDebugGroupKHR");
    glPopDebugGroupKHR = (PFNGLPOPDEBUGGROUPKHRPROC)get_gles_proc("glPopDebugGroupKHR");
    glObjectLabelKHR = (PFNGLOBJECTLABELKHRPROC)get_gles_proc("glObjectLabelKHR");
    glGetObjectLabelKHR = (PFNGLGETOBJECTLABELKHRPROC)get_gles_proc("glGetObjectLabelKHR");
    glObjectPtrLabelKHR = (PFNGLOBJECTPTRLABELKHRPROC)get_gles_proc("glObjectPtrLabelKHR");
    glGetObjectPtrLabelKHR = (PFNGLGETOBJECTPTRLABELKHRPROC)get_gles_proc("glGetObjectPtrLabelKHR");
    glGetPointervKHR = (PFNGLGETPOINTERVKHRPROC)get_gles_proc("glGetPointervKHR");
    glGetPerfMonitorGroupsAMD = (PFNGLGETPERFMONITORGROUPSAMDPROC)get_gles_proc("glGetPerfMonitorGroupsAMD");
    glGetPerfMonitorCountersAMD = (PFNGLGETPERFMONITORCOUNTERSAMDPROC)get_gles_proc("glGetPerfMonitorCountersAMD");
    glGetPerfMonitorGroupStringAMD = (PFNGLGETPERFMONITORGROUPSTRINGAMDPROC)get_gles_proc("glGetPerfMonitorGroupStringAMD");
    glGetPerfMonitorCounterStringAMD = (PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC)get_gles_proc("glGetPerfMonitorCounterStringAMD");
    glGetPerfMonitorCounterInfoAMD = (PFNGLGETPERFMONITORCOUNTERINFOAMDPROC)get_gles_proc("glGetPerfMonitorCounterInfoAMD");
    glGenPerfMonitorsAMD = (PFNGLGENPERFMONITORSAMDPROC)get_gles_proc("glGenPerfMonitorsAMD");
    glDeletePerfMonitorsAMD = (PFNGLDELETEPERFMONITORSAMDPROC)get_gles_proc("glDeletePerfMonitorsAMD");
    glSelectPerfMonitorCountersAMD = (PFNGLSELECTPERFMONITORCOUNTERSAMDPROC)get_gles_proc("glSelectPerfMonitorCountersAMD");
    glBeginPerfMonitorAMD = (PFNGLBEGINPERFMONITORAMDPROC)get_gles_proc("glBeginPerfMonitorAMD");
    glEndPerfMonitorAMD = (PFNGLENDPERFMONITORAMDPROC)get_gles_proc("glEndPerfMonitorAMD");
    glGetPerfMonitorCounterDataAMD = (PFNGLGETPERFMONITORCOUNTERDATAAMDPROC)get_gles_proc("glGetPerfMonitorCounterDataAMD");
    glBlitFramebufferANGLE = (PFNGLBLITFRAMEBUFFERANGLEPROC)get_gles_proc("glBlitFramebufferANGLE");
    glRenderbufferStorageMultisampleANGLE = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC)get_gles_proc("glRenderbufferStorageMultisampleANGLE");
    glDrawArraysInstancedANGLE = (PFNGLDRAWARRAYSINSTANCEDANGLEPROC)get_gles_proc("glDrawArraysInstancedANGLE");
    glDrawElementsInstancedANGLE = (PFNGLDRAWELEMENTSINSTANCEDANGLEPROC)get_gles_proc("glDrawElementsInstancedANGLE");
    glVertexAttribDivisorANGLE = (PFNGLVERTEXATTRIBDIVISORANGLEPROC)get_gles_proc("glVertexAttribDivisorANGLE");
    glGetTranslatedShaderSourceANGLE = (PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC)get_gles_proc("glGetTranslatedShaderSourceANGLE");
    glDrawArraysInstancedEXT = (PFNGLDRAWARRAYSINSTANCEDEXTPROC)get_gles_proc("glDrawArraysInstancedEXT");
    glDrawElementsInstancedEXT = (PFNGLDRAWELEMENTSINSTANCEDEXTPROC)get_gles_proc("glDrawElementsInstancedEXT");
    glVertexAttribDivisorEXT = (PFNGLVERTEXATTRIBDIVISOREXTPROC)get_gles_proc("glVertexAttribDivisorEXT");
    glCopyTextureLevelsAPPLE = (PFNGLCOPYTEXTURELEVELSAPPLEPROC)get_gles_proc("glCopyTextureLevelsAPPLE");
    glRenderbufferStorageMultisampleAPPLE = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC)get_gles_proc("glRenderbufferStorageMultisampleAPPLE");
    glResolveMultisampleFramebufferAPPLE = (PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC)get_gles_proc("glResolveMultisampleFramebufferAPPLE");
    glFenceSyncAPPLE = (PFNGLFENCESYNCAPPLEPROC)get_gles_proc("glFenceSyncAPPLE");
    glIsSyncAPPLE = (PFNGLISSYNCAPPLEPROC)get_gles_proc("glIsSyncAPPLE");
    glDeleteSyncAPPLE = (PFNGLDELETESYNCAPPLEPROC)get_gles_proc("glDeleteSyncAPPLE");
    glClientWaitSyncAPPLE = (PFNGLCLIENTWAITSYNCAPPLEPROC)get_gles_proc("glClientWaitSyncAPPLE");
    glWaitSyncAPPLE = (PFNGLWAITSYNCAPPLEPROC)get_gles_proc("glWaitSyncAPPLE");
    glGetInteger64vAPPLE = (PFNGLGETINTEGER64VAPPLEPROC)get_gles_proc("glGetInteger64vAPPLE");
    glGetSyncivAPPLE = (PFNGLGETSYNCIVAPPLEPROC)get_gles_proc("glGetSyncivAPPLE");
    glLabelObjectEXT = (PFNGLLABELOBJECTEXTPROC)get_gles_proc("glLabelObjectEXT");
    glGetObjectLabelEXT = (PFNGLGETOBJECTLABELEXTPROC)get_gles_proc("glGetObjectLabelEXT");
    glInsertEventMarkerEXT = (PFNGLINSERTEVENTMARKEREXTPROC)get_gles_proc("glInsertEventMarkerEXT");
    glPushGroupMarkerEXT = (PFNGLPUSHGROUPMARKEREXTPROC)get_gles_proc("glPushGroupMarkerEXT");
    glPopGroupMarkerEXT = (PFNGLPOPGROUPMARKEREXTPROC)get_gles_proc("glPopGroupMarkerEXT");
    glDiscardFramebufferEXT = (PFNGLDISCARDFRAMEBUFFEREXTPROC)get_gles_proc("glDiscardFramebufferEXT");
    glGenQueriesEXT = (PFNGLGENQUERIESEXTPROC)get_gles_proc("glGenQueriesEXT");
    glDeleteQueriesEXT = (PFNGLDELETEQUERIESEXTPROC)get_gles_proc("glDeleteQueriesEXT");
    glIsQueryEXT = (PFNGLISQUERYEXTPROC)get_gles_proc("glIsQueryEXT");
    glBeginQueryEXT = (PFNGLBEGINQUERYEXTPROC)get_gles_proc("glBeginQueryEXT");
    glEndQueryEXT = (PFNGLENDQUERYEXTPROC)get_gles_proc("glEndQueryEXT");
    glQueryCounterEXT = (PFNGLQUERYCOUNTEREXTPROC)get_gles_proc("glQueryCounterEXT");
    glGetQueryivEXT = (PFNGLGETQUERYIVEXTPROC)get_gles_proc("glGetQueryivEXT");
    glGetQueryObjectivEXT = (PFNGLGETQUERYOBJECTIVEXTPROC)get_gles_proc("glGetQueryObjectivEXT");
    glGetQueryObjectuivEXT = (PFNGLGETQUERYOBJECTUIVEXTPROC)get_gles_proc("glGetQueryObjectuivEXT");
    glGetQueryObjecti64vEXT = (PFNGLGETQUERYOBJECTI64VEXTPROC)get_gles_proc("glGetQueryObjecti64vEXT");
    glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)get_gles_proc("glGetQueryObjectui64vEXT");
    glMapBufferRangeEXT = (PFNGLMAPBUFFERRANGEEXTPROC)get_gles_proc("glMapBufferRangeEXT");
    glFlushMappedBufferRangeEXT = (PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC)get_gles_proc("glFlushMappedBufferRangeEXT");
    glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)get_gles_proc("glRenderbufferStorageMultisampleEXT");
    glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)get_gles_proc("glFramebufferTexture2DMultisampleEXT");
    glReadBufferIndexedEXT = (PFNGLREADBUFFERINDEXEDEXTPROC)get_gles_proc("glReadBufferIndexedEXT");
    glDrawBuffersIndexedEXT = (PFNGLDRAWBUFFERSINDEXEDEXTPROC)get_gles_proc("glDrawBuffersIndexedEXT");
    glGetIntegeri_vEXT = (PFNGLGETINTEGERI_VEXTPROC)get_gles_proc("glGetIntegeri_vEXT");
    glMultiDrawArraysEXT = (PFNGLMULTIDRAWARRAYSEXTPROC)get_gles_proc("glMultiDrawArraysEXT");
    glMultiDrawElementsEXT = (PFNGLMULTIDRAWELEMENTSEXTPROC)get_gles_proc("glMultiDrawElementsEXT");
    glGetGraphicsResetStatusEXT = (PFNGLGETGRAPHICSRESETSTATUSEXTPROC)get_gles_proc("glGetGraphicsResetStatusEXT");
    glReadnPixelsEXT = (PFNGLREADNPIXELSEXTPROC)get_gles_proc("glReadnPixelsEXT");
    glGetnUniformfvEXT = (PFNGLGETNUNIFORMFVEXTPROC)get_gles_proc("glGetnUniformfvEXT");
    glGetnUniformivEXT = (PFNGLGETNUNIFORMIVEXTPROC)get_gles_proc("glGetnUniformivEXT");
    glUseProgramStagesEXT = (PFNGLUSEPROGRAMSTAGESEXTPROC)get_gles_proc("glUseProgramStagesEXT");
    glActiveShaderProgramEXT = (PFNGLACTIVESHADERPROGRAMEXTPROC)get_gles_proc("glActiveShaderProgramEXT");
    glCreateShaderProgramvEXT = (PFNGLCREATESHADERPROGRAMVEXTPROC)get_gles_proc("glCreateShaderProgramvEXT");
    glBindProgramPipelineEXT = (PFNGLBINDPROGRAMPIPELINEEXTPROC)get_gles_proc("glBindProgramPipelineEXT");
    glDeleteProgramPipelinesEXT = (PFNGLDELETEPROGRAMPIPELINESEXTPROC)get_gles_proc("glDeleteProgramPipelinesEXT");
    glGenProgramPipelinesEXT = (PFNGLGENPROGRAMPIPELINESEXTPROC)get_gles_proc("glGenProgramPipelinesEXT");
    glIsProgramPipelineEXT = (PFNGLISPROGRAMPIPELINEEXTPROC)get_gles_proc("glIsProgramPipelineEXT");
    glProgramParameteriEXT = (PFNGLPROGRAMPARAMETERIEXTPROC)get_gles_proc("glProgramParameteriEXT");
    glGetProgramPipelineivEXT = (PFNGLGETPROGRAMPIPELINEIVEXTPROC)get_gles_proc("glGetProgramPipelineivEXT");
    glProgramUniform1iEXT = (PFNGLPROGRAMUNIFORM1IEXTPROC)get_gles_proc("glProgramUniform1iEXT");
    glProgramUniform2iEXT = (PFNGLPROGRAMUNIFORM2IEXTPROC)get_gles_proc("glProgramUniform2iEXT");
    glProgramUniform3iEXT = (PFNGLPROGRAMUNIFORM3IEXTPROC)get_gles_proc("glProgramUniform3iEXT");
    glProgramUniform4iEXT = (PFNGLPROGRAMUNIFORM4IEXTPROC)get_gles_proc("glProgramUniform4iEXT");
    glProgramUniform1fEXT = (PFNGLPROGRAMUNIFORM1FEXTPROC)get_gles_proc("glProgramUniform1fEXT");
    glProgramUniform2fEXT = (PFNGLPROGRAMUNIFORM2FEXTPROC)get_gles_proc("glProgramUniform2fEXT");
    glProgramUniform3fEXT = (PFNGLPROGRAMUNIFORM3FEXTPROC)get_gles_proc("glProgramUniform3fEXT");
    glProgramUniform4fEXT = (PFNGLPROGRAMUNIFORM4FEXTPROC)get_gles_proc("glProgramUniform4fEXT");
    glProgramUniform1ivEXT = (PFNGLPROGRAMUNIFORM1IVEXTPROC)get_gles_proc("glProgramUniform1ivEXT");
    glProgramUniform2ivEXT = (PFNGLPROGRAMUNIFORM2IVEXTPROC)get_gles_proc("glProgramUniform2ivEXT");
    glProgramUniform3ivEXT = (PFNGLPROGRAMUNIFORM3IVEXTPROC)get_gles_proc("glProgramUniform3ivEXT");
    glProgramUniform4ivEXT = (PFNGLPROGRAMUNIFORM4IVEXTPROC)get_gles_proc("glProgramUniform4ivEXT");
    glProgramUniform1fvEXT = (PFNGLPROGRAMUNIFORM1FVEXTPROC)get_gles_proc("glProgramUniform1fvEXT");
    glProgramUniform2fvEXT = (PFNGLPROGRAMUNIFORM2FVEXTPROC)get_gles_proc("glProgramUniform2fvEXT");
    glProgramUniform3fvEXT = (PFNGLPROGRAMUNIFORM3FVEXTPROC)get_gles_proc("glProgramUniform3fvEXT");
    glProgramUniform4fvEXT = (PFNGLPROGRAMUNIFORM4FVEXTPROC)get_gles_proc("glProgramUniform4fvEXT");
    glProgramUniformMatrix2fvEXT = (PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC)get_gles_proc("glProgramUniformMatrix2fvEXT");
    glProgramUniformMatrix3fvEXT = (PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC)get_gles_proc("glProgramUniformMatrix3fvEXT");
    glProgramUniformMatrix4fvEXT = (PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC)get_gles_proc("glProgramUniformMatrix4fvEXT");
    glValidateProgramPipelineEXT = (PFNGLVALIDATEPROGRAMPIPELINEEXTPROC)get_gles_proc("glValidateProgramPipelineEXT");
    glGetProgramPipelineInfoLogEXT = (PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC)get_gles_proc("glGetProgramPipelineInfoLogEXT");
    glTexStorage1DEXT = (PFNGLTEXSTORAGE1DEXTPROC)get_gles_proc("glTexStorage1DEXT");
    glTexStorage2DEXT = (PFNGLTEXSTORAGE2DEXTPROC)get_gles_proc("glTexStorage2DEXT");
    glTexStorage3DEXT = (PFNGLTEXSTORAGE3DEXTPROC)get_gles_proc("glTexStorage3DEXT");
    glTextureStorage1DEXT = (PFNGLTEXTURESTORAGE1DEXTPROC)get_gles_proc("glTextureStorage1DEXT");
    glTextureStorage2DEXT = (PFNGLTEXTURESTORAGE2DEXTPROC)get_gles_proc("glTextureStorage2DEXT");
    glTextureStorage3DEXT = (PFNGLTEXTURESTORAGE3DEXTPROC)get_gles_proc("glTextureStorage3DEXT");
    glRenderbufferStorageMultisampleIMG = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC)get_gles_proc("glRenderbufferStorageMultisampleIMG");
    glFramebufferTexture2DMultisampleIMG = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC)get_gles_proc("glFramebufferTexture2DMultisampleIMG");
    glCoverageMaskNV = (PFNGLCOVERAGEMASKNVPROC)get_gles_proc("glCoverageMaskNV");
    glCoverageOperationNV = (PFNGLCOVERAGEOPERATIONNVPROC)get_gles_proc("glCoverageOperationNV");
    glDrawBuffersNV = (PFNGLDRAWBUFFERSNVPROC)get_gles_proc("glDrawBuffersNV");
    glDrawArraysInstancedNV = (PFNGLDRAWARRAYSINSTANCEDNVPROC)get_gles_proc("glDrawArraysInstancedNV");
    glDrawElementsInstancedNV = (PFNGLDRAWELEMENTSINSTANCEDNVPROC)get_gles_proc("glDrawElementsInstancedNV");
    glDeleteFencesNV = (PFNGLDELETEFENCESNVPROC)get_gles_proc("glDeleteFencesNV");
    glGenFencesNV = (PFNGLGENFENCESNVPROC)get_gles_proc("glGenFencesNV");
    glIsFenceNV = (PFNGLISFENCENVPROC)get_gles_proc("glIsFenceNV");
    glTestFenceNV = (PFNGLTESTFENCENVPROC)get_gles_proc("glTestFenceNV");
    glGetFenceivNV = (PFNGLGETFENCEIVNVPROC)get_gles_proc("glGetFenceivNV");
    glFinishFenceNV = (PFNGLFINISHFENCENVPROC)get_gles_proc("glFinishFenceNV");
    glSetFenceNV = (PFNGLSETFENCENVPROC)get_gles_proc("glSetFenceNV");
    glBlitFramebufferNV = (PFNGLBLITFRAMEBUFFERNVPROC)get_gles_proc("glBlitFramebufferNV");
    glRenderbufferStorageMultisampleNV = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC)get_gles_proc("glRenderbufferStorageMultisampleNV");
    glVertexAttribDivisorNV = (PFNGLVERTEXATTRIBDIVISORNVPROC)get_gles_proc("glVertexAttribDivisorNV");
    glReadBufferNV = (PFNGLREADBUFFERNVPROC)get_gles_proc("glReadBufferNV");
    glAlphaFuncQCOM = (PFNGLALPHAFUNCQCOMPROC)get_gles_proc("glAlphaFuncQCOM");
    glGetDriverControlsQCOM = (PFNGLGETDRIVERCONTROLSQCOMPROC)get_gles_proc("glGetDriverControlsQCOM");
    glGetDriverControlStringQCOM = (PFNGLGETDRIVERCONTROLSTRINGQCOMPROC)get_gles_proc("glGetDriverControlStringQCOM");
    glEnableDriverControlQCOM = (PFNGLENABLEDRIVERCONTROLQCOMPROC)get_gles_proc("glEnableDriverControlQCOM");
    glDisableDriverControlQCOM = (PFNGLDISABLEDRIVERCONTROLQCOMPROC)get_gles_proc("glDisableDriverControlQCOM");
    glExtGetTexturesQCOM = (PFNGLEXTGETTEXTURESQCOMPROC)get_gles_proc("glExtGetTexturesQCOM");
    glExtGetBuffersQCOM = (PFNGLEXTGETBUFFERSQCOMPROC)get_gles_proc("glExtGetBuffersQCOM");
    glExtGetRenderbuffersQCOM = (PFNGLEXTGETRENDERBUFFERSQCOMPROC)get_gles_proc("glExtGetRenderbuffersQCOM");
    glExtGetFramebuffersQCOM = (PFNGLEXTGETFRAMEBUFFERSQCOMPROC)get_gles_proc("glExtGetFramebuffersQCOM");
    glExtGetTexLevelParameterivQCOM = (PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC)get_gles_proc("glExtGetTexLevelParameterivQCOM");
    glExtTexObjectStateOverrideiQCOM = (PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC)get_gles_proc("glExtTexObjectStateOverrideiQCOM");
    glExtGetTexSubImageQCOM = (PFNGLEXTGETTEXSUBIMAGEQCOMPROC)get_gles_proc("glExtGetTexSubImageQCOM");
    glExtGetBufferPointervQCOM = (PFNGLEXTGETBUFFERPOINTERVQCOMPROC)get_gles_proc("glExtGetBufferPointervQCOM");
    glExtGetShadersQCOM = (PFNGLEXTGETSHADERSQCOMPROC)get_gles_proc("glExtGetShadersQCOM");
    glExtGetProgramsQCOM = (PFNGLEXTGETPROGRAMSQCOMPROC)get_gles_proc("glExtGetProgramsQCOM");
    glExtIsProgramBinaryQCOM = (PFNGLEXTISPROGRAMBINARYQCOMPROC)get_gles_proc("glExtIsProgramBinaryQCOM");
    glExtGetProgramBinarySourceQCOM = (PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC)get_gles_proc("glExtGetProgramBinarySourceQCOM");
    glStartTilingQCOM = (PFNGLSTARTTILINGQCOMPROC)get_gles_proc("glStartTilingQCOM");
    glEndTilingQCOM = (PFNGLENDTILINGQCOMPROC)get_gles_proc("glEndTilingQCOM");
}
