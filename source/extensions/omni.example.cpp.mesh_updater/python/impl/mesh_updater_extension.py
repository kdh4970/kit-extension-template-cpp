## Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
##
## NVIDIA CORPORATION and its licensors retain all intellectual property
## and proprietary rights in and to this software, related documentation
## and any modifications thereto.  Any use, reproduction, disclosure or
## distribution of this software and related documentation without an express
## license agreement from NVIDIA CORPORATION is strictly prohibited.
##
import omni.ext
import omni.usd
import omni.kit.commands
from .._mesh_updater_bindings import *

# Global public interface object.
_mesh_updater_interface = None

# Public API.
def get_mesh_updater_interface() -> IMeshUpdaterInterface:
    return _mesh_updater_interface


# Use the extension entry points to acquire and release the interface,
# and to subscribe to usd stage events.
class MeshUpdaterExtension(omni.ext.IExt):
    def on_startup(self):
        # Acquire the example USD interface.
        # breakpoint()
        global _mesh_updater_interface
        _mesh_updater_interface = acquire_mesh_updater_interface()

        # Inform the C++ plugin if a USD stage is already open.
        usd_context = omni.usd.get_context()
        if usd_context.get_stage_state() == omni.usd.StageState.OPENED:
            _mesh_updater_interface.on_default_usd_stage_changed(usd_context.get_stage_id())

        # Subscribe to omni.usd stage events so we can inform the C++ plugin when a new stage opens.
        self._stage_event_sub = usd_context.get_stage_event_stream().create_subscription_to_pop(
            self._on_stage_event, name="omni.example.cpp.mesh_updater"
        )

        # Print some info about the stage from C++.
        _mesh_updater_interface.print_stage_info()
        print("Calling SHM C++ Initializer...")
        _mesh_updater_interface.init_shm()
        print("SHM C++ Initializer called.")




        # _mesh_updater_interface.print_stage_info()

        # # Create some example prims from C++.
        _mesh_updater_interface.create_prims()

        omni.kit.commands.execute("CreateCollidersCommand",stage=omni.usd.get_context().get_stage(),prim_paths="/World/mymesh")
        # # Print some info about the stage from C++.
        # _mesh_updater_interface.print_stage_info()

        # # Animate the example prims from C++.
        _mesh_updater_interface.start_timeline_animation()

        

    def on_shutdown(self):
        global _mesh_updater_interface

        # Stop animating the example prims from C++.
        _mesh_updater_interface.stop_timeline_animation()

        # Remove the example prims from C++.
        _mesh_updater_interface.remove_prims()

        # Unsubscribe from omni.usd stage events.
        self._stage_event_sub = None

        # Release the example USD interface.
        release_mesh_updater_interface(_mesh_updater_interface)
        _mesh_updater_interface = None

    def _on_stage_event(self, event):
        # print("Stage event: ", event.type)
        # omni.kit.commands.execute('ClearPhysicsComponentsCommand', stage=omni.usd.get_context().get_stage(), prim_paths=['/World/mymesh'])
        if event.type == int(omni.usd.StageEventType.OPENED):
            _mesh_updater_interface.on_default_usd_stage_changed(omni.usd.get_context().get_stage_id())
            # omni.kit.commands.execute("CreateCollidersCommand",stage=omni.usd.get_context().get_stage(),prim_paths="/World/mymesh")
        elif event.type == int(omni.usd.StageEventType.CLOSED):
            _mesh_updater_interface.on_default_usd_stage_changed(0)
       
