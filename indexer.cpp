#include "indexer.h"
#include "document.h"
#include "scoring.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <ostream>
#include <sstream>
#include <map>
#include <string>
#include <filesystem>
#include <unordered_set>
#include <chrono>

static std::int64_t fileTimeToTicks(std::filesystem::file_time_type ft);
static std::filesystem::file_time_type ticksToFileTime(std::int64_t ticks);

static void writeU32(std::ostream &os, std::uint32_t v)
{
    os.write(reinterpret_cast<const char *>(&v), sizeof(v));
}

static void writeI64(std::ostream &os, std::int64_t v)
{
    os.write(reinterpret_cast<const char *>(&v), sizeof(v));
}

Posting::Posting(int docID, int tf) : docID(docID), tf(tf) {}

int Posting::getID() const { return docID; }

int Posting::getTF() const { return tf; }

void Indexer::build(const std::filesystem::path &root_path)
{
    std::vector<std::filesystem::path> files = crawler(root_path);

    int docID = 0;

    for (const std::filesystem::path &partial : files)
    {
        std::map<std::string, int> tempDict;

        std::filesystem::path full = root_path / partial;

        std::vector<std::string> toks = tokenizeFile(full);

        // Count the frequency of each term in the document.
        int tokens = static_cast<int>(toks.size());
        for (const std::string &t : toks)
        {
            tempDict[t]++;
        }

        auto last_modified = std::filesystem::last_write_time(full);

        Document doc(docID, partial, tokens, last_modified);
        this->documents.push_back(doc);

        // Add each term's document ID and frequency to the inverted index
        for (const auto &[term, count] : tempDict)
        {
            this->inverted_index[term].push_back(Posting(docID, count));
        }

        docID++;
    }
}

std::vector<std::string> Indexer::tokenizeStream(std::istream &in)
{
    std::vector<std::string> tokens;
    std::string cur;
    char ch;

    while (in.get(ch))
    {
        // Cast to unsigned char for safe character classification
        unsigned char c = static_cast<unsigned char>(ch);

        if (std::isalnum(c))
        {
            cur.push_back(static_cast<char>(std::tolower(c)));
        }
        else
        {
            if (!cur.empty())
            {
                tokens.push_back(cur);
                cur.clear();
            }
        }
    }

    // Fallback for the final token if the stream ends without a separator
    if (!cur.empty())
        tokens.push_back(cur);
    return tokens;
}

std::vector<std::string> Indexer::tokenizeFile(const std::filesystem::path &path)
{
    std::ifstream f(path);
    if (!f)
        return {};
    return tokenizeStream(f);
}

std::vector<std::string> Indexer::tokenizeQuery(const std::string &q)
{
    std::istringstream iss(q);
    return tokenizeStream(iss);
}

void Indexer::search(const std::string &input)
{
    std::vector<std::string> terms = tokenizeQuery(input);

    if (terms.empty())
    {
        std::cout << "Empty query\n";
        return;
    }

    std::unordered_map<int, double> scores;
    bool found_any = false;

    // Use OR-based retrieval, summing TF-IDF scores across matching terms
    for (const std::string &term : terms)
    {
        auto it = inverted_index.find(term);
        if (it == inverted_index.end())
            continue;

        found_any = true;
        const std::vector<Posting> &postings = it->second;

        for (const Posting &p : postings)
        {
            scores[p.getID()] += computeTFIDF(p, documents, inverted_index, term);
        }
    }

    if (!found_any)
    {
        std::cout << "No documents containing any query terms were found\n";
        return;
    }

    // Convert scores to pairs for sorting by relevance
    std::vector<std::pair<double, int>> scored;
    scored.reserve(scores.size());
    for (const auto &[docID, score] : scores)
    {
        scored.push_back({score, docID});
    }

    // Rank documents by descending TF-IDF score
    std::sort(scored.begin(), scored.end(),
              [](const auto &a, const auto &b)
              { return a.first > b.first; });

    std::size_t n = std::min<std::size_t>(5, scored.size());
    std::cout << "Found " << n << " results:\n";

    for (std::size_t i = 0; i < n; i++)
    {
        int id = scored[i].second;
        if (id < 0 || static_cast<std::size_t>(id) >= documents.size())
            continue;
        std::cout << i + 1 << ". " << documents[id].getPath()
                  << " (score=" << scored[i].first << ")\n";
    }
}

bool Indexer::saveIndex(const std::filesystem::path &index_path) const
{
    std::error_code ec;
    std::filesystem::create_directories(index_path, ec);
    if (ec)
        return false;

    // Stores the index format identifier and version
    {
        std::ofstream meta(index_path / "meta.bin", std::ios::binary);
        if (!meta)
            return false;
        meta.write("IDX1", 4);
        std::uint32_t version = 1;
        meta.write(reinterpret_cast<const char *>(&version), sizeof(version));

        meta.close();
        if (!meta)
            return false;
    }

    // Stores document metadata: ID, relative path, token count, and modification time
    {
        std::ofstream out(index_path / "docs.bin", std::ios::binary);
        if (!out)
            return false;

        writeU32(out, static_cast<std::uint32_t>(documents.size()));

        for (const auto &d : documents)
        {
            writeU32(out, static_cast<std::uint32_t>(d.getID()));

            std::string s = d.getPath().u8string();
            writeU32(out, static_cast<std::uint32_t>(s.size()));
            out.write(s.data(), s.size());

            writeU32(out, static_cast<std::uint32_t>(d.getLength()));

            writeI64(out, fileTimeToTicks(d.getModified()));
        }

        out.close();
        if (!out)
            return false;
    }

    // Stores each term followed by its document IDs and term frequencies
    {
        std::ofstream out(index_path / "postings.bin", std::ios::binary);
        if (!out)
            return false;

        std::uint32_t termCount = static_cast<std::uint32_t>(inverted_index.size());
        writeU32(out, termCount);

        for (const auto &[term, postings] : inverted_index)
        {
            writeU32(out, static_cast<std::uint32_t>(term.size()));
            out.write(term.data(), term.size());
            writeU32(out, static_cast<std::uint32_t>(postings.size()));

            for (const auto &posting : postings)
            {
                writeU32(out, static_cast<std::uint32_t>(posting.getID()));
                writeU32(out, static_cast<std::uint32_t>(posting.getTF()));
            }
        }

        out.close();
        if (!out)
            return false;
    }

    return true;
}

bool Indexer::loadIndex(const std::filesystem::path &index_path)
{

    // Reads the index format identifier and version
    {
        std::ifstream meta(index_path / "meta.bin", std::ios::binary);
        if (!meta)
            return false;

        char magic[4];
        if (!meta.read(magic, 4))
            return false;
        if (std::string(magic, 4) != "IDX1")
            return false;

        std::uint32_t version = 0;
        if (!meta.read(reinterpret_cast<char *>(&version), sizeof(version)))
            return false;

        if (version != 1)
            return false;
    }

    // Reconstructs document metadata: ID, relative path, token count, and modification time
    {
        std::ifstream docs(index_path / "docs.bin", std::ios::binary);
        if (!docs)
            return false;

        std::uint32_t docCount = 0;
        if (!docs.read(reinterpret_cast<char *>(&docCount), sizeof(docCount)))
            return false;

        for (std::uint32_t i = 0; i < docCount; i++)
        {

            std::uint32_t docID;
            if (!docs.read(reinterpret_cast<char *>(&docID), sizeof(docID)))
                return false;

            std::uint32_t pathLen;
            if (!docs.read(reinterpret_cast<char *>(&pathLen), sizeof(pathLen)))
                return false;

            std::string path_raw(pathLen, '\0');
            if (!docs.read(&path_raw[0], pathLen))
                return false;
            std::filesystem::path partial(path_raw);

            std::uint32_t tokens;
            if (!docs.read(reinterpret_cast<char *>(&tokens), sizeof(tokens)))
                return false;

            std::int64_t last_modified_raw;
            if (!docs.read(reinterpret_cast<char *>(&last_modified_raw), sizeof(last_modified_raw)))
                return false;
            auto last_modified = ticksToFileTime(last_modified_raw);

            Document doc(docID, partial, tokens, last_modified);
            this->documents.push_back(doc);
        }
    }

    // Reconstructs the inverted index from stored terms, document IDs, and term frequencies
    {
        std::ifstream docs(index_path / "postings.bin", std::ios::binary);
        if (!docs)
            return false;

        std::uint32_t termCount = 0;
        if (!docs.read(reinterpret_cast<char *>(&termCount), sizeof(termCount)))
            return false;

        for (std::uint32_t i = 0; i < termCount; i++)
        {

            std::uint32_t termLength;
            if (!docs.read(reinterpret_cast<char *>(&termLength), sizeof(termLength)))
                return false;

            std::string term;
            term.resize(termLength);

            if (!docs.read(term.data(), termLength))
                return false;

            std::uint32_t postingCount;
            if (!docs.read(reinterpret_cast<char *>(&postingCount), sizeof(postingCount)))
                return false;

            std::vector<Posting> postings;

            for (std::uint32_t j = 0; j < postingCount; j++)
            {

                std::uint32_t postingID;
                std::uint32_t postingTF;

                if (!docs.read(reinterpret_cast<char *>(&postingID), sizeof(postingID)) ||
                    !docs.read(reinterpret_cast<char *>(&postingTF), sizeof(postingTF)))
                    return false;

                if (postingID >= documents.size())
                    return false;
                postings.emplace_back(postingID, postingTF);
            }

            inverted_index[term] = std::move(postings);
        }
    }

    return true;
}

bool Indexer::isIndexStale(const std::filesystem::path &root_path) const
{

    std::unordered_set<std::filesystem::path> indexedPaths;

    // Check for deleted or modified documents
    for (const auto &doc : documents)
    {

        std::filesystem::path fullPath = root_path / doc.getPath();
        indexedPaths.insert(doc.getPath());

        if (!std::filesystem::exists(fullPath))
        {
            return true;
        }

        auto indexedTime = doc.getModified();
        auto currentTime = std::filesystem::last_write_time(fullPath);

        if (currentTime != indexedTime)
        {
            return true;
        }
    }

    // Check for newly added documents
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root_path))
    {

        if (!entry.is_regular_file())
            continue;

        auto path = std::filesystem::relative(
            entry.path(), root_path);

        if (indexedPaths.find(path) == indexedPaths.end())
        {
            return true;
        }
    }

    return false;
}

// Convert filesystem timestamps to and from ticks for binary serialization
static std::int64_t fileTimeToTicks(std::filesystem::file_time_type ft)
{
    return static_cast<std::int64_t>(ft.time_since_epoch().count());
}

static std::filesystem::file_time_type ticksToFileTime(std::int64_t ticks)
{
    using FileTime = std::filesystem::file_time_type;

    return FileTime{FileTime::duration{ticks}};
}