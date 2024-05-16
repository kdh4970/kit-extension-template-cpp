// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto.  Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#define CARB_EXPORTS
#include <carb/PluginUtils.h>

#include <omni/example/cpp/mesh_updater/IMeshUpdaterInterface.h>
#include <omni/ext/ExtensionsUtils.h>
#include <omni/ext/IExt.h>
#include <omni/kit/IApp.h>
#include <omni/timeline/ITimeline.h>
#include <omni/timeline/TimelineTypes.h>

#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/stageCache.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdUtils/stageCache.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/vt/array.h>
// #include "omni/example/cpp/mesh_updater/defaultMesh.pb.h"
#include "../SharedMemoryReader.hpp"

const struct carb::PluginImplDesc pluginImplDesc = { "omni.example.cpp.mesh_updater.plugin",
                                                     "An example C++ extension.", "NVIDIA",
                                                     carb::PluginHotReload::eEnabled, "dev" };


namespace omni
{
namespace example
{
namespace cpp
{
namespace mesh_updater
{

class MeshUpdaterExtension : public IMeshUpdaterInterface
                             , public PXR_NS::TfWeakBase
{
protected:
    void createPrims() override
    {
        // It is important that all USD stage reads/writes happen from the main thread:
        // https ://graphics.pixar.com/usd/release/api/_usd__page__multi_threading.html
        if (!m_stage)
        {
            return;
        }
        // Create a Mesh prim as USD geom.Mesh
        const PXR_NS::SdfPath primPath("/World/mymesh");
        if (m_stage->GetPrimAtPath(primPath))
        {
            // A prim already exists at this path.
            return;
        }
        mesh_prim = m_stage->DefinePrim(primPath);
        mesh = PXR_NS::UsdGeomMesh::Define(m_stage, primPath);
        mesh.CreatePointsAttr();
        mesh.CreateFaceVertexCountsAttr();
        mesh.CreateFaceVertexIndicesAttr();

        // Below Xform is for Omniverse, not for IsaacSim
        // Create Xformation for the Mesh
        PXR_NS::UsdGeomXformable xformable = PXR_NS::UsdGeomXformable(mesh_prim);

        // Setup the translation operation.
        const PXR_NS::GfVec3f translation(0.0f, 1000.0f, 0.0f);
        xformable.AddTranslateOp(PXR_NS::UsdGeomXformOp::PrecisionFloat).Set(translation);

        // Setup the scale operation.
        const PXR_NS::GfVec3f scale(1000.0f, 1000.0f, 1000.0f);
        xformable.AddScaleOp(PXR_NS::UsdGeomXformOp::PrecisionFloat).Set(scale);

        // Setup the rotation operation.
        const PXR_NS::GfVec3f rotation(-90.0f, 0.0f, 0.0f);
        xformable.AddRotateXYZOp(PXR_NS::UsdGeomXformOp::PrecisionFloat).Set(rotation);


        // Subscribe to timeline events so we know when to start or stop animating the prims.
        if (auto timeline = omni::timeline::getTimeline())
        {
            m_timelineEventsSubscription = carb::events::createSubscriptionToPop(
                timeline->getTimelineEventStream(),
                [this](carb::events::IEvent* timelineEvent) {
                onTimelineEvent(static_cast<omni::timeline::TimelineEventType>(timelineEvent->type));
            });
        }
    }

    void removePrims() override
    {
        if (!m_stage)
        {
            return;
        }

        // Release all event subscriptions.
        PXR_NS::TfNotice::Revoke(m_usdNoticeListenerKey);
        m_timelineEventsSubscription = nullptr;
        m_updateEventsSubscription = nullptr;

        // Remove Mesh Prims
        m_stage->RemovePrim(mesh_prim.GetPath());

    void printStageInfo() const override
    {
        if (!m_stage)
        {
            return;
        }

        printf("---Stage Info Begin---\n");

        // Print the USD stage's up-axis.
        const PXR_NS::TfToken stageUpAxis = PXR_NS::UsdGeomGetStageUpAxis(m_stage);
        printf("Stage up-axis is: %s.\n", stageUpAxis.GetText());

        // Print the USD stage's meters per unit.
        const double metersPerUnit = PXR_NS::UsdGeomGetStageMetersPerUnit(m_stage);
        printf("Stage meters per unit: %f.\n", metersPerUnit);

        // Print the USD stage's prims.
        const PXR_NS::UsdPrimRange primRange = m_stage->Traverse();
        for (const PXR_NS::UsdPrim& prim : primRange)
        {
            printf("Stage contains prim: %s.\n", prim.GetPath().GetString().c_str());
        }

        printf("---Stage Info End---\n\n");
    }

    void startTimelineAnimation() override
    {
        if (auto timeline = omni::timeline::getTimeline())
        {
            timeline->play();
        }
    }

    void stopTimelineAnimation() override
    {
        if (auto timeline = omni::timeline::getTimeline())
        {
            timeline->stop();
        }
    }

    void onDefaultUsdStageChanged(long stageId) override
    {
        PXR_NS::TfNotice::Revoke(m_usdNoticeListenerKey);
        m_timelineEventsSubscription = nullptr;
        m_updateEventsSubscription = nullptr;
        // m_primsWithRotationOps.clear();
        m_stage.Reset();

        if (stageId)
        {
            m_stage = PXR_NS::UsdUtilsStageCache::Get().Find(PXR_NS::UsdStageCache::Id::FromLongInt(stageId));
            m_usdNoticeListenerKey = PXR_NS::TfNotice::Register(PXR_NS::TfCreateWeakPtr(this), &MeshUpdaterExtension::onObjectsChanged);
        }
    }

    // object change callback function
    void onObjectsChanged(const PXR_NS::UsdNotice::ObjectsChanged& objectsChanged)
    {
        // printf(">> onObjectsChanged\n");
        // Check whether any of the prims we created have been (potentially) invalidated.
        // This may be too broad a check, but handles prims being removed from the stage.
        // for (auto& primWithRotationOps : m_primsWithRotationOps)
        // {
        //     if (!primWithRotationOps.m_invalid &&
        //         objectsChanged.ResyncedObject(primWithRotationOps.m_prim))
        //     {
        //         primWithRotationOps.m_invalid = true;
        //     }
        // }
    }

    void onTimelineEvent(omni::timeline::TimelineEventType timelineEventType)
    {
        switch (timelineEventType)
        {
        case omni::timeline::TimelineEventType::ePlay:
        {
            startAnimatingPrims();
        }
        break;
        case omni::timeline::TimelineEventType::eStop:
        {
            stopAnimatingPrims();
        }
        break;
        default:
        {

        }
        break;
        }
    }

    void startAnimatingPrims()
    {
        if (m_updateEventsSubscription)
        {
            // We're already animating the prims.
            return;
        }

        // Subscribe to update events so we can animate the prims.
        if (omni::kit::IApp* app = carb::getCachedInterface<omni::kit::IApp>())
        {
            m_updateEventsSubscription = carb::events::createSubscriptionToPop(app->getUpdateEventStream(), [this](carb::events::IEvent*)
            {
                onUpdateEvent();
            });
        }
    }

    void stopAnimatingPrims()
    {
        m_updateEventsSubscription = nullptr;
        onUpdateEvent(); // Reset positions.
    }

    void onUpdateEvent()
    {
        // It is important that all USD stage reads/writes happen from the main thread:
        // https ://graphics.pixar.com/usd/release/api/_usd__page__multi_threading.html
        if (!m_stage)
        {
            return;
        }
        auto start = std::chrono::high_resolution_clock::now();
        printf(">> onUpdateEvent\n");
        SHM.ReadAndDeserializeBinary();
        double capture_time = SHM.GetCaptureTime();
        int num_vertices = SHM.GetNumVertices();
        int num_triangles = SHM.GetNumTriangles();
        float3_t* vertices = SHM.GetVertexPtr();
        int3_t* triangles = SHM.GetTrianglePtr();
        if (num_vertices == 0 || num_triangles == 0)
        {
            printf("No vertices or triangles\n");
            return;
        }
        PXR_NS::VtArray<PXR_NS::GfVec3f> points;
        PXR_NS::VtArray<int> faceVertexCounts;
        PXR_NS::VtArray<int> faceVertexIndices;

        points.reserve(num_vertices);
        faceVertexCounts.reserve(num_triangles);
        faceVertexIndices.reserve(num_triangles * 3);

        for (int i = 0; i < num_vertices; i++)
        {
            float3_t vertex = vertices[i];
            // if(i < 5 || i>=num_vertices-5)
            // {
            //     printf("Vertex[%d]: %f %f %f\n", i, vertex.x, vertex.y, vertex.z);
            // }
            // if(vertex.x == 0.0f && vertex.y == 0.0f && vertex.z == 0.0f)
            // {
            //     printf("Invalid vertex id : %d\n", i);
            //     continue;
            // }
            points.push_back(PXR_NS::GfVec3f(vertex.x, vertex.y, vertex.z));
        }

        for (int i = 0; i < num_triangles; i++)
        {
            int3_t triangle = triangles[i];
            // if(i < 5 || i>=num_triangles-5)
            // {
            //     printf("Triangle[%d]: %d %d %d\n", i, triangle.x, triangle.y, triangle.z);
            // }
            // if (triangle.x < 0 || triangle.y < 0 || triangle.z < 0)
            // {
            //     // printf("Invalid triangle\n");
            //     continue;
            // }


            faceVertexIndices.push_back(triangle.x);
            faceVertexIndices.push_back(triangle.y);
            faceVertexIndices.push_back(triangle.z);
            faceVertexCounts.push_back(3);
        }

        mesh.GetPointsAttr().Set(points);
        mesh.GetFaceVertexCountsAttr().Set(faceVertexCounts);
        mesh.GetFaceVertexIndicesAttr().Set(faceVertexIndices);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_time = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        printf("[Read & Update] Elapsed time            : %f\n", elapsed_time.count());

        auto curr = std::chrono::high_resolution_clock::now().time_since_epoch();
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(curr);
        auto under_sec = std::chrono::duration_cast<std::chrono::microseconds>(curr-sec);
        double curr_time = sec.count() + (under_sec.count()/1000000.0);
        printf("End to End Time (from capture to update): %f\n", curr_time - capture_time);

    void InitSHM() override
    {
        SHM.init(1000);
    }

private:
    struct PrimWithRotationOps
    {
        PXR_NS::UsdPrim m_prim;
        bool m_invalid = false;
    };

    PXR_NS::UsdStageRefPtr m_stage;
    PXR_NS::TfNotice::Key m_usdNoticeListenerKey;
    // std::vector<PrimWithRotationOps> m_primsWithRotationOps;
    carb::events::ISubscriptionPtr m_updateEventsSubscription;
    carb::events::ISubscriptionPtr m_timelineEventsSubscription;

    PXR_NS::UsdPrim mesh_prim;
    PXR_NS::UsdGeomMesh mesh;
    SharedMemoryReader SHM;
};

}
}
}
}

CARB_PLUGIN_IMPL(pluginImplDesc, omni::example::cpp::mesh_updater::MeshUpdaterExtension)

void fillInterface(omni::example::cpp::mesh_updater::MeshUpdaterExtension& iface)
{
}
