#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <functional>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace ll::form {
    class SimpleForm;
};

namespace LOICollection::Form {
    class PaginatedForm : public std::enable_shared_from_this<PaginatedForm> {
    public:
        using FormCallback = std::function<void(Player& player, const std::string& response)>;
        using FormCallbackIndex = std::function<void(Player& player, int index)>;
        using ButtonCallback = std::function<void(Player& player)>;
        using CloseCallback = std::function<void(Player& player)>;

    public:
        struct Page {
            int page = 0;

            std::shared_ptr<ll::form::SimpleForm> form = nullptr; 

            std::vector<std::pair<std::string, std::string>> elements;
        };

    public:
        LOICOLLECTION_A_API   PaginatedForm(
            const std::string& title,
            const std::string& content,
            const std::vector<std::pair<std::string, std::string>>& elements,
            int elementPerPage = 10
        );
        LOICOLLECTION_A_API   PaginatedForm(
            const std::string& title,
            const std::string& content,
            const std::vector<std::string>& elements,
            int elementPerPage = 10
        );
        LOICOLLECTION_A_API   ~PaginatedForm();

        LOICOLLECTION_A_NDAPI int getNumPages() const;
        LOICOLLECTION_A_NDAPI int getCurrentPage() const;
        LOICOLLECTION_A_NDAPI int getElementPerPage() const;

        LOICOLLECTION_A_API   std::optional<Page> getPage(int page);

        LOICOLLECTION_A_API   void setTitle(const std::string& title);
        LOICOLLECTION_A_API   void setContent(const std::string& content);
        LOICOLLECTION_A_API   void setPreviousButton(const std::string& text);
        LOICOLLECTION_A_API   void setNextButton(const std::string& text);
        LOICOLLECTION_A_API   void setChooseButton(const std::string& text);
        LOICOLLECTION_A_API   void setChooseInput(const std::string& text);

        LOICOLLECTION_A_API   void setCallback(FormCallback callback);
        LOICOLLECTION_A_API   void setCallback(FormCallbackIndex callback);
        LOICOLLECTION_A_API   void setCloseCallback(CloseCallback callback);

        LOICOLLECTION_A_API   void appendHeader(const std::string& text);
        LOICOLLECTION_A_API   void appendLabel(const std::string& text);
        LOICOLLECTION_A_API   void appendDivider();
        LOICOLLECTION_A_API   void appendButton(const std::string& text, const std::string& image, ButtonCallback callback);

        LOICOLLECTION_A_API   void sendPage(Player& player, int page);
        LOICOLLECTION_A_API   void sendChoosedPage(Player& player);
        
    private:
        void refreshPage(Page& page);

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
