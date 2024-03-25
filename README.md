# WPE Android Backend

[![LGPLv2.1 License](https://img.shields.io/badge/License-LGPLv2.1-blue.svg?style=flat)](/LICENSE.md)

WPE backend for [WPE-Android](https://github.com/Igalia/wpe-android). Backend handles
Android specific graphics buffer handling for WPE WebKit. More details about backends
can be found from [WPE Architecture](https://wpewebkit.org/about/architecture.html) page.

# Building

Project has dependencies to Glib libraries and those dependencies are built for Android by
[wpe-android-cerbero](https://github.com/Igalia/wpe-android-cerbero) build system.
Thus this project is also built by wpe-android-cerbero. Cerbero recipe for building
this can be found in [wpebackend-android.recipe](https://github.com/Igalia/wpe-android-cerbero/blob/main/recipes/wpebackend-android.recipe)
