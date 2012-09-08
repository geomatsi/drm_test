cmake_minimum_required(VERSION 2.6)
project(drm_tests)

add_executable(test test.c)
add_executable(drm_info drm_info.c)
add_executable(drm_dumb_bo drm_dumb_bo.c)

# find headers

FIND_PATH(EGL_INCLUDE_DIR
NAMES EGL/egl.h
PATHS /usr/include /usr/local/include
)

FIND_PATH(GL_INCLUDE_DIR
NAMES GL/gl.h
PATHS /usr/include /usr/local/include
)

FIND_PATH(DRM_INCLUDE_DIR
NAMES drm.h
PATHS /usr/include/libdrm /usr/local/include/libdrm
)

FIND_PATH(GBM_INCLUDE_DIR
NAMES gbm.h
PATHS /usr/include /usr/local/include
)

# find libraries

FIND_LIBRARY(GL_LIBRARY
NAMES GL
PATHS /usr/lib /usr/local/lib
)

FIND_LIBRARY(EGL_LIBRARY
NAMES EGL
PATHS /usr/lib /usr/local/lib
)

FIND_LIBRARY(DRM_LIBRARY
NAMES drm
PATHS /usr/lib /usr/local/lib
)

FIND_LIBRARY(GBM_LIBRARY
NAMES gbm
PATHS /usr/lib /usr/local/lib
)

# set headers paths

include_directories(${EGL_INCLUDE_DIR})
include_directories(${DRM_INCLUDE_DIR})
include_directories(${GBM_INCLUDE_DIR})
include_directories(${GL_INCLUDE_DIR})

# set libraries paths

target_link_libraries(test ${EGL_LIBRARY} ${DRM_LIBRARY} ${GBM_LIBRARY} ${GL_LIBRARY})
target_link_libraries(drm_info ${DRM_LIBRARY} ${GBM_LIBRARY})
target_link_libraries(drm_dumb_bo ${DRM_LIBRARY})