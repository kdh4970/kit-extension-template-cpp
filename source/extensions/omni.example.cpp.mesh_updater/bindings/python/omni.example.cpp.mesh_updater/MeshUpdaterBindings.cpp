// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto.  Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#include <carb/BindingsPythonUtils.h>

#include "omni/example/cpp/mesh_updater/IMeshUpdaterInterface.h"

CARB_BINDINGS("omni.example.cpp.mesh_updater.python")

DISABLE_PYBIND11_DYNAMIC_CAST(omni::example::cpp::mesh_updater::IMeshUpdaterInterface)

namespace
{

// Define the pybind11 module using the same name specified in premake5.lua
PYBIND11_MODULE(_mesh_updater_bindings, m)
{
    using namespace omni::example::cpp::mesh_updater;

    m.doc() = "pybind11 omni.example.cpp.mesh_updater bindings";

    carb::defineInterfaceClass<IMeshUpdaterInterface>(
        m, "IMeshUpdaterInterface", "acquire_mesh_updater_interface", "release_mesh_updater_interface")
        .def("create_prims", &IMeshUpdaterInterface::createPrims)
        .def("remove_prims", &IMeshUpdaterInterface::removePrims)
        .def("print_stage_info", &IMeshUpdaterInterface::printStageInfo)
        .def("start_timeline_animation", &IMeshUpdaterInterface::startTimelineAnimation)
        .def("stop_timeline_animation", &IMeshUpdaterInterface::stopTimelineAnimation)
        .def("on_default_usd_stage_changed", &IMeshUpdaterInterface::onDefaultUsdStageChanged)
        .def("init_shm", &IMeshUpdaterInterface::InitSHM)
    /**/;
}
}
