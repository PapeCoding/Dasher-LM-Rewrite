#include "XmlSettingsStore.h" 
#include "PPMLanguageModel.h"
#include "NewPPM.h"

#include <climits>
#include <fstream>
#include <memory>
#include <iostream>
#include <ostream>
#include <chrono>

#define numSymbols 58
#define norm INT_MAX

#define symbolsTrain 500000
#define symbolsTest  500000
#define symbolsProbs  5000000

std::unique_ptr<Dasher::XmlSettingsStore> Settings;

//Some needed classes
class XMLErrorDisplay : public CMessageDisplay {
public:
		void Message(const std::string& strText, bool bInterrupt) override
		{
				std::cout << strText << std::endl;
		}
};

inline char translateChar(const char& c){
    return c - 'A' + 1;
}

void printVector(std::vector<unsigned int> v){
    for(int i = 0; i < v.size(); i++){
        if(i != 0) std::cout << ',';
        std::cout << v[i];
    }
    std::cout << std::endl;
}

bool printCompareVector(std::vector<unsigned int> v1, std::vector<unsigned int> v2){
    bool differenceFound = false;
    for(int i = 0; i < v1.size(); i++){
        if(i != 0) std::cout << ',';
        if(v1[i] != v2[i]){
            std::cout << v1[i] << "|" << v2[i];
        } else {
            std::cout << v1[i];
        }
    }
    std::cout << std::endl;
    return differenceFound;
}

bool compareVector(std::vector<unsigned int> v1, std::vector<unsigned int> v2){
    if(v1 != v2) return printCompareVector(v1, v2);
    return false;
}

int testCorrectnessFunction(){
    std::unique_ptr<Dasher::CPPMLanguageModel> lm1 = std::make_unique<Dasher::CPPMLanguageModel>(Settings.get(), numSymbols);
    std::unique_ptr<NewPPM> lm2 = std::make_unique<NewPPM>(Settings.get(), numSymbols);

    // open train file
    std::ifstream trainFile("../trainText.txt");
    if (!trainFile.is_open()) return 1;

    // some vars for training and testing
    char c;
    long i = 0;
    Dasher::CLanguageModel::Context context1;
    Dasher::CLanguageModel::Context context2;
    std::vector<unsigned int> probs1;
    std::vector<unsigned int> probs2;

    // train language model
    context1 = lm1->CreateEmptyContext();
    context2 = lm2->CreateEmptyContext();
    while(trainFile >> c && i < symbolsTrain){
        //learn symbols
        lm1->LearnSymbol(context1, translateChar(c));
        lm2->LearnSymbol(context2, translateChar(c));

        lm1->GetProbs(context1, probs1, norm, 0);
        lm2->GetProbs(context2, probs2, norm, 0);

        if(compareVector(probs1, probs2)){
            lm1->printTree();
        }

        i++;
    }
    lm1->ReleaseContext(context1);
    lm2->ReleaseContext(context2);
    
    
    // open test file 
    std::ifstream testFile("../testText.txt");
    if (!testFile.is_open()) return 1;
    
    // test language model
    i = 0;
    context1 = lm1->CreateEmptyContext();
    context2 = lm2->CreateEmptyContext();
    while(testFile >> c && i < symbolsTest){
        lm1->EnterSymbol(context1, translateChar(c));
        lm2->EnterSymbol(context2, translateChar(c));

        lm1->GetProbs(context1, probs1, norm, 0);
        lm2->GetProbs(context2, probs2, norm, 0);

        compareVector(probs1, probs2);

        i++;
    }
    lm1->ReleaseContext(context1);
    lm2->ReleaseContext(context2);

    return 0;
}

template<typename T>
int testSpeedFunction(){
    std::unique_ptr<T> lm = std::make_unique<T>(Settings.get(), numSymbols);

    // open train file
    std::ifstream trainFile("../trainText.txt");
    if (!trainFile.is_open()) return 1;

    // read buffer into memory before timing
    char* heapBuffer = new char[symbolsTrain];
    trainFile.read(heapBuffer, symbolsTrain);

    std::chrono::time_point start = std::chrono::high_resolution_clock::now();

    // some vars for training and testing
    long i = 0;
    Dasher::CLanguageModel::Context context;
    std::vector<unsigned int> probs;

    // train language model
    context = lm->CreateEmptyContext();
    while(i < symbolsTrain){
        //learn symbols
        lm->LearnSymbol(context, translateChar(heapBuffer[i]));
        lm->GetProbs(context, probs, norm, 0);
        i++;
    }
    lm->ReleaseContext(context);

    std::cout << "Train: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() << "ms" << std::endl;
    
    delete[] heapBuffer;
    
    // open test file 
    std::ifstream testFile("../testText.txt");
    if (!testFile.is_open()) return 1;
    
    // read buffer into memory before timing
    heapBuffer = new char[symbolsTest];
    testFile.read(heapBuffer, symbolsTest);

    start = std::chrono::high_resolution_clock::now();
    // test language model
    i = 0;
    context = lm->CreateEmptyContext();
    while(i < symbolsTest){
        lm->EnterSymbol(context, translateChar(heapBuffer[i]));
        lm->GetProbs(context, probs, norm, 0);
        i++;
    }
    lm->ReleaseContext(context);

    std::cout << "Test: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() << "ms" << std::endl;

    delete[] heapBuffer;

    start = std::chrono::high_resolution_clock::now();
    // test language model
    i = 0;
    context = lm->CreateEmptyContext();
    while(i < symbolsProbs){
        lm->GetProbs(context, probs, norm, 0);
        i++;
    }
    lm->ReleaseContext(context);

    std::cout << "Probs: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() << "ms" << std::endl;

    return 0;
}

int main(int argc, char* argv[]) {
    //bunch of needed initializations
    static XMLErrorDisplay display;
    Settings = std::make_unique<Dasher::XmlSettingsStore>("../Settings.xml", &display);
    Settings->Load();
    Settings->Save();
    
    for(int i = 0; i < 5; i++){
        std::cout << "======================\nNewPPM" << std::endl;
        if(testSpeedFunction<NewPPM>()) return 1;
        //std::cout << "======================\nOldPPM" << std::endl;
        //if(testSpeedFunction<Dasher::CPPMLanguageModel>()) return 1;
        std::cout << "======================" << std::endl;
    }

    //if(testCorrectnessFunction()) return 1;

    return 0;
}