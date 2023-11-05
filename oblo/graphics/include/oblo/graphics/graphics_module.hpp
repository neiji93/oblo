#pragma once

#include <oblo/modules/module_interface.hpp>

namespace oblo::graphics
{
    class graphics_module final : public module_interface
    {
    public:
        bool startup() override;
        void shutdown() override;
    };
}