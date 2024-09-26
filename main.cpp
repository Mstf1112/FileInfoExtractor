#include <iostream>
#include <fstream>
#include <string>
#include <filesystem> // C++17
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

// ANSI escape codes for colored output
const std::string RESET = "\033[0m";  // Resets color to default
const std::string GREEN = "\033[32m"; // Green text
const std::string YELLOW = "\033[33m"; // Yellow text for negative search results
const std::string RED = "\033[31m";   // Red text for files containing search term

// Function to show supported formats
void showSupportedFormats()
{
    std::cout << "Supported formats: .json, .txt, .sql, .log, .csv" << std::endl;
}

// Function to generate a unique file name based on current time
std::string generateUniqueFileName()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "search_results_" << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S") << ".txt";
    
    return ss.str();
}

// Function to log found results into a file (for non-negative searches)
void logToFile(const std::string& filePath, const std::string& foundLine, std::ofstream& logFile)
{
    if (!logFile.is_open())
    {
        std::cerr << "Could not open or create log file!" << std::endl;
        return;
    }
    logFile << "File: " << filePath << "\n";
    logFile << "Line: " << foundLine << "\n\n";
}

// Function to search for the term in a file
bool searchInFile(const std::string& filePath, const std::string& searchTerm, bool listAll, std::ofstream& logFile)
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
            // Output with color highlighting only if a match is found
            std::cout << GREEN << "Line found: " << line << RESET << std::endl;
            std::cout << GREEN << "File: " << filePath << RESET << std::endl;

            // Log the result to the file
            logToFile(filePath, line, logFile);

            found = true;
            if (!listAll) break; // End search for a single match
        }
    }
    file.close();
    return found;
}

// Function to search for files where the search term is NOT found (Negative Search)
bool searchNegativeInFile(const std::string& filePath, const std::string& searchTerm, std::ofstream& logFile)
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
            found = true;
            break; // If the term is found, mark as found and stop
        }
    }
    file.close();

    // If the term is found, print in RED (skip logging)
    if (found)
    {
        std::cout << RED << "Term found in file: " << filePath << RESET << std::endl;
    }
    // If the term is NOT found, print in YELLOW and log to the file
    else
    {
        std::cout << YELLOW << "Search term NOT found in file: " << filePath << RESET << std::endl;
        logFile << "File: " << filePath << "\n"; // Log the file that doesn't contain the term
    }
    return !found; // Return true if term is NOT found in the file
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
    std::cout << "4- List files without the specified value (Negative Search)\n";
    std::cin >> userChoice;

    if (userChoice == 3)
    {
        showSupportedFormats();
        return 0;
    }

    std::cout << "Enter the word or e-mail address you want to search for: ";
    std::cin >> searchEmailOrWord;

    // Generate a unique file name for logging results
    std::string logFileName = generateUniqueFileName();
    std::ofstream logFile(logFileName); // Create a new log file with a unique name

    // Iterate over files in the current directory
    for (const auto &entry : fs::directory_iterator(fs::current_path()))
    {
        std::string ext = entry.path().extension().string();

        // Check if the file format is supported
        if (ext == ".json" || ext == ".txt" || ext == ".sql" || ext == ".log" || ext == ".csv")
        {
            std::cout << "Searching in: " << entry.path().string() << std::endl;  // Show file being searched
            
            // Regular search
            if (userChoice == 1 || userChoice == 2)
            {
                found = searchInFile(entry.path().string(), searchEmailOrWord, userChoice == 2, logFile) || found;
            }
            // Negative search (4th option)
            else if (userChoice == 4)
            {
                searchNegativeInFile(entry.path().string(), searchEmailOrWord, logFile);
            }
        }
    }

    if (!found && (userChoice == 1 || userChoice == 2))
    {
        std::cout << searchEmailOrWord << " CANNOT FIND!" << std::endl;
    }

    std::cout << "Search complete. Results saved to " << logFileName << std::endl;  // Indicate the end of search and the log file

    return 0;
}
