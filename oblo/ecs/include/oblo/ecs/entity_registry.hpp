#pragma once

#include <oblo/core/flat_dense_map.hpp>
#include <oblo/core/type_id.hpp>
#include <oblo/ecs/handles.hpp>
#include <oblo/ecs/traits.hpp>

#include <memory>
#include <tuple>

namespace oblo::ecs
{
    class type_registry;
    struct component_and_tags_sets;
    struct type_set;

    class entity_registry final
    {
    public:
        template <typename... Components>
        class typed_range;

        entity_registry();
        explicit entity_registry(const type_registry* typeRegistry);
        entity_registry(const entity_registry&) = delete;
        entity_registry(entity_registry&&) noexcept;
        entity_registry& operator=(const entity_registry&) = delete;
        entity_registry& operator=(entity_registry&&) noexcept;
        ~entity_registry();

        void init(const type_registry* typeRegistry);

        entity create(const component_and_tags_sets& types, u32 count = 1);

        template <typename... ComponentsOrTags>
        entity create(u32 count = 1);

        void destroy(entity e);

        void add(entity e, const component_and_tags_sets& types);

        template <typename... ComponentsOrTags>
        void add(entity e);

        void remove(entity e, const component_and_tags_sets& types);

        template <typename... ComponentsOrTags>
        void remove(entity e);

        bool contains(entity e) const;

        template <typename Component>
        const Component& get(entity e) const;

        template <typename Component>
        Component& get(entity e);

        template <typename... Components>
            requires(sizeof...(Components) > 1)
        std::tuple<const Components&...> get(entity e) const;

        template <typename... Components>
            requires(sizeof...(Components) > 1)
        std::tuple<Components&...> get(entity e);

        // Requires including oblo/ecs/range.hpp
        template <typename... Components>
        typed_range<Components...> range();

    private:
        struct components_storage;
        struct memory_pool;
        struct tags_storage;
        struct entity_data;

    private:
        const components_storage* find_first_match(const components_storage* begin,
                                                   usize increment,
                                                   const component_and_tags_sets& types);

        static void sort_and_map(std::span<component_type> componentTypes, std::span<u8> mapping);

        static u32 get_used_chunks_count(const components_storage& storage);

        static bool fetch_component_offsets(const components_storage& storage,
                                            std::span<const component_type> componentTypes,
                                            std::span<u32> offsets);

        static u32 fetch_chunk_data(const components_storage& storage,
                                    u32 chunkIndex,
                                    std::span<const u32> offsets,
                                    const entity** entities,
                                    std::span<std::byte*> componentData);

        const components_storage& find_or_create_storage(const component_and_tags_sets& types);

        void find_component_types(std::span<const type_id> typeIds, std::span<component_type> types);

        void find_component_data(entity e,
                                 const std::span<const type_id> typeIds,
                                 std::span<std::byte*> outComponents) const;

    private:
        const type_registry* m_typeRegistry{nullptr};
        std::unique_ptr<memory_pool> m_pool;
        flat_dense_map<entity, entity_data> m_entities;
        std::vector<components_storage> m_componentsStorage;
        entity m_nextId{1};
    };

    template <typename... ComponentsOrTags>
    entity entity_registry::create(u32 count)
    {
        return create(make_type_sets<ComponentsOrTags...>(*m_typeRegistry), count);
    }

    template <typename Component>
    const Component& entity_registry::get(entity e) const
    {
        constexpr type_id types[] = {get_type_id<Component>()};
        std::byte* pointers[1];

        find_component_data(e, types, pointers);
        OBLO_ASSERT(pointers[0]);

        return *reinterpret_cast<const Component*>(pointers[0]);
    }

    template <typename Component>
    Component& entity_registry::get(entity e)
    {
        constexpr type_id types[] = {get_type_id<Component>()};
        std::byte* pointers[1];

        find_component_data(e, types, pointers);
        OBLO_ASSERT(pointers[0]);

        return *reinterpret_cast<Component*>(pointers[0]);
    }

    template <typename... Components>
        requires(sizeof...(Components) > 1)
    std::tuple<const Components&...> entity_registry::get(entity e) const
    {
        constexpr auto N = sizeof...(Components);
        constexpr type_id types[] = {get_type_id<Components>()...};
        std::byte* pointers[N];

        find_component_data(e, types, pointers);

        constexpr auto makeTuple = []<std::size_t... I>(std::byte** pointers, std::index_sequence<I...>)
        {
            OBLO_ASSERT((pointers[I] && ...));
            return std::tuple<const Components&...>{*reinterpret_cast<Components*>(pointers[I])...};
        };

        return makeTuple(pointers, std::make_index_sequence<N>());
    }

    template <typename... Components>
        requires(sizeof...(Components) > 1)
    std::tuple<Components&...> entity_registry::get(entity e)
    {
        constexpr auto N = sizeof...(Components);
        constexpr type_id types[] = {get_type_id<Components>()...};
        std::byte* pointers[N];

        find_component_data(e, types, pointers);

        constexpr auto makeTuple = []<std::size_t... I>(std::byte** pointers, std::index_sequence<I...>)
        {
            OBLO_ASSERT((pointers[I] && ...));
            return std::tuple<Components&...>{*reinterpret_cast<Components*>(pointers[I])...};
        };

        return makeTuple(pointers, std::make_index_sequence<N>());
    }
}