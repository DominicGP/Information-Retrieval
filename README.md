# Information Retrieval Search Engine

## General Info

This project is a command-line search engine written in C++ that indexes files in a directory and lets users search their contents. It uses an inverted index and TF-IDF scoring to rank matching documents by relevance.

## Features

- Recursively indexes files in a specified directory.
- Tokenizes text and records how often each term appears in each document.
- Supports searches with multiple terms and ranks matching documents using TF-IDF.
- Displays the top 5 matching documents with their relevance scores.
- Saves the index to binary files so it can be reused between runs.
- Automatically rebuilds the index when files are added, modified, or deleted.

## Usage Instructions

You'll need a C++17 compatible compiler to build the program.

Open a terminal in the project directory and compile the program:

```bash
g++ -std=c++17 main.cpp indexer.cpp crawler.cpp document.cpp scoring.cpp -o program.exe
```

Then run it from PowerShell, specifying the directory to search and where to save the index:

```powershell
.\program.exe --root docs --index index
```

The included `docs` folder contains sample text files. The `index` folder will be created automatically if it doesn't exist.

Once the program starts, enter search terms at the `>` prompt. Results are ranked by relevance, and a document can match any of the query terms. Type `quit` or `exit` to close the program.

On later runs, the program loads the saved index if the documents haven't changed. Otherwise, it rebuilds the index before accepting searches.

## Limitations

- Searches are based on individual terms; phrase searches and more advanced query operators are not supported.
- All regular files in the source directory are considered for indexing, so the program is intended for directories containing text files.
- The index is checked for changes when the program starts, not while the search session is running.