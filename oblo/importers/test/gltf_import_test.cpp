#include <gtest/gtest.h>

#include <oblo/asset/asset_registry.hpp>
#include <oblo/asset/importer.hpp>
#include <oblo/asset/importers/registration.hpp>
#include <oblo/asset/meta.hpp>
#include <oblo/scene/assets/bundle.hpp>
#include <oblo/scene/assets/model.hpp>
#include <oblo/scene/assets/registration.hpp>

namespace oblo::asset::importers
{
    namespace
    {
        void clear_directoy(const std::filesystem::path& path)
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            ASSERT_FALSE(ec);
        }
    }

    TEST(gltf_importer, box)
    {
        asset_registry registry;

        const std::filesystem::path testDir{"./test/gltf_importer_suzanne/"};
        const std::filesystem::path assetsDir{testDir / "assets"};
        const std::filesystem::path artifactsDir{testDir / "artifacts"};

        clear_directoy(testDir);

        ASSERT_TRUE(registry.initialize(assetsDir, artifactsDir));

        scene::register_asset_types(registry);

        register_gltf_importer(registry);

        const std::filesystem::path gltfSampleModels{OBLO_GLTF_SAMPLE_MODELS};

        auto importer = registry.create_importer(gltfSampleModels / "2.0" / "Box" / "glTF-Embedded" / "Box.gltf");

        ASSERT_TRUE(importer.is_valid());

        ASSERT_TRUE(importer.init());
        ASSERT_TRUE(importer.execute("Box"));

        uuid bundleId;
        uuid meshId;

        asset_meta bundleMeta;
        asset_meta meshMeta;

        ASSERT_TRUE(registry.find_asset_by_path("Box/Box", bundleId, bundleMeta));
        ASSERT_TRUE(registry.find_asset_by_path("Box/Mesh", meshId, meshMeta));

        ASSERT_EQ(bundleMeta.type, get_type_id<scene::bundle>());
        ASSERT_EQ(meshMeta.type, get_type_id<scene::model>());
    }
}