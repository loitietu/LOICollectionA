#ifndef LOICOLLECTION_A_TPAPLUGIN_H
#define LOICOLLECTION_A_TPAPLUGIN_H

namespace tpaPlugin {
    namespace MainGui {
        void setting(void* player_ptr);
        void tpa(void* player_ptr, void* target_ptr, bool type);
        void open(void* player_ptr);
    }

    void registery(void* database);
    void unregistery();

    bool isInvite(void* player_ptr);
}

#endif