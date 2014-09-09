/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GRALLOC_DRM_PUBLIC_H
#define GRALLOC_DRM_PUBLIC_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Android graphics.h defines the formats and leaves 0x100 - 0x1FF
 * range available for HAL implementation specific formats.
 */
enum {
    HAL_PIXEL_FORMAT_DRM_NV12 = 0x102,
};

/* used with the 'perform' method of gralloc_module_t */
enum {
    GRALLOC_MODULE_PERFORM_GET_DRM_FD                = 0x80000002,
    GRALLOC_MODULE_PERFORM_GET_DRM_MAGIC             = 0x80000003,
    GRALLOC_MODULE_PERFORM_AUTH_DRM_MAGIC            = 0x80000004,
    GRALLOC_MODULE_PERFORM_ENTER_VT                  = 0x80000005,
    GRALLOC_MODULE_PERFORM_LEAVE_VT                  = 0x80000006,
};

#ifdef __cplusplus
}
#endif

#endif
