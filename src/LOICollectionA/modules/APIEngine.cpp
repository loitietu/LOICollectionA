#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <string_view>

#include "LOICollectionA/include/APIEngine.h"

namespace LOICollection::LOICollectionAPI {
    struct Entry {
        APIEngineConfig config;

        std::unique_ptr<IContentProcessor> processor;

        Entry(std::unique_ptr<IContentProcessor> processor, APIEngineConfig  config) :
            config(std::move(config)), processor(std::move(processor)) {}
    };

    struct APIEngine::Impl {
        std::vector<Entry> mProcessors;
    };

    APIEngine::APIEngine() : mImpl(std::make_unique<Impl>()) {};
    APIEngine::~APIEngine() = default;

    APIEngine& APIEngine::getInstance() {
        static APIEngine instance;
        return instance;
    }

    bool APIEngine::has(const std::string& name) const {
        return std::find_if(this->mImpl->mProcessors.begin(), this->mImpl->mProcessors.end(), [&name](const Entry& entry) -> bool {
            return entry.config.name == name;
        }) != this->mImpl->mProcessors.end();
    }

    std::string APIEngine::process(const std::string& content, const Context& context) {
        if (this->mImpl->mProcessors.empty())
            return content;

        std::vector<const Entry*> mEntries;
        for (const auto& entry : this->mImpl->mProcessors) {
            if (content.find(entry.config.delimiters) != std::string::npos && content.find(entry.config.delimitere) != std::string::npos)
                mEntries.push_back(&entry);
        }

        if (mEntries.empty())
            return content;

        std::sort(mEntries.begin(), mEntries.end(), [](const Entry* a, const Entry* b) -> bool {
            return a->config.priority > b->config.priority;
        });

        std::string result = content;
        for (const Entry* entry : mEntries)
            result = this->get(entry->config.name, result, context);

        return result;
    }

    std::string APIEngine::get(const std::string& name, const std::string& content, const Context& context) {
        auto it = std::find_if(this->mImpl->mProcessors.begin(), this->mImpl->mProcessors.end(), [&name](const Entry& entry) -> bool {
            return entry.config.name == name;
        });

        if (it == this->mImpl->mProcessors.end())
            return content;

        std::string result;
        result.reserve(content.size() * 2);
        
        std::string_view view(content);

        size_t mIndex = 0, mResultIndex = 0;

        size_t mEscapeSize = it->config.escape.size();
        size_t mDelimitersSize = it->config.delimiters.size();
        size_t mDelimitereSize = it->config.delimitere.size();
        
        bool isEscape = !it->config.escape.empty();
        while ((mIndex = view.find(it->config.delimiters, mIndex)) != std::string_view::npos) {
            size_t mEndPos = view.find(it->config.delimitere, mIndex + mDelimitersSize);
            if (isEscape && mIndex >= mEscapeSize && view.compare(mIndex - mEscapeSize, mEscapeSize, it->config.escape) == 0) {
                result.append(content, mResultIndex, mIndex - mEscapeSize - mResultIndex);
                result.append(content, mIndex, mDelimitersSize);
                
                mResultIndex = mIndex + mDelimitersSize;
                mIndex = mResultIndex;
                continue;
            }

            if (mEndPos == std::string_view::npos)
                break;

            std::string_view var = view.substr(mIndex + mDelimitersSize, mEndPos - mIndex - mDelimitersSize);
            
            std::string replacement = it->processor->process(std::string(var), context);
            result.append(content, mResultIndex, mIndex - mResultIndex);
            result.append(replacement);

            mResultIndex = mEndPos + mDelimitereSize;
            mIndex = mResultIndex;
        }

        result.append(content, mResultIndex, content.size() - mResultIndex);

        return result;
    }

    void APIEngine::registery(std::unique_ptr<IContentProcessor> processor, const APIEngineConfig& config) {
        if (this->has(config.name))
            return;

        this->mImpl->mProcessors.emplace_back(std::move(processor), config);
    }
}
