#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <optional>

#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>

#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

namespace LOICollection::form {
    struct Element {
        enum class ElementType {
            Header,
            Label,
            Button,
            Divider
        };

        [[nodiscard]] virtual ElementType getType() const = 0;

        virtual ~Element() = default;
    };

    struct Button : Element {
        std::string text;
        std::string image;
        PaginatedForm::ButtonCallback callback;

        [[nodiscard]] ElementType getType() const override {
            return ElementType::Button;
        }
    };

    struct Label : Element {
        std::string text;

        [[nodiscard]] ElementType getType() const override {
            return ElementType::Label;
        }
    };

    struct Header : Element {
        std::string text;

        [[nodiscard]] ElementType getType() const override {
            return ElementType::Header;
        }
    };

    struct Divider : Element {
        [[nodiscard]] ElementType getType() const override {
            return ElementType::Divider;
        }
    };

    struct PaginatedForm::Impl {
        std::string mTitle;
        std::string mContent;

        std::string mPreviousButton;
        std::string mNextButton;
        std::string mChooseButton;
        std::string mInputLabel;

        int mNumPerPage = 0;
        int mNumCurrentPage = 1;
        int mNumElementPerPage = 10;
    
        std::vector<std::unique_ptr<Page>> mPages;
        std::vector<std::unique_ptr<Element>> mElements;

        FormCallback mCallback;
        FormCallbackIndex mCallbackIndex;
        CloseCallback mCloseCallback;
    };

    PaginatedForm::PaginatedForm(
        const std::string& title,
        const std::string& content,
        const std::vector<std::pair<std::string, std::string>>& elements,
        int elementPerPage
    ) : mImpl(std::make_unique<Impl>()) {
        this->mImpl->mTitle = title;
        this->mImpl->mContent = content;
        this->mImpl->mNumElementPerPage = std::max(elementPerPage, 1);

        if (elements.empty()) {
            this->mImpl->mNumPerPage = 1;

            Page mPage;
            mPage.page = 1;

            this->mImpl->mPages.emplace_back(std::make_unique<Page>(std::move(mPage)));

            return;
        }

        for (auto&& chunk : elements | std::views::chunk(this->mImpl->mNumElementPerPage)) {
            ++this->mImpl->mNumPerPage;

            Page mPage;
            mPage.page = this->mImpl->mNumPerPage;
            mPage.elements = std::vector<std::pair<std::string, std::string>>(chunk.begin(), chunk.end());

            this->mImpl->mPages.emplace_back(std::make_unique<Page>(std::move(mPage)));
        }
    }

    PaginatedForm::PaginatedForm(
        const std::string& title,
        const std::string& content,
        const std::vector<std::string>& elements,
        int elementPerPage
    ) : mImpl(std::make_unique<Impl>()) {
        this->mImpl->mTitle = title;
        this->mImpl->mContent = content;
        this->mImpl->mNumElementPerPage = std::max(elementPerPage, 1);

        if (elements.empty()) {
            this->mImpl->mNumPerPage = 1;

            Page mPage;
            mPage.page = 1;

            this->mImpl->mPages.emplace_back(std::make_unique<Page>(std::move(mPage)));

            return;
        }

        for (auto&& chunk : elements | std::views::chunk(this->mImpl->mNumElementPerPage)) {
            ++this->mImpl->mNumPerPage;

            Page mPage;
            mPage.page = this->mImpl->mNumPerPage;
            mPage.elements = chunk | std::views::transform([](const std::string& str) {
                return std::make_pair(str, "");
            }) | std::ranges::to<std::vector<std::pair<std::string, std::string>>>();

            this->mImpl->mPages.emplace_back(std::make_unique<Page>(std::move(mPage)));
        }
    }

    PaginatedForm::~PaginatedForm() = default;

    void PaginatedForm::refreshPage(Page& page) {
        page.form = std::make_shared<ll::form::SimpleForm>(
            this->mImpl->mTitle + " [" + std::to_string(page.page) + "/" + std::to_string(this->getNumPages()) + "]",
            this->mImpl->mContent
        );

        for (const auto& [index, element] : std::views::enumerate(page.elements)) {
            page.form->appendButton(element.first, element.second, "path", [self = shared_from_this(), element = element.first, page = page.page, index](Player& pl) -> void {
                if (self->mImpl->mCallback)
                    self->mImpl->mCallback(pl, element);

                if (self->mImpl->mCallbackIndex)
                    self->mImpl->mCallbackIndex(pl, (page - 1) * self->mImpl->mNumElementPerPage + static_cast<int>(index));
            });
        }

        if (this->getNumPages() > 1)
            page.form->appendDivider();
        
        if (this->getCurrentPage() > 1) {
            page.form->appendButton(this->mImpl->mPreviousButton, [self = shared_from_this()](Player& pl) -> void {
                self->sendPage(pl, self->getCurrentPage() - 1);
            });
        }

        if (this->getCurrentPage() < this->getNumPages()) {
            page.form->appendButton(this->mImpl->mNextButton, [self = shared_from_this()](Player& pl) -> void  {
                self->sendPage(pl, self->getCurrentPage() + 1);
            });
        }
        
        if (this->getNumPages() > 2) {
            page.form->appendButton(this->mImpl->mChooseButton, [self = shared_from_this()](Player& pl) -> void {
                self->sendChoosedPage(pl);
            });
        }

        for (auto& element : this->mImpl->mElements) {
            switch (element->getType()) {
                case Element::ElementType::Header:
                    page.form->appendHeader(static_cast<Header&>(*element).text);
                    break;
                case Element::ElementType::Label:
                    page.form->appendLabel(static_cast<Label&>(*element).text);
                    break;
                case Element::ElementType::Divider:
                    page.form->appendDivider();
                    break;
                case Element::ElementType::Button: {
                    auto mButton = static_cast<Button&>(*element);

                    page.form->appendButton(mButton.text, mButton.image, "path", [callback = mButton.callback](Player& pl) -> void {
                        if (!callback)
                            return;
                        
                        callback(pl);
                    });
                    break;
                }
            }
        }
    }

    int PaginatedForm::getNumPages() const {
        return this->mImpl->mNumPerPage;
    }

    int PaginatedForm::getCurrentPage() const {
        return this->mImpl->mNumCurrentPage;
    }

    int PaginatedForm::getElementPerPage() const {
        return this->mImpl->mNumElementPerPage;
    }

    std::optional<std::reference_wrapper<PaginatedForm::Page>> PaginatedForm::getPage(int page) {
        if (this->mImpl->mPages.empty())
            return std::nullopt;

        if (page < 2)
            return *this->mImpl->mPages.front();

        if (page >= this->getNumPages())
            return *this->mImpl->mPages.back();

        return *this->mImpl->mPages.at(page - 1);
    }

    void PaginatedForm::setTitle(const std::string& title) {
        this->mImpl->mTitle = title;
    }

    void PaginatedForm::setContent(const std::string& content) {
        this->mImpl->mContent = content;
    }

    void PaginatedForm::setPreviousButton(const std::string& text) {
        this->mImpl->mPreviousButton = text;
    }
    
    void PaginatedForm::setNextButton(const std::string& text) {
        this->mImpl->mNextButton = text;
    }

    void PaginatedForm::setChooseButton(const std::string& text) {
        this->mImpl->mChooseButton = text;
    }

    void PaginatedForm::setChooseInput(const std::string& text) {
        this->mImpl->mInputLabel = text;
    }

    void PaginatedForm::setCallback(FormCallback callback) {
        this->mImpl->mCallback = std::move(callback);
    }

    void PaginatedForm::setCallback(FormCallbackIndex callback) {
        this->mImpl->mCallbackIndex = std::move(callback);
    }

    void PaginatedForm::setCloseCallback(CloseCallback callback) {
        this->mImpl->mCloseCallback = std::move(callback);
    }

    void PaginatedForm::appendHeader(const std::string& text) {
        Header header;
        header.text = text;

        this->mImpl->mElements.emplace_back(std::make_unique<Header>(std::move(header)));
    }

    void PaginatedForm::appendLabel(const std::string& text) {
        Label label;
        label.text = text;

        this->mImpl->mElements.emplace_back(std::make_unique<Label>(std::move(label)));
    }

    void PaginatedForm::appendDivider() {
        Divider divider;

        this->mImpl->mElements.emplace_back(std::make_unique<Divider>(std::move(divider)));
    }

    void PaginatedForm::appendButton(const std::string& text, const std::string& image, ButtonCallback callback) {
        Button button;
        button.text = text;
        button.image = image;
        button.callback = std::move(callback);

        this->mImpl->mElements.emplace_back(std::make_unique<Button>(std::move(button)));
    }

    void PaginatedForm::sendPage(Player& player, int page) {
        auto mPage = this->getPage(page);
        if (!mPage.has_value())
            return;

        this->mImpl->mNumCurrentPage = std::min(page, this->getNumPages());

        this->refreshPage(*mPage);

        mPage.value().get().form->sendTo(player, [self = shared_from_this()](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1 && self->mImpl->mCloseCallback) 
                self->mImpl->mCloseCallback(pl);
        });
    }

    void PaginatedForm::sendChoosedPage(Player& player) {
        ll::form::CustomForm form(this->mImpl->mTitle);
        form.appendInput("Input", this->mImpl->mInputLabel);
        form.sendTo(player, [self = shared_from_this()](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return self->sendPage(pl, 1);

            self->sendPage(pl, SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0));
        });
    }
}
