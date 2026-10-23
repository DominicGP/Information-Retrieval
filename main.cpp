#include "indexer.h"

#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <string>
#include <algorithm>

struct Options
{
    std::filesystem::path root_path;
    std::filesystem::path index_path;
};

static void printUsage()
{
    std::cerr << "Usage: indexer --root <documents_folder> --index <index_folder>\n";
}

static bool parseArgs(int argc, char *argv[], Options &opt)
{
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        // Ensure the argument has a value and store it in the provided path
        auto requireValue = [&](std::filesystem::path &out) -> bool
        {
            if (i + 1 >= argc)
                return false;
            out = argv[++i];
            return true;
        };

        if (arg == "--root")
        {
            if (!requireValue(opt.root_path))
            {
                std::cerr << "Error: --root requires a directory path\n";
                return false;
            }
        }
        else if (arg == "--index")
        {
            if (!requireValue(opt.index_path))
            {
                std::cerr << "Error: --index requires a directory path\n";
                return false;
            }
        }
        else
        {
            std::cerr << "Error: unknown argument: " << arg << "\n";
            return false;
        }
    }

    bool has_error = false;
    if (opt.root_path.empty())
    {
        std::cerr << "Missing required argument: --root\n";
        has_error = true;
    }
    if (opt.index_path.empty())
    {
        std::cerr << "Missing required argument: --index\n";
        has_error = true;
    }
    if (has_error)
        printUsage();
    return !has_error;
}

static void repl(Indexer &index)
{
    std::cout << "Enter a command:\n";
    for (std::string input;;)
    {
        std::cout << "> ";
        if (!std::getline(std::cin, input))
            return;

        if (input == "quit" || input == "exit")
            return;
        if (input.empty())
            continue;

        index.search(input);
    }
}

int main(int argc, char *argv[])
{
    Options opt;
    if (!parseArgs(argc, argv, opt))
        return 1;

    if (!std::filesystem::is_directory(opt.root_path))
    {
        std::cerr << "Invalid documents directory\n";
        return 1;
    }

    // Prevent the index directory from being indexed
    auto root = std::filesystem::weakly_canonical(opt.root_path);
    auto indexPath = std::filesystem::weakly_canonical(opt.index_path);

    auto mismatch = std::mismatch(
        root.begin(), root.end(),
        indexPath.begin(), indexPath.end());

    if (mismatch.first == root.end())
    {
        std::cerr << "Index directory cannot be inside the documents directory\n";
        return 1;
    }

    Indexer index;

    // A saved index requires all three binary files
    const bool indexExists =
        std::filesystem::exists(opt.index_path / "meta.bin") &&
        std::filesystem::exists(opt.index_path / "docs.bin") &&
        std::filesystem::exists(opt.index_path / "postings.bin");

    bool needsBuild = !indexExists;

    if (indexExists)
    {

        if (!index.loadIndex(opt.index_path))
        {
            std::cerr << "Failed to load index\n";
            return 1;
        }

        needsBuild = index.isIndexStale(opt.root_path);
    }

    if (needsBuild)
    {

        std::cout << "Building new index...\n";

        // Rebuild to avoid retaining outdated documents and postings
        Indexer freshIndex;
        freshIndex.build(opt.root_path);

        if (!freshIndex.saveIndex(opt.index_path))
        {
            std::cerr << "Failed to save index\n";
            return 1;
        }

        index = std::move(freshIndex);
    }
    else
    {
        std::cout << "Using existing index...\n";
    }

    repl(index);
    return 0;
}
