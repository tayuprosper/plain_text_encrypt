#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include <ostream>
#include <queue>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <condition_variable>

std::mutex mt;
std::condition_variable cv;
std::queue<int> workerIds;
std::vector<std::thread> threads;

// struct chunk for a chunk of the file to be ecrypted
struct Chunk
{
    std::string content;
    int size;
    bool is_encrypted;
    int chunk_number;
};
std::vector<Chunk> result;

void resort_chunks(){
   std::sort(result.begin(), result.end(), [](const Chunk& a, const Chunk& b){ return a.chunk_number < b.chunk_number; });
}

// break down file into smaller chunks for encryption
void chop_text(std::string s, std::queue<Chunk> &q, std::size_t &s_chunkSize, std::size_t minChunksize = 20)
{
    int i = 0;
    std::size_t totalChunkSize = s.length();
    s_chunkSize = totalChunkSize;

    std::size_t number_chunks = std::ceil(static_cast<double>(totalChunkSize) / minChunksize);
    if (number_chunks == 0)
        number_chunks = 1;
    std::size_t chunkSize = std::ceil(static_cast<double>(totalChunkSize) / number_chunks);

    for (std::size_t i = 0; i < totalChunkSize; i += chunkSize)
    {
        Chunk chunk;
        chunk.content = s.substr(i, chunkSize);
        chunk.size = s.substr(1, chunkSize).length();
        chunk.is_encrypted = false;
        chunk.chunk_number = i;
        ++i;
        q.push(chunk);
    }
}
//
void encrypt(Chunk chunk){
    std::string encrypted_str = " ";
        for (char c : chunk.content){
            char encrypted;
            if (isalpha(c)){
                char base = isupper(c) ? 'A' : 'a';
                encrypted = (c - base + 3) % 26 + base;
                // std::cout << encrypted << std::endl;
                encrypted_str+=encrypted;
            }else{
                encrypted_str += c;
            }
        }
    chunk.content = encrypted_str;
    chunk.is_encrypted = true;
    result.push_back(chunk);
}

// to be used in a thread to encrypt chuck of file.
void encrypt_file(std::queue<Chunk> &chunk,std::vector<std::thread>& th)
{
    while (!chunk.empty()){
         std::thread t;
        {
            std::unique_lock<std::mutex> loc(mt);
            cv.wait(loc, [&th](){
               return !th.empty();
            });

            t = std::move(th.back());
            th.pop_back();
        }
            if (t.joinable()) t.join();

            t = std::thread(encrypt, chunk.front());
        {
            std::lock_guard<std::mutex> lo(mt);
            chunk.pop();
        }
        {
            std::lock_guard<std::mutex> lo(mt);
            t.join();
            th.push_back(std::move(t));
        }
        cv.notify_all();
        
    }
}

int main(int argc, char *argv[])
{
    std::stringstream ss;
    std::queue<Chunk> chunks;
    if (argc <= 2)
    {
        try
        {
            std::fstream f(std::string(argv[1]).c_str());
            ss << f.rdbuf();
            // std::cout << ss.str() << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cout << "Something went wrong" << e.what() << std::endl;
        }
    }
    else
    {
        std::cout << "Error!! usade ./<name> <filename/dir>" << std::endl;
    }

    std::size_t chunkSize;
    chop_text(ss.str(), chunks, chunkSize);
    std::cout << "Total Chunk Size is: " << static_cast<double>(chunkSize) << std::endl;
    std::vector<std::thread> threads(3);
    for(int i =0 ;i<3;i++){
        threads.emplace_back();
    }

    encrypt_file(chunks, threads);
    resort_chunks();
    for (Chunk& k : result){
        std::cout << "Chunk number: " << k.chunk_number << std::endl;
        std::cout << "Chunk content: " << k.content << std::endl;
        std::cout << "Chunk encrypted " << k.is_encrypted << std::endl;
    }

    std::string filename = "encrypt-";
    filename += std::string(argv[1]).c_str();
    filename += ".crpt";
    std::ofstream f;
    try{
    f = std::ofstream(filename);
    if (f.is_open()){
         for (const Chunk& c : result){
        f << c.content;
         }
    }else{
        std::cout << "Failed to save encryted file: " << std::endl;
    }
}catch(const std::exception& e){ std::cout << "Failed to save encryted file: " << e.what() << std::endl;}

   f.close();
    return 0;
}
