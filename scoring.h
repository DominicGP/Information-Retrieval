#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "document.h"
#include "indexer.h"

double computeTFIDF(const Posting &p,
                    const std::vector<Document> &documents,
                    const std::unordered_map<std::string, std::vector<Posting>> &inverted_index,
                    const std::string &term);
