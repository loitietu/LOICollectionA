#ifndef LOICOLLECTION_A_MARKETPLUGIN_H
#define LOICOLLECTION_A_MARKETPLUGIN_H

#include <map>
#include <string>

namespace marketPlugin {
    namespace MainGui {
        void buyItem(void* player_ptr, std::string mItemId);
        void itemContent(void* player_ptr, std::string mItemId);
        void sellItem(void* player_ptr);
        void sellItemContent(void* player_ptr);
        void sell(void* player_ptr);
        void buy(void* player_ptr);
    }

    void delItem(std::string mItemId);

    void registery(void* database, std::map<std::string, std::string>& options);
    void unregistery();
}

#endif