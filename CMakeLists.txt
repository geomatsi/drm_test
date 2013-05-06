cmake_minimum_required(VERSION 2.6)
project(drm_tests)

# options

option (WITH_DUMB_BO    "Build dumb buffer object examples" ON)
option (WITH_LIBKMS     "Build libkms examples"             ON)
option (WITH_GL         "Build OpenGL/OpenGLES examples"    ON)

# executables

add_executable(drm_info drm_info.c)

if (WITH_DUMB_BO)
    add_executable(drm_dumb_bo drm_dumb_bo.c drm_utils.c bitmap_utils.c)
    add_executable(drm_dumb_bo_plane drm_dumb_bo_plane.c drm_utils.c bitmap_utils.c)
endif (WITH_DUMB_BO)

if (WITH_LIBKMS)
    add_executable(drm_dumb_bo_libkms drm_dumb_bo_libkms.c drm_utils.c bitmap_utils.c)
    add_executable(drm_server drm_server.c drm_utils.c bitmap_utils.c)
    add_executable(drm_client_crtc drm_client_crtc.c drm_utils.c bitmap_utils.c)
    add_executable(drm_client_plane drm_client_plane.c drm_utils.c bitmap_utils.c)
    add_executable(drm_dumb_bo_mult drm_dumb_bo_mult.c bitmap_utils.c drm_utils.c )
endif (WITH_LIBKMS)

if (WITH_GL)
    add_executable(drm_gl_test1a drm_gl_test1a.c drm_utils.c gl_utils.c)
    add_executable(drm_gl_test1b drm_gl_test1b.c drm_utils.c gl_utils.c)
    add_executable(drm_gl_test2 drm_gl_test2.c drm_utils.c gl_utils.c)
    add_executable(drm_gl_test3 drm_gl_test3.c drm_utils.c gl_utils.c)
endif (WITH_GL)

# find headers

FIND_PATH(DRM_INCLUDE_DIR
    NAMES drm.h
    PATHS /usr/include/libdrm /usr/local/include/libdrm
)

if (WITH_LIBKMS)
    FIND_PATH(KMS_INCLUDE_DIR
        NAMES libkms.h
        PATHS /usr/include/libkms /usr/local/include/libkms
    )
endif (WITH_LIBKMS)

if (WITH_GL)
    FIND_PATH(GBM_INCLUDE_DIR
        NAMES gbm.h
        PATHS /usr/include /usr/local/include
    )

    FIND_PATH(EGL_INCLUDE_DIR
        NAMES EGL/egl.h
        PATHS /usr/include /usr/local/include
    )

    FIND_PATH(GL_INCLUDE_DIR
        NAMES GL/gl.h
        PATHS /usr/include /usr/local/include
    )
endif (WITH_GL)

# find libraries

FIND_LIBRARY(DRM_LIBRARY
    NAMES drm
    PATHS /usr/lib /usr/local/lib
)

if (WITH_LIBKMS)
    FIND_LIBRARY(KMS_LIBRARY
        NAMES kms
        PATHS /usr/lib /usr/lib/i386-linux/gnu
    )
endif (WITH_LIBKMS)

if (WITH_GL)
    FIND_LIBRARY(GL_LIBRARY
        NAMES GL
        PATHS /usr/lib /usr/local/lib
    )

    FIND_LIBRARY(EGL_LIBRARY
        NAMES EGL
        PATHS /usr/lib /usr/local/lib
    )

    FIND_LIBRARY(GBM_LIBRARY
        NAMES gbm
        PATHS /usr/lib /usr/local/lib
    )
endif (WITH_GL)

# set headers paths

include_directories(${DRM_INCLUDE_DIR})

if (WITH_LIBKMS)
    include_directories(${KMS_INCLUDE_DIR})
endif (WITH_LIBKMS)

if (WITH_GL)
    include_directories(${EGL_INCLUDE_DIR})
    include_directories(${GBM_INCLUDE_DIR})
    include_directories(${GL_INCLUDE_DIR})
endif (WITH_GL)

# set libraries paths

target_link_libraries(drm_info ${DRM_LIBRARY})

if (WITH_DUMB_BO)
    target_link_libraries(drm_dumb_bo ${DRM_LIBRARY})
    target_link_libraries(drm_dumb_bo_plane ${DRM_LIBRARY})
endif (WITH_DUMB_BO)

if (WITH_LIBKMS)
    target_link_libraries(drm_server ${KMS_LIBRARY} ${DRM_LIBRARY})
    target_link_libraries(drm_client_plane ${KMS_LIBRARY} ${DRM_LIBRARY})
    target_link_libraries(drm_client_crtc ${KMS_LIBRARY} ${DRM_LIBRARY})
    target_link_libraries(drm_dumb_bo_libkms ${KMS_LIBRARY} ${DRM_LIBRARY})
    target_link_libraries(drm_dumb_bo_mult ${KMS_LIBRARY} ${DRM_LIBRARY})
endif (WITH_LIBKMS)

if (WITH_GL)
    target_link_libraries(drm_gl_test1a ${EGL_LIBRARY} ${DRM_LIBRARY} ${GBM_LIBRARY} ${GL_LIBRARY})
    target_link_libraries(drm_gl_test1b ${EGL_LIBRARY} ${DRM_LIBRARY} ${GBM_LIBRARY} ${GL_LIBRARY})
    target_link_libraries(drm_gl_test2 ${EGL_LIBRARY} ${DRM_LIBRARY} ${GBM_LIBRARY} ${GL_LIBRARY})
    target_link_libraries(drm_gl_test3 ${EGL_LIBRARY} ${DRM_LIBRARY} ${GBM_LIBRARY} ${GL_LIBRARY})
endif (WITH_GL)
