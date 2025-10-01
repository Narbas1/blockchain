#include "functions.h"

int main(){

    while(true){
        std::string input;

        std::cout << "Options: 1. Enter a string to hash" << std::endl;
        std::cout << "         2. Hash files with singular character" << std::endl;
        std::cout << "         3. Hash files with 3000 random characters" << std::endl;
        std::cout << "         4. Hash files that differ in 1 character" << std::endl;
        std::cout << "         5. Hash empty file" << std::endl;
        std::cout << "         6. Test with file konstitucija.txt" << std::endl;
        std::cout << "         7. Generate 100 000 random pairs of 10, 100, 500, 1000 characters length" << std::endl;
        std::cout << "         8. Collision search in pair_num files" << std::endl;
        std::cout << "         9. Avalanche test" << std::endl;
        int option;
        std::cin >> option;

        if(option == 1){
            std::cout << "Enter a string to hash: ";
            std::cin.ignore();
            std::getline(std::cin, input);

            auto digest = hash(input);
            std::cout << "Hash: ";
            for (const auto &part : digest) {
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            }
            std::cout << std::endl;
        }

        if (option == 2) {
            std::ifstream a("files/a.txt", std::ios::binary);
            std::ifstream b("files/b.txt", std::ios::binary);

            std::string content_a((std::istreambuf_iterator<char>(a)), {});
            std::string content_b((std::istreambuf_iterator<char>(b)), {});

            auto digest_a = hash(content_a);
            std::cout << "Hash of a.txt: ";
            for (const auto &part : digest_a)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;

            auto digest_b = hash(content_b);
            std::cout << "Hash of b.txt: ";
            for (const auto &part : digest_b)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;
        }



        if (option == 3) {
            std::ifstream file1("files/random3000_1.txt", std::ios::binary);
            std::ifstream file2("files/random3000_2.txt", std::ios::binary);

            std::string content1((std::istreambuf_iterator<char>(file1)), {});
            std::string content2((std::istreambuf_iterator<char>(file2)), {});

            auto digest1 = hash(content1);
            std::cout << "Hash of random3000_1.txt: ";
            for (const auto &part : digest1)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;

            auto digest2 = hash(content2);
            std::cout << "Hash of random3000_2.txt: ";
            for (const auto &part : digest2)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;
}

        if(option == 4){
            std::ifstream file1("files/random1.txt", std::ios::binary);
            std::ifstream file2("files/random2.txt", std::ios::binary);

            std::string content1((std::istreambuf_iterator<char>(file1)), {});
            std::string content2((std::istreambuf_iterator<char>(file2)), {});

            auto digest1 = hash(content1);
            std::cout << "Hash of random1.txt: ";
            for (const auto &part : digest1)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;

            auto digest2 = hash(content2);
            std::cout << "Hash of random2.txt: ";
            for (const auto &part : digest2)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;
        }

        if(option == 5){
            std::ifstream file("files/empty.txt", std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(file)), {});

            auto digest = hash(content);
            std::cout << "Hash of empty.txt: ";
            for (const auto &part : digest)
                std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
            std::cout << std::endl;
        }

        if(option == 6){

            std::ifstream file("files/konstitucija.txt", std::ios::binary);

            std::vector<std::string> lines;
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(line + "\n");
            }
            std::vector<size_t> line_counts = {1,2,4,8,16,32,64,128, 256, 512, 789};

            for (size_t n : line_counts) {

                if (n > lines.size()) break;

                std::string combined;
                for (size_t i = 0; i < n; ++i) {
                    combined += lines[i];
                }

                auto start = std::chrono::high_resolution_clock::now();
                auto digest = hash(combined);
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> duration = end - start;

                std::cout << "Hash of first " << std::dec << n << " lines: ";
                for (const auto& part : digest) {
                    std::cout << std::hex << std::setw(8) << std::setfill('0') << part;
                }
                std::cout << " (Time: " << duration.count() << " ms)";
                std::cout << std::endl;
            }
        }

    if (option == 7) {
        const size_t pairs = 100000;
        const std::array<size_t, 4> lengths = {10, 100, 500, 1000};

        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, CHARSET.size() - 1);

        for (size_t L : lengths) {
            std::string filename = "pairs_" + std::to_string(L) + ".txt";
            std::ofstream out(filename, std::ios::binary);

            for (size_t i = 0; i < pairs; ++i) {
                std::string a = random_string(L, gen, dist);
                std::string b = random_string(L, gen, dist);
                out << a << '\t' << b << '\n';
            }
            out.close();
            std::cout << "Wrote " << pairs << " pairs to " << filename << "\n";
        }
    }

    if (option == 8) {
        const std::array<size_t, 4> lengths = {10, 100, 500, 1000};

        for (size_t L : lengths) {
            std::string filename = "pairs_" + std::to_string(L) + ".txt";
            std::ifstream in(filename, std::ios::binary);

            size_t collision_count = 0;
            std::string line;
            while (std::getline(in, line)) {
                std::istringstream iss(line);
                std::string a, b;
                if (!(iss >> a >> b)){
                    continue;
                }

                auto hash_a = hash(a);
                auto hash_b = hash(b);
                if (hash_a == hash_b) {
                    ++collision_count;
                }
            }
            in.close();
            std::cout << "Total collisions for length " << std::dec << L << ": " << collision_count << "\n";
        }
    }
    if (option == 9) {
        const size_t N = 25000;
        const std::array<size_t,4> lengths = {10, 20, 50, 100};

        std::mt19937 gen(std::random_device{}());

        for (size_t L : lengths) {
            avalanche_stats_for_length(L, N, gen);
        }
    }

        }

    return 0;
}