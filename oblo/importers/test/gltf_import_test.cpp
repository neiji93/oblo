#include <gtest/gtest.h>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/asset/importers/registration.hpp>
#include <oblo/asset/meta.hpp>
#include <oblo/resource/resource_handle.hpp>
#include <oblo/resource/resource_registry.hpp>
#include <oblo/scene/assets/bundle.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/assets/registration.hpp>

namespace oblo::asset::importers
{
    namespace
    {
        void clear_directory(const std::filesystem::path& path)
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            ASSERT_FALSE(ec);
        }
    }

    TEST(gltf_importer, box)
    {
        resource_registry resources;
        scene::register_resource_types(resources);

        asset_registry registry;

        const std::filesystem::path testDir{"./test/gltf_importer_suzanne/"};
        const std::filesystem::path assetsDir{testDir / "assets"};
        const std::filesystem::path artifactsDir{testDir / "artifacts"};
        const std::filesystem::path sourceFilesDir{testDir / "sourcefiles"};

        clear_directory(testDir);

        ASSERT_TRUE(registry.initialize(assetsDir, artifactsDir, sourceFilesDir));
        scene::register_asset_types(registry);

        register_gltf_importer(registry);

        resources.register_provider(&asset_registry::find_artifact_resource, &registry);

        const std::filesystem::path gltfSampleModels{OBLO_GLTF_SAMPLE_MODELS};

        const std::filesystem::path files[] = {
            gltfSampleModels / "2.0" / "Box" / "glTF-Embedded" / "Box.gltf",
            gltfSampleModels / "2.0" / "Box" / "glTF" / "Box.gltf",
            gltfSampleModels / "2.0" / "Box" / "glTF-Binary" / "Box.glb",
        };

        for (const auto& file : files)
        {
            auto importer = registry.create_importer(file);

            const auto dirName = file.parent_path().filename();

            ASSERT_TRUE(importer.is_valid());

            ASSERT_TRUE(importer.init());
            ASSERT_TRUE(importer.execute(dirName));

            uuid bundleId;
            uuid meshId;

            asset_meta bundleMeta;
            asset_meta meshMeta;

            ASSERT_TRUE(registry.find_asset_by_path(dirName / file.filename(), bundleId, bundleMeta));
            ASSERT_TRUE(registry.find_asset_by_path(dirName / "Mesh", meshId, meshMeta));

            ASSERT_EQ(bundleMeta.type, get_type_id<scene::bundle>());
            ASSERT_EQ(meshMeta.type, get_type_id<scene::model>());

            const auto bundleResource = resources.get_resource(bundleMeta.id);
            ASSERT_TRUE(bundleResource);

            const auto meshResource = resources.get_resource(meshMeta.id);
            ASSERT_TRUE(meshResource);
        }
    }
}