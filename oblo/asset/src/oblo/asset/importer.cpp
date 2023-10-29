#include <oblo/asset/importer.hpp>

#include <oblo/asset/asset_meta.hpp>
#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/import_artifact.hpp>
#include <oblo/core/array_size.hpp>
#include <oblo/core/uuid.hpp>
#include <oblo/core/uuid_generator.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

namespace oblo
{
    namespace
    {
        constexpr std::string_view ImportConfigFilename{"import.json"};

        bool write_import_config(const importer_config& config, const std::filesystem::path& destination)
        {
            nlohmann::ordered_json json;

            if (!config.importer.name.empty())
            {
                json["importer"] = config.importer.name;
            }

            json["filename"] = config.sourceFile.filename();
            json["source"] = config.sourceFile;

            std::ofstream ofs{destination};

            if (!ofs)
            {
                return false;
            }

            ofs << json.dump(1, '\t');
            return !ofs.bad();
        }
    }

    struct importer::pending_asset_import
    {
        std::vector<import_artifact> artifacts;
    };

    importer::importer() = default;

    importer::importer(importer&&) noexcept = default;

    importer::importer(importer_config config, std::unique_ptr<file_importer> fileImporter) :
        m_config{std::move(config)}, m_importer{std::move(fileImporter)}
    {
        m_sourceFiles.emplace_back(m_config.sourceFile);
    }

    importer::~importer() = default;

    importer& importer::operator=(importer&&) noexcept = default;

    bool importer::init()
    {
        if (!m_importer)
        {
            return false;
        }

        if (!m_importer->init(m_config, m_preview))
        {
            return false;
        }

        m_importNodesConfig.assign(m_preview.nodes.size(), {.enabled = true});
        return true;
    }

    bool importer::execute(const std::filesystem::path& destinationDir)
    {
        if (!begin_import(*m_config.registry, m_importNodesConfig))
        {
            return false;
        }

        const auto importUuid = m_config.registry->generate_uuid();

        const import_context context{
            .importer = this,
            .registry = m_config.registry,
            .preview = &m_preview,
            .importNodesConfig = m_importNodesConfig,
            .importUuid = importUuid,
        };

        if (!m_importer->import(context))
        {
            // TODO: Cleanup
            return false;
        }

        // TODO: Cleanup if finalize_import fails too, e.g. remove the saved artifacts, if any
        return finalize_import(*m_config.registry, destinationDir);
    }

    bool importer::add_asset(import_artifact mainArtifact, std::span<import_artifact> otherArtifacts)
    {
        const auto it = m_artifacts.find(mainArtifact.id);

        if (it == m_artifacts.end())
        {
            return false;
        }

        if (mainArtifact.data.empty())
        {
            return false;
        }

        if (mainArtifact.data.get_type() != it->second.type)
        {
            return false;
        }

        it->second.id = mainArtifact.id;

        auto& importedAsset = m_assets.emplace_back();

        importedAsset.artifacts.clear();
        importedAsset.artifacts.reserve(otherArtifacts.size() + 1);
        importedAsset.artifacts.emplace_back(std::move(mainArtifact));

        for (auto& otherArtifact : otherArtifacts)
        {
            const auto currentArtifactIt = m_artifacts.find(otherArtifact.id);

            if (currentArtifactIt == m_artifacts.end())
            {
                OBLO_ASSERT(false);
                continue;
            }

            currentArtifactIt->second.id = otherArtifact.id;
            importedAsset.artifacts.emplace_back(std::move(otherArtifact));
        }

        return true;
    }

    bool importer::begin_import(asset_registry& registry, std::span<import_node_config> importNodesConfig)
    {
        // TODO: Could maybe create the folder here to ensure it's unique
        m_importId = registry.generate_uuid();

        if (importNodesConfig.size() != m_preview.nodes.size())
        {
            return false;
        }

        const auto uuidGenerator = uuid_namespace_generator{m_importId};

        for (usize i = 0; i < importNodesConfig.size(); ++i)
        {
            const auto& node = m_preview.nodes[i];

            if (!registry.has_asset_type(node.type))
            {
                return false;
            }

            // TODO: Ensure that names are unique
            auto& config = importNodesConfig[i];
            config.id = uuidGenerator.generate(node.name);

            const auto [artifactIt, artifactInserted] = m_artifacts.emplace(config.id,
                artifact_meta{
                    .id = config.id,
                    .type = node.type,
                    .importId = m_importId,
                    .importName = node.name,
                });

            OBLO_ASSERT(artifactInserted);
        }

        return true;
    }

    bool importer::finalize_import(asset_registry& registry, const std::filesystem::path& destination)
    {
        if (!registry.create_directories(destination))
        {
            return false;
        }

        bool allSucceeded = true;

        std::string assetFileName;

        for (const pending_asset_import& assetImport : m_assets)
        {
            OBLO_ASSERT(!assetImport.artifacts.empty(), "The first artifact is expected to be the asset");

            const auto& mainArtifact = assetImport.artifacts[0];

            const auto mainArtifactIt = m_artifacts.find(mainArtifact.id);

            if (mainArtifactIt == m_artifacts.end())
            {
                OBLO_ASSERT(false);
                allSucceeded = false;
                continue;
            }

            asset_meta assetMeta{
                .id = mainArtifact.id,
                .type = mainArtifact.data.get_type(),
                .importId = m_importId,
                .importName = mainArtifact.name,
            };

            assetFileName = mainArtifactIt->second.importName;

            for (const import_artifact& artifact : assetImport.artifacts)
            {
                const auto artifactIt = m_artifacts.find(artifact.id);

                if (artifactIt == m_artifacts.end())
                {
                    OBLO_ASSERT(false);
                    allSucceeded = false;
                    continue;
                }

                auto* const artifactPtr = artifact.data.try_get();

                if (!artifactPtr)
                {
                    OBLO_ASSERT(false); // TODO: Log?
                    allSucceeded = false;
                    continue;
                }

                const artifact_meta meta{
                    .id = artifact.id,
                    .type = artifact.data.get_type(),
                    .importId = m_importId,
                    .importName = artifact.name,
                };

                if (!registry.save_artifact(artifact.id, artifact.data.get_type(), artifactPtr, meta))
                {
                    OBLO_ASSERT(false); // TODO: Log?
                    allSucceeded = false;
                    continue;
                }
            }

            allSucceeded &= registry.save_asset(destination, assetFileName, std::move(assetMeta));
        }

        allSucceeded &= write_source_files();

        return allSucceeded;
    }

    void importer::add_source_files(std::span<const std::filesystem::path> sourceFiles)
    {
        m_sourceFiles.insert(m_sourceFiles.end(), sourceFiles.begin(), sourceFiles.end());
    }

    const importer_config& importer::get_config() const
    {
        return m_config;
    }

    bool importer::write_source_files()
    {
        const auto importDir = m_config.registry->create_source_files_dir(m_importId);

        if (importDir.empty())
        {
            return false;
        }

        bool allSucceeded = true;

        for (const auto& sourceFile : m_sourceFiles)
        {
            std::error_code ec;
            allSucceeded &= std::filesystem::copy_file(sourceFile, importDir / sourceFile.filename(), ec) && !ec;
        }

        allSucceeded &= write_import_config(m_config, importDir / ImportConfigFilename);

        return allSucceeded;
    }
}