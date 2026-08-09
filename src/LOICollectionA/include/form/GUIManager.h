#pragma once

#include <memory>
#include <string>
#include <functional>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace CustomFormClass {
    struct CustomFormHandle;
}

namespace MessageBoxClass {
    struct MessageBoxHandle;
}

namespace LOICollection::frontend {
    struct ArrayValue;
    using ArrayRef = std::shared_ptr<ArrayValue>;
}

namespace LOICollection::form {
    enum class GUIManagerType : int {
        CustomForm = 1,
        MessageBox = 2
    };

    class GUIManager {
    public:
        using ValueCallback = std::function<frontend::ArrayRef(Player&)>;
        using Callback = std::function<ll::Expected<void>(frontend::ArrayRef, Player&)>;

    public:
        ~GUIManager();

        GUIManager(GUIManager const&) = delete;
        GUIManager(GUIManager&&) = delete;
        GUIManager& operator=(GUIManager const&) = delete;
        GUIManager& operator=(GUIManager&&) = delete;

        LOICOLLECTION_A_NDAPI static GUIManager& getInstance();

        LOICOLLECTION_A_API   ll::Expected<void> load(const std::string& id, const std::string& path);
        LOICOLLECTION_A_API   ll::Expected<void> open(const std::string& id, const std::string& formId, GUIManagerType type, Player& player);
        LOICOLLECTION_A_API   ll::Expected<void> switchToCustomForm(const std::string& id, Player& player);
        LOICOLLECTION_A_API   ll::Expected<void> switchToMessageBox(const std::string& id, Player& player);

        LOICOLLECTION_A_API   void registerCustomFormUI(const std::string& id, std::shared_ptr<CustomFormClass::CustomFormHandle> form, Player& player);
        LOICOLLECTION_A_API   void registerMessageBoxUI(const std::string& id, std::shared_ptr<MessageBoxClass::MessageBoxHandle> box, Player& player);

        LOICOLLECTION_A_NDAPI ll::Expected<std::shared_ptr<CustomFormClass::CustomFormHandle>> getCustomFormUI(const std::string& id, Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<std::shared_ptr<MessageBoxClass::MessageBoxHandle>> getMessageBoxUI(const std::string& id, Player& player);

        LOICOLLECTION_A_API   void registerValue(const std::string& id, ValueCallback callback);
        LOICOLLECTION_A_API   void registerCallback(const std::string& id, Callback callback);

        LOICOLLECTION_A_NDAPI ll::Expected<frontend::ArrayRef> getValue(const std::string& id, Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> getCallback(const std::string& id, frontend::ArrayRef args, Player& player);

    private:
        GUIManager();

        ll::Expected<std::string> readFile(const std::string& path);

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
