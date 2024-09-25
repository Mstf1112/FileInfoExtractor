#include <iostream>
#include <fstream>
#include <string>
#include <filesystem> // C++17 ve üstü
namespace fs = std::filesystem;

int main()
{
    std::string searchEmail = "deneme@gmail.com";
    bool found = false;

    for (const auto &entry : fs::directory_iterator(fs::current_path()))
    {
        if (entry.path().extension() == ".json" || entry.path().extension() == ".txt")
        {
            std::ifstream file(entry.path());
            if (!file.is_open())
            {
                std::cerr << "Dosya açılamadı: " << entry.path() << std::endl;
                continue;
            }

            std::string line;
            while (std::getline(file, line))
            {
                if (line.find(searchEmail) != std::string::npos)
                {
                    std::cout << "Bulunan satır: " << line << std::endl;
                    std::cout << "Dosya: " << entry.path() << std::endl;
                    found = true;
                    break; // İstiyorsan dosya içinde aramaya devam edebilirsin, buradan çıkmazsan devam eder
                }
            }

            std::cout << "Dosya arandı: " << entry.path() << std::endl;

            file.close();
        }
    }

    if (!found)
    {
        std::cout << searchEmail << " e-posta adresi bulunamadı!" << std::endl;
    }

    return 0;
}
