#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <vector>
#include <sstream>
#include <algorithm>
#include <future>
#include <mutex>
#include <atomic>
#include <semaphore>

namespace fs = std::filesystem;

// ANSI escape codes
const std::string RESET = "\033[0m";
const std::string GREEN = "\033[32m";
// İstersen ekle:
// const std::string YELLOW = "\033[33m";

// Mutex for thread-safe logging and output
std::mutex logMutex;

// Semaphore to limit parallelism
constexpr int MAX_PARALLEL = 8;
std::counting_semaphore<MAX_PARALLEL> semaphore(MAX_PARALLEL);

// Atomic counter for processed files
std::atomic<int> filesProcessed{0};

void showSupportedFormats()
{
    std::cout << "Supported formats: .json, .txt, .sql, .log, .csv" << std::endl;
}

std::string generateUniqueFileName(const std::string& extension)
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "search_results_" << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S") << extension;
    return ss.str();
}

void logToCSV(const std::string& filePath, const std::string& foundLine, std::ofstream& logFile)
{
    std::lock_guard<std::mutex> guard(logMutex);
    logFile << "\"" << filePath << "\",\"" << foundLine << "\"\n";
}

std::vector<std::string> readEmailsFromFile(const std::string& path)
{
    std::ifstream file(path);
    std::vector<std::string> emails;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty()) emails.push_back(line);
    }
    return emails;
}

bool searchInFile(const std::string& filePath, const std::vector<std::string>& searchTerms, bool listAll, std::ofstream& logFile)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::lock_guard<std::mutex> guard(logMutex);
        std::cerr << "Cannot open file: " << filePath << std::endl;
        return false;
    }

    std::string line;
    bool found = false;

    while (std::getline(file, line))
    {
        for (const auto& term : searchTerms)
        {
            if (line.find(term) != std::string::npos)
            {
                {
                    std::lock_guard<std::mutex> guard(logMutex);
                    std::cout << GREEN << "Found in: " << filePath << "\nLine: " << line << RESET << std::endl;
                    logToCSV(filePath, line, logFile);
                }

                found = true;
                if (!listAll) return true;
            }
        }
    }

    return found;
}

fs::path createResultsFolder()
{
    fs::path resultsFolder = fs::current_path() / "Results";
    if (!fs::exists(resultsFolder))
    {
        fs::create_directory(resultsFolder);
        std::cout << "Created folder: " << resultsFolder << std::endl;
    }
    return resultsFolder;
}

bool isSupportedExtension(const std::string& ext)
{
    std::string e = ext;
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e == ".json" || e == ".txt" || e == ".sql" || e == ".log" || e == ".csv";
}

int main()
{
    std::string input;
    int userChoice;

    std::cout << "Search options:\n";
    std::cout << "1- Search with manual input\n";
    std::cout << "2- Search with multiple entries from file\n";
    std::cout << "3- Show supported formats\n";
    std::cout << "4- (Not implemented) Negative Search\n";
    std::cin >> userChoice;

    if (userChoice == 3)
    {
        showSupportedFormats();
        return 0;
    }

    std::vector<std::string> searchTerms;

    if (userChoice == 1)
    {
        std::cout << "Enter term to search: ";
        std::cin >> input;
        searchTerms.push_back(input);
    }
    else if (userChoice == 2)
    {
        std::string filePath;
        std::cout << "Enter path to term list file: ";
        std::cin >> filePath;
        searchTerms = readEmailsFromFile(filePath);
        if (searchTerms.empty())
        {
            std::cerr << "No valid entries found!" << std::endl;
            return 1;
        }
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    fs::path resultsFolder = createResultsFolder();
    std::string logFileName = (resultsFolder / generateUniqueFileName(".csv")).string();
    std::ofstream logFile(logFileName);
    logFile << "File Path,Found Line\n";

    std::vector<std::future<bool>> futures;
    std::vector<fs::path> filesToScan;
    for (const auto& entry : fs::recursive_directory_iterator(fs::current_path()))
    {
        if (fs::is_regular_file(entry))
        {
            if (isSupportedExtension(entry.path().extension().string()))
            {
                filesToScan.push_back(entry.path());
            }
        }
    }

    int totalFiles = static_cast<int>(filesToScan.size());

    if (totalFiles == 0)
    {
        std::cout << "No supported files found to scan." << std::endl;
        return 0;
    }

    for (const auto& filePath : filesToScan)
    {
        semaphore.acquire();

        futures.push_back(std::async(std::launch::async, [filePath, &searchTerms, userChoice, &logFile, startTime, totalFiles]() {
            bool result = searchInFile(filePath.string(), searchTerms, userChoice == 2, logFile);

            int processed = ++filesProcessed;

            if (processed % 10 == 0 || processed == totalFiles)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

                double avgPerFile = elapsedMs / static_cast<double>(processed);
                int remaining = totalFiles - processed;
                double etaMs = avgPerFile * remaining;

                double percentDone = (processed * 100.0) / totalFiles;

                std::lock_guard<std::mutex> guard(logMutex);
                std::cout << "\rProgress: " << processed << "/" << totalFiles
                          << " (" << std::fixed << std::setprecision(2) << percentDone << "%), "
                          << "Elapsed: " << elapsedMs / 1000.0 << "s, "
                          << "ETA: " << etaMs / 1000.0 << "s        " << std::flush;
            }

            semaphore.release();
            return result;
        }));
    }

    for (auto& f : futures)
        f.get();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << std::endl << "\n Search completed." << std::endl;
    std::cout << "Time elapsed: " << duration.count() / 1000.0 << " seconds" << std::endl;
    std::cout << "Total files scanned: " << totalFiles << std::endl;
    std::cout << "Results saved to: " << logFileName << std::endl;

    return 0;
}
