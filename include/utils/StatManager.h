#pragma once

namespace StatManager {

    
    inline bool enable_set_stat = false;
    inline bool enable_get_stat = false;
    inline bool enable_copy_stat = false;
    inline bool enable_filtered_stat = false;
    inline bool needs_profile_translation = false;

    inline void disable_all_flags() {
        enable_set_stat = false;
        enable_get_stat = false;
        enable_copy_stat = false;
        enable_filtered_stat = false;
        needs_profile_translation = false;
    };


}