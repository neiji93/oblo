#pragma once

#include <oblo/core/allocation_helpers.hpp>
#include <oblo/core/deque.hpp>
#include <oblo/core/iterator/deque_chunk_iterator.hpp>
#include <oblo/core/iterator/deque_chunk_range.hpp>
#include <oblo/core/iterator/zip_range.hpp>
#include <oblo/ecs/entity_registry.hpp>
#include <oblo/ecs/range.hpp>
#include <oblo/vulkan/data/components.hpp>
#include <oblo/vulkan/graph/node_common.hpp>
#include <oblo/vulkan/nodes/providers/ecs_entity_set_provider.hpp>

namespace oblo::vk
{
    namespace
    {
        struct ecs_entity_set_entry
        {
            ecs::entity entity;
            u32 globalInstanceId;
        };
    }

    void ecs_entity_set_provider::build(const frame_graph_build_context& ctx)
    {
        ctx.begin_pass(pass_kind::transfer);

        ecs::entity_registry& reg = ctx.get_entity_registry();

        auto& frameAllocator = ctx.get_frame_allocator();

        deque<ecs_entity_set_entry> entities{&frameAllocator,
            deque_config{
                .elementsPerChunk = (1u << 14) / sizeof(ecs_entity_set_entry),
            }};

        for (const auto [entityIds, instanceIds] : reg.range<draw_instance_id_component>())
        {
            for (const auto [e, id] : zip_range(entityIds, instanceIds))
            {
                const u32 entityIndex = reg.extract_entity_index(e);

                if (entityIndex >= entities.size())
                {
                    entities.resize(entityIndex + 1);
                }

                entities[entityIndex] = {
                    .entity = e,
                    .globalInstanceId = id.rtInstanceId,
                };
            }
        }

        const auto stagedSpans = allocate_n_span<staging_buffer_span>(frameAllocator, 1 + entities.chunks_count());
        auto nextStagedSpanIt = stagedSpans.data();

        // Before the actual entries we have a header containing the number of entries to avoid OOB access
        const u32 setEntriesCount = u32(entities.size());

        *nextStagedSpanIt = ctx.stage_upload(as_bytes(std::span{&setEntriesCount, 1}));
        ++nextStagedSpanIt;

        for (auto chunk : deque_chunk_range(entities))
        {
            const auto stagedSpan = ctx.stage_upload(as_bytes(chunk));
            *nextStagedSpanIt = stagedSpan;
            ++nextStagedSpanIt;
        }

        ctx.create(outEntitySet,
            buffer_resource_initializer{
                .size = u32(sizeof(u32) + sizeof(ecs_entity_set_entry) * entities.size()),
            },
            buffer_usage::storage_read);
    }

    void ecs_entity_set_provider::execute(const frame_graph_execute_context& ctx)
    {
        for (u32 offset = 0, index = 0; index < stagedData.size(); ++index)
        {
            const auto& stagedSpan = stagedData[index];

            ctx.upload(outEntitySet, stagedSpan);

            const auto bytesCount = (stagedSpan.segments[0].end - stagedSpan.segments[0].begin) +
                (stagedSpan.segments[1].end - stagedSpan.segments[1].begin);

            offset += bytesCount;
        }
    }
}