#pragma once

#include <oblo/core/dynamic_array.hpp>
#include <oblo/core/expected.hpp>
#include <oblo/core/flat_dense_forward.hpp>
#include <oblo/core/handle.hpp>
#include <oblo/core/string/string_view.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/core/types.hpp>
#include <oblo/vulkan/graph/frame_graph_resources.hpp>
#include <oblo/vulkan/graph/pins.hpp>

#include <vulkan/vulkan_core.h>

#include <span>

namespace oblo
{
    class frame_allocator;
    class random_generator;
    class string_interner;

    class string;

    namespace ecs
    {
        class entity_registry;
    }
}

namespace oblo::vk
{
    class draw_registry;
    class pass_manager;
    class renderer;
    class resource_manager;
    class resource_pool;

    struct bindable_object;
    struct buffer;
    struct texture;
    struct image_initializer;

    struct loaded_functions;
    struct frame_graph_impl;
    struct frame_graph_pin_storage;
    struct frame_graph_pass;
    struct resident_texture;
    struct staging_buffer_span;

    using binding_table = flat_dense_map<h32<string>, bindable_object>;

    enum class pass_kind : u8
    {
        none,
        graphics,
        compute,
        raytracing,
        transfer,
    };

    enum class texture_usage : u8
    {
        render_target_write,
        depth_stencil_read,
        depth_stencil_write,
        shader_read,
        storage_read,
        storage_write,
        transfer_source,
        transfer_destination,
    };

    enum class buffer_usage : u8
    {
        storage_read,
        storage_write,
        /// @brief This means the buffer is not actually used on GPU in this node, just uploaded on.
        storage_upload,
        uniform,
        indirect,
        enum_max,
    };

    class frame_graph_init_context
    {
    public:
        explicit frame_graph_init_context(frame_graph_impl& frameGraph, renderer& renderer);

        pass_manager& get_pass_manager() const;

        string_interner& get_string_interner() const;

    private:
        frame_graph_impl& m_frameGraph;
        renderer& m_renderer;
    };

    class frame_graph_build_context
    {
    public:
        explicit frame_graph_build_context(
            frame_graph_impl& frameGraph, renderer& renderer, resource_pool& resourcePool);

        h32<frame_graph_pass> begin_pass(pass_kind kind) const;

        void create(
            resource<texture> texture, const texture_resource_initializer& initializer, texture_usage usage) const;

        void create(resource<buffer> buffer, const buffer_resource_initializer& initializer, buffer_usage usage) const;

        void create(resource<buffer> buffer, const staging_buffer_span& stagedData, buffer_usage usage) const;

        void acquire(resource<texture> texture, texture_usage usage) const;

        h32<resident_texture> acquire_bindless(resource<texture> texture, texture_usage usage) const;

        void acquire(resource<buffer> buffer, buffer_usage usage) const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(resource<buffer> buffer) const;

        [[nodiscard]] resource<buffer> create_dynamic_buffer(const buffer_resource_initializer& initializer,
            buffer_usage usage) const;

        [[nodiscard]] resource<buffer> create_dynamic_buffer(const staging_buffer_span& stagedData,
            buffer_usage usage) const;

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        template <typename T>
        std::span<const T> access(data_sink<T> data) const
        {
            return *static_cast<data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        template <typename T>
        void push(data_sink<T> data, T&& value) const
        {
            auto* a = static_cast<data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
            a->push_back(std::move(value));
        }

        template <typename T>
        void push(data_sink<T> data, const T& value) const
        {
            auto* a = static_cast<data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
            a->push_back(value);
        }

        expected<image_initializer> get_current_initializer(resource<texture> texture) const;

        frame_allocator& get_frame_allocator() const;

        const draw_registry& get_draw_registry() const;

        ecs::entity_registry& get_entity_registry() const;

        random_generator& get_random_generator() const;

        staging_buffer_span stage_upload(std::span<const byte> data) const;

        template <typename T>
        bool has_event() const
        {
            return has_event_impl(get_type_id<T>());
        }

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

        bool has_event_impl(const type_id& type) const;

    private:
        frame_graph_impl& m_frameGraph;
        renderer& m_renderer;
        resource_pool& m_resourcePool;
    };

    struct buffer_binding_desc
    {
        string_view name;
        resource<buffer> resource;
    };

    struct texture_binding_desc
    {
        string_view name;
        resource<texture> resource;
    };

    class frame_graph_execute_context
    {
    public:
        explicit frame_graph_execute_context(
            frame_graph_impl& frameGraph, renderer& renderer, VkCommandBuffer commandBuffer);

        void begin_pass(h32<frame_graph_pass> handle) const;

        template <typename T>
        T& access(data<T> data) const
        {
            return *static_cast<T*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        template <typename T>
        std::span<const T> access(data_sink<T> data) const
        {
            return *static_cast<data_sink_container<T>*>(access_storage(h32<frame_graph_pin_storage>{data.value}));
        }

        texture access(resource<texture> h) const;

        buffer access(resource<buffer> h) const;

        /// @brief Determines whether the pin has an incoming edge.
        bool has_source(resource<buffer> buffer) const;

        /// @brief Queries the number of frames a stable texture has been alive for.
        /// On the first frame of usage the function will return 0.
        /// For transient textures it will always return 0.
        /// @param texture A valid texture resource.
        u32 get_frames_alive_count(resource<texture> texture) const;

        /// @brief Queries the number of frames a stable buffer has been alive for.
        /// On the first frame of usage the function will return 0.
        /// For transient buffers it will always return 0.
        /// @param buffer A valid buffer resource.
        u32 get_frames_alive_count(resource<buffer> buffer) const;

        u32 get_current_frames_count() const;

        // TODO: This should probably be deprepcated, it would be hard to make this thread-safe, staging should happen
        // when building instead.
        void upload(resource<buffer> h, std::span<const byte> data, u32 bufferOffset = 0) const;

        void upload(resource<buffer> h, const staging_buffer_span& data, u32 bufferOffset = 0) const;

        VkCommandBuffer get_command_buffer() const;

        VkDevice get_device() const;

        pass_manager& get_pass_manager() const;

        draw_registry& get_draw_registry() const;

        string_interner& get_string_interner() const;

        const loaded_functions& get_loaded_functions() const;

        void bind_buffers(binding_table& table, std::initializer_list<buffer_binding_desc> bindings) const;
        void bind_textures(binding_table& table, std::initializer_list<texture_binding_desc> bindings) const;

    private:
        void* access_storage(h32<frame_graph_pin_storage> handle) const;

    private:
        frame_graph_impl& m_frameGraph;
        renderer& m_renderer;
        VkCommandBuffer m_commandBuffer;
    };
}