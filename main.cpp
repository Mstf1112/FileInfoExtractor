#include <iostream>
#include <fstream>
#include <string>
#include <filesystem> // C++17
#include <chrono>
#include <iomanip>
#include <vector>
#include <sstream>

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
std::string generateUniqueFileName(const std::string& extension)
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "search_results_" << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S") << extension;
    
    return ss.str();
}

// Function to log found results into a CSV file
void logToCSV(const std::string& filePath, const std::string& foundLine, std::ofstream& logFile)
{
    if (!logFile.is_open())
    {
        std::cerr << "Could not open or create log file!" << std::endl;
        return;
    }
    logFile << "\"" << filePath << "\",\"" << foundLine << "\"\n";
}

// Function to read multiple emails from a file
std::vector<std::string> readEmailsFromFile(const std::string& emailFilePath)
{
    std::ifstream emailFile(emailFilePath);
    std::vector<std::string> emails;
    if (emailFile.is_open())
    {
        std::string email;
        while (std::getline(emailFile, email))
        {
            emails.push_back(email);
        }
        emailFile.close();
    }
    else
    {
        std::cerr << "Could not open the email file!" << std::endl;
    }
    return emails;
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

            // Log the result to the CSV file
            logToCSV(filePath, line, logFile);

            found = true;
            if (!listAll) break; // End search for a single match
        }
    }
    file.close();
    return found;
}

// Function to recursively search for files in all directories
void searchInDirectory(const fs::path& dir, const std::vector<std::string>& searchTerms, bool listAll, std::ofstream& logFile)
{
    for (const auto& entry : fs::recursive_directory_iterator(dir))
    {
        if (fs::is_regular_file(entry))
        {
            std::string ext = entry.path().extension().string();

            // Check if the file format is supported
            if (ext == ".json" || ext == ".txt" || ext == ".sql" || ext == ".log" || ext == ".csv")
            {
                for (const auto& searchTerm : searchTerms)
                {
                    std::cout << "Searching for '" << searchTerm << "' in: " << entry.path().string() << std::endl;
                    searchInFile(entry.path().string(), searchTerm, listAll, logFile);
                }
            }
        }
    }
}

int main()
{
    std::string searchEmailOrWord;
    int userChoice;
    bool found = false;

    std::cout << "Search options:\n";
    std::cout << "1- Search with a unique object (manual input)\n";
    std::cout << "2- Search with multiple objects from a file (email list)\n";
    std::cout << "3- Supported Formats\n";
    std::cout << "4- List files without the specified value (Negative Search)\n";
    std::cin >> userChoice;

    if (userChoice == 3)
    {
        showSupportedFormats();
        return 0;
    }

    std::vector<std::string> searchTerms;

    // If user chooses to input manually
    if (userChoice == 1)
    {
        std::cout << "Enter the word or e-mail address you want to search for: ";
        std::cin >> searchEmailOrWord;
        searchTerms.push_back(searchEmailOrWord);
    }
    // If user chooses to input from a file
    else if (userChoice == 2)
    {
        std::string emailFilePath;
        std::cout << "Enter the file path containing e-mail addresses: ";
        std::cin >> emailFilePath;
        searchTerms = readEmailsFromFile(emailFilePath);
        if (searchTerms.empty())
        {
            std::cerr << "No e-mail addresses found in the file!" << std::endl;
            return 0;
        }
    }

    // Generate a unique file name for logging results in CSV format
    std::string logFileName = generateUniqueFileName(".csv");
    std::ofstream logFile(logFileName); // Create a new log file with a unique name

    // Write CSV headers
    logFile << "File Path,Found Line\n";

    // Search in the current directory and its subdirectories
    searchInDirectory(fs::current_path(), searchTerms, userChoice == 2, logFile);

    std::cout << "Search complete. Results saved to " << logFileName << std::endl;  // Indicate the end of search and the log file

    return 0;
}
