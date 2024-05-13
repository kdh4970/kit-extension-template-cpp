-- Setup the extension.
local ext = get_current_extension_info()
project_ext(ext)

-- Link folders that should be packaged with the extension.
repo_build.prebuild_link {
    { "data", ext.target_dir.."/data" },
    { "docs", ext.target_dir.."/docs" },
}

-- Build the C++ plugin that will be loaded by the extension.
project_ext_plugin(ext, "omni.example.cpp.mesh_updater.plugin")
    defines("PROTOBUF_BUILTIN_ATOMIC=1")
    defines("PROTOBUF_ENABLE_DEBUG_LOGGING_MAY_LEAK_PII=0")
    add_files("source", "source/SharedMemoryReader.hpp")
    add_files("include", "include/omni/example/cpp/mesh_updater")
    -- add_files("source", "plugins/defaultMesh.pb.cc")
    add_files("source", "plugins/omni.example.cpp.mesh_updater")
    -- add_files("source", "plugins/SharedMemoryReader.cpp")

    -- add_files("source", "plugins/defaultmesh.pb.h")
    includedirs {
        "include",
        "plugins/omni.example.cpp.mesh_updater",
        "%{target_deps}/nv_usd/release/include"}
    libdirs { "%{target_deps}/nv_usd/release/lib", "lib" }
    links { "arch", "gf", "sdf", "tf", "usd", "usdGeom", "usdUtils"}
    defines { "NOMINMAX", "NDEBUG" }
    runtime "Release"
    rtti "On"

    filter { "system:linux" }
        exceptionhandling "On"
        staticruntime "Off"
        cppdialect "C++14"
        includedirs { "%{target_deps}/python/include/python3.10" }
        buildoptions { "-D_GLIBCXX_USE_CXX11_ABI=0 -Wno-deprecated-declarations -Wno-deprecated -Wno-unused-variable -pthread -lstdc++fs -Wno-error" }
        linkoptions { "-Wl,--disable-new-dtags -Wl,-rpath,%{target_deps}/nv_usd/release/lib:%{target_deps}/python/lib:" }
    filter { "system:windows" }
        buildoptions { "/wd4244 /wd4305" }
    filter {}



-- Build Python bindings that will be loaded by the extension.
project_ext_bindings {
    ext = ext,
    project_name = "omni.example.cpp.mesh_updater.python",
    module = "_mesh_updater_bindings",
    src = "bindings/python/omni.example.cpp.mesh_updater",
    target_subdir = "omni/example/cpp/mesh_updater"
}
    includedirs { "include" }
    repo_build.prebuild_link {
        { "python/impl", ext.target_dir.."/omni/example/cpp/mesh_updater/impl" },
    }
