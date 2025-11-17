#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include "catboost_lib/c_api.h"

// Добавление директив using namespace
using namespace std;
using namespace std::chrono;

// Функция softmax для преобразования logits в вероятности
vector<double> softmax(const vector<double>& logits) {
    vector<double> probs(logits.size());
    double max_logit = *max_element(logits.begin(), logits.end());
    double sum = 0.0;
    
    for (size_t i = 0; i < logits.size(); ++i) {
        probs[i] = exp(logits[i] - max_logit);
        sum += probs[i];
    }
    
    for (size_t i = 0; i < logits.size(); ++i) {
        probs[i] /= sum;
    }
    
    return probs;
}

// Функция загрузки CSV
vector<vector<float>> load_csv(const string& filename, size_t expected_features) {
    vector<vector<float>> data;
    ifstream file(filename);
    
    if (!file.is_open()) {
        cerr << "ERROR: Cannot open file: " << filename << endl;
        return data;
    }
    
    string line;
    bool first_line = true;
    int line_num = 0;
    
    while (getline(file, line)) {
        line_num++;
        
        if (first_line) {
            first_line = false;
            continue;
        }
        
        vector<float> row;
        stringstream ss(line);
        string value;
        
        while (getline(ss, value, ',')) {
            try {
                row.push_back(stof(value));
            } catch (...) {
                row.push_back(0.0f);
            }
        }
        
        if (row.size() != expected_features) {
            cout << "WARNING: Line " << line_num
                         << " has " << row.size()
                         << " features, expected " << expected_features
                         << ". Skipping..." << endl;
            continue;
        }
        
        data.push_back(row);
    }
    
    file.close();
    return data;
}

int main() {
    cout << "=====================================================" << endl;
    cout << "           CatBoost C++ Inference" << endl;
    cout << "=====================================================" << endl;
    
    //handle модели
    ModelCalcerHandle* model = ModelCalcerCreate();
    if (!model) {
        cerr << "ERROR: Cannot create model handle!" << endl;
        system("pause");
        return 1;
    }
    cout << "[OK] Model handle created" << endl;
    
    //Загрузка модели
    const char* model_path = "catboost_model_classifier.cbm";
    if (!LoadFullModelFromFile(model, model_path)) {
        cerr << "ERROR loading model: " << GetErrorString() << endl;
        ModelCalcerDelete(model);
        system("pause");
        return 1;
    }
    cout << "[OK] Model loaded: " << model_path << endl;
    
    size_t num_features = GetFloatFeaturesCount(model);
    size_t num_cat_features = GetCatFeaturesCount(model);
    size_t num_classes = GetDimensionsCount(model);
    size_t num_trees = GetTreeCount(model);
    
    cout << "\n----- Model Info -----" << endl;
    cout << "Float Features: " << num_features << endl;
    cout << "Cat Features:   " << num_cat_features << endl;
    cout << "Classes:        " << num_classes << endl;
    cout << "Trees:          " << num_trees << endl;
    cout << "----------------------" << endl;
    
    const char* class_names[] = {"Benign", "PortScan", "DoS", "DDoS"};

    
    //CSV и batch inference
    cout << "\n===== Batch Inference on test_data.csv =====" << endl;
    
    auto test_data = load_csv("test_data.csv", num_features);
    
    if (test_data.empty()) {
        cout << "No test data loaded. Skipping batch inference." << endl;
    } else {
        size_t num_samples = min(test_data.size(), size_t(20));
        cout << "Loaded " << test_data.size() << " samples from CSV" << endl;
        cout << "Running inference on first " << num_samples << " samples..." << endl;
        cout << string(60, '-') << endl;
        
        double total_time = 0.0;
        vector<int> predicted_classes;
        
        for (size_t i = 0; i < num_samples; ++i) {
            const float* sample_ptr = test_data[i].data();
            vector<double> preds(num_classes);
            
            auto t1 = high_resolution_clock::now();
            
            CalcModelPrediction(
                model, 1, &sample_ptr, num_features,
                nullptr, num_cat_features,
                preds.data(), num_classes
            );
            
            auto t2 = high_resolution_clock::now();
            double elapsed = duration<double, milli>(t2 - t1).count();
            total_time += elapsed;
            
            auto probs = softmax(preds);
            int pred_class = distance(probs.begin(),
                                              max_element(probs.begin(), probs.end()));
            predicted_classes.push_back(pred_class);
            
            cout << "Sample #" << setw(2) << (i+1) << ": "
                         << setw(12) << class_names[pred_class]
                         << " (conf: " << fixed << setprecision(2)
                         << probs[pred_class] * 100.0 << "%, "
                         << "time: " << setprecision(3) << elapsed << " ms)" << endl;
        }
        
        cout << string(60, '-') << endl;
        cout << "\n----- Statistics -----" << endl;
        cout << "Average inference time: " << fixed << setprecision(4)
                     << (total_time / num_samples) << " ms" << endl;
        cout << "Throughput: ~" << fixed << setprecision(0)
                     << (1000.0 / (total_time / num_samples)) << " predictions/sec" << endl;
        
        cout << "\nClass distribution in predictions:" << endl;
        for (size_t c = 0; c < num_classes; ++c) {
            int count = std::count(predicted_classes.begin(), predicted_classes.end(), c);
            double percent = (count * 100.0) / num_samples;
            cout << "  " << setw(12) << class_names[c] << ": "
                         << count << " (" << fixed << setprecision(1)
                         << percent << "%)" << endl;
        }
    }
    
    ModelCalcerDelete(model);
    cout << "\n[OK] Model deleted" << endl;
    cout << "=====================================================" << endl;
    
    system("pause");
    return 0;
}