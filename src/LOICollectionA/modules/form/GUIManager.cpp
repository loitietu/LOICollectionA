#include <memory>
#include <string>
#include <fstream>
#include <filesystem>
#include <functional>
#include <unordered_map>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/ui/form/CustomForm.h>
#include <ll/api/ui/form/MessageBox.h>

#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"
#include "LOICollectionA/frontend/SemanticAnalyzer.h"

#include "LOICollectionA/frontend/ir/VM.h"
#include "LOICollectionA/frontend/ir/Compiler.h"
#include "LOICollectionA/frontend/ir/Optimizer.h"

#include "LOICollectionA/frontend/builtin/ui/form/CustomFormClass.h"
#include "LOICollectionA/frontend/builtin/ui/form/MessageBoxClass.h"

#include "LOICollectionA/include/form/GUIManager.h"

namespace LOICollection::form {
    struct GUIManager::Impl {
        std::unordered_map<std::string, std::shared_ptr<frontend::ir::BytecodeChunk>> cache;

        std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<CustomFormClass::CustomFormHandle>>> forms;
        std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<MessageBoxClass::MessageBoxHandle>>> boxs;

        std::unordered_map<std::string, ValueCallback> values;
        std::unordered_map<std::string, Callback> callbacks;
    };

    GUIManager::GUIManager() : mImpl(std::make_unique<Impl>()) {}
    GUIManager::~GUIManager() = default;

    GUIManager& GUIManager::getInstance() {
        static GUIManager instance;
        return instance;
    }

    ll::Expected<std::string> GUIManager::readFile(const std::string& path) {
        std::error_code ec;
        auto size = std::filesystem::file_size(path, ec);
        if (ec)
            return ll::makeErrorCodeError(ec);

        std::ifstream file(path, std::ios::binary);
        if (!file)
            return ll::makeErrorCodeError(std::make_error_code(std::errc::no_such_file_or_directory));

        std::string content(size, '\0');
        if (!file.read(content.data(), static_cast<std::streamsize>(size)))
            return ll::makeErrorCodeError(std::make_error_code(std::errc::io_error));

        return content;
    }

    ll::Expected<void> GUIManager::load(const std::string& id, const std::string& path) {
        auto content = this->readFile(path);
        if (!content.has_value())
            return ll::Unexpected(content.error());

        frontend::DiagnosticEngine diagnostics;

        frontend::ir::Compiler mCompiler(diagnostics);

        frontend::Lexer mLexer(content.value(), diagnostics);
        frontend::Parser mParser(mLexer, diagnostics);

        auto mAst = mParser.parse();
        if (diagnostics.hasErrors())
            return ll::makeStringError(diagnostics.getErrorMessage());

        frontend::SemanticAnalyzer analyzer(diagnostics);
        if (mAst->getType() == frontend::ASTNode::Type::Program)
            analyzer.analyze(static_cast<frontend::ProgramNode&>(*mAst));

        if (diagnostics.hasErrors())
            return ll::makeStringError(diagnostics.getErrorMessage());

        if (diagnostics.hasWarnings())
            return ll::makeStringError(diagnostics.getWarningMessage());

        auto bytecode = std::make_shared<frontend::ir::BytecodeChunk>(mCompiler.compile(*mAst));
        if (diagnostics.hasErrors())
            return ll::makeStringError(diagnostics.getErrorMessage());

        frontend::ir::Optimizer optimizer;
        optimizer.optimize(*bytecode);

        this->mImpl->cache.emplace(id, bytecode);

        return {};
    }

    ll::Expected<void> GUIManager::open(const std::string& id, const std::string& formId, GUIManagerType type, Player& player) {
        frontend::DiagnosticEngine diagnostics;

        frontend::ir::VM mVM(diagnostics);

        if (this->mImpl->cache.contains(id)) {
            auto cached = this->mImpl->cache.at(id);

            auto result = mVM.run(cached, { std::ref(player) });
            if (diagnostics.hasErrors())
                return ll::makeStringError(diagnostics.getErrorMessage());

            switch (type) {
                case GUIManagerType::CustomForm: return this->switchToCustomForm(formId, player);
                case GUIManagerType::MessageBox: return this->switchToMessageBox(formId, player); 
            }
        }

        return ll::makeStringError("open: No corresponding bytecode cache was found");
    }

    ll::Expected<void> GUIManager::switchToCustomForm(const std::string& id, Player& player) {
        auto handle = this->getCustomFormUI(id, player);
        if (!handle.has_value())
            return ll::Unexpected(handle.error());

        if (!handle.value()->show) {
            auto result = handle.value()->base->show([this, id, player = std::ref(player)](ll::ui::ScreenSession::Result) mutable -> void {
                this->unregisterCustomFormUI(id, player.get());
            });
            if (!result) {
                this->unregisterCustomFormUI(id, player);

                return ll::Unexpected(result.error());
            }

            return {};
        }

        auto result = handle.value()->base->show([this, id, handle = handle.value(), player = std::ref(player)](ll::ui::ScreenSession::Result closeResult) mutable -> void {
            frontend::DiagnosticEngine diagnostics;
            frontend::CallbackTypeValues values;

            if (closeResult.has_value())
                values.emplace_back(static_cast<int>(*closeResult));
            else
                values.emplace_back(std::monostate{});

            [[maybe_unused]] auto cbResult = frontend::ir::VM::callFunctionRef(
                handle->show, values, frontend::Context{ player }.params, diagnostics
            );

            if (diagnostics.hasErrors()) {
                ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA")
                    ->error("CustomForm::show callback: {}", diagnostics.getErrorMessage());
            }
            
            this->unregisterCustomFormUI(id, player.get());
        });

        if (!result) {
            this->unregisterCustomFormUI(id, player);

            return ll::Unexpected(result.error());
        }

        return {};
    }

    ll::Expected<void> GUIManager::switchToMessageBox(const std::string& id, Player& player) {
        auto handle = this->getMessageBoxUI(id, player);
        if (!handle.has_value())
            return ll::Unexpected(handle.error());

        if (!handle.value()->show) {
            auto result = handle.value()->base->show([this, id, player = std::ref(player)](ll::ui::MessageBox::Result) mutable -> void{
                this->unregisterMessageBoxUI(id, player.get());
            });
            if (!result) {
                this->unregisterMessageBoxUI(id, player);

                return ll::Unexpected(result.error());
            }

            return {};
        }

        auto result = handle.value()->base->show([this, id, handle = handle.value(), player = std::ref(player)](ll::ui::MessageBox::Result closeResult) mutable -> void {
            frontend::DiagnosticEngine diagnostics;
            frontend::CallbackTypeValues values;

            if (closeResult.has_value()) {
                auto resultObj = std::make_shared<frontend::Object>();
                resultObj->className = "MessageBoxResult";
                resultObj->classIndex = -1;
                resultObj->fields["closeReason"] = static_cast<int>(closeResult->closeReason);
                resultObj->fields["selection"] = closeResult->selection
                    ? frontend::TypedValue(static_cast<int>(*closeResult->selection))
                    : frontend::TypedValue(std::monostate{});
                
                values.emplace_back(resultObj);
            } else {
                values.emplace_back(std::monostate{});
            }

            [[maybe_unused]] auto cbResult = frontend::ir::VM::callFunctionRef(
                handle->show, values, frontend::Context{ player }.params, diagnostics
            );

            if (diagnostics.hasErrors()) {
                ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA")
                    ->error("MessageBox::show callback: {}", diagnostics.getErrorMessage());
            }
            
            this->unregisterMessageBoxUI(id, player.get());
        });

        if (!result) {
            this->unregisterMessageBoxUI(id, player);
            
            return ll::Unexpected(result.error());
        }

        return {};
    }

    void GUIManager::registerCustomFormUI(const std::string& id, std::shared_ptr<CustomFormClass::CustomFormHandle> form, Player& player) {
        auto [it, _] = this->mImpl->forms.try_emplace(player.getUuid().asString());
        it->second.insert_or_assign(id, std::move(form));
    }

    void GUIManager::registerMessageBoxUI(const std::string& id, std::shared_ptr<MessageBoxClass::MessageBoxHandle> box, Player& player) {
        auto [it, _] = this->mImpl->boxs.try_emplace(player.getUuid().asString());
        it->second.insert_or_assign(id, std::move(box));
    }

    bool GUIManager::unregisterCustomFormUI(const std::string& id, Player& player) {
        std::string uuid = player.getUuid().asString();

        if (auto it = this->mImpl->forms.find(uuid); it != this->mImpl->forms.end()) {
            it->second.erase(id);
            if (it->second.empty())
                this->mImpl->forms.erase(it);

            return true;
        }

        return false;
    }

    bool GUIManager::unregisterMessageBoxUI(const std::string& id, Player& player) {
        std::string uuid = player.getUuid().asString();

        if (auto it = this->mImpl->boxs.find(uuid); it != this->mImpl->boxs.end()) {
            it->second.erase(id);
            if (it->second.empty())
                this->mImpl->boxs.erase(it);

            return true;
        }

        return false;
    }

    ll::Expected<std::shared_ptr<CustomFormClass::CustomFormHandle>> GUIManager::getCustomFormUI(const std::string& id, Player& player) {
        if (auto it = this->mImpl->forms.find(player.getUuid().asString()); it != this->mImpl->forms.end()) {
            auto& innerMap = it->second;
            if (auto innerIt = innerMap.find(id); innerIt != innerMap.end())
                return innerIt->second;
            
            return ll::makeStringError("Form not found for player: " + id);
        }

        return ll::makeStringError("Player has no registered forms");
    }

    ll::Expected<std::shared_ptr<MessageBoxClass::MessageBoxHandle>> GUIManager::getMessageBoxUI(const std::string& id, Player& player) {
        if (auto it = this->mImpl->boxs.find(player.getUuid().asString()); it != this->mImpl->boxs.end()) {
            auto& innerMap = it->second;
            if (auto innerIt = innerMap.find(id); innerIt != innerMap.end())
                return innerIt->second;
            
            return ll::makeStringError("MessageBox not found for player: " + id);
        }

        return ll::makeStringError("Player has no registered messageboxs");
    }

    void GUIManager::registerValue(const std::string& id, ValueCallback callback) {
        this->mImpl->values.insert_or_assign(id, std::move(callback));
    }

    void GUIManager::registerCallback(const std::string& id, Callback callback) {
        this->mImpl->callbacks.insert_or_assign(id, std::move(callback));
    }

    ll::Expected<frontend::ArrayRef> GUIManager::getValue(const std::string& id, Player& player) {
        auto it = this->mImpl->values.find(id);
        if (it == this->mImpl->values.end())
            return ll::makeStringError("Value not registered: " + id);

        return it->second(player);
    }

    ll::Expected<void> GUIManager::getCallback(const std::string& id, frontend::ArrayRef args, Player& player) {
        auto it = this->mImpl->callbacks.find(id);
        if (it == this->mImpl->callbacks.end())
            return ll::makeStringError("Callback not registered: " + id);

        return it->second(args, player);
    }
}
