/*
 * Copyright (C) 2011 Chia-I Wu <olvaffe@gmail.com>
 * Copyright (C) 2011 LunarG Inc.
 *
 * Based on xf86-video-openfimg, which has
 *
 * Copyright © 2007 Red Hat, Inc.
 * Copyright © 2008 Maarten Maathuis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define LOG_TAG "GRALLOC-OPENFIMG"

#include <cutils/log.h>
#include <stdlib.h>
#include <errno.h>
#include <drm.h>

#include "freedreno_drmif.h"

#include "gralloc_drm.h"
#include "gralloc_drm_priv.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct openfimg_info {
	struct gralloc_drm_drv_t base;

	int fd;
	struct fd_device *dev;
};

struct openfimg_buffer {
	struct gralloc_drm_bo_t base;

	struct fd_bo *bo;
};

static struct gralloc_drm_bo_t *
openfimg_alloc(struct gralloc_drm_drv_t *drv,
	       struct gralloc_drm_handle_t *handle)
{
	struct openfimg_info *info = (struct openfimg_info *)drv;
	struct openfimg_buffer *ob;

	ob = calloc(1, sizeof(*ob));
	if (!ob) {
		ALOGE("%s: allocation failed", __func__);
		return NULL;
	}

	if (handle->fd) {
		ALOGE("%s: import: fd = %d", __func__, handle->fd);

		ob->bo = fd_bo_from_dmabuf(info->dev, handle->fd);
		if (!ob->bo) {
			ALOGE("%s: fd_bo_from_dmabuf() failed", __func__);
			free(ob);
			return NULL;
		}
	} else if (handle->name) {
		ALOGE("%s: import: name = %d", __func__, handle->name);

		ob->bo = fd_bo_from_name(info->dev, handle->name);
		if (!ob->bo) {
			ALOGE("%s: fd_bo_from_name() failed", __func__);
			free(ob);
			return NULL;
		}
	} else {
		int width, height, pitch, cpp;
		uint32_t name = 0;

		width = handle->width;
		height = handle->height;
		gralloc_drm_align_geometry(handle->format, &width, &height);

		cpp = gralloc_drm_get_bpp(handle->format);
		if (!cpp) {
			ALOGE("%s: unrecognized format 0x%x",
				__func__, handle->format);
			free(ob);
			return NULL;
		}

		ob->bo = fd_bo_new(info->dev, width * height * cpp, 0);
		if (!ob->bo) {
			ALOGE("%s: fd_bo_new() failed", __func__);
			free(ob);
			return NULL;
		}

		handle->fd = fd_bo_dmabuf(ob->bo);
		handle->stride = width * cpp;

		ALOGE("%s: create: handle = %u, fd = %d", __func__,
			fd_bo_handle(ob->bo), handle->fd);
	}

	if (handle->usage & GRALLOC_USAGE_HW_FB)
		ob->base.fb_handle = fd_bo_handle(ob->bo);

	ob->base.handle = handle;

	return &ob->base;
}

static void
openfimg_free(struct gralloc_drm_drv_t *drv, struct gralloc_drm_bo_t *bo)
{
	struct openfimg_buffer *ob = (struct openfimg_buffer *)bo;

	fd_bo_del(ob->bo);
	free(ob);
}

static int
openfimg_map(struct gralloc_drm_drv_t *drv, struct gralloc_drm_bo_t *bo,
	     int x, int y, int w, int h, int enable_write, void **addr)
{
	struct openfimg_buffer *ob = (struct openfimg_buffer *)bo;
	int op = DRM_FREEDRENO_PREP_READ;
	void *map;
	int ret;

	if (enable_write)
		op |= DRM_FREEDRENO_PREP_WRITE;

	map = fd_bo_map(ob->bo);
	if (!map)
		return -EFAULT;

	ret = fd_bo_cpu_prep(ob->bo, NULL, op);
	if (!ret)
		*addr = map;

	return ret;
}

static void
openfimg_unmap(struct gralloc_drm_drv_t *drv, struct gralloc_drm_bo_t *bo)
{
	struct openfimg_buffer *ob = (struct openfimg_buffer *)bo;

	fd_bo_cpu_fini(ob->bo);
}

static void
openfimg_init_kms_features(struct gralloc_drm_drv_t *drv,
			   struct gralloc_drm_t *drm)
{
	struct openfimg_info *info = (struct openfimg_info *)drv;

	switch (drm->primary.fb_format) {
	case HAL_PIXEL_FORMAT_BGRA_8888:
	case HAL_PIXEL_FORMAT_RGB_565:
		break;
	default:
		drm->primary.fb_format = HAL_PIXEL_FORMAT_BGRA_8888;
		break;
	}

	drm->mode_quirk_vmwgfx = 0;
	drm->swap_mode = DRM_SWAP_FLIP;
	drm->mode_sync_flip = 1;
	drm->swap_interval = 1;
	drm->vblank_secondary = 0;
}

static void
openfimg_destroy(struct gralloc_drm_drv_t *drv)
{
	struct openfimg_info *info = (struct openfimg_info *)drv;

	fd_device_del(info->dev);
	free(info);
}

struct gralloc_drm_drv_t *
gralloc_drm_drv_create_for_openfimg(int fd)
{
	struct openfimg_info *info;
	int err;

	info = calloc(1, sizeof(*info));
	if (!info)
		return NULL;

	info->fd = fd;
	info->dev = fd_device_new(info->fd);
	if (!info->dev) {
		ALOGE("failed to create openfimg device");
		free(info);
		return NULL;
	}

	info->base.destroy = openfimg_destroy;
	info->base.init_kms_features = openfimg_init_kms_features;
	info->base.alloc = openfimg_alloc;
	info->base.free = openfimg_free;
	info->base.map = openfimg_map;
	info->base.unmap = openfimg_unmap;

	return &info->base;
}
