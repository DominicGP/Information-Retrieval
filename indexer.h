#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

#include "document.h"
#include "crawler.h"

class Posting
{
private:
    int docID;
    int tf;

public:
    Posting(int docID, int tf);

    int getID() const;
    int getTF() const;
};

class Indexer
{
private:
    std::vector<Document> documents;
    std::unordered_map<std::string, std::vector<Posting>> inverted_index;

public:
    void build(const std::filesystem::path &root_path);
    void search(const std::string &input);
    static std::vector<std::string> tokenizeStream(std::istream &in);
    std::vector<std::string> tokenizeFile(const std::filesystem::path &path);
    std::vector<std::string> tokenizeQuery(const std::string &q);
    bool saveIndex(const std::filesystem::path &dir) const;
    bool loadIndex(const std::filesystem::path &index_path);
    bool isIndexStale(const std::filesystem::path &root_path) const;
};