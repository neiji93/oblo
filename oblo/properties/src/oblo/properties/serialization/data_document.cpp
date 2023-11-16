#include <oblo/properties/serialization/data_document.hpp>

#include <oblo/core/debug.hpp>
#include <oblo/core/utility.hpp>
#include <oblo/properties/property_kind.hpp>
#include <oblo/properties/serialization/data_node.hpp>

#include <memory>

namespace oblo
{
    struct data_document::data_chunk
    {
        data_chunk* next;
        u32 size;
        u32 used;
    };

    data_document::data_document() = default;

    data_document::~data_document() = default;

    void data_document::init(u32 firstChunkSize)
    {
        m_firstChunkSize = firstChunkSize;

        if (!m_firstChunk)
        {
            m_firstChunk = allocate_chunk(1);
            m_chunksCount = 1;
        }
        else
        {
            for (auto* chunk = m_firstChunk; chunk != nullptr; chunk = chunk->next)
            {
                chunk->used = 0;
            }
        }

        m_currentChunk = m_firstChunk;

        const data_node root{
            .kind = data_node_kind::object,
            .nextSibling = data_node::Invalid,
            .object =
                {
                    .firstChild = data_node::Invalid,
                    .lastChild = data_node::Invalid,
                },
        };

        m_nodes.assign(1, root);
    }

    u32 data_document::get_root() const
    {
        if (m_nodes.empty())
        {
            return data_node::Invalid;
        }

        return 0u;
    }

    void data_document::child_value(
        u32 parentIndex, std::string_view key, property_kind kind, std::span<const std::byte> data)
    {
        u32 newChild = u32(m_nodes.size());
        auto& newValue = m_nodes.emplace_back();

        auto& parent = m_nodes[parentIndex];
        OBLO_ASSERT(parent.kind == data_node_kind::object);

        const auto [size, alignment] = get_size_and_alignment(kind);
        OBLO_ASSERT(data.size() >= size);

        auto* const newData = allocate(size, alignment);
        std::memcpy(newData, data.data(), size);

        auto* const newKey = static_cast<char*>(allocate(key.size() + 1, 1));
        std::memcpy(newKey, key.data(), key.size());
        newKey[key.size()] = '\0';

        newValue = {
            .kind = data_node_kind::value,
            .valueKind = kind,
            .keyLen = narrow_cast<u16>(key.size()),
            .nextSibling = data_node::Invalid,
            .key = newKey,
            .value =
                {
                    .data = newData,
                },
        };

        if (parent.object.firstChild == data_node::Invalid)
        {
            parent.object.firstChild = newChild;
        }

        if (parent.object.lastChild != data_node::Invalid)
        {
            m_nodes[parent.object.lastChild].nextSibling = newChild;
        }

        parent.object.lastChild = newChild;
    }

    void* data_document::allocate(usize size, usize alignment)
    {
        usize space = m_currentChunk->size - m_currentChunk->used;

        u8* const begin = reinterpret_cast<u8*>(m_currentChunk + 1);
        void* firstUnused = begin + m_currentChunk->used;

        void* const ptr = std::align(alignment, size, firstUnused, space);

        if (!ptr)
        {
            auto* newChunk = allocate_chunk(m_chunksCount + 1);
            m_currentChunk->next = newChunk;
            m_currentChunk = newChunk;
            ++m_chunksCount;

            return allocate(size, alignment);
        }
        else
        {
            m_currentChunk->used = u32(static_cast<u8*>(ptr) - begin) + size;
        }

        return ptr;
    }

    data_document::data_chunk* data_document::allocate_chunk(u8 exponent)
    {
        const auto memorySize = m_firstChunkSize << (exponent - 1);
        auto* const memory = new u8[memorySize];
        return new (memory) data_chunk{
            .next = nullptr,
            .size = u32(memorySize - sizeof(data_chunk)),
            .used = 0,
        };
    }

    std::span<const data_node> data_document::get_nodes() const
    {
        return m_nodes;
    }
}