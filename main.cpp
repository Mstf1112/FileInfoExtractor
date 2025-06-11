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
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <semaphore>

namespace fs = std::filesystem;

// Renkler
const std::string RESET = "\033[0m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";

// Maksimum paralel thread sayısı
constexpr int MAX_PARALLEL = 8;
std::counting_semaphore<MAX_PARALLEL> semaphore(MAX_PARALLEL);

// Mutexler
std::mutex coutMutex;
std::mutex queueMutex;
std::condition_variable cv;

// Log kuyruğu: (dosya yolu, bulunan satır)
std::queue<std::pair<std::string, std::string>> logQueue;

// Dosya başına işlenen sayısı için atomic sayaç
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

// Log kuyruğuna ekle
void enqueueLog(const std::string& filePath, const std::string& foundLine)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        logQueue.emplace(filePath, foundLine);
    }
    cv.notify_one();
}

// Log dosyasına yazan thread fonksiyonu
void logWorker(std::ofstream& logFile, std::atomic<bool>& doneFlag)
{
    while (!doneFlag.load() || !logQueue.empty())
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, [&] { return doneFlag.load() || !logQueue.empty(); });
        while (!logQueue.empty())
        {
            auto& entry = logQueue.front();
            logFile << "\"" << entry.first << "\",\"" << entry.second << "\"\n";
            logQueue.pop();
        }
        logFile.flush();
    }
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

bool searchInFile(const std::string& filePath, const std::vector<std::string>& searchTerms, bool listAll)
{
    semaphore.acquire(); // Thread sınırlandırması

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cerr << "Cannot open file: " << filePath << std::endl;
        }
        semaphore.release();
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
                    std::lock_guard<std::mutex> lock(coutMutex);
                    std::cout << GREEN << "Found in: " << filePath << "\nLine: " << line << RESET << std::endl;
                }
                enqueueLog(filePath, line);
                found = true;
                if (!listAll)
                {
                    semaphore.release();
                    return true;
                }
            }
        }
    }

    semaphore.release();
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

    // Başlangıç zamanı
    auto startTime = std::chrono::high_resolution_clock::now();

    fs::path resultsFolder = createResultsFolder();
    std::string logFileName = (resultsFolder / generateUniqueFileName(".csv")).string();
    std::ofstream logFile(logFileName);
    logFile << "File Path,Found Line\n";

    std::atomic<bool> doneFlag{false};

    // Log yazma threadi
    std::thread loggerThread(logWorker, std::ref(logFile), std::ref(doneFlag));

    std::vector<std::future<bool>> futures;

    // Toplam dosya sayısını hesapla
    int totalFiles = 0;
    for (const auto& entry : fs::recursive_directory_iterator(fs::current_path()))
    {
        if (fs::is_regular_file(entry))
        {
            if (isSupportedExtension(entry.path().extension().string()))
            {
                ++totalFiles;
            }
        }
    }

    if (totalFiles == 0)
    {
        std::cout << "No supported files found in current directory.\n";
        doneFlag = true;
        cv.notify_one();
        loggerThread.join();
        return 0;
    }

    // Dosyaları işleme
    for (const auto& entry : fs::recursive_directory_iterator(fs::current_path()))
    {
        if (fs::is_regular_file(entry))
        {
            std::string ext = entry.path().extension().string();
            if (isSupportedExtension(ext))
            {
                std::string filePath = entry.path().string();

                futures.push_back(std::async(std::launch::async, [filePath, &searchTerms, userChoice, totalFiles, startTime]() {
                    bool res = searchInFile(filePath, searchTerms, userChoice == 2);

                    int processed = ++filesProcessed;

                    // İlerleme bilgisi
                    if (processed % 10 == 0 || processed == totalFiles)
                    {
                        auto now = std::chrono::high_resolution_clock::now();
                        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

                        double avgPerFile = elapsedMs / static_cast<double>(processed);
                        int remaining = totalFiles - processed;
                        double etaMs = avgPerFile * remaining;

                        double percentDone = (processed * 100.0) / totalFiles;

                        std::lock_guard<std::mutex> lock(coutMutex);
                        std::cout << "\rProgress: " << processed << "/" << totalFiles
                                  << " (" << std::fixed << std::setprecision(2) << percentDone << "%), "
                                  << "Elapsed: " << elapsedMs / 1000.0 << "s, "
                                  << "ETA: " << etaMs / 1000.0 << "s        " << std::flush;
                    }

                    return res;
                }));
            }
        }
    }

    for (auto& f : futures) f.get();

    doneFlag = true;
    cv.notify_one();
    loggerThread.join();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "\n\n Search completed.\n";
    std::cout << YELLOW << "Time elapsed: " << duration.count() / 1000.0 << " seconds\n" << RESET;
    std::cout << YELLOW << "Total files scanned: " << totalFiles << "\n" << RESET;
    std::cout << YELLOW << "Results saved to: " << logFileName << "\n" << RESET;

    return 0;
}
