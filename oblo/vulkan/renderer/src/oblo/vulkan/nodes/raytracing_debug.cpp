#include <oblo/vulkan/nodes/raytracing_debug.hpp>

#include <oblo/core/unreachable.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/math/vec2u.hpp>
#include <oblo/vulkan/data/draw_buffer_data.hpp>
#include <oblo/vulkan/data/picking_configuration.hpp>
#include <oblo/vulkan/draw/compute_pass_initializer.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/nodes/frustum_culling.hpp>
#include <oblo/vulkan/utility.hpp>

namespace oblo::vk
{
    void raytracing_debug::init(const frame_graph_init_context& context)
    {
        auto& passManager = context.get_pass_manager();

        rtDebugPass = passManager.register_compute_pass({
            .name = "Ray-Tracing Debug Pass",
            .shaderSourcePath = "./vulkan/shaders/raytracing_debug/ray_query.comp",
        });
    }

    void raytracing_debug::build(const frame_graph_build_context& ctx)
    {
        const auto resolution = ctx.access(inResolution);

        ctx.create(outShadedImage,
            {
                .width = resolution.x,
                .height = resolution.y,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            },
            texture_usage::storage_write);

        ctx.acquire(inCameraBuffer, pass_kind::compute, buffer_usage::uniform);

        ctx.acquire(inMeshDatabase, pass_kind::compute, buffer_usage::storage_read);

        acquire_instance_tables(ctx,
            inInstanceTables,
            inInstanceBuffers,
            pass_kind::compute,
            buffer_usage::storage_read);
    }

    void raytracing_debug::execute(const frame_graph_execute_context& ctx)
    {
        auto& pm = ctx.get_pass_manager();

        binding_table bindingTable;

        ctx.bind_buffers(bindingTable,
            {
                {"b_InstanceTables", inInstanceTables},
                {"b_MeshTables", inMeshDatabase},
                {"b_CameraBuffer", inCameraBuffer},
            });

        ctx.bind_textures(bindingTable,
            {
                {"t_OutShadedImage", outShadedImage},
            });

        bindingTable.emplace(ctx.get_string_interner().get_or_add("as_SceneTLAS"),
            make_bindable_object(ctx.get_draw_registry().get_tlas()));

        const auto commandBuffer = ctx.get_command_buffer();

        const auto pipeline = pm.get_or_create_pipeline(rtDebugPass, {});

        if (const auto pass = pm.begin_compute_pass(commandBuffer, pipeline))
        {
            const auto resolution = ctx.access(inResolution);

            pm.push_constants(*pass, VK_SHADER_STAGE_COMPUTE_BIT, 0, as_bytes(std::span{&resolution, 1}));

            const binding_table* bindingTables[] = {
                &bindingTable,
            };

            pm.bind_descriptor_sets(*pass, bindingTables);

            vkCmdDispatch(ctx.get_command_buffer(), round_up_multiple(resolution.x, 64u), resolution.y, 1);

            pm.end_compute_pass(*pass);
        }
    }
}