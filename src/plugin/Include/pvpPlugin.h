#ifndef LOICOLLECTION_A_PVPPLUGIN_H
#define LOICOLLECTION_A_PVPPLUGIN_H

namespace pvpPlugin {
    namespace MainGui {
        void open(void* player_ptr);
    }

    bool isEnable(void* player_ptr);

    void registery(void* database);
    void unregistery();
}

#endif