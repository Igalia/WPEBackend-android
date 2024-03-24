/**
 * Copyright (C) 2024 Igalia S.L. <info@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef WPE_ANDROID_VIEW_BACKEND_H
#define WPE_ANDROID_VIEW_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <wpe/wpe.h>

typedef struct AHardwareBuffer AHardwareBuffer;

struct WPEAndroidBuffer;
struct WPEAndroidViewBackend;

typedef struct WPEAndroidBuffer WPEAndroidBuffer;
typedef struct WPEAndroidViewBackend WPEAndroidViewBackend;
typedef struct wpe_view_backend WPEViewBackend;

WPEAndroidViewBackend* WPEAndroidViewBackend_create(uint32_t width, uint32_t height);

void WPEAndroidViewBackend_destroy(WPEAndroidViewBackend*);

WPEViewBackend* WPEAndroidViewBackend_getWPEViewBackend(WPEAndroidViewBackend*);

typedef void (*WPEAndroidViewBackend_CommitBuffer)(void* context, WPEAndroidBuffer*, int fenceID);
void WPEAndroidViewBackend_setCommitBufferHandler(WPEAndroidViewBackend*, void* context, WPEAndroidViewBackend_CommitBuffer func);

void WPEAndroidViewBackend_dispatchReleaseBuffer(WPEAndroidViewBackend*, WPEAndroidBuffer*);

void WPEAndroidViewBackend_dispatchFrameComplete(WPEAndroidViewBackend*);

AHardwareBuffer* WPEAndroidBuffer_getAHardwareBuffer(WPEAndroidBuffer*);

#ifdef __cplusplus
}
#endif

#endif // WPE_ANDROID_VIEW_BACKEND_H
