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

#include <wpe/wpe.h>

#include "interfaces.h"
#include "logging.h"
#include <cstring>

extern "C" {

__attribute__((visibility("default")))
struct wpe_loader_interface _wpe_loader_interface = {
    [] (const char* object_name) -> void* {
        ALOGD("_wpe_loader_interface queried for object_name %s", object_name);

        if (!std::strcmp(object_name, "_wpe_renderer_host_interface"))
            return &android_renderer_host_impl;

        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_interface"))
            return &android_renderer_backend_egl_impl;

        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_target_interface"))
            return &android_renderer_backend_egl_target_impl;

        if (!std::strcmp(object_name, "_wpe_renderer_backend_egl_offscreen_target_interface"))
            return &android_renderer_backend_egl_offscreen_target_impl;

        return nullptr;
    },
};

}


#include <wpe/wpe.h>
#include <wpe/wpe-egl.h>

extern struct wpe_renderer_host_interface android_renderer_host_impl;

extern struct wpe_renderer_backend_egl_interface android_renderer_backend_egl_impl;
extern struct wpe_renderer_backend_egl_target_interface android_renderer_backend_egl_target_impl;
extern struct wpe_renderer_backend_egl_offscreen_target_interface android_renderer_backend_egl_offscreen_target_impl;
