#include "scoring.h"
#include "indexer.h"

#include <cmath>

double computeTFIDF(const Posting &p,
                    const std::vector<Document> &documents,
                    const std::unordered_map<std::string, std::vector<Posting>> &inverted_index,
                    const std::string &term)
{
    // Normalize term frequency by the document's total token count
    double tf = (double)p.getTF() / documents[p.getID()].getLength();

    int N = documents.size();
    int df = inverted_index.at(term).size();

    // Apply smoothed IDF to give common terms a nonzero weight
    double idf = std::log((N + 1.0) / (df + 1.0)) + 1.0;

    return tf * idf;
}