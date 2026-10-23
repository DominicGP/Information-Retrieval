#include "document.h"

Document::Document(int docID,
                   const std::filesystem::path &path,
                   int doc_length,
                   std::filesystem::file_time_type last_modified)
    : docID(docID),
      path(path),
      doc_length(doc_length),
      last_modified(last_modified) {}

int Document::getID() const
{
    return docID;
}

const std::filesystem::path &Document::getPath() const
{
    return path;
}

int Document::getLength() const
{
    return doc_length;
}

std::filesystem::file_time_type Document::getModified() const
{
    return last_modified;
}