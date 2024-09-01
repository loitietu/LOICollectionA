#ifndef LOICOLLECTION_A_ACPLUGIN_H
#define LOICOLLECTION_A_ACPLUGIN_H

namespace announcementPlugin {
    namespace MainGui {
        void setting(void* player_ptr);
        void open(void* player_ptr);
    }

    void registery(void* database);
    void unregistery();
}

#endif