#include <iostream>
#include <fstream>
#include <string>
#include <filesystem> // C++17 
namespace fs = std::filesystem;

void showSupportedFormats()
{
    std::cout << "Supported formats: .json, .txt, .sql, .log" << std::endl;
}

bool searchInFile(const std::string& filePath, const std::string& searchTerm, bool listAll)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "File cannot be opened!!: " << filePath << std::endl;
        return false;
    }

    std::string line;
    bool found = false;
    while (std::getline(file, line))
    {
        if (line.find(searchTerm) != std::string::npos)
        {
            std::cout << "Line found in: " << line << std::endl;
            std::cout << "File: " << filePath << std::endl;
            found = true;
            if (!listAll) break; // End search for a single match
        }
    }
    file.close();
    return found;
}

int main()
{
    std::string searchEmailOrWord;
    int userChoice;
    bool found = false;

    std::cout << "Search options:\n";
    std::cout << "1- Search with a unique object\n";
    std::cout << "2- List all with the specified value in it\n";
    std::cout << "3- Supported Formats\n";
    std::cin >> userChoice;

    if (userChoice == 3)
    {
        showSupportedFormats();
        return 0;
    }

    std::cout << "Enter the word or e-mail address you want to search for: ";
    std::cin >> searchEmailOrWord;

    for (const auto &entry : fs::directory_iterator(fs::current_path()))
    {
        std::string ext = entry.path().extension().string();
        
        // SUPPORTED FORMATS
        if (ext == ".json" || ext == ".txt" || ext == ".sql" || ext == ".log" || ext == ".csv")
        {
            found = searchInFile(entry.path().string(), searchEmailOrWord, userChoice == 2) || found;
        }
        
    }

    if (!found)
    {
        std::cout << searchEmailOrWord << " CANNOT FIND!" << std::endl;
    }

    return 0;
}
