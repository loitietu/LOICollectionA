#ifndef LOICOLLECTION_A_CDK_H
#define LOICOLLECTION_A_CDK_H

#include <string>

namespace cdkPlugin {
    namespace MainGui {
        void convert(void* player_ptr);
        void cdkNew(void* player_ptr);
        void cdkRemove(void* player_ptr);
        void cdkAwardScore(void* player_ptr);
        void cdkAwardItem(void* player_ptr);
        void cdkAward(void* player_ptr);
        void open(void* player_ptr);
    }

    void registery(void* database);
    void cdkConvert(void* player_ptr, std::string convertString);
}

#endif