#pragma once

namespace oblo::editor
{
    struct window_update_context;

    class inspector final
    {
    public:
        bool update(const window_update_context&);
    };
}