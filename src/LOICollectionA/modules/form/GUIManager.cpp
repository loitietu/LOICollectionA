#include <memory>
#include <string>
#include <fstream>
#include <filesystem>
#include <functional>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

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
#include "LOICollectionA/frontend/ir/BytecodeCache.h"

#include "LOICollectionA/frontend/builtin/ui/form/CustomFormClass.h"
#include "LOICollectionA/frontend/builtin/ui/form/MessageBoxClass.h"
#include "LOICollectionA/frontend/builtin/ui/form/PaginatedFormClass.h"
#include "LOICollectionA/frontend/builtin/ui/form/ScriptFormClass.h"

#include "LOICollectionA/include/form/GUIManager.h"

namespace LOICollection::form {
    struct GUIManager::Impl {
        std::unordered_map<std::string, std::shared_ptr<frontend::ir::BytecodeChunk>> cache;

        std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<CustomFormClass::CustomFormHandle>>> forms;
        std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<MessageBoxClass::MessageBoxHandle>>> boxs;
        std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<PaginatedFormClass::PaginatedFormHandle>>> paginatedForms;
        std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<ScriptFormClass::ScriptFormHandle>>> scriptForms;

        std::unordered_map<std::string, ValueCallback> values;
        std::unordered_map<std::string, RequestCallback> requests;
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

    std::unique_ptr<frontend::ProgramNode> GUIManager::parseFile(
        const std::string& path, frontend::DiagnosticEngine& diagnostics
    ) {
        auto content = this->readFile(path);
        if (!content.has_value()) {
            diagnostics.addError({}, "Failed to read file: " + path);
            return nullptr;
        }

        frontend::Lexer lexer(content.value(), diagnostics);
        frontend::Parser parser(lexer, diagnostics);

        auto ast = parser.parse();
        if (!ast)
            return nullptr;

        return std::unique_ptr<frontend::ProgramNode>(
            static_cast<frontend::ProgramNode*>(ast.release())
        );
    }

    bool GUIManager::collectImports(
        const std::string& path,
        const std::filesystem::path& baseDir,
        std::vector<std::string>& stack,
        std::unordered_set<std::string>& visited,
        std::vector<std::string>& order,
        std::unordered_map<std::string, std::unique_ptr<frontend::ProgramNode>>& parsed,
        frontend::DiagnosticEngine& diagnostics
    ) {
        std::string resolved = std::filesystem::weakly_canonical(path).string();

        if (visited.contains(resolved))
            return true;

        if (auto it = std::ranges::find(stack, resolved); it != stack.end()) {
            std::string ring;
            for (; it != stack.end(); ++it)
                ring += (ring.empty() ? "" : " -> ") + *it;
            ring += " -> " + resolved;

            diagnostics.addError({}, "Circular import: " + ring);
            return false;
        }

        stack.push_back(resolved);

        if (!parsed.contains(resolved)) {
            auto ast = this->parseFile(resolved, diagnostics);
            if (!ast) {
                stack.pop_back();
                return false;
            }
            parsed.emplace(resolved, std::move(ast));
        }

        for (auto& part : parsed.at(resolved)->parts) {
            if (part->getType() != frontend::ASTNode::Type::Import)
                continue;

            auto& import = static_cast<frontend::ImportNode&>(*part);
            auto depPath = (baseDir / import.path).lexically_normal().string();

            if (!this->collectImports(depPath, baseDir, stack, visited, order, parsed, diagnostics)) {
                stack.pop_back();
                return false;
            }
        }

        stack.pop_back();
        visited.insert(resolved);
        order.push_back(resolved);
        return true;
    }

    namespace {
        bool isTopLevelDefinition(frontend::ASTNode::Type type) {
            return type == frontend::ASTNode::Type::Class ||
                   type == frontend::ASTNode::Type::FunctionDef ||
                   type == frontend::ASTNode::Type::Using;
        }

        std::string definitionName(frontend::ASTNode& node) {
            switch (node.getType()) {
                case frontend::ASTNode::Type::Class:
                    return static_cast<frontend::ClassNode&>(node).name;
                case frontend::ASTNode::Type::Using:
                    return static_cast<frontend::UsingNode&>(node).name;
                default:
                    return {};
            }
        }

        /* Source location of any top-level part. The base ASTNode exposes no
         * `loc` member, so it is recovered per concrete node type. */
        frontend::SourceLocation nodeLoc(const frontend::ASTNode& node) {
            switch (node.getType()) {
                case frontend::ASTNode::Type::Class:
                    return static_cast<const frontend::ClassNode&>(node).loc;
                case frontend::ASTNode::Type::FunctionDef:
                    return static_cast<const frontend::FunctionDefNode&>(node).loc;
                case frontend::ASTNode::Type::Using:
                    return static_cast<const frontend::UsingNode&>(node).loc;
                case frontend::ASTNode::Type::Import:
                    return static_cast<const frontend::ImportNode&>(node).loc;
                case frontend::ASTNode::Type::Assignment:
                    return static_cast<const frontend::AssignmentNode&>(node).loc;
                case frontend::ASTNode::Type::CompoundAssign:
                    return static_cast<const frontend::CompoundAssignNode&>(node).loc;
                case frontend::ASTNode::Type::If:
                    return static_cast<const frontend::IfNode&>(node).loc;
                case frontend::ASTNode::Type::While:
                    return static_cast<const frontend::WhileNode&>(node).loc;
                case frontend::ASTNode::Type::For:
                    return static_cast<const frontend::ForNode&>(node).loc;
                case frontend::ASTNode::Type::ForIn:
                    return static_cast<const frontend::ForInNode&>(node).loc;
                case frontend::ASTNode::Type::Return:
                    return static_cast<const frontend::ReturnNode&>(node).loc;
                case frontend::ASTNode::Type::Break:
                    return static_cast<const frontend::BreakNode&>(node).loc;
                case frontend::ASTNode::Type::Continue:
                    return static_cast<const frontend::ContinueNode&>(node).loc;
                case frontend::ASTNode::Type::FuncCall:
                    return static_cast<const frontend::FuncCallNode&>(node).loc;
                case frontend::ASTNode::Type::Function:
                    return static_cast<const frontend::FunctionNode&>(node).loc;
                case frontend::ASTNode::Type::Macro:
                    return static_cast<const frontend::MacroNode&>(node).loc;
                default:
                    return {};
            }
        }

        struct DefinitionOrigin {
            std::string file;
            frontend::SourceLocation loc;
        };
    }

    std::unique_ptr<frontend::ProgramNode> GUIManager::buildProgram(
        const std::string& path, frontend::DiagnosticEngine& diagnostics,
        std::vector<std::string>* importOrder
    ) {
        auto program = std::make_unique<frontend::ProgramNode>();

        std::filesystem::path baseDir = std::filesystem::path(path).parent_path();
        std::string rootPath = std::filesystem::weakly_canonical(path).string();

        std::vector<std::string> stack;
        std::unordered_set<std::string> visited;
        std::vector<std::string> order;
        std::unordered_map<std::string, std::unique_ptr<frontend::ProgramNode>> parsed;

        if (!this->collectImports(path, baseDir, stack, visited, order, parsed, diagnostics))
            return program;

        if (importOrder) {
            importOrder->clear();
            for (const auto& dep : order)
                if (dep != rootPath)
                    importOrder->push_back(dep);
        }

        /* Definition origins across files, for conflict diagnostics that point
         * at both the original and the colliding definition. */
        std::unordered_map<std::string, DefinitionOrigin> origins;

        auto merge = [&](const std::string& file, bool isRoot) -> bool {
            auto it = parsed.find(file);
            if (it == parsed.end())
                return false;

            for (auto& part : it->second->parts) {
                if (part->getType() == frontend::ASTNode::Type::Import)
                    continue;

                const auto loc = nodeLoc(*part);

                if (!isRoot && !isTopLevelDefinition(part->getType())) {
                    diagnostics.addError(loc,
                        "Imported file '" + file + "' must only contain top-level definitions");
                    continue;
                }

                std::string name = isTopLevelDefinition(part->getType()) ? definitionName(*part) : "";
                if (!name.empty()) {
                    if (auto it = origins.find(name); it != origins.end()) {
                        if (it->second.file != file) {
                            diagnostics.addError(loc,
                                "Naming conflict for '" + name + "' between '" + it->second.file +
                                "' (line " + std::to_string(it->second.loc.line) + ") and '" + file +
                                "' (line " + std::to_string(loc.line) + ")");
                            continue;
                        }
                    } else {
                        origins.emplace(name, DefinitionOrigin{ file, loc });
                    }
                }

                program->addPart(std::move(part));
            }

            return true;
        };

        /* Imported files come first (dependency order), then the root file. */
        for (const auto& dep : order) {
            if (dep == rootPath)
                continue;

            if (!merge(dep, false))
                return program;
        }

        if (!merge(rootPath, true))
            return program;

        return program;
    }

    std::string GUIManager::cacheFilePath(const std::string& id, const std::string& path) {
        return (std::filesystem::path(path).parent_path() / "cache" / (id + ".lcbc")).string();
    }

    std::shared_ptr<frontend::ir::BytecodeChunk> GUIManager::loadFromCache(
        const std::string& cachePath, const std::string& sourceHash
    ) {
        auto importPaths = frontend::ir::bytecode_cache::readImportPaths(cachePath);
        if (!importPaths.has_value())
            return nullptr;

        std::vector<frontend::ir::bytecode_cache::ImportHash> imports;
        imports.reserve(importPaths->size());
        for (const auto& importPath : *importPaths) {
            auto content = this->readFile(importPath);
            if (!content.has_value())
                return nullptr;

            imports.push_back({
                importPath, frontend::ir::bytecode_cache::sha256(content.value())
            });
        }

        return frontend::ir::bytecode_cache::read(cachePath, sourceHash, imports);
    }

    ll::Expected<void> GUIManager::load(const std::string& id, const std::string& path) {
        frontend::DiagnosticEngine diagnostics;

        auto content = this->readFile(path);
        if (!content.has_value())
            return ll::Unexpected(content.error());

        const std::string sourceHash = frontend::ir::bytecode_cache::sha256(content.value());
        const std::string cachePath = GUIManager::cacheFilePath(id, path);

        auto bytecode = this->loadFromCache(cachePath, sourceHash);
        if (!bytecode) {
            std::vector<std::string> imports;
            auto program = this->buildProgram(path, diagnostics, &imports);
            if (diagnostics.hasErrors())
                return ll::makeStringError(diagnostics.getErrorMessage());
            if (diagnostics.hasWarnings())
                return ll::makeStringError(diagnostics.getWarningMessage());

            frontend::SemanticAnalyzer analyzer(diagnostics);
            analyzer.analyze(*program);
            if (diagnostics.hasErrors())
                return ll::makeStringError(diagnostics.getErrorMessage());

            frontend::ir::Compiler mCompiler(diagnostics);
            bytecode = std::make_shared<frontend::ir::BytecodeChunk>(mCompiler.compile(*program));
            if (diagnostics.hasErrors())
                return ll::makeStringError(diagnostics.getErrorMessage());

            frontend::ir::Optimizer optimizer;
            optimizer.optimize(*bytecode);

            std::vector<frontend::ir::bytecode_cache::ImportHash> importHashes;
            importHashes.reserve(imports.size());
            for (const auto& importPath : imports) {
                auto importContent = this->readFile(importPath);
                if (!importContent.has_value())
                    return ll::Unexpected(importContent.error());

                importHashes.push_back({
                    importPath, frontend::ir::bytecode_cache::sha256(importContent.value())
                });
            }

            frontend::ir::bytecode_cache::write(cachePath, *bytecode, sourceHash, importHashes);
        }

        this->mImpl->cache.insert_or_assign(id, std::move(bytecode));

        return {};
    }

    ll::Expected<void> GUIManager::execute(const std::string& id) {
        auto it = this->mImpl->cache.find(id);
        if (it == this->mImpl->cache.end())
            return ll::makeStringError("execute: No corresponding bytecode cache was found");

        frontend::DiagnosticEngine diagnostics;
        frontend::ir::VM mVM(diagnostics);

        auto result = mVM.run(it->second, {});
        if (diagnostics.hasErrors())
            return ll::makeStringError(diagnostics.getErrorMessage());

        return {};
    }

    ll::Expected<void> GUIManager::open(const std::string& id, const std::string& formId, Player& player, const frontend::ArrayRef& ctx) {
        frontend::DiagnosticEngine diagnostics;

        frontend::ir::VM mVM(diagnostics);

        if (this->mImpl->cache.contains(id)) {
            auto cached = this->mImpl->cache.at(id);

            auto mCtx = ctx ? ctx : std::make_shared<frontend::ArrayValue>();
            auto result = mVM.run(cached, { std::ref(player), mCtx });
            if (diagnostics.hasErrors())
                return ll::makeStringError(diagnostics.getErrorMessage());

            std::string uuid = player.getUuid().asString();
            if (this->mImpl->forms.contains(uuid) && this->mImpl->forms.at(uuid).contains(formId))
                return this->switchToCustomForm(formId, player);

            if (this->mImpl->boxs.contains(uuid) && this->mImpl->boxs.at(uuid).contains(formId))
                return this->switchToMessageBox(formId, player); 

            if (this->mImpl->paginatedForms.contains(uuid) && this->mImpl->paginatedForms.at(uuid).contains(formId))
                return this->switchToPaginatedForm(formId, player);

            if (this->mImpl->scriptForms.contains(uuid) && this->mImpl->scriptForms.at(uuid).contains(formId))
                return this->switchToScriptForm(formId, player);

            return ll::makeStringError("open: Fuzzy matching can't find the specific form type");
        }

        return ll::makeStringError("open: No corresponding bytecode cache was found");
    }

    ll::Expected<void> GUIManager::open(
        const std::string& id, const std::string& formId, GUIManagerType type, Player& player, const frontend::ArrayRef& ctx
    ) {
        frontend::DiagnosticEngine diagnostics;

        frontend::ir::VM mVM(diagnostics);

        if (this->mImpl->cache.contains(id)) {
            auto cached = this->mImpl->cache.at(id);

            auto mCtx = ctx ? ctx : std::make_shared<frontend::ArrayValue>();
            auto result = mVM.run(cached, { std::ref(player), mCtx });
            if (diagnostics.hasErrors())
                return ll::makeStringError(diagnostics.getErrorMessage());

            switch (type) {
                case GUIManagerType::CustomForm: return this->switchToCustomForm(formId, player);
                case GUIManagerType::MessageBox: return this->switchToMessageBox(formId, player); 
                case GUIManagerType::PaginatedForm: return this->switchToPaginatedForm(formId, player);
                case GUIManagerType::ScriptForm: return this->switchToScriptForm(formId, player);
            }
        }

        return ll::makeStringError("open: No corresponding bytecode cache was found");
    }

    ll::Expected<void> GUIManager::switchToCustomForm(const std::string& id, Player& player) {
        auto handle = this->getCustomFormUI(id, player);
        if (!handle.has_value())
            return ll::Unexpected(handle.error());

        if (!handle.value()->show) {
            auto result = handle.value()->base->show([this, id, handle = handle.value(), player = std::ref(player)](ll::ui::ScreenSession::Result) mutable -> void {
                if (auto current = this->getCustomFormUI(id, player.get());
                    current.has_value() && current.value() == handle)
                    this->unregisterCustomFormUI(id, player.get());
            });
            if (!result)
                return ll::Unexpected(result.error());

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
            
            if (auto current = this->getCustomFormUI(id, player.get());
                current.has_value() && current.value() == handle)
                this->unregisterCustomFormUI(id, player.get());
        });

        if (!result)
            return ll::Unexpected(result.error());

        return {};
    }

    ll::Expected<void> GUIManager::switchToMessageBox(const std::string& id, Player& player) {
        auto handle = this->getMessageBoxUI(id, player);
        if (!handle.has_value())
            return ll::Unexpected(handle.error());

        if (!handle.value()->show) {
            auto result = handle.value()->base->show([this, id, handle = handle.value(), player = std::ref(player)](ll::ui::MessageBox::Result) mutable -> void{
                if (auto current = this->getMessageBoxUI(id, player.get());
                    current.has_value() && current.value() == handle)
                    this->unregisterMessageBoxUI(id, player.get());
            });
            if (!result)
                return ll::Unexpected(result.error());

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
            
            if (auto current = this->getMessageBoxUI(id, player.get());
                current.has_value() && current.value() == handle)
                this->unregisterMessageBoxUI(id, player.get());
        });

        if (!result)
            return ll::Unexpected(result.error());

        return {};
    }

    ll::Expected<void> GUIManager::switchToPaginatedForm(const std::string& id, Player& player) {
        auto handle = this->getPaginatedFormUI(id, player);
        if (!handle.has_value())
            return ll::Unexpected(handle.error());

        if (!handle.value()->base)
            return ll::makeStringError("PaginatedForm is not built");

        auto result = handle.value()->base->show([this, id, handle = handle.value(), player = std::ref(player)](ll::ui::ScreenSession::Result closeResult) mutable -> void {
            auto resultObj = std::make_shared<frontend::Object>();
            resultObj->className = "PaginatedFormResult";
            resultObj->classIndex = -1;

            if (closeResult.has_value())
                resultObj->fields["closeReason"] = static_cast<int>(*closeResult);
            else
                resultObj->fields["closeReason"] = std::monostate{};

            resultObj->fields["selection"] = handle->selection;
            resultObj->fields["selectionIndex"] = handle->selectionIndex;
            resultObj->fields["page"] = handle->selectionPage;

            if (handle->show) {
                frontend::DiagnosticEngine diagnostics;
                frontend::CallbackTypeValues values{ resultObj };

                [[maybe_unused]] auto cbResult = frontend::ir::VM::callFunctionRef(
                    handle->show, values, frontend::Context{ player }.params, diagnostics
                );

                if (diagnostics.hasErrors()) {
                    ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA")
                        ->error("PaginatedForm::show callback: {}", diagnostics.getErrorMessage());
                }
            }

            if (auto current = this->getPaginatedFormUI(id, player.get());
                current.has_value() && current.value() == handle)
                this->unregisterPaginatedFormUI(id, player.get());
        });

        if (!result)
            return ll::Unexpected(result.error());

        return {};
    }

    ll::Expected<void> GUIManager::switchToScriptForm(const std::string& id, Player& player) {
        auto handle = this->getScriptFormUI(id, player);
        if (!handle.has_value())
            return ll::Unexpected(handle.error());

        auto finish = [this, id, handle = handle.value(), player = std::ref(player)]() mutable -> void {
            if (handle->pendingSubflow) {
                if (handle->onClosed)
                    handle->onClosed(player.get());

                if (auto current = this->getScriptFormUI(id, player.get());
                    current.has_value() && current.value() == handle)
                    this->unregisterScriptFormUI(id, player.get());
                return;
            }

            if (handle->onClosed)
                handle->onClosed(player.get());

            if (handle->show) {
                frontend::DiagnosticEngine diagnostics;
                frontend::CallbackTypeValues values;

                auto resultObj = handle->makeResult ? handle->makeResult() : nullptr;
                values.emplace_back(resultObj ? frontend::TypedValue(resultObj) : frontend::TypedValue{});

                [[maybe_unused]] auto cbResult = frontend::ir::VM::callFunctionRef(
                    handle->show, values, frontend::Context{ player }.params, diagnostics
                );

                if (diagnostics.hasErrors()) {
                    ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA")
                        ->error("ScriptForm::show callback: {}", diagnostics.getErrorMessage());
                }
            }

            if (auto current = this->getScriptFormUI(id, player.get());
                current.has_value() && current.value() == handle)
                this->unregisterScriptFormUI(id, player.get());
        };

        if (handle.value()->base) {
            auto result = handle.value()->base->show([finish](ll::ui::ScreenSession::Result) mutable -> void {
                finish();
            });
            if (!result)
                return ll::Unexpected(result.error());

            return {};
        }

        if (handle.value()->box) {
            auto result = handle.value()->box->show([finish, handle = handle.value()](ll::ui::MessageBox::Result closeResult) mutable -> void {
                if (handle->onBoxResult)
                    handle->onBoxResult(closeResult);

                finish();
            });
            if (!result)
                return ll::Unexpected(result.error());

            return {};
        }

        return ll::makeStringError("ScriptForm is not built");
    }

    void GUIManager::registerCustomFormUI(const std::string& id, std::shared_ptr<CustomFormClass::CustomFormHandle> form, Player& player) {
        auto [it, _] = this->mImpl->forms.try_emplace(player.getUuid().asString());
        it->second.insert_or_assign(id, std::move(form));
    }

    void GUIManager::registerMessageBoxUI(const std::string& id, std::shared_ptr<MessageBoxClass::MessageBoxHandle> box, Player& player) {
        auto [it, _] = this->mImpl->boxs.try_emplace(player.getUuid().asString());
        it->second.insert_or_assign(id, std::move(box));
    }

    void GUIManager::registerPaginatedFormUI(const std::string& id, std::shared_ptr<PaginatedFormClass::PaginatedFormHandle> form, Player& player) {
        auto [it, _] = this->mImpl->paginatedForms.try_emplace(player.getUuid().asString());
        it->second.insert_or_assign(id, std::move(form));
    }

    void GUIManager::registerScriptFormUI(const std::string& id, std::shared_ptr<ScriptFormClass::ScriptFormHandle> form, Player& player) {
        auto [it, _] = this->mImpl->scriptForms.try_emplace(player.getUuid().asString());
        it->second.insert_or_assign(id, std::move(form));
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

    bool GUIManager::unregisterPaginatedFormUI(const std::string& id, Player& player) {
        std::string uuid = player.getUuid().asString();

        if (auto it = this->mImpl->paginatedForms.find(uuid); it != this->mImpl->paginatedForms.end()) {
            it->second.erase(id);
            if (it->second.empty())
                this->mImpl->paginatedForms.erase(it);

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

    bool GUIManager::unregisterScriptFormUI(const std::string& id, Player& player) {
        std::string uuid = player.getUuid().asString();

        if (auto it = this->mImpl->scriptForms.find(uuid); it != this->mImpl->scriptForms.end()) {
            it->second.erase(id);
            if (it->second.empty())
                this->mImpl->scriptForms.erase(it);

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

    ll::Expected<std::shared_ptr<PaginatedFormClass::PaginatedFormHandle>> GUIManager::getPaginatedFormUI(const std::string& id, Player& player) {
        if (auto it = this->mImpl->paginatedForms.find(player.getUuid().asString()); it != this->mImpl->paginatedForms.end()) {
            auto& innerMap = it->second;
            if (auto innerIt = innerMap.find(id); innerIt != innerMap.end())
                return innerIt->second;

            return ll::makeStringError("PaginatedForm not found for player: " + id);
        }

        return ll::makeStringError("Player has no registered paginated forms");
    }

    ll::Expected<std::shared_ptr<ScriptFormClass::ScriptFormHandle>> GUIManager::getScriptFormUI(const std::string& id, Player& player) {
        if (auto it = this->mImpl->scriptForms.find(player.getUuid().asString()); it != this->mImpl->scriptForms.end()) {
            auto& innerMap = it->second;
            if (auto innerIt = innerMap.find(id); innerIt != innerMap.end())
                return innerIt->second;

            return ll::makeStringError("ScriptForm not found for player: " + id);
        }

        return ll::makeStringError("Player has no registered script forms");
    }

    void GUIManager::registerValue(const std::string& id, ValueCallback callback) {
        this->mImpl->values.insert_or_assign(id, std::move(callback));
    }

    void GUIManager::registerRequest(const std::string& id, RequestCallback callback) {
        this->mImpl->requests.insert_or_assign(id, std::move(callback));
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

    ll::Expected<frontend::ArrayRef> GUIManager::getRequest(const std::string& id, frontend::ArrayRef args, Player& player) {
        auto it = this->mImpl->requests.find(id);
        if (it == this->mImpl->requests.end())
            return ll::makeStringError("Request not registered: " + id);

        return it->second(args, player);
    }

    ll::Expected<void> GUIManager::getCallback(const std::string& id, frontend::ArrayRef args, Player& player) {
        auto it = this->mImpl->callbacks.find(id);
        if (it == this->mImpl->callbacks.end())
            return ll::makeStringError("Callback not registered: " + id);

        return it->second(args, player);
    }
}
