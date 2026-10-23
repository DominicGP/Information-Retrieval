#pragma once

#include <filesystem>

class Document
{
private:
    int docID;
    std::filesystem::path path;
    int doc_length;
    std::filesystem::file_time_type last_modified;

public:
    Document(int docID,
             const std::filesystem::path &path,
             int doc_length,
             std::filesystem::file_time_type last_modified);

    int getID() const;
    const std::filesystem::path &getPath() const;
    int getLength() const;
    std::filesystem::file_time_type getModified() const;
};